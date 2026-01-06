/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "QGCTileCompositor.h"

#include <QtGui/QPainter>
#include <QtGui/QImageReader>
#include <QtGui/QImageWriter>
#include <QtCore/QBuffer>
#include <algorithm>

Q_LOGGING_CATEGORY(QGCTileCompositorLog, "qgc.qtlocationplugin.qgctilecompositor")

TileImageData TileCompositor::composite(const QList<MapLayer> &layers,
                                         const QList<TileImageData> &tiles) {
    TileImageData result;
    
    if (layers.isEmpty() || tiles.isEmpty() || layers.count() != tiles.count()) {
        qCWarning(QGCTileCompositorLog) << "Invalid input for compositing";
        return result;
    }

    // 找到第一个有效的图层作为基础
    QImage baseImage;
    QString outputFormat = "png";  // 默认使用 PNG 以支持透明度
    
    int firstValidIndex = -1;
    for (int i = 0; i < layers.count(); ++i) {
        if (layers.at(i).visible() && tiles.at(i).isValid) {
            baseImage = imageFromData(tiles.at(i).imageData, tiles.at(i).format);
            if (!baseImage.isNull()) {
                outputFormat = tiles.at(i).format;
                firstValidIndex = i;
                break;
            }
        }
    }

    if (baseImage.isNull()) {
        qCWarning(QGCTileCompositorLog) << "No valid base image found";
        return result;
    }

    // 从第一个有效图层之后开始合成
    for (int i = firstValidIndex + 1; i < layers.count(); ++i) {
        const MapLayer &layer = layers.at(i);
        const TileImageData &tile = tiles.at(i);

        if (!layer.visible() || !tile.isValid) {
            continue;
        }

        QImage overlayImage = imageFromData(tile.imageData, tile.format);
        if (overlayImage.isNull()) {
            continue;
        }

        // 确保尺寸一致
        if (overlayImage.size() != baseImage.size()) {
            overlayImage = overlayImage.scaled(baseImage.size(), 
                                               Qt::IgnoreAspectRatio, 
                                               Qt::SmoothTransformation);
        }

        // 合成图像
        baseImage = compositeImages(baseImage, overlayImage, layer.opacity());
    }

    // 转换回字节数组
    result.imageData = imageToData(baseImage, outputFormat);
    result.format = outputFormat;
    result.isValid = !result.imageData.isEmpty();

    return result;
}

QImage TileCompositor::compositeImages(const QImage &base, const QImage &overlay, qreal opacity) {
    if (base.isNull()) {
        return overlay;
    }
    if (overlay.isNull()) {
        return base;
    }

    QImage result = base.copy();
    QPainter painter(&result);
    
    // 设置合成模式
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    
    // 设置透明度
    painter.setOpacity(opacity);
    
    // 绘制叠加层
    painter.drawImage(0, 0, overlay);
    painter.end();

    return result;
}

QImage TileCompositor::imageFromData(const QByteArray &data, const QString &format) {
    if (data.isEmpty()) {
        return QImage();
    }

    QImage image;
    
    // 使用 QBuffer 作为 QImageReader 的输入设备
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    
    QImageReader reader(&buffer);
    if (!format.isEmpty()) {
        reader.setFormat(format.toUtf8());
    }
    
    if (reader.canRead()) {
        image = reader.read();
    }
    
    // 如果读取失败，尝试使用 QImage::loadFromData 自动检测格式
    if (image.isNull()) {
        image.loadFromData(data);
    }

    // 确保图像格式支持透明度
    if (!image.isNull() && 
        image.format() != QImage::Format_ARGB32 && 
        image.format() != QImage::Format_ARGB32_Premultiplied) {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    return image;
}

QByteArray TileCompositor::imageToData(const QImage &image, const QString &format) {
    if (image.isNull()) {
        return QByteArray();
    }

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);

    QImageWriter writer(&buffer, format.toUtf8());
    if (format == "png") {
        writer.setQuality(100);  // PNG 无损
    } else if (format == "jpg" || format == "jpeg") {
        writer.setQuality(90);   // JPG 质量
    }

    if (!writer.write(image)) {
        qCWarning(QGCTileCompositorLog) << "Failed to write image:" << writer.errorString();
        return QByteArray();
    }

    return data;
}

