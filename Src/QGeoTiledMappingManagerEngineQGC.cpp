/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/
#include <mutex>

#include <QtCore/QDir>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkDiskCache>
#include <QtNetwork/QNetworkProxy>
#include <QtLocation/private/qgeocameracapabilities_p.h>
#include <QtLocation/private/qgeomaptype_p.h>
#include <QtLocation/private/qgeotiledmap_p.h>
#include <QtLocation/private/qgeofiletilecache_p.h>
#include "QGeoTiledMappingManagerEngineQGC.h"
#include "QGCMapEngine.h"
#include "QGeoTileFetcherQGC.h"
#include "QGeoFileTileCacheQGC.h"
#include "QGeoTiledMapQGC.h"
#include "QGCMapUrlEngine.h"
#include "TmsMapProvider.h"
#include "TiandiMapProvider.h"

Q_LOGGING_CATEGORY(QGeoTiledMappingManagerEngineQGCLog, "qgc.qtlocationplugin.qgeotiledmappingmanagerengineqgc")

QGeoTiledMappingManagerEngineQGC::QGeoTiledMappingManagerEngineQGC(const QVariantMap &parameters, QGeoServiceProvider::Error *error, QString *errorString, QNetworkAccessManager *networkManager, QObject *parent)
    : QGeoTiledMappingManagerEngine(parent)
    , m_networkManager(networkManager)
{
    // qCDebug(QGeoTiledMappingManagerEngineQGCLog) << Q_FUNC_INFO << this;

    // TODO: Better way to get current language without qgcApp()?

    QGeoCameraCapabilities cameraCaps;
    cameraCaps.setTileSize(256);
    cameraCaps.setMinimumZoomLevel(2.0);
    cameraCaps.setMaximumZoomLevel(MAX_MAP_ZOOM);
    cameraCaps.setSupportsBearing(true);
    cameraCaps.setSupportsRolling(false);
    cameraCaps.setSupportsTilting(false);
    cameraCaps.setMinimumTilt(0.0);
    cameraCaps.setMaximumTilt(0.0);
    cameraCaps.setMinimumFieldOfView(45.0);
    cameraCaps.setMaximumFieldOfView(45.0);
    cameraCaps.setOverzoomEnabled(true);
    setCameraCapabilities(cameraCaps);

    setTileVersion(kTileVersion);
    setTileSize(QSize(256, 256));

    if (parameters.contains(QStringLiteral("tmsUrl"))) {
        TmsMapProvider::loadTmsFile(parameters[QStringLiteral("tmsUrl")].toString());
    }
    if(parameters.contains(QStringLiteral("TiandiTuKey"))){
        TiandiMapProvider::_key = parameters[QStringLiteral("TiandiTuKey")].toString();
    }

    // 解析图层配置
    parseLayerConfiguration(parameters);

    QList<QGeoMapType> mapList;
    const QList<SharedMapProvider> providers = UrlFactory::getProviders();
    for (const SharedMapProvider &provider : providers) {
        QVariantMap variantMap;
        variantMap.insert("minimumZoomLevel",provider->minimumZoomLevel());
        variantMap.insert("maximumZoomLevel", provider->maximumZoomLevel());
        const QGeoMapType map = QGeoMapType(
            provider->getMapStyle(),
            provider->getMapName(),
            provider->getMapName(),
            false,
            false,
            provider->getMapId(),
            QByteArrayLiteral("QGroundControl"),
            cameraCapabilities(),
            variantMap
        );
        (void) mapList.append(map);
    }

    if (!m_layerStack.isEmpty() && m_compositeMapId > 0) {
        int minimumZoom = cameraCapabilities().minimumZoomLevel();
        int maximumZoom = cameraCapabilities().maximumZoomLevel();
        for (const MapLayer &layer : m_layerStack.layers()) {
            if (!layer.visible()) {
                continue;
            }
            const SharedMapProvider provider =
                UrlFactory::getMapProviderFromQtMapId(layer.mapId());
            if (provider) {
                minimumZoom = qMax(minimumZoom, provider->minimumZoomLevel());
                maximumZoom = qMin(maximumZoom, provider->maximumZoomLevel());
            }
        }

        QVariantMap variantMap;
        variantMap.insert("minimumZoomLevel", minimumZoom);
        variantMap.insert("maximumZoomLevel", maximumZoom);
        variantMap.insert("isComposite", true);
        variantMap.insert("layerStackKey", m_layerStack.generateCacheKey());
        const QString compositeName =
            QStringLiteral("Composite_%1").arg(m_layerStack.generateCacheKey());
        const QGeoMapType compositeMap(
            QGeoMapType::CustomMap, compositeName, compositeName, false, false,
            m_compositeMapId, QByteArrayLiteral("QGroundControl"),
            cameraCapabilities(), variantMap);

        // 多图层配置表示一个固定地图产品，只向 Qt Location 暴露该产品，
        // 确保 QGeoTileSpec 使用 compositeMapId，避免普通 mapId 造成重复缓存。
        mapList = {compositeMap};
    }
    setSupportedMapTypes(mapList);

    setCacheHint(QAbstractGeoTileCache::CacheArea::AllCaches);
    QGeoFileTileCacheQGC* const fileTileCache = new QGeoFileTileCacheQGC(parameters);
    setTileCache(fileTileCache);

    // MapEngine must be init after fileTileCache
    static std::once_flag mapEngineInit;
    std::call_once(mapEngineInit, [fileTileCache]() {
        getQGCMapEngine()->init(fileTileCache->getDatabaseFilePath());
    });

    m_prefetchStyle = m_layerStack.isEmpty()
        ? QGeoTiledMap::PrefetchTwoNeighbourLayers
        : QGeoTiledMap::PrefetchNeighbourLayer;

    if (!m_networkManager) {
        m_networkManager = new QNetworkAccessManager(this);
        #if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
            QNetworkProxy proxy = m_networkManager->proxy();
            proxy.setType(QNetworkProxy::DefaultProxy);
            m_networkManager->setProxy(proxy);
        #endif
        m_networkManager->setTransferTimeout(10000);
        // m_networkManager->setAutoDeleteReplies(true);
        QNetworkDiskCache *const diskCache = new QNetworkDiskCache(this);
        diskCache->setCacheDirectory(fileTileCache->getCachePath() + "/Downloads");
        const qint64 maxCacheSize =
            static_cast<quint64>(fileTileCache->getMaxDiskCacheSetting()) *
                                    pow(1024, 2);
        diskCache->setMaximumCacheSize(maxCacheSize);
        m_networkManager->setCache(diskCache);
    }

    QGeoTileFetcherQGC* const tileFetcher = new QGeoTileFetcherQGC(m_networkManager, parameters, this);

    *error = QGeoServiceProvider::NoError;
    errorString->clear();
    setTileFetcher(tileFetcher); // Calls engineInitialized()
}

QGeoTiledMappingManagerEngineQGC::~QGeoTiledMappingManagerEngineQGC()
{
    // qCDebug(QGeoTiledMappingManagerEngineQGCLog) << Q_FUNC_INFO << this;
}

QGeoMap *QGeoTiledMappingManagerEngineQGC::createMap()
{
    QGeoTiledMapQGC* const map = new QGeoTiledMapQGC(this, this);
    map->setPrefetchStyle(m_prefetchStyle);
    return map;
}

void QGeoTiledMappingManagerEngineQGC::parseLayerConfiguration(const QVariantMap &parameters)
{
    // 从参数解析图层配置
    m_layerStack = MapLayerStack::fromParameters(parameters);

    if (m_layerStack.isEmpty()) {
        m_compositeMapId = -1;
        return;
    }

    // 多图层模式只向 Qt Location 暴露一个地图类型。Qt Location 的 mapId
    // 需要从 1 开始且连续；配置身份由独立的 SHA-256 缓存键负责。
    m_compositeMapId = 1;
    
    m_mapIdToLayerStack.insert(m_compositeMapId, m_layerStack);
}

MapLayerStack QGeoTiledMappingManagerEngineQGC::getLayerStackForMapId(int mapId) const
{
    if (m_mapIdToLayerStack.contains(mapId)) {
        return m_mapIdToLayerStack.value(mapId);
    }

    return MapLayerStack();
}
