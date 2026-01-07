/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "QGeoTileFetcherQGC.h"
#include "MapProvider.h"
#include "QGCMapUrlEngine.h"
#include "QGeoMapReplyQGC.h"
#include "QGeoMultiLayerMapReplyQGC.h"
#include "QGeoTiledMappingManagerEngineQGC.h"
#include "QGCTileCompositor.h"
#include "QGeoFileTileCacheQGC.h"

#include <QtLocation/private/qgeotiledmappingmanagerengine_p.h>
#include <QtLocation/private/qgeotilespec_p.h>
#include <QtNetwork/QNetworkRequest>

Q_LOGGING_CATEGORY(QGeoTileFetcherQGCLog,
                   "qgc.qtlocationplugin.qgeotilefetcherqgc")

QGeoTileFetcherQGC::QGeoTileFetcherQGC(QNetworkAccessManager *networkManager,
                                       const QVariantMap &parameters,
                                       QGeoTiledMappingManagerEngineQGC *parent)
    : QGeoTileFetcher(parent), m_networkManager(networkManager), m_engine(parent) {
    Q_CHECK_PTR(networkManager);
}

QGeoTileFetcherQGC::~QGeoTileFetcherQGC() {}

QGeoTiledMapReply *QGeoTileFetcherQGC::getTileImage(const QGeoTileSpec &spec) {
    // 检查是否启用多图层
    if (m_engine) {
        MapLayerStack layerStack = getLayerStackForMapId(spec.mapId());
        if (!layerStack.isEmpty()) {
            return getMultiLayerTileImage(spec, layerStack);
        }
    }

    // 单图层模式（原有逻辑）
    const SharedMapProvider provider =
        UrlFactory::getMapProviderFromQtMapId(spec.mapId());
    if (!provider) {
        return nullptr;
    }

    if (spec.zoom() > provider->maximumZoomLevel() ||
        spec.zoom() < provider->minimumZoomLevel()) {
        return nullptr;
    }

    const QNetworkRequest request =
        getNetworkRequest(spec.mapId(), spec.x(), spec.y(), spec.zoom());
    if (request.url().isEmpty()) {
        return nullptr;
    }

    return new QGeoTiledMapReplyQGC(m_networkManager, request, spec);
}

bool QGeoTileFetcherQGC::initialized() const {
    return (m_networkManager != nullptr);
}

bool QGeoTileFetcherQGC::fetchingEnabled() const { return initialized(); }

void QGeoTileFetcherQGC::timerEvent(QTimerEvent *event) {
    QGeoTileFetcher::timerEvent(event);
}

void QGeoTileFetcherQGC::handleReply(QGeoTiledMapReply *reply,
                                     const QGeoTileSpec &spec) {
    if (!reply) {
        return;
    }

    reply->deleteLater();

    if (!initialized()) {
        return;
    }

    if (reply->error() == QGeoTiledMapReply::NoError) {
        emit tileFinished(spec, reply->mapImageData(), reply->mapImageFormat());
    } else {
        emit tileError(spec, reply->errorString());
    }
}

QNetworkRequest QGeoTileFetcherQGC::getNetworkRequest(int mapId, int x, int y,
                                                      int zoom) {
    const SharedMapProvider mapProvider =
        UrlFactory::getMapProviderFromQtMapId(mapId);

    QNetworkRequest request;
    request.setUrl(mapProvider->getTileURL(x, y, zoom));
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("*/*"));
    request.setHeader(QNetworkRequest::UserAgentHeader, s_userAgent);
    const QByteArray referrer = mapProvider->getReferrer().toUtf8();
    if (!referrer.isEmpty()) {
        request.setRawHeader(QByteArrayLiteral("Referrer"), referrer);
    }
    const QByteArray token = mapProvider->getToken();
    if (!token.isEmpty()) {
        request.setRawHeader(QByteArrayLiteral("User-Token"), token);
    }
    // request.setOriginatingObject(this);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::PreferCache);
    request.setAttribute(QNetworkRequest::BackgroundRequestAttribute, true);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);
    request.setAttribute(QNetworkRequest::DoNotBufferUploadDataAttribute, false);
    // request.setAttribute(QNetworkRequest::AutoDeleteReplyOnFinishAttribute,
    // true);
    request.setPriority(QNetworkRequest::NormalPriority);
    request.setTransferTimeout(10000);

    return request;
}

MapLayerStack QGeoTileFetcherQGC::getLayerStackForMapId(int mapId) const
{
    if (m_engine) {
        return m_engine->getLayerStackForMapId(mapId);
    }
    return MapLayerStack();
}

QGeoTiledMapReply *QGeoTileFetcherQGC::getMultiLayerTileImage(const QGeoTileSpec &spec, const MapLayerStack &layerStack)
{
    if (layerStack.isEmpty()) {
        return nullptr;
    }

    QList<MapLayer> layers = layerStack.layers();
    
    // 过滤出可见的图层
    QList<MapLayer> visibleLayers;
    for (const MapLayer &layer : layers) {
        if (layer.visible()) {
            visibleLayers.append(layer);
        }
    }

    if (visibleLayers.isEmpty()) {
        return nullptr;
    }

    // 如果只有一个图层，直接使用单图层模式
    if (visibleLayers.count() == 1) {
        const MapLayer &layer = visibleLayers.first();
        const SharedMapProvider provider = UrlFactory::getMapProviderFromQtMapId(layer.mapId());
        if (!provider) {
            return nullptr;
        }

        if (spec.zoom() > provider->maximumZoomLevel() ||
            spec.zoom() < provider->minimumZoomLevel()) {
            return nullptr;
        }

        const QNetworkRequest request = getNetworkRequest(layer.mapId(), spec.x(), spec.y(), spec.zoom());
        if (request.url().isEmpty()) {
            return nullptr;
        }

        return new QGeoTiledMapReplyQGC(m_networkManager, request, spec);
    }
    
    return new QGeoMultiLayerMapReplyQGC(m_networkManager, spec, layerStack);
}
