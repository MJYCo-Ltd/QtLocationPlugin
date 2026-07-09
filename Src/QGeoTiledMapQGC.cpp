/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "QGeoTiledMapQGC.h"
#include "QGeoTiledMappingManagerEngineQGC.h"

#include <QtLocation/private/qgeotiledmap_p_p.h>
#include <QtLocation/private/qgeotiledmapscene_p.h>
#include <QtLocation/private/qgeotilespec_p.h>

Q_LOGGING_CATEGORY(QGeoTiledMapQGCLog, "qgc.qtlocationplugin.qgeotiledmapqgc")

namespace {
constexpr int kEvalDebounceMs = 50;
constexpr int kReadyDebounceMs = 100;
} // namespace

QGeoTiledMapQGC::QGeoTiledMapQGC(QGeoTiledMappingManagerEngineQGC *engine,
                                 QObject *parent)
    : QGeoTiledMap(engine, parent) {
    m_evalDebounce.setSingleShot(true);
    m_evalDebounce.setInterval(kEvalDebounceMs);
    m_readyDebounce.setSingleShot(true);
    m_readyDebounce.setInterval(kReadyDebounceMs);

    connect(this, &QGeoMap::sgNodeChanged, this, &QGeoTiledMapQGC::scheduleEvaluate);
    connect(this, &QGeoMap::cameraDataChanged, this,
            &QGeoTiledMapQGC::scheduleEvaluate);
    connect(this, &QGeoMap::visibleAreaChanged, this,
            &QGeoTiledMapQGC::scheduleEvaluate);
    connect(&m_evalDebounce, &QTimer::timeout, this,
            &QGeoTiledMapQGC::evaluatePending);
    connect(&m_readyDebounce, &QTimer::timeout, this,
            &QGeoTiledMapQGC::onReadyDebounceTimeout);
}

QGeoTiledMapQGC::~QGeoTiledMapQGC() {}

QGeoMap::Capabilities QGeoTiledMapQGC::capabilities() const {
    return Capabilities(SupportsVisibleRegion | SupportsAnchoringCoordinate |
                        SupportsVisibleArea);
}

void QGeoTiledMapQGC::clearData() {
    m_evalDebounce.stop();
    m_readyDebounce.stop();
    setTilesReady(false);
    setPendingTileCount(0);

    QGeoTiledMap::clearData();
    scheduleEvaluate();
}

void QGeoTiledMapQGC::scheduleEvaluate() {
    m_evalDebounce.start();
}

int QGeoTiledMapQGC::countPendingTiles() const {
    Q_D(const QGeoTiledMap);
    const auto *const tiledPrivate =
        static_cast<const QGeoTiledMapPrivate *>(d);

    if (!tiledPrivate->m_visibleTiles || !tiledPrivate->m_mapScene) {
        return 0;
    }

    const QSet<QGeoTileSpec> needed =
        tiledPrivate->m_visibleTiles->createTiles();
    const QSet<QGeoTileSpec> textured =
        tiledPrivate->m_mapScene->texturedTiles();

    QSet<QGeoTileSpec> pending = needed;
    pending.subtract(textured);
    return pending.size();
}

bool QGeoTiledMapQGC::hasVisibleTiles() const {
    Q_D(const QGeoTiledMap);
    const auto *const tiledPrivate =
        static_cast<const QGeoTiledMapPrivate *>(d);

    if (!tiledPrivate->m_visibleTiles) {
        return false;
    }

    return !tiledPrivate->m_visibleTiles->createTiles().isEmpty();
}

void QGeoTiledMapQGC::evaluatePending() {
    if (!hasVisibleTiles()) {
        m_readyDebounce.stop();
        setPendingTileCount(0);
        setTilesReady(false);
        return;
    }

    const int pending = countPendingTiles();
    setPendingTileCount(pending);

    if (pending > 0) {
        m_readyDebounce.stop();
        setTilesReady(false);
        return;
    }

    m_readyDebounce.start();
}

void QGeoTiledMapQGC::onReadyDebounceTimeout() {
    if (!hasVisibleTiles()) {
        setPendingTileCount(0);
        setTilesReady(false);
        return;
    }

    const int pending = countPendingTiles();
    setPendingTileCount(pending);

    if (pending > 0) {
        setTilesReady(false);
        return;
    }

    setTilesReady(true);
    emit tilesReady();
}

void QGeoTiledMapQGC::setPendingTileCount(int count) {
    if (m_pendingTileCount == count) {
        return;
    }

    m_pendingTileCount = count;
    emit pendingTileCountChanged(m_pendingTileCount);
}

void QGeoTiledMapQGC::setTilesReady(bool ready) {
    if (m_tilesReady == ready) {
        return;
    }

    m_tilesReady = ready;
    emit tilesReadyChanged(m_tilesReady);
}
