/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "EsriMapProvider.h"

QByteArray EsriMapProvider::getToken() const { return "esriToken()"; }

QString EsriMapProvider::_getURL(int x, int y, int zoom) const {
    return _mapUrl.arg(_mapTypeId).arg(zoom).arg(y).arg(x);
}
