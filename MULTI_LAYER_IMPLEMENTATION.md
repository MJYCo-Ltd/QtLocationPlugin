# 多图层渲染实现总结

## 已完成的工作

### 1. 核心数据结构 ✅
- **MapLayer** (`Inc/QGCMapLayerConfig.h`)
  - 单个图层配置（mapId, zOrder, opacity, visible）
  - 支持图层属性管理

- **MapLayerStack** (`Inc/QGCMapLayerConfig.h`)
  - 图层组合配置管理
  - 支持从 PluginParameter 解析配置
  - 支持三种配置方式：
    - baseLayer + overlayLayers
    - layers 列表
    - JSON 配置

### 2. 瓦片合成器 ✅
- **TileCompositor** (`Inc/QGCTileCompositor.h`)
  - 实现多图层瓦片合成
  - 支持 Alpha 混合
  - 支持不同图像格式转换

### 3. 引擎扩展 ✅
- **QGeoTiledMappingManagerEngineQGC**
  - 添加图层配置解析
  - 支持从参数读取图层配置
  - 提供图层配置查询接口

### 4. 瓦片获取器扩展 ✅
- **QGeoTileFetcherQGC**
  - 添加多图层检测逻辑
  - 支持多图层瓦片请求（基础框架）
  - 保持单图层模式兼容性

### 5. 构建系统 ✅
- 更新 `CMakeLists.txt`
  - 添加新源文件和头文件
  - 确保所有文件正确编译

## 当前实现状态

### ✅ 已完成
1. 图层配置数据结构
2. 图层配置解析（从 PluginParameter）
3. 瓦片合成算法（TileCompositor）
4. 多图层检测逻辑
5. 基础框架集成

### ⚠️ 部分实现
1. **多图层瓦片获取和合成**
   - 当前：返回基础图层作为占位
   - 需要：实现完整的异步合成逻辑

### 📝 待完善功能

#### 1. 异步多图层合成（高优先级）
**位置**: `Src/QGeoTileFetcherQGC.cpp::getMultiLayerTileImage()`

**需要实现**:
```cpp
// 1. 创建多个网络请求获取所有图层的瓦片
QList<QNetworkRequest> requests;
for (const MapLayer &layer : visibleLayers) {
    requests.append(getNetworkRequest(layer.mapId(), x, y, zoom));
}

// 2. 并行获取所有瓦片
// 使用 QNetworkAccessManager 或 QFuture 并行下载

// 3. 等待所有瓦片加载完成
// 使用信号槽或 QFutureWatcher

// 4. 使用 TileCompositor 进行合成
TileImageData compositeResult = TileCompositor::composite(layers, tiles);

// 5. 创建合成后的回复对象
// 需要扩展 QGeoTiledMapReplyQGC 或创建新类
```

**建议方案**:
- 创建 `QGeoMultiLayerMapReplyQGC` 类
- 继承或组合 `QGeoTiledMapReply`
- 管理多个网络请求
- 在全部完成后进行合成

#### 2. 多图层缓存（中优先级）
**位置**: `Src/QGeoFileTileCacheQGC.cpp`

**需要实现**:
- 生成包含图层配置的缓存键
- 检查合成瓦片缓存
- 缓存合成后的瓦片

**缓存键格式**:
```cpp
QString cacheKey = QString("%1_%2_%3_%4_%5")
    .arg(layerStack.generateCacheKey())  // 图层配置哈希
    .arg(x).arg(y).arg(zoom)
    .arg("composite");  // 标识为合成瓦片
```

#### 3. 性能优化（中优先级）
- 缓存合成结果，避免重复合成
- 优化图像合成算法
- 减少内存拷贝

#### 4. 错误处理（低优先级）
- 部分图层加载失败时的降级策略
- 网络超时处理
- 合成失败时的回退方案

## 使用示例

### QML 配置
```qml
Map {
    plugin: Plugin {
        name: "QGroundControl"
        PluginParameter { name: "multiLayer"; value: "true" }
        PluginParameter { name: "baseLayer"; value: "Google Satellite" }
        PluginParameter { name: "overlayLayers"; value: "Google Labels" }
        PluginParameter { name: "overlayOpacities"; value: "0.8" }
    }
    activeMapType: supportedMapTypes[0]
}
```

## 架构设计

```
QML Map
    ↓
Plugin (PluginParameter)
    ↓
QGeoTiledMappingManagerEngineQGC
    ├─ parseLayerConfiguration() 解析图层配置
    └─ getLayerStackForMapId() 查询图层配置
    ↓
QGeoTileFetcherQGC
    ├─ getTileImage() 检测多图层
    └─ getMultiLayerTileImage() 多图层处理
        ├─ 并行获取所有图层瓦片 (TODO)
        ├─ 等待所有瓦片完成 (TODO)
        └─ TileCompositor::composite() 合成
    ↓
QGeoTiledMapReplyQGC (或新的多图层回复类)
    └─ 返回合成后的瓦片
```

## 下一步工作

1. **实现异步多图层合成**（最重要）
   - 创建 `QGeoMultiLayerMapReplyQGC` 类
   - 实现并行瓦片获取
   - 实现合成逻辑

2. **扩展缓存机制**
   - 支持合成瓦片缓存
   - 优化缓存键生成

3. **测试和优化**
   - 单元测试
   - 性能测试
   - 内存优化

## 注意事项

1. **向后兼容**: 单图层模式完全兼容，无需修改现有代码
2. **性能影响**: 多图层会增加网络请求和计算开销
3. **内存使用**: 合成过程需要临时内存存储多个瓦片
4. **线程安全**: 确保多线程环境下的安全性

## 相关文件

- `Inc/QGCMapLayerConfig.h` - 图层配置数据结构
- `Src/QGCMapLayerConfig.cpp` - 图层配置解析实现
- `Inc/QGCTileCompositor.h` - 瓦片合成器接口
- `Src/QGCTileCompositor.cpp` - 瓦片合成实现
- `Inc/QGeoTiledMappingManagerEngineQGC.h` - 引擎扩展
- `Src/QGeoTiledMappingManagerEngineQGC.cpp` - 引擎实现
- `Inc/QGeoTileFetcherQGC.h` - 瓦片获取器扩展
- `Src/QGeoTileFetcherQGC.cpp` - 瓦片获取实现
- `MULTI_LAYER_USAGE.md` - 使用说明文档

