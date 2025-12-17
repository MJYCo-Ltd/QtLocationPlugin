#ifndef TIANDIMAPPROVIDER_H
#define TIANDIMAPPROVIDER_H

#include "MapProvider.h"

class TiandiMapProvider : public MapProvider {
public:
    static QString _key;
protected:
    TiandiMapProvider(const QString &mapName, QString type,
                      QGeoMapType::MapStyle mapType)
        : MapProvider(mapName, QStringLiteral("https://map.tianditu.gov.cn/"),
                      QStringLiteral(""), AVERAGE_TILE_SIZE, mapType,1,18),
        _type(type){}

private:
    QString _getURL(int x, int y, int zoom) const final;

    const QString _type;
    const QString _mapUrl =
        QStringLiteral("http://t%1.tianditu.gov.cn/%2/"
                                           "wmts?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&LAYER="
                                           "img&STYLE=default&TILEMATRIXSET=w&FORMAT=tiles&"
                                           "TILEMATRIX=%3&TILEROW=%4&TILECOL=%5&tk=%6");
};

class TiandiStreetMapProvider : public TiandiMapProvider {
public:
    TiandiStreetMapProvider()
        : TiandiMapProvider(QStringLiteral("天地图街道"),"vec_w",
                           QGeoMapType::StreetMap) {}
};

class TiandiSatelliteMapProvider : public TiandiMapProvider {
public:
    TiandiSatelliteMapProvider()
        : TiandiMapProvider(QStringLiteral("天地图卫星"), "img_w",
                           QGeoMapType::SatelliteMapDay) {}
};

class TiandiTerrainMapProvider : public TiandiMapProvider {
public:
    TiandiTerrainMapProvider()
        : TiandiMapProvider(QStringLiteral("天地图高程"), "ter_w",
                            QGeoMapType::TerrainMap) {}
};
#endif // TIANDIMAPPROVIDER_H
