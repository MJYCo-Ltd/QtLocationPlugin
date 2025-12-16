#include "TmsMapProvider.h"
#include <QFile>
#include <QXmlStreamReader>
TmsMapProvider::TMSMeta TmsMapProvider::_tmsMeta;
QString TmsMapProvider::_mapUrl;

TmsMapProvider::TmsMapProvider()
    : MapProvider(QStringLiteral("TmsLocal"), QStringLiteral(""),
                  QStringLiteral("")) {}

void TmsMapProvider::loadTmsFile(const QString &sTmsFileName) {
    _mapUrl = sTmsFileName.left(sTmsFileName.lastIndexOf('/'));
    QUrl url(sTmsFileName);
    QFile f(url.toLocalFile());
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }

    QXmlStreamReader xml(&f);

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement()) {

            if (xml.name().compare("SRS", Qt::CaseInsensitive) == 0) {
                _tmsMeta.srs = xml.readElementText().trimmed();
            }

            else if (xml.name().compare("TileFormat", Qt::CaseInsensitive) == 0) {
                auto a = xml.attributes();
                _tmsMeta.tileWidth = a.value("width").toInt();
                _tmsMeta.tileHeight = a.value("height").toInt();
                _tmsMeta.extension = a.value("extension").toString();
            }

            else if (xml.name().compare("TileSet", Qt::CaseInsensitive) == 0) {
                int z = xml.attributes().value("order").toInt();
                _tmsMeta.minZoom = qMin(_tmsMeta.minZoom, z);
                _tmsMeta.maxZoom = qMax(_tmsMeta.maxZoom, z);
            }

            else if (xml.name().compare("Origin", Qt::CaseInsensitive) == 0) {
                double y = xml.attributes().value("y").toDouble();
                _tmsMeta.originBottomLeft = (y < 0);
            }

            else if (xml.name().compare("BoundingBox", Qt::CaseInsensitive) == 0) {
                auto a = xml.attributes();
                _tmsMeta.bbox = QRectF(
                    QPointF(a.value("minx").toDouble(), a.value("miny").toDouble()),
                    QPointF(a.value("maxx").toDouble(), a.value("maxy").toDouble()));
            }
        }
    }
}

QString TmsMapProvider::_getURL(int x, int y, int zoom) const {
    QString path = QString("%1/%2/%3/%4.%5")
                       .arg(_mapUrl)
                       .arg(zoom)
                       .arg(x)
                       .arg((1 << zoom) - 1 - y)
                       .arg(_tmsMeta.extension);
    return (path);
}
