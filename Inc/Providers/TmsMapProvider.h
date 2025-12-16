#ifndef TMSMAPPROVIDER_H
#define TMSMAPPROVIDER_H
#include <QRectF>
#include <QString>
#include "MapProvider.h"

class TmsMapProvider : public MapProvider
{
public:
    TmsMapProvider();
    static void loadTmsFile(const QString& sTmsFileName);

protected:
    struct TMSMeta
    {
        int tileWidth = 256;
        int tileHeight = 256;

        int minZoom = 0;
        int maxZoom = 18;

        QString srs;              // EPSG:3857
        bool originBottomLeft;    // TMS 核心

        QString extension;        // png / jpg
        QRectF bbox;              // 可选
    };

private:
    QString _getURL(int x, int y, int zoom) const final;

    static TMSMeta _tmsMeta;
    static QString _mapUrl;
};
#endif // TMSMAPPROVIDER_H
