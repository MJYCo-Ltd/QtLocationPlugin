#include "TiandiMapProvider.h"

QString TiandiMapProvider::_key;

QString TiandiMapProvider::_getURL(int x, int y, int zoom) const {
    QString stemp = _mapUrl.arg(_getServerNum(x, y, 8))
                        .arg(_type)
                        .arg(zoom)
                        .arg(y)
                        .arg(x)
                        .arg(_key);
    qDebug()<<stemp;

    return (stemp);
}
