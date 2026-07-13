/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "QGeoMultiLayerMapReplyQGC.h"
#include "MapProvider.h"
#include "ElevationMapProvider.h"
#include "QGCMapUrlEngine.h"
#include "QGeoFileTileCacheQGC.h"
#include "QGCTileSyncReader.h"
#include "QGCMapEngine.h"
#include "QGCFileDownload.h"
#include "QGeoTileFetcherQGC.h"

#include <QtLocation/private/qgeotilespec_p.h>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QSslError>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <utility>

Q_LOGGING_CATEGORY(QGeoMultiLayerMapReplyQGCLog,
                   "qgc.qtlocationplugin.qgeomultilayermapreplyqgc")

QGeoMultiLayerMapReplyQGC::QGeoMultiLayerMapReplyQGC(
    QNetworkAccessManager *networkManager,
    const QGeoTileSpec &spec,
    const MapLayerStack &layerStack,
    int compositeMapId,
    QObject *parent)
    // 使用延迟初始化构造函数，避免父类自动从缓存获取
    : QGeoTiledMapReplyQGC(networkManager, spec, parent)
    , _layerStack(layerStack)
    , _compositeMapId(compositeMapId > 0 ? compositeMapId : layerStack.generateMapId())
{
    // 过滤可见图层
    QList<MapLayer> allLayers = _layerStack.layers();
    for (const MapLayer &layer : allLayers) {
        if (layer.visible()) {
            _visibleLayers.append(layer);
        }
    }

    if (_visibleLayers.isEmpty()) {
        setError(QGeoTiledMapReply::UnknownError, tr("No visible layers"));
        setFinished(true);
        return;
    }

    // 开始获取瓦片
    _startFetching();
}

QGeoMultiLayerMapReplyQGC::~QGeoMultiLayerMapReplyQGC() {
    // 清理所有回复
    for (QNetworkReply *reply : _replies) {
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
    }
    _replies.clear();
}

void QGeoMultiLayerMapReplyQGC::abort() {
    // 中止所有网络请求
    for (QNetworkReply *reply : _replies) {
        if (reply) {
            reply->abort();
        }
    }
    // 调用父类的 abort
    QGeoTiledMapReplyQGC::abort();
}

void QGeoMultiLayerMapReplyQGC::_startFetching() {
    const QGeoTileSpec &spec = tileSpec();
    int x = spec.x();
    int y = spec.y();
    int zoom = spec.zoom();

    // 首先尝试从文件系统（providers 文件夹）检查合成瓦片缓存
    // 文件名使用完整图层栈摘要，避免短 mapId 碰撞污染缓存。
    const QString layerStackKey = _layerStack.generateCacheKey();
    if (!layerStackKey.isEmpty()) {
        QString cachePath = QGeoFileTileCacheQGC::getCachePath() + QLatin1String("/providers");
        // 尝试常见的图片格式
        QStringList formats = {"png", "jpg", "jpeg"};
        for (const QString &format : formats) {
            QString filename = QString("composite-%1-%2-%3-%4.%5")
                              .arg(layerStackKey)
                              .arg(zoom)
                              .arg(x)
                              .arg(y)
                              .arg(format);
            QString filePath = cachePath + "/" + filename;
            
            QFile file(filePath);
            if (file.exists() && file.open(QIODevice::ReadOnly)) {
                QByteArray imageData = file.readAll();
                file.close();
                
                if (!imageData.isEmpty()) {
                    // 从文件系统读取成功，设置数据并完成
                    setMapImageData(imageData);
                    setMapImageFormat(format);
                    setCached(true);
                    setFinished(true);
                    
                    // 同时更新数据库缓存（异步，不阻塞）
                    QGeoFileTileCacheQGC::cacheCompositeTile(layerStackKey, x, y, zoom,
                                                              imageData, format);
                    return;
                }
            }
        }
    }

    // 文件系统未命中，尝试从数据库获取合成瓦片缓存
    if (!layerStackKey.isEmpty()) {
        const QString compositeHash =
            QStringLiteral("composite_%1_%2_%3_%4")
                .arg(layerStackKey)
                .arg(x, 8, 10, QChar('0'))
                .arg(y, 8, 10, QChar('0'))
                .arg(zoom, 3, 10, QChar('0'));
        QByteArray compositeImage;
        QString compositeFormat;
        if (QGCTileSyncReader::tryFetchTile(QGeoFileTileCacheQGC::getDatabaseFilePath(),
                                            compositeHash, &compositeImage,
                                            &compositeFormat)) {
            setMapImageData(compositeImage);
            setMapImageFormat(compositeFormat);
            setCached(true);
            setFinished(true);
            return;
        }

        QGCFetchTileTask *compositeTask = QGeoFileTileCacheQGC::createFetchCompositeTileTask(
            layerStackKey, x, y, zoom);
        if (compositeTask) {
            (void)connect(compositeTask, &QGCFetchTileTask::tileFetched, this,
                           [this](QGCCacheTile *tile) {
                               if (tile) {
                                   QByteArray imgData = tile->img();
                                   QString imgFormat = tile->format();
                                   if (!imgData.isEmpty() && !imgFormat.isEmpty()) {
                                       setMapImageData(imgData);
                                       setMapImageFormat(imgFormat);
                                       setCached(true);
                                       setFinished(true);
                                   } else {
                                       qCWarning(QGeoMultiLayerMapReplyQGCLog) << "Invalid composite tile data";
                                       setError(QGeoTiledMapReply::ParseError, tr("Invalid composite tile data"));
                                       setFinished(true);
                                   }
                                   delete tile;
                               } else {
                                   _startFetchingLayers();
                               }
                           });
            (void)connect(compositeTask, &QGCMapTask::error, this,
                           [this](QGCMapTask::TaskType type, const QString &errorString) {
                               Q_UNUSED(type);
                               Q_UNUSED(errorString);
                               // 缓存未命中，继续获取单个图层
                               _startFetchingLayers();
                           });
            getQGCMapEngine()->addTask(compositeTask);
            return;  // 等待缓存结果
        }
    }

    // 缓存未命中或单图层，获取单个图层
    _startFetchingLayers();
}

void QGeoMultiLayerMapReplyQGC::_startFetchingLayers() {
    const QGeoTileSpec &spec = tileSpec();
    int x = spec.x();
    int y = spec.y();
    int zoom = spec.zoom();

    _layerStates.clear();
    _layerErrors.clear();
    _tiles.clear();

    // 每个图层只占一个生命周期槽位。缓存未命中转网络不会改变槽位数量。
    for (const MapLayer &layer : _visibleLayers) {
        const SharedMapProvider provider = UrlFactory::getMapProviderFromQtMapId(layer.mapId());
        if (!provider || zoom > provider->maximumZoomLevel() ||
            zoom < provider->minimumZoomLevel()) {
            _layerStates.insert(layer.mapId(), LayerState::Failed);
            _layerErrors.append(tr("Layer %1 is unavailable at zoom %2")
                                    .arg(layer.mapId()).arg(zoom));
            continue;
        }
        _layerStates.insert(layer.mapId(), LayerState::CachePending);
    }

    // 首先尝试从缓存获取单个图层
    for (const MapLayer &layer : _visibleLayers) {
        if (_layerStates.value(layer.mapId(), LayerState::Failed) !=
            LayerState::CachePending) {
            continue;
        }
        const SharedMapProvider provider = UrlFactory::getMapProviderFromQtMapId(layer.mapId());
        Q_ASSERT(provider);

        // 尝试从缓存获取
        QString providerType = UrlFactory::getProviderTypeFromQtMapId(layer.mapId());
        QGCFetchTileTask *task = QGeoFileTileCacheQGC::createFetchTileTask(providerType, x, y, zoom);
        
        if (task) {
            _cacheTasks.insert(layer.mapId(), task);
            (void)connect(task, &QGCFetchTileTask::tileFetched, this,
                          [this, mapId = layer.mapId()](QGCCacheTile *tile) {
                              _handleCacheReply(mapId, tile);
                          });
            (void)connect(task, &QGCMapTask::error, this,
                          [this, mapId = layer.mapId()](QGCMapTask::TaskType type,
                                                        const QString &errorString) {
                              Q_UNUSED(type);
                              Q_UNUSED(errorString);
                              _handleCacheError(mapId);
                          });
            getQGCMapEngine()->addTask(task);
        } else {
            _handleCacheError(layer.mapId());
        }
    }
    _finishIfAllLayersComplete();
}

void QGeoMultiLayerMapReplyQGC::_cacheReply(QGCCacheTile *tile) {
    QGCFetchTileTask *task = qobject_cast<QGCFetchTileTask*>(sender());
    if (!task || !tile) {
        if (tile) {
            delete tile;
        }
        return;
    }

    // 找到对应的图层
    int mapId = -1;
    for (auto it = _cacheTasks.begin(); it != _cacheTasks.end(); ++it) {
        if (it.value() == task) {
            mapId = it.key();
            break;
        }
    }

    if (mapId < 0) {
        delete tile;
        return;
    }

    _handleCacheReply(mapId, tile);
}

void QGeoMultiLayerMapReplyQGC::_handleCacheReply(int mapId, QGCCacheTile *tile) {
    if (!tile) {
        return;
    }

    // 存储瓦片数据（在删除 tile 之前保存数据）
    TileImageData tileData;
    tileData.imageData = tile->img();
    tileData.format = tile->format();
    tileData.isValid = !tileData.imageData.isEmpty() && !tileData.format.isEmpty();

    if (tileData.isValid) {
        _tiles.insert(mapId, tileData);
    }

    // 清理
    _cacheTasks.remove(mapId);
    delete tile;

    _finishLayer(mapId, tileData.isValid, tr("Invalid cached tile for layer %1").arg(mapId));
}

void QGeoMultiLayerMapReplyQGC::_cacheError(QGCMapTask::TaskType type,
                                             QStringView errorString) {
    Q_UNUSED(errorString);
    Q_UNUSED(type);

    QGCFetchTileTask *task = qobject_cast<QGCFetchTileTask*>(sender());
    if (!task) {
        return;
    }

    // 找到对应的图层
    int mapId = -1;
    for (auto it = _cacheTasks.begin(); it != _cacheTasks.end(); ++it) {
        if (it.value() == task) {
            mapId = it.key();
            break;
        }
    }

    if (mapId < 0) {
        return;
    }

    _handleCacheError(mapId);
}

void QGeoMultiLayerMapReplyQGC::_handleCacheError(int mapId) {
    if (_layerStates.value(mapId, LayerState::Failed) != LayerState::CachePending) {
        return;
    }

    // 缓存未命中，发起网络请求
    const QGeoTileSpec &spec = tileSpec();
    const MapLayer *layer = nullptr;
    for (const MapLayer &l : _visibleLayers) {
        if (l.mapId() == mapId) {
            layer = &l;
            break;
        }
    }

    _cacheTasks.remove(mapId);
    _layerStates[mapId] = LayerState::NetworkPending;
    if (!layer || !_createLayerNetworkRequest(mapId, spec.x(), spec.y(), spec.zoom())) {
        _finishLayer(mapId, false, tr("Unable to create request for layer %1").arg(mapId));
    }
}

void QGeoMultiLayerMapReplyQGC::_networkReplyFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    // 找到对应的图层
    int mapId = -1;
    for (auto it = _replies.begin(); it != _replies.end(); ++it) {
        if (it.value() == reply) {
            mapId = it.key();
            break;
        }
    }

    if (mapId < 0) {
        reply->deleteLater();
        return;
    }

    // 在调用 processNetworkReply 之前保存所有需要的信息
    // 因为 processNetworkReply 内部会读取 reply 的数据
    // 注意：必须在 deleteLater() 之前保存，避免悬空指针访问
    QNetworkReply::NetworkError replyError = reply->error();
    QString errorString = reply->errorString();
    
    QByteArray image;
    QString format;
    bool processSuccess = processNetworkReply(reply, mapId, image, format);
    
    // 现在可以安全地删除 reply
    reply->deleteLater();
    _replies.remove(mapId);

    // 处理结果
    if (!processSuccess) {
        const QString failure = replyError != QNetworkReply::NoError
            ? errorString : tr("Failed to process layer %1").arg(mapId);
        _finishLayer(mapId, false, failure);
        return;
    }

    // 存储瓦片数据
    TileImageData tileData;
    tileData.imageData = image;
    tileData.format = format;
    tileData.isValid = !image.isEmpty() && !format.isEmpty();
    if (tileData.isValid) {
        _tiles.insert(mapId, tileData);
    }

    // 缓存单个图层的瓦片（与父类行为一致）
    const SharedMapProvider mapProvider = UrlFactory::getMapProviderFromQtMapId(mapId);
    if (mapProvider && !image.isEmpty() && !format.isEmpty()) {
        const QGeoTileSpec &spec = tileSpec();
        QGeoFileTileCacheQGC::cacheTile(mapProvider->getMapName(), spec.x(), spec.y(), spec.zoom(), image, format);
    }

    _finishLayer(mapId, tileData.isValid,
                 tr("Invalid network tile for layer %1").arg(mapId));
}

void QGeoMultiLayerMapReplyQGC::_networkReplyError(QNetworkReply::NetworkError error) {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    int mapId = -1;
    for (auto it = _replies.begin(); it != _replies.end(); ++it) {
        if (it.value() == reply) {
            mapId = it.key();
            break;
        }
    }

    if (mapId < 0) {
        return;
    }

    const QString errorString = error == QNetworkReply::OperationCanceledError
        ? tr("Layer %1 request canceled").arg(mapId) : reply->errorString();
    reply->deleteLater();
    _replies.remove(mapId);
    _finishLayer(mapId, false, errorString);
}

void QGeoMultiLayerMapReplyQGC::_finishLayer(int mapId, bool success,
                                              const QString &errorString) {
    const LayerState state = _layerStates.value(mapId, LayerState::Failed);
    if (state == LayerState::Succeeded || state == LayerState::Failed) {
        return;
    }

    _layerStates[mapId] = success ? LayerState::Succeeded : LayerState::Failed;
    if (!success && !errorString.isEmpty()) {
        _layerErrors.append(errorString);
    }
    _finishIfAllLayersComplete();
}

void QGeoMultiLayerMapReplyQGC::_finishIfAllLayersComplete() {
    if (_layerStates.isEmpty()) {
        setError(QGeoTiledMapReply::UnknownError, tr("No fetchable layers"));
        setFinished(true);
        return;
    }

    for (LayerState state : std::as_const(_layerStates)) {
        if (state == LayerState::CachePending || state == LayerState::NetworkPending) {
            return;
        }
    }

    for (LayerState state : std::as_const(_layerStates)) {
        if (state == LayerState::Failed) {
            setError(QGeoTiledMapReply::CommunicationError,
                     _layerErrors.isEmpty() ? tr("One or more layers failed")
                                            : _layerErrors.join(QLatin1String("; ")));
            setFinished(true);
            return;
        }
    }

    _compositeTiles();
}

void QGeoMultiLayerMapReplyQGC::_networkReplySslErrors(const QList<QSslError> &errors) {
    QString errorString;
    for (const QSslError &error : errors) {
        if (!errorString.isEmpty()) {
            (void)errorString.append('\n');
        }
        (void)errorString.append(error.errorString());
    }

    if (!errorString.isEmpty()) {
        setError(QGeoTiledMapReply::CommunicationError, errorString);
    }
}

void QGeoMultiLayerMapReplyQGC::_compositeTiles() {
    if (_compositing) {
        return;
    }
    _compositing = true;

    // 所有可见图层必须成功，禁止将临时缺层结果写入完整配置缓存。
    QList<MapLayer> layers;
    QList<TileImageData> tiles;

    // 验证 _visibleLayers 和 _tiles 的有效性
    if (_visibleLayers.isEmpty()) {
        setError(QGeoTiledMapReply::UnknownError, tr("No visible layers"));
        setFinished(true);
        return;
    }

    for (const MapLayer &layer : _visibleLayers) {
        // 验证 layer 的有效性
        if (layer.mapId() < 0) {
            continue;
        }

        if (_tiles.contains(layer.mapId())) {
            const TileImageData &tileData = _tiles.value(layer.mapId());
            // 只添加有效的瓦片数据
            if (tileData.isValid && !tileData.imageData.isEmpty() && !tileData.format.isEmpty()) {
                layers.append(layer);
                tiles.append(tileData);
            }
        }
    }

    if (layers.count() != _visibleLayers.count()) {
        setError(QGeoTiledMapReply::UnknownError, tr("Incomplete layer set"));
        setFinished(true);
        return;
    }

    if (layers.isEmpty() || tiles.isEmpty()) {
        setError(QGeoTiledMapReply::UnknownError, tr("No valid tiles to composite"));
        setFinished(true);
        return;
    }

    // 如果只有一个图层，直接使用
    if (layers.count() == 1) {
        const TileImageData &tile = tiles.first();
        if (tile.isValid && !tile.imageData.isEmpty() && !tile.format.isEmpty()) {
            setMapImageData(tile.imageData);
            setMapImageFormat(tile.format);
            setCached(false);
            setFinished(true);
        } else {
            setError(QGeoTiledMapReply::ParseError, tr("Invalid tile data"));
            setFinished(true);
        }
        return;
    }

    // 进行合成
    TileImageData compositeResult = TileCompositor::composite(layers, tiles);
    
    if (!compositeResult.isValid || compositeResult.imageData.isEmpty() || compositeResult.format.isEmpty()) {
        setError(QGeoTiledMapReply::ParseError, tr("Failed to composite tiles"));
        setFinished(true);
        return;
    }

    // 设置合成结果
    setMapImageData(compositeResult.imageData);
    setMapImageFormat(compositeResult.format);
    setCached(false);

    // 缓存合成后的瓦片（数据库）
    const QGeoTileSpec &spec = tileSpec();
    QString cacheKey = _layerStack.generateCacheKey();
    if (!cacheKey.isEmpty()) {
        QGeoFileTileCacheQGC::cacheCompositeTile(cacheKey, spec.x(), spec.y(), spec.zoom(),
                                                  compositeResult.imageData, compositeResult.format);
    }

    // 文件缓存与数据库缓存使用同一个完整配置摘要。
    if (!cacheKey.isEmpty()) {
        QString cachePath = QGeoFileTileCacheQGC::getCachePath() + QLatin1String("/providers");
        QDir cacheDir;
        if (cacheDir.mkpath(cachePath)) {
            QString filename = QString("composite-%1-%2-%3-%4.%5")
                              .arg(cacheKey)
                              .arg(spec.zoom())
                              .arg(spec.x())
                              .arg(spec.y())
                              .arg(compositeResult.format);
            QString filePath = cachePath + "/" + filename;
            
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                file.write(compositeResult.imageData);
                file.close();
            } else {
                qCWarning(QGeoMultiLayerMapReplyQGCLog) << "Failed to save composite tile to file:" << filePath << file.errorString();
            }
        }
    }

    setFinished(true);
}

bool QGeoMultiLayerMapReplyQGC::_createLayerNetworkRequest(int mapId, int x, int y, int zoom) {
    QNetworkRequest request = QGeoTileFetcherQGC::getNetworkRequest(mapId, x, y, zoom);
    if (request.url().isEmpty()) {
        return false;
    }

    // 复用父类方法创建网络请求（不连接父类信号）
    QNetworkReply *reply = createNetworkRequest(request, false);
    if (reply) {
        // 连接信号到子类方法
        (void)connect(reply, &QNetworkReply::finished, this,
                       &QGeoMultiLayerMapReplyQGC::_networkReplyFinished);
        (void)connect(reply, &QNetworkReply::errorOccurred, this,
                       &QGeoMultiLayerMapReplyQGC::_networkReplyError);
        (void)connect(reply, &QNetworkReply::sslErrors, this,
                       &QGeoMultiLayerMapReplyQGC::_networkReplySslErrors);
        (void)connect(this, &QGeoMultiLayerMapReplyQGC::aborted, reply,
                       &QNetworkReply::abort);

        _replies.insert(mapId, reply);
        return true;
    }
    return false;
}

