/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "QGCMapTileMonitor.h"

#include <QtLocation/private/qdeclarativegeomap_p.h>

namespace {
constexpr char kPendingTileCountProperty[] = "pendingTileCount";
constexpr char kTilesReadyStateProperty[] = "tilesReadyState";
} // namespace

QGCMapTileMonitor::QGCMapTileMonitor(QObject *parent)
    : QObject(parent) {}

void QGCMapTileMonitor::setMap(QObject *map) {
    if (m_map == map) {
        return;
    }

    if (m_map) {
        if (auto *declarativeMap = qobject_cast<QDeclarativeGeoMap *>(m_map)) {
            disconnect(declarativeMap, &QDeclarativeGeoMap::mapReadyChanged, this,
                       &QGCMapTileMonitor::onMapReadyChanged);
        }
    }

    disconnectTiledMap();
    m_map = map;

    if (auto *declarativeMap = qobject_cast<QDeclarativeGeoMap *>(m_map)) {
        connect(declarativeMap, &QDeclarativeGeoMap::mapReadyChanged, this,
                &QGCMapTileMonitor::onMapReadyChanged);
        onMapReadyChanged(declarativeMap->mapReady());
    }

    emit mapChanged(m_map);
    syncFromTiledMap();
}

int QGCMapTileMonitor::pendingTileCount() const {
    return m_tiledMap ? m_tiledMap->property(kPendingTileCountProperty).toInt() : 0;
}

bool QGCMapTileMonitor::isTilesReady() const {
    return m_tiledMap ? m_tiledMap->property(kTilesReadyStateProperty).toBool()
                      : false;
}

void QGCMapTileMonitor::onMapReadyChanged(bool ready) {
    disconnectTiledMap();

    if (!ready) {
        syncFromTiledMap();
        return;
    }

    if (auto *declarativeMap = qobject_cast<QDeclarativeGeoMap *>(m_map)) {
        connectTiledMap(declarativeMap->map());
    }

    syncFromTiledMap();
}

void QGCMapTileMonitor::syncFromTiledMap() {
    emit pendingTileCountChanged(pendingTileCount());
    emit tilesReadyChanged(isTilesReady());
}

void QGCMapTileMonitor::disconnectTiledMap() {
    if (!m_tiledMap) {
        return;
    }

    disconnect(m_tiledMap, nullptr, this, nullptr);
    m_tiledMap.clear();
}

void QGCMapTileMonitor::connectTiledMap(QObject *tiledMap) {
    if (!tiledMap) {
        return;
    }

    m_tiledMap = tiledMap;
    connect(tiledMap, SIGNAL(pendingTileCountChanged(int)), this,
            SIGNAL(pendingTileCountChanged(int)));
    connect(tiledMap, SIGNAL(tilesReadyChanged(bool)), this,
            SIGNAL(tilesReadyChanged(bool)));
    connect(tiledMap, SIGNAL(tilesReady()), this, SIGNAL(tilesReady()));
}
