cmake_minimum_required(VERSION 3.16)

project(Test VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS
    Quick
    Location
    Positioning
    LocationPrivate
)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appTest
    main.cpp
    ../Inc/QGCMapTileMonitor.h
    ../Src/QGCMapTileMonitor.cpp
    ../Src/QGCQmlRegistration.cpp
)

qt_add_qml_module(appTest
    URI Test
    QML_FILES
        Main.qml
)

set_target_properties(appTest PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    AUTOMOC ON
)

target_include_directories(appTest PRIVATE ${CMAKE_SOURCE_DIR}/Inc)

target_link_libraries(appTest
    PRIVATE
    Qt6::Quick
    Qt6::Location
    Qt6::LocationPrivate
    Qt6::Positioning
)
