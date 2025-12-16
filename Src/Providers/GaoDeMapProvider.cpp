#include "GaoDeMapProvider.h"

QString GaoDeMapProvider::_getURL(int x, int y, int zoom) const
{
    QString stemp= _mapUrl
        .arg(_getServerNum(x, y, 4)+1)
        .arg(_mapTypeId)
        .arg(x)
        .arg(y)
        .arg(zoom);

    return(stemp);
}
