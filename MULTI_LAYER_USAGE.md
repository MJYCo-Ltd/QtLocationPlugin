# 多图层渲染使用说明

## 概述

本插件现在支持多图层渲染功能，可以将多个地图图层叠加显示，实现更丰富的地图展示效果。

## QML 使用方式

### 方式1：基础图层 + 叠加图层

```qml
import QtLocation 6.9
import QtPositioning 6.9

Map {
    id: map
    anchors.fill: parent
    
    plugin: Plugin {
        name: "QGroundControl"
        
        // 启用多图层模式
        PluginParameter {
            name: "multiLayer"
            value: "true"
        }
        
        // 设置基础图层（底层）
        PluginParameter {
            name: "baseLayer"
            value: "Google Satellite"  // 卫星图作为底层
        }
        
        // 设置叠加图层（逗号分隔）
        PluginParameter {
            name: "overlayLayers"
            value: "Google Labels"  // 标签层叠加
        }
        
        // 设置叠加图层透明度（可选，默认1.0）
        PluginParameter {
            name: "overlayOpacities"
            value: "0.8"  // 80% 透明度
        }
    }
    
    center: QtPositioning.coordinate(38.045474, 114.502461)
    zoomLevel: 10
    
    // 选择基础图层作为 activeMapType
    activeMapType: supportedMapTypes.find(function(type) {
        return type.name === "Google Satellite"
    })
}
```

### 方式2：图层列表方式

```qml
Map {
    plugin: Plugin {
        name: "QGroundControl"
        
        PluginParameter {
            name: "multiLayer"
            value: "true"
        }
        
        // 直接指定图层列表（按顺序从底到顶）
        PluginParameter {
            name: "layers"
            value: "Google Satellite,Google Labels"
        }
        
        // 对应透明度
        PluginParameter {
            name: "opacities"
            value: "1.0,0.8"
        }
    }
    
    activeMapType: supportedMapTypes[0]  // 可以是任意一个
}
```

### 方式3：JSON 配置方式（最灵活）

```qml
import QtQuick 2.15

Map {
    plugin: Plugin {
        name: "QGroundControl"
        
        PluginParameter {
            name: "multiLayer"
            value: "true"
        }
        
        PluginParameter {
            name: "layerConfig"
            value: JSON.stringify([
                {
                    "name": "Google Satellite",
                    "opacity": 1.0,
                    "zOrder": 0,
                    "visible": true
                },
                {
                    "name": "Google Labels",
                    "opacity": 0.8,
                    "zOrder": 1,
                    "visible": true
                }
            ])
        }
    }
}
```

## 参数说明

### multiLayer / multiLayerEnabled
- **类型**: String ("true"/"false") 或 Boolean
- **说明**: 启用多图层模式
- **必需**: 是

### baseLayer
- **类型**: String
- **说明**: 基础图层名称（底层）
- **必需**: 方式1中使用

### overlayLayers
- **类型**: String
- **说明**: 叠加图层名称列表，逗号分隔
- **必需**: 方式1中使用

### overlayOpacities
- **类型**: String
- **说明**: 叠加图层透明度列表，逗号分隔，范围 0.0-1.0
- **可选**: 默认 1.0

### layers
- **类型**: String
- **说明**: 所有图层名称列表，逗号分隔，按 zOrder 从低到高
- **必需**: 方式2中使用

### opacities
- **类型**: String
- **说明**: 所有图层透明度列表，逗号分隔
- **可选**: 默认 1.0

### layerConfig
- **类型**: String (JSON)
- **说明**: 完整的图层配置 JSON 数组
- **必需**: 方式3中使用

## 图层配置 JSON 格式

```json
[
    {
        "name": "图层名称",
        "opacity": 1.0,      // 透明度 0.0-1.0
        "zOrder": 0,         // 渲染顺序，越小越底层
        "visible": true      // 是否可见
    }
]
```

## 注意事项

1. **图层名称**: 必须使用 MapProvider 的 `getMapName()` 返回的名称
2. **透明度**: 范围 0.0-1.0，0.0 完全透明，1.0 完全不透明
3. **渲染顺序**: zOrder 越小越底层，相同 zOrder 按配置顺序
4. **性能**: 多图层会增加网络请求和合成计算，可能影响性能
5. **缓存**: 合成后的瓦片会单独缓存，缓存键包含图层配置信息

## 示例场景

### 卫星图 + 标签
```qml
PluginParameter { name: "baseLayer"; value: "Google Satellite" }
PluginParameter { name: "overlayLayers"; value: "Google Labels" }
PluginParameter { name: "overlayOpacities"; value: "0.9" }
```

### 地形图 + 道路图
```qml
PluginParameter { name: "layers"; value: "Google Terrain,Google Street Map" }
PluginParameter { name: "opacities"; value: "1.0,0.7" }
```

### 多图层叠加
```qml
PluginParameter { 
    name: "layerConfig"
    value: JSON.stringify([
        {"name": "Google Satellite", "opacity": 1.0, "zOrder": 0},
        {"name": "Google Labels", "opacity": 0.8, "zOrder": 1},
        {"name": "Google Terrain", "opacity": 0.3, "zOrder": 2}
    ])
}
```

## 兼容性

- 单图层模式：完全兼容，无需修改现有代码
- 多图层模式：需要添加相应的 PluginParameter 配置
- 向后兼容：未启用多图层时，行为与之前完全一致

