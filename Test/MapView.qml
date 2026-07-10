import QtQuick
import QtLocation

Item {
    id: root

    property alias map: map

    Map {
        id: map
        anchors.fill: parent
    }
}
