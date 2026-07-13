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

#include <QtLocation/private/qgeocameratiles_p.h>
#include <QtLocation/private/qgeotiledmap_p_p.h>
#include <QtLocation/private/qgeotiledmapscene_p.h>
#include <QtLocation/private/qgeotilespec_p.h>

Q_LOGGING_CATEGORY(QGeoTiledMapQGCLog, "qgc.qtlocationplugin.qgeotiledmapqgc")

namespace {
constexpr int kEvalDebounceMs = 50;
constexpr int kReadyDebounceMs = 100;
} // namespace

class QGeoTiledMapQGCPrivate : public QGeoTiledMapPrivate {
public:
    explicit QGeoTiledMapQGCPrivate(QGeoTiledMappingManagerEngine *engine)
        : QGeoTiledMapPrivate(engine) {}

    int countPendingTiles() const {
        if (!m_visibleTiles || !m_mapScene) {
            return 0;
        }

        const QSet<QGeoTileSpec> needed = m_visibleTiles->createTiles();
        const QSet<QGeoTileSpec> textured = m_mapScene->texturedTiles();

        QSet<QGeoTileSpec> pending = needed;
        pending.subtract(textured);
        return pending.size();
    }

    bool hasVisibleTiles() const {
        if (!m_visibleTiles) {
            return false;
        }

        return !m_visibleTiles->createTiles().isEmpty();
    }
};

QGeoTiledMapQGC::QGeoTiledMapQGC(QGeoTiledMappingManagerEngineQGC *engine,
                                 QObject *parent)
    : QGeoTiledMap(*new QGeoTiledMapQGCPrivate(engine), engine, parent) {
    m_evalDebounce.setSingleShot(true);
    m_evalDebounce.setInterval(kEvalDebounceMs);
    m_readyDebounce.setSingleShot(true);
    m_readyDebounce.setInterval(kReadyDebounceMs);

    connect(this, &QGeoMap::sgNodeChanged, this, &QGeoTiledMapQGC::scheduleEvaluate);
    connect(this, &QGeoMap::cameraDataChanged, this,
            &QGeoTiledMapQGC::beginViewportChange);
    connect(this, &QGeoMap::visibleAreaChanged, this,
            &QGeoTiledMapQGC::beginViewportChange);
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
    setViewportReady(false);
    setPendingTileCount(0);

    QGeoTiledMap::clearData();
    scheduleEvaluate();
}

void QGeoTiledMapQGC::beginViewportChange() {
    m_readyDebounce.stop();
    setViewportReady(false);
    scheduleEvaluate();
}

void QGeoTiledMapQGC::scheduleEvaluate() {
    m_evalDebounce.start();
}

void QGeoTiledMapQGC::evaluatePending() {
    Q_D(const QGeoTiledMapQGC);

    if (!d->hasVisibleTiles()) {
        m_readyDebounce.stop();
        setPendingTileCount(0);
        setTilesReady(false);
        setViewportReady(false);
        return;
    }

    const int pending = d->countPendingTiles();
    setPendingTileCount(pending);

    if (pending > 0) {
        m_readyDebounce.stop();
        setTilesReady(false);
        setViewportReady(false);
        return;
    }

    m_readyDebounce.start();
}

void QGeoTiledMapQGC::onReadyDebounceTimeout() {
    Q_D(const QGeoTiledMapQGC);

    if (!d->hasVisibleTiles()) {
        setPendingTileCount(0);
        setTilesReady(false);
        setViewportReady(false);
        return;
    }

    const int pending = d->countPendingTiles();
    setPendingTileCount(pending);

    if (pending > 0) {
        setTilesReady(false);
        setViewportReady(false);
        return;
    }

    if (!m_tilesReady) {
        setTilesReady(true);
        emit tilesReady();
    }
    setViewportReady(true);
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

void QGeoTiledMapQGC::setViewportReady(bool ready) {
    if (m_viewportReady == ready) {
        return;
    }

    m_viewportReady = ready;
    emit viewportReadyStateChanged(m_viewportReady);
}
