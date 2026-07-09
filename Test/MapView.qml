import QtQuick
import QtLocation
import QGroundControl 1.0

Item {
    id: root

    property alias map: map
    readonly property int pendingTileCount: tileMonitor.pendingTileCount
    readonly property bool tilesReadyState: tileMonitor.tilesReady

    signal tilesReady()

    Map {
        id: map
        anchors.fill: parent
    }

    MapTileMonitor {
        id: tileMonitor
        map: map
        onTilesReady: root.tilesReady()
    }
}
