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

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc/Providers
)

# 设置源文件
set(SOURCES
    Src/QGCCachedTileSet.cpp
    Src/QGCFileDownload.cc
    Src/QGCMapEngine.cpp
    Src/QGCMapEngineManager.cc
    Src/QGCMapUrlEngine.cpp
    Src/QGCTileCacheWorker.cpp
    Src/QGeoFileTileCacheQGC.cpp
    Src/QGeoMapReplyQGC.cpp
    Src/QGeoServiceProviderPluginQGC.cpp
    Src/QGeoTiledMappingManagerEngineQGC.cpp
    Src/QGeoTiledMapQGC.cpp
    Src/QGeoTileFetcherQGC.cpp
    Src/QmlObjectListModel.cc
    Src/Providers/BingMapProvider.cpp
    Src/Providers/ElevationMapProvider.cpp
    Src/Providers/EsriMapProvider.cpp
    Src/Providers/GenericMapProvider.cpp
    Src/Providers/GoogleMapProvider.cpp
    Src/Providers/MapboxMapProvider.cpp
    Src/Providers/MapProvider.cpp
)

set(HEADERS
    Inc/QGCCachedTileSet.h
    Inc/QGCCacheTile.h
    Inc/QGCFileDownload.h
    Inc/QGCMapEngine.h
    Inc/QGCMapEngineManager.h
    Inc/QGCMapTasks.h
    Inc/QGCMapUrlEngine.h
    Inc/QGCTile.h
    Inc/QGCTileCacheWorker.h
    Inc/QGCTileSet.h
    Inc/QGeoFileTileCacheQGC.h
    Inc/QGeoMapReplyQGC.h
    Inc/QGeoServiceProviderPluginQGC.h
    Inc/QGeoTiledMappingManagerEngineQGC.h
    Inc/QGeoTiledMapQGC.h
    Inc/QGeoTileFetcherQGC.h
    Inc/QmlObjectListModel.h
    Inc/Providers/BingMapProvider.h
    Inc/Providers/ElevationMapProvider.h
    Inc/Providers/EsriMapProvider.h
    Inc/Providers/GenericMapProvider.h
    Inc/Providers/GoogleMapProvider.h
    Inc/Providers/MapboxMapProvider.h
    Inc/Providers/MapProvider.h
)


qt_add_plugin(QGCLocation
    CLASS_NAME QGeoServiceProviderFactoryQGC
    ${HEADERS}
    ${SOURCES}
)

target_link_libraries(QGCLocation
    PRIVATE
        Qt6::LocationPrivate
    PUBLIC
        Qt6::Core
        Qt6::Location
        Qt6::Network
        Qt6::Positioning
        Qt6::Sql
)

get_filename_component(_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)

# 安装库文件
install(TARGETS QGCLocation
    RUNTIME DESTINATION ${_qt_prefix}/plugins/geoservices
    LIBRARY DESTINATION ${_qt_prefix}/plugins/geoservices
)
