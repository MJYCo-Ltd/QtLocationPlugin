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
    // 检查网络管理器是否已初始化
    if (!m_networkManager) {
        return nullptr;
    }

    // 检查是否启用多图层（优先检查，确保所有 mapId 都使用多图层配置）
    if (m_engine) {
        MapLayerStack layerStack = getLayerStackForMapId(spec.mapId());
        if (!layerStack.isEmpty()) {
            // 多图层模式：无论 spec.mapId() 是什么，都使用配置的图层栈
            // 这样可以确保即使 activeMapType 在初始化时是街道地图，也会使用配置的卫星图层
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
        qCWarning(QGeoTileFetcherQGCLog) << "Empty layer stack for tile" << spec.x() << spec.y();
        return nullptr;
    }

    // 确保网络管理器已初始化
    if (!m_networkManager) {
        qCWarning(QGeoTileFetcherQGCLog) << "Network manager not initialized";
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
    
    // 多图层模式：获取生成的 mapId（用于文件保存）
    // 注意：我们仍然使用原始的 spec，但会在 QGeoMultiLayerMapReplyQGC 中
    // 使用 compositeMapId 来保存文件
    int compositeMapId = -1;
    if (m_engine) {
        compositeMapId = m_engine->getCompositeMapId();
    }
    
    // 如果无法获取 compositeMapId，使用图层栈生成的 mapId
    if (compositeMapId <= 0) {
        compositeMapId = layerStack.generateMapId();
    }
    
    // 直接使用原始的 spec，compositeMapId 会在 QGeoMultiLayerMapReplyQGC 中使用
    return new QGeoMultiLayerMapReplyQGC(m_networkManager, spec, layerStack, compositeMapId);
}
