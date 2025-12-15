cmake_minimum_required(VERSION 3.16)

project(QGCLocation VERSION 0.1 LANGUAGES CXX)
set(CMAKE_DEBUG_POSTFIX "d")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Network
    Sql
    Location
    Positioning
)

qt_standard_project_setup(REQUIRES 6.9)


# 把源码先收集到一个变量，避免宏内部出现意外的文件名解析
set(QGCL_LOCATION_SOURCES
    Providers/BingMapProvider.cpp
    Providers/BingMapProvider.h
    Providers/ElevationMapProvider.cpp
    Providers/ElevationMapProvider.h
    Providers/EsriMapProvider.cpp
    Providers/EsriMapProvider.h
    Providers/GenericMapProvider.cpp
    Providers/GenericMapProvider.h
    Providers/GoogleMapProvider.cpp
    Providers/GoogleMapProvider.h
    Providers/MapboxMapProvider.cpp
    Providers/MapboxMapProvider.h
    Providers/MapProvider.cpp
    Providers/MapProvider.h
    QGCCachedTileSet.cpp
    QGCCachedTileSet.h
    QGCCacheTile.h
    QGCMapEngine.cpp
    QGCMapEngine.h
    QGCMapEngineManager.cc
    QGCMapEngineManager.h
    QGCMapTasks.h
    QGCMapUrlEngine.cpp
    QGCMapUrlEngine.h
    QGCTile.h
    QGCTileCacheWorker.cpp
    QGCTileCacheWorker.h
    QGCTileSet.h
    QGeoFileTileCacheQGC.cpp
    QGeoFileTileCacheQGC.h
    QGeoMapReplyQGC.cpp
    QGeoMapReplyQGC.h
    QGeoServiceProviderPluginQGC.cpp
    QGeoServiceProviderPluginQGC.h
    QGeoTiledMappingManagerEngineQGC.cpp
    QGeoTiledMappingManagerEngineQGC.h
    QGeoTiledMapQGC.cpp
    QGeoTiledMapQGC.h
    QGeoTileFetcherQGC.cpp
    QGeoTileFetcherQGC.h
    QmlObjectListModel.cc
    QmlObjectListModel.h
    QGCFileDownload.cc
    QGCFileDownload.h
)

qt_add_plugin(QGCLocation
    CLASS_NAME QGeoServiceProviderFactoryQGC
    OUTPUT_TARGETS QGCLocation_targets
    ${QGCL_LOCATION_SOURCES}
)

target_link_libraries(QGCLocation
    PRIVATE
        Qt6::Positioning
        Qt6::LocationPrivate
        Qt6::Sql
    PUBLIC
        Qt6::Core
        Qt6::Location
        Qt6::Network
)

# 使用生成表达式，在“构建时”使用源码目录，在“安装/导出时”使用安装目录（避免导出时引用源码目录）
target_include_directories(QGCLocation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Providers>
        $<INSTALL_INTERFACE:include/QGCLocation>
)

get_filename_component(_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)

# 安装库文件
install(TARGETS QGCLocation
    RUNTIME DESTINATION ${_qt_prefix}/plugins/geoservices
    LIBRARY DESTINATION ${_qt_prefix}/plugins/geoservices
)
