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
#include "QGCMapEngine.h"
#include "QGCFileDownload.h"
#include "QGeoTileFetcherQGC.h"

#include <QtLocation/private/qgeotilespec_p.h>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QSslError>
#include <QtCore/QFile>

Q_LOGGING_CATEGORY(QGeoMultiLayerMapReplyQGCLog,
                   "qgc.qtlocationplugin.qgeomultilayermapreplyqgc")

QGeoMultiLayerMapReplyQGC::QGeoMultiLayerMapReplyQGC(
    QNetworkAccessManager *networkManager,
    const QGeoTileSpec &spec,
    const MapLayerStack &layerStack,
    QObject *parent)
    // 使用延迟初始化构造函数，避免父类自动从缓存获取
    : QGeoTiledMapReplyQGC(networkManager, spec, parent)
    , _layerStack(layerStack)
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

    _pendingReplies = 0;

    // 首先尝试从合成瓦片缓存获取（即使只有一个图层，如果之前缓存过 composite 瓦片也应该尝试）
    QString layerStackKey = _layerStack.generateCacheKey();
    if (!layerStackKey.isEmpty()) {
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

    _pendingReplies = 0;

    // 首先尝试从缓存获取单个图层
    for (const MapLayer &layer : _visibleLayers) {
        const SharedMapProvider provider = UrlFactory::getMapProviderFromQtMapId(layer.mapId());
        if (!provider) {
            continue;
        }

        // 检查缩放级别
        if (zoom > provider->maximumZoomLevel() ||
            zoom < provider->minimumZoomLevel()) {
            continue;
        }

        // 尝试从缓存获取
        QString providerType = UrlFactory::getProviderTypeFromQtMapId(layer.mapId());
        QGCFetchTileTask *task = QGeoFileTileCacheQGC::createFetchTileTask(providerType, x, y, zoom);
        
        if (task) {
            _cacheTasks.insert(layer.mapId(), task);
            (void)connect(task, &QGCFetchTileTask::tileFetched, this,
                           &QGeoMultiLayerMapReplyQGC::_cacheReply);
            (void)connect(task, &QGCMapTask::error, this,
                           &QGeoMultiLayerMapReplyQGC::_cacheError);
            getQGCMapEngine()->addTask(task);
            _pendingReplies++;
        }
    }

    // 如果没有缓存任务，直接开始网络请求
    if (_pendingReplies == 0) {
        for (const MapLayer &layer : _visibleLayers) {
            const SharedMapProvider provider = UrlFactory::getMapProviderFromQtMapId(layer.mapId());
            if (!provider) {
                continue;
            }

            if (zoom > provider->maximumZoomLevel() ||
                zoom < provider->minimumZoomLevel()) {
                continue;
            }

            _createLayerNetworkRequest(layer.mapId(), x, y, zoom);
        }
    }
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
    task->deleteLater();

    // 检查是否全部完成
    _pendingReplies--;
    if (_pendingReplies == 0) {
        _compositeTiles();
    }
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

    // 缓存未命中，发起网络请求
    const QGeoTileSpec &spec = tileSpec();
    const MapLayer *layer = nullptr;
    for (const MapLayer &l : _visibleLayers) {
        if (l.mapId() == mapId) {
            layer = &l;
            break;
        }
    }

    if (layer) {
        _createLayerNetworkRequest(mapId, spec.x(), spec.y(), spec.zoom());
    }

    _cacheTasks.remove(mapId);
    task->deleteLater();
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
        // 处理失败，检查错误类型
        _pendingReplies--;
        if (_pendingReplies == 0) {
            if (replyError != QNetworkReply::NoError) {
                setError(QGeoTiledMapReply::CommunicationError, errorString);
            } else {
                setError(QGeoTiledMapReply::ParseError, tr("Failed to process tile"));
            }
        }
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

    // 检查是否全部完成
    _pendingReplies--;
    if (_pendingReplies == 0) {
        _compositeTiles();
    }
}

void QGeoMultiLayerMapReplyQGC::_networkReplyError(QNetworkReply::NetworkError error) {
    if (error != QNetworkReply::OperationCanceledError) {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (reply) {
            // 保存错误信息，避免在 reply 被删除后访问
            QString errorString = reply->errorString();
            
            // 找到对应的图层并清理
            int mapId = -1;
            for (auto it = _replies.begin(); it != _replies.end(); ++it) {
                if (it.value() == reply) {
                    mapId = it.key();
                    break;
                }
            }
            
            if (mapId >= 0) {
                reply->deleteLater();
                _replies.remove(mapId);
            }
            
            _pendingReplies--;
            if (_pendingReplies == 0) {
                setError(QGeoTiledMapReply::CommunicationError, errorString);
            }
        }
    }
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

    // 准备合成数据：只包含有效的瓦片
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

    // 缓存合成后的瓦片
    const QGeoTileSpec &spec = tileSpec();
    QString cacheKey = _layerStack.generateCacheKey();
    if (!cacheKey.isEmpty()) {
        QGeoFileTileCacheQGC::cacheCompositeTile(cacheKey, spec.x(), spec.y(), spec.zoom(),
                                                  compositeResult.imageData, compositeResult.format);
    }

    setFinished(true);
}

void QGeoMultiLayerMapReplyQGC::_createLayerNetworkRequest(int mapId, int x, int y, int zoom) {
    QNetworkRequest request = QGeoTileFetcherQGC::getNetworkRequest(mapId, x, y, zoom);
    if (request.url().isEmpty()) {
        return;
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
        _pendingReplies++;
    }
}

