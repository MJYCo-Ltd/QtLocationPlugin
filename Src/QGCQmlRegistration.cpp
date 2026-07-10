/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "QGCQmlRegistration.h"
#include "QGCMapTileMonitor.h"

#include <QtQml/qqml.h>

namespace {
constexpr int kQgcModuleVersionMajor = 1;
constexpr int kQgcModuleVersionMinor = 0;
constexpr char kQgcModuleUri[] = "QGroundControl";
} // namespace

void registerQgcLocationQmlTypes() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    qmlRegisterModule(kQgcModuleUri, kQgcModuleVersionMajor,
                      kQgcModuleVersionMinor);
    (void)qmlRegisterType<QGCMapTileMonitor>(
        kQgcModuleUri, kQgcModuleVersionMajor, kQgcModuleVersionMinor,
        "MapTileMonitor");
}
