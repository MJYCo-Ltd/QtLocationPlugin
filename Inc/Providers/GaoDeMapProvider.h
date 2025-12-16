#ifndef GAODEMAPPROVIDER_H
#define GAODEMAPPROVIDER_H

#include "MapProvider.h"

class GaoDeMapProvider : public MapProvider
{
protected:
    GaoDeMapProvider(const QString &mapName, int mapTypeId,QString type,
                      QGeoMapType::MapStyle mapType)
        : MapProvider(
              mapName,
              QStringLiteral("https://ditu.amap.com/"),
              QStringLiteral(""),
              19597,
              mapType),_mapTypeId(mapTypeId),_type(type) {}

private:
    QString _getURL(int x, int y, int zoom) const final;

    const int _mapTypeId;
    const QString _type;
    const QString _mapUrl = QStringLiteral("https://web%1%2.is.autonavi.com/appmaptile?lang=%3&size=1&scale=1&style=%4&x=%5&y=%6&z=%7");
};

class GaoDeStreetMapProvider : public GaoDeMapProvider
{
public:
    GaoDeStreetMapProvider()
        : GaoDeMapProvider(
              QStringLiteral("高德街道"),
              8,
              "rd0",
              QGeoMapType::StreetMap) {}
};

class GaoDeSatelliteMapProvider : public GaoDeMapProvider
{
public:
    GaoDeSatelliteMapProvider()
        : GaoDeMapProvider(
              QStringLiteral("高德卫星"),
              6,
              "st0",
              QGeoMapType::SatelliteMapDay) {}
};
#endif // GAODEMAPPROVIDER_H
