/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

class QGCMapTileMonitor : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *map READ map WRITE setMap NOTIFY mapChanged)
    Q_PROPERTY(int pendingTileCount READ pendingTileCount NOTIFY
                   pendingTileCountChanged)
    Q_PROPERTY(bool tilesReadyState READ isTilesReady NOTIFY tilesReadyChanged)
    Q_PROPERTY(bool viewportReadyState READ isViewportReady NOTIFY
                   viewportReadyStateChanged)

public:
    explicit QGCMapTileMonitor(QObject *parent = nullptr);

    QObject *map() const { return m_map; }
    void setMap(QObject *map);

    int pendingTileCount() const;
    bool isTilesReady() const;
    bool isViewportReady() const;

    Q_INVOKABLE void beginViewportChange();

Q_SIGNALS:
    void mapChanged(QObject *map);
    void pendingTileCountChanged(int pendingTileCount);
    void tilesReadyChanged(bool ready);
    void tilesReady();
    void viewportReadyStateChanged(bool ready);

private Q_SLOTS:
    void onMapReadyChanged(bool ready);
    void syncFromTiledMap();

private:
    void disconnectTiledMap();
    void connectTiledMap(QObject *tiledMap);

    QPointer<QObject> m_map;
    QPointer<QObject> m_tiledMap;
};
