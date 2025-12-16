# QtLocationPlugin
从QGC中剥离出来的locationPlugin
## 使用示例

```c
Map {
        id:map
        anchors.fill: parent
        plugin: Plugin {
            name: "QGroundControl"   // 使用 OpenStreetMap 插件
            PluginParameter {
                name: "TiandiTuKey"
                value: "************************************************"
            }
            PluginParameter {
                name: "tmsUrl"
                value: "file:///E:/out/tms.xml"
            }
        }

        // 设置初始中心点
        center: QtPositioning.coordinate(38.045474, 114.502461) // 石家庄
        zoomLevel: 8
        activeMapType:supportedMapTypes[39];
        Component.onCompleted: {
            console.log("Supported map types:")
            for (var i = 0; i < supportedMapTypes.length; ++i) {
                var t = supportedMapTypes[i]
                console.log(
                            i,
                            t.name,
                            t.description,
                            t.style
                            )
            }
        }
}
```