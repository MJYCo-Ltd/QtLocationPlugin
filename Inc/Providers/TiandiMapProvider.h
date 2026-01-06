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
        QStringLiteral("http://t%1.tianditu.gov.cn/%2_w/"
                                           "wmts?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&LAYER="
                                           "%2&STYLE=default&TILEMATRIXSET=w&FORMAT=tiles&"
                                           "TILEMATRIX=%3&TILEROW=%4&TILECOL=%5&tk=%6");
};

class TiandiStreetMapProvider : public TiandiMapProvider {
public:
    TiandiStreetMapProvider()
        : TiandiMapProvider(QStringLiteral("天地图街道"),"vec",
                           QGeoMapType::StreetMap) {}
};

class TiandiStreetMapNoteProvider : public TiandiMapProvider {
public:
    TiandiStreetMapNoteProvider()
        : TiandiMapProvider(QStringLiteral("天地图街道注记"),"cva",
                            QGeoMapType::StreetMap) {}
};

class TiandiSatelliteMapProvider : public TiandiMapProvider {
public:
    TiandiSatelliteMapProvider()
        : TiandiMapProvider(QStringLiteral("天地图卫星"), "img",
                           QGeoMapType::SatelliteMapDay) {}
};

class TiandiSatelliteMapNoteProvider : public TiandiMapProvider {
public:
    TiandiSatelliteMapNoteProvider()
        : TiandiMapProvider(QStringLiteral("天地图卫星注记"), "cia",
                            QGeoMapType::SatelliteMapDay) {}
};

class TiandiTerrainMapProvider : public TiandiMapProvider {
public:
    TiandiTerrainMapProvider()
        : TiandiMapProvider(QStringLiteral("天地图高程"), "ter",
                            QGeoMapType::TerrainMap) {}
};

class TiandiTerrainMapNoteProvider : public TiandiMapProvider {
public:
    TiandiTerrainMapNoteProvider()
        : TiandiMapProvider(QStringLiteral("天地图高程注记"), "cta",
                            QGeoMapType::TerrainMap) {}
};

class TiandiBorderProvider : public TiandiMapProvider {
public:
    TiandiBorderProvider()
        : TiandiMapProvider(QStringLiteral("天地图边界"), "ibo",
                            QGeoMapType::TerrainMap) {}
};
#endif // TIANDIMAPPROVIDER_H
