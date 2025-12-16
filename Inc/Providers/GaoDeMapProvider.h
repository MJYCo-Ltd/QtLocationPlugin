#ifndef GAODEMAPPROVIDER_H
#define GAODEMAPPROVIDER_H

#include "MapProvider.h"

class GaoDeMapProvider : public MapProvider
{
protected:
    GaoDeMapProvider(const QString &mapName, int mapTypeId,
                      QGeoMapType::MapStyle mapType)
        : MapProvider(
              mapName,
              QStringLiteral("https://ditu.amap.com/"),
              QStringLiteral(""),
              19597,
              mapType),_mapTypeId(mapTypeId){}

private:
    QString _getURL(int x, int y, int zoom) const final;

    const int _mapTypeId;
    const QString _mapUrl = QStringLiteral("https://webst0%1.is.autonavi.com/appmaptile?style=%2&x=%3&y=%4&z=%5");
};

class GaoDeStreetMapProvider : public GaoDeMapProvider
{
public:
    GaoDeStreetMapProvider()
        : GaoDeMapProvider(
              QStringLiteral("高德街道"),
              7,
              QGeoMapType::StreetMap) {}
};

class GaoDeSatelliteMapProvider : public GaoDeMapProvider
{
public:
    GaoDeSatelliteMapProvider()
        : GaoDeMapProvider(
              QStringLiteral("高德卫星"),
              6,
              QGeoMapType::SatelliteMapDay) {}
};
#endif // GAODEMAPPROVIDER_H
