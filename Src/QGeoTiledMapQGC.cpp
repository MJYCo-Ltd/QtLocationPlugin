/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "QGeoTiledMapQGC.h"
#include "QGeoTiledMappingManagerEngineQGC.h"

Q_LOGGING_CATEGORY(QGeoTiledMapQGCLog, "qgc.qtlocationplugin.qgeotiledmapqgc")

QGeoTiledMapQGC::QGeoTiledMapQGC(QGeoTiledMappingManagerEngineQGC *engine,
                                 QObject *parent)
    : QGeoTiledMap(engine, parent) {}

QGeoTiledMapQGC::~QGeoTiledMapQGC() {}

QGeoMap::Capabilities QGeoTiledMapQGC::capabilities() const {
    return Capabilities(SupportsVisibleRegion | SupportsAnchoringCoordinate |
                        SupportsVisibleArea);
}
