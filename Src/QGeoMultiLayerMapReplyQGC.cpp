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

#include <QtLocation/private/qgeotilespec_p.h>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QSslError>
#include <QtCore/QFile>

Q_LOGGING_CATEGORY(QGeoMultiLayerMapReplyQGCLog,
                   "qgc.qtlocationplugin.qgeomultilayermapreplyqgc")

QByteArray QGeoMultiLayerMapReplyQGC::_bingNoTileImage;
QByteArray QGeoMultiLayerMapReplyQGC::_badTile;

QGeoMultiLayerMapReplyQGC::QGeoMultiLayerMapReplyQGC(
    QNetworkAccessManager *networkManager,
    const QGeoTileSpec &spec,
    const MapLayerStack &layerStack,
    QObject *parent)
    : QGeoTiledMapReply(spec, parent)
    , _networkManager(networkManager)
    , _layerStack(layerStack)
{
    _initDataFromResources();

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

    // 连接错误处理
    (void)connect(
        this, &QGeoMultiLayerMapReplyQGC::errorOccurred, this,
        [this](QGeoTiledMapReply::Error error, const QString &errorString) {
            qCWarning(QGeoMultiLayerMapReplyQGCLog) << error << errorString;
            setMapImageData(_badTile);
            setMapImageFormat("png");
            setCached(false);
        },
        Qt::AutoConnection);

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
    QGeoTiledMapReply::abort();
}

void QGeoMultiLayerMapReplyQGC::_initDataFromResources() {
    if (_bingNoTileImage.isEmpty()) {
        QFile file(":/res/NoTileBytes.dat");
        if (file.open(QFile::ReadOnly)) {
            _bingNoTileImage = file.readAll();
            file.close();
        }
    }

    if (_badTile.isEmpty()) {
        QFile file(":/res/notile.png");
        if (file.open(QFile::ReadOnly)) {
            _badTile = file.readAll();
            file.close();
        }
    }
}

void QGeoMultiLayerMapReplyQGC::_startFetching() {
    const QGeoTileSpec &spec = tileSpec();
    int x = spec.x();
    int y = spec.y();
    int zoom = spec.zoom();

    _pendingReplies = 0;

    // 首先尝试从合成瓦片缓存获取
    QString layerStackKey = _layerStack.generateCacheKey();
    if (!layerStackKey.isEmpty() && _visibleLayers.count() > 1) {
        QGCFetchTileTask *compositeTask = QGeoFileTileCacheQGC::createFetchCompositeTileTask(
            layerStackKey, x, y, zoom);
        if (compositeTask) {
            (void)connect(compositeTask, &QGCFetchTileTask::tileFetched, this,
                           [this](QGCCacheTile *tile) {
                               if (tile) {
                                   setMapImageData(tile->img());
                                   setMapImageFormat(tile->format());
                                   setCached(true);
                                   setFinished(true);
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

            QNetworkRequest request = _createRequest(layer.mapId(), x, y, zoom);
            if (request.url().isEmpty()) {
                continue;
            }

            QNetworkReply *reply = _networkManager->get(request);
            reply->setParent(this);
            QGCFileDownload::setIgnoreSSLErrorsIfNeeded(*reply);

            _replies.insert(layer.mapId(), reply);
            _pendingReplies++;

            (void)connect(reply, &QNetworkReply::finished, this,
                           &QGeoMultiLayerMapReplyQGC::_networkReplyFinished);
            (void)connect(reply, &QNetworkReply::errorOccurred, this,
                           &QGeoMultiLayerMapReplyQGC::_networkReplyError);
            (void)connect(reply, &QNetworkReply::sslErrors, this,
                           &QGeoMultiLayerMapReplyQGC::_networkReplySslErrors);
            (void)connect(this, &QGeoMultiLayerMapReplyQGC::aborted, reply,
                           &QNetworkReply::abort);
        }
    }
}

QNetworkRequest QGeoMultiLayerMapReplyQGC::_createRequest(int mapId, int x, int y, int zoom) {
    const SharedMapProvider mapProvider = UrlFactory::getMapProviderFromQtMapId(mapId);
    if (!mapProvider) {
        return QNetworkRequest();
    }

    QNetworkRequest request;
    request.setUrl(mapProvider->getTileURL(x, y, zoom));
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("*/*"));
    
#if defined Q_OS_MACOS
    static constexpr const char* s_userAgent = "Mozilla/5.0 (Macintosh; Intel Mac OS X 14.5; rv:125.0) Gecko/20100101 Firefox/125.0";
#elif defined Q_OS_WIN
    static constexpr const char* s_userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:100.0) Gecko/20100101 Firefox/112.0";
#elif defined Q_OS_ANDROID
    static constexpr const char* s_userAgent = "Mozilla/5.0 (Android 13; Tablet; rv:68.0) Gecko/68.0 Firefox/112.0";
#elif defined Q_OS_LINUX
    static constexpr const char* s_userAgent = "Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/112.0";
#else
    static constexpr const char* s_userAgent = "Qt Location based application";
#endif
    
    request.setHeader(QNetworkRequest::UserAgentHeader, s_userAgent);
    const QByteArray referrer = mapProvider->getReferrer().toUtf8();
    if (!referrer.isEmpty()) {
        request.setRawHeader(QByteArrayLiteral("Referrer"), referrer);
    }
    const QByteArray token = mapProvider->getToken();
    if (!token.isEmpty()) {
        request.setRawHeader(QByteArrayLiteral("User-Token"), token);
    }
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::PreferCache);
    request.setAttribute(QNetworkRequest::BackgroundRequestAttribute, true);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);
    request.setAttribute(QNetworkRequest::DoNotBufferUploadDataAttribute, false);
    request.setPriority(QNetworkRequest::NormalPriority);
    request.setTransferTimeout(10000);

    return request;
}

void QGeoMultiLayerMapReplyQGC::_cacheReply(QGCCacheTile *tile) {
    QGCFetchTileTask *task = qobject_cast<QGCFetchTileTask*>(sender());
    if (!task || !tile) {
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

    // 存储瓦片数据
    TileImageData tileData;
    tileData.imageData = tile->img();
    tileData.format = tile->format();
    tileData.isValid = true;
    _tiles.insert(mapId, tileData);

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
        QNetworkRequest request = _createRequest(mapId, spec.x(), spec.y(), spec.zoom());
        if (!request.url().isEmpty()) {
            QNetworkReply *reply = _networkManager->get(request);
            reply->setParent(this);
            QGCFileDownload::setIgnoreSSLErrorsIfNeeded(*reply);

            _replies.insert(mapId, reply);
            _pendingReplies++;

            (void)connect(reply, &QNetworkReply::finished, this,
                           &QGeoMultiLayerMapReplyQGC::_networkReplyFinished);
            (void)connect(reply, &QNetworkReply::errorOccurred, this,
                           &QGeoMultiLayerMapReplyQGC::_networkReplyError);
            (void)connect(reply, &QNetworkReply::sslErrors, this,
                           &QGeoMultiLayerMapReplyQGC::_networkReplySslErrors);
            (void)connect(this, &QGeoMultiLayerMapReplyQGC::aborted, reply,
                           &QNetworkReply::abort);
        }
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

    reply->deleteLater();
    _replies.remove(mapId);

    if (reply->error() != QNetworkReply::NoError) {
        _pendingReplies--;
        if (_pendingReplies == 0) {
            // 所有请求都失败了
            setError(QGeoTiledMapReply::CommunicationError, reply->errorString());
        }
        return;
    }

    if (!reply->isOpen()) {
        _pendingReplies--;
        if (_pendingReplies == 0) {
            setError(QGeoTiledMapReply::ParseError, tr("Empty Reply"));
        }
        return;
    }

    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if ((statusCode < SUCCESS_OK) ||
        (statusCode >= REDIRECTION_MULTIPLE_CHOICES)) {
        _pendingReplies--;
        if (_pendingReplies == 0) {
            setError(QGeoTiledMapReply::CommunicationError,
                     reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute)
                         .toString());
        }
        return;
    }

    QByteArray image = reply->readAll();
    if (image.isEmpty()) {
        _pendingReplies--;
        if (_pendingReplies == 0) {
            setError(QGeoTiledMapReply::ParseError, tr("Image is Empty"));
        }
        return;
    }

    const SharedMapProvider mapProvider =
        UrlFactory::getMapProviderFromQtMapId(mapId);
    if (!mapProvider) {
        _pendingReplies--;
        if (_pendingReplies == 0) {
            setError(QGeoTiledMapReply::UnknownError, tr("Invalid Provider"));
        }
        return;
    }

    if (mapProvider->isBingProvider() && (image == _bingNoTileImage)) {
        _pendingReplies--;
        if (_pendingReplies == 0) {
            setError(QGeoTiledMapReply::CommunicationError,
                     tr("Bing Tile Above Zoom Level"));
        }
        return;
    }

    if (mapProvider->isElevationProvider()) {
        const SharedElevationProvider elevationProvider =
            std::dynamic_pointer_cast<const ElevationProvider>(mapProvider);
        image = elevationProvider->serialize(image);
        if (image.isEmpty()) {
            _pendingReplies--;
            if (_pendingReplies == 0) {
                setError(QGeoTiledMapReply::ParseError,
                         tr("Failed to Serialize Terrain Tile"));
            }
            return;
        }
    }

    // 存储瓦片数据
    TileImageData tileData;
    tileData.imageData = image;
    tileData.format = mapProvider->getImageFormat(image);
    tileData.isValid = !tileData.format.isEmpty();
    _tiles.insert(mapId, tileData);

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
            _pendingReplies--;
            if (_pendingReplies == 0) {
                setError(QGeoTiledMapReply::CommunicationError, reply->errorString());
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

    // 准备合成数据
    QList<MapLayer> layers;
    QList<TileImageData> tiles;

    for (const MapLayer &layer : _visibleLayers) {
        if (_tiles.contains(layer.mapId())) {
            layers.append(layer);
            tiles.append(_tiles.value(layer.mapId()));
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
        setMapImageData(tile.imageData);
        setMapImageFormat(tile.format);
        setCached(false);
        setFinished(true);
        return;
    }

    // 进行合成
    TileImageData compositeResult = TileCompositor::composite(layers, tiles);
    
    if (!compositeResult.isValid) {
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

