import QtQuick
import QtLocation
import QtPositioning

Window {
    width: 640
    height: 480
    visible: true
    title: qsTr("Hello World")
    MapView {
        id: mapView
        anchors.fill: parent

        map.plugin: Plugin {
            name: "QGroundControl"   // 使用 OpenStreetMap 插件
            PluginParameter {
                name: "TiandiTuKey"
                value: ""
            }
            PluginParameter {
                name: "multiLayer"
                value: "true"
            }

            // 直接指定图层列表（按顺序从底到顶）
            PluginParameter {
                name: "layers"
                value: "天地图街道,天地图街道注记"
            }
        }
        map.activeMapType: map.supportedMapTypes[37]
        map.center: QtPositioning.coordinate(39.9, 116.4) // 北京坐标
        map.zoomLevel: 12
        map.minimumZoomLevel: 3
        map.maximumZoomLevel: 18
    }
}
