/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QLoggingCategory>
#include <QtCore/QTimer>
#include <QtLocation/private/qgeotiledmap_p.h>

Q_DECLARE_LOGGING_CATEGORY(QGeoTiledMapQGCLog)

class QGeoTiledMappingManagerEngineQGC;
class QGeoTiledMapQGCPrivate;

class QGeoTiledMapQGC : public QGeoTiledMap {
    Q_OBJECT
    Q_DECLARE_PRIVATE(QGeoTiledMapQGC)
    Q_PROPERTY(int pendingTileCount READ pendingTileCount NOTIFY pendingTileCountChanged)
    Q_PROPERTY(bool tilesReady READ isTilesReady NOTIFY tilesReadyChanged)

public:
    explicit QGeoTiledMapQGC(QGeoTiledMappingManagerEngineQGC *engine,
                             QObject *parent = nullptr);
    ~QGeoTiledMapQGC();

    QGeoMap::Capabilities capabilities() const final;

    int pendingTileCount() const { return m_pendingTileCount; }
    bool isTilesReady() const { return m_tilesReady; }

    void clearData() override;

Q_SIGNALS:
    void pendingTileCountChanged(int pendingTileCount);
    void tilesReadyChanged(bool ready);
    void tilesReady();

private Q_SLOTS:
    void scheduleEvaluate();
    void evaluatePending();
    void onReadyDebounceTimeout();

private:
    void setPendingTileCount(int count);
    void setTilesReady(bool ready);

    QTimer m_evalDebounce;
    QTimer m_readyDebounce;
    int m_pendingTileCount = 0;
    bool m_tilesReady = false;
};
