import QtQuick
import QtQuick.Controls
import QtLocation
import QtPositioning

Window {
    width: 640
    height: 480
    visible: true
    title: qsTr("QtLocation 插件测试 — 切换底图")

    MapView {
        id: mapView
        anchors.fill: parent

        map.plugin: Plugin {
            name: "QGroundControl"
            PluginParameter {
                name: "TiandiTuKey"
                value: ""
            }
            // 单图层切换：开启 multiLayer 时引擎会固定使用 layers，忽略 activeMapType 的 mapId，
            // 不适合在这里验证「切换矢量/卫星」与 clearData。
            //PluginParameter {
            //    name: "multiLayer"
            //    value: "true"
            //}

            // 直接指定图层列表（按顺序从底到顶）
            //PluginParameter {
            //    name: "layers"
            //    value: "天地图卫星,天地图卫星注记"
            //}
        }
        map.center: QtPositioning.coordinate(38.045474, 114.502461)
        map.zoomLevel: 10
        map.minimumZoomLevel: 3
        map.maximumZoomLevel: 18
    }

    // 勿在 onActiveMapTypeChanged 里无条件 clearData：启动时 Component.onCompleted
    // 设置默认类型也会触发该信号，会清空 Qt 侧纹理缓存，瓦片只能再走一遍异步取图
    //（插件会先查 qgcMapCache.db，仍比纯网慢）。仅在用户主动换底图且类型真的变化时 clearData。

    Flow {
        z: 10
        width: parent.width - 16
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 8
        spacing: 6

        Repeater {
            model: Math.min(12, mapView.map.supportedMapTypes.length)
            delegate: Button {
                required property int index
                text: mapView.map.supportedMapTypes[index].name
                font.pixelSize: 11
                padding: 6
                onClicked: {
                    const next = mapView.map.supportedMapTypes[index]
                    if (next === mapView.map.activeMapType)
                        return
                    mapView.map.activeMapType = next
                    mapView.map.clearData()
                }
            }
        }
    }

    Component.onCompleted: {
        const types = mapView.map.supportedMapTypes
        if (types.length > 1)
            mapView.map.activeMapType = types[1]
        else if (types.length === 1)
            mapView.map.activeMapType = types[0]
    }

    Connections {
        target: mapView
        function onTilesReady() {
            console.log("tilesReady, pending =", mapView.pendingTileCount)
        }
    }
}
