/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtLocation/QGeoServiceProvider>
#include <QtLocation/private/qgeotiledmappingmanagerengine_p.h>
#include <QtCore/QLoggingCategory>
#include "QGCMapLayerConfig.h"

Q_DECLARE_LOGGING_CATEGORY(QGeoTiledMappingManagerEngineQGCLog)

class QNetworkAccessManager;

class QGeoTiledMappingManagerEngineQGC : public QGeoTiledMappingManagerEngine
{
    Q_OBJECT

public:
    QGeoTiledMappingManagerEngineQGC(const QVariantMap &parameters, QGeoServiceProvider::Error *error, QString *errorString, QNetworkAccessManager *networkManager = nullptr, QObject *parent = nullptr);
    ~QGeoTiledMappingManagerEngineQGC();

    QGeoMap* createMap() final;
    QNetworkAccessManager* networkManager() const { return m_networkManager; }

    // 图层配置管理
    const MapLayerStack& layerStack() const { return m_layerStack; }
    bool isMultiLayerEnabled() const { return !m_layerStack.isEmpty(); }
    MapLayerStack getLayerStackForMapId(int mapId) const;

private:
    void parseLayerConfiguration(const QVariantMap &parameters);

    QNetworkAccessManager *m_networkManager = nullptr;
    MapLayerStack m_layerStack;  // 全局图层配置
    QHash<int, MapLayerStack> m_mapIdToLayerStack;  // mapId 到图层配置的映射

    static constexpr int kTileVersion = 1;
};
