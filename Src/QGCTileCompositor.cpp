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
        // 检查索引有效性
        if (i >= tiles.count()) {
            qCWarning(QGCTileCompositorLog) << "Index out of range:" << i;
            break;
        }
        
        const TileImageData &tile = tiles.at(i);
        // 验证瓦片数据有效性
        if (layers.at(i).visible() && tile.isValid && 
            !tile.imageData.isEmpty() && !tile.format.isEmpty()) {
            baseImage = imageFromData(tile.imageData, tile.format);
            if (!baseImage.isNull() && baseImage.width() > 0 && baseImage.height() > 0) {
                outputFormat = tile.format;
                firstValidIndex = i;
                break;
            }
        }
    }

    if (baseImage.isNull() || baseImage.width() == 0 || baseImage.height() == 0) {
        qCWarning(QGCTileCompositorLog) << "No valid base image found";
        return result;
    }

    // 从第一个有效图层之后开始合成
    for (int i = firstValidIndex + 1; i < layers.count(); ++i) {
        // 检查索引有效性
        if (i >= layers.count() || i >= tiles.count()) {
            qCWarning(QGCTileCompositorLog) << "Index out of range:" << i;
            break;
        }
        
        const MapLayer &layer = layers.at(i);
        const TileImageData &tile = tiles.at(i);

        // 验证瓦片数据有效性
        if (!layer.visible() || !tile.isValid || 
            tile.imageData.isEmpty() || tile.format.isEmpty()) {
            continue;
        }

        QImage overlayImage = imageFromData(tile.imageData, tile.format);
        if (overlayImage.isNull() || overlayImage.width() == 0 || overlayImage.height() == 0) {
            continue;
        }

        // 确保尺寸一致
        if (overlayImage.size() != baseImage.size()) {
            overlayImage = overlayImage.scaled(baseImage.size(), 
                                               Qt::IgnoreAspectRatio, 
                                               Qt::SmoothTransformation);
            // 检查缩放后的图像是否有效
            if (overlayImage.isNull() || overlayImage.width() == 0 || overlayImage.height() == 0) {
                qCWarning(QGCTileCompositorLog) << "Failed to scale overlay image";
                continue;
            }
        }

        // 合成图像
        QImage compositeResult = compositeImages(baseImage, overlayImage, layer.opacity());
        if (compositeResult.isNull() || compositeResult.width() == 0 || compositeResult.height() == 0) {
            qCWarning(QGCTileCompositorLog) << "Failed to composite images at layer" << i;
            continue;
        }
        
        baseImage = compositeResult;
    }

    // 转换回字节数组
    result.imageData = imageToData(baseImage, outputFormat);
    result.format = outputFormat;
    result.isValid = !result.imageData.isEmpty();

    return result;
}

QImage TileCompositor::compositeImages(const QImage &base, const QImage &overlay, qreal opacity) {
    if (base.isNull() || base.width() == 0 || base.height() == 0) {
        return overlay;
    }
    if (overlay.isNull() || overlay.width() == 0 || overlay.height() == 0) {
        return base;
    }

    QImage result = base.copy();
    if (result.isNull() || result.width() == 0 || result.height() == 0) {
        qCWarning(QGCTileCompositorLog) << "Failed to copy base image";
        return base;
    }

    QPainter painter(&result);
    if (!painter.isActive()) {
        qCWarning(QGCTileCompositorLog) << "Failed to create painter";
        return base;
    }
    
    // 设置合成模式
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    
    // 设置透明度（确保在有效范围内）
    painter.setOpacity(qBound(0.0, opacity, 1.0));
    
    // 绘制叠加层
    painter.drawImage(0, 0, overlay);
    painter.end();

    // 验证结果
    if (result.isNull() || result.width() == 0 || result.height() == 0) {
        qCWarning(QGCTileCompositorLog) << "Composite result is invalid";
        return base;
    }

    return result;
}

QImage TileCompositor::imageFromData(const QByteArray &data, const QString &format) {
    if (data.isEmpty() || data.isNull()) {
        return QImage();
    }

    QImage image;
    
    // 使用 QBuffer 作为 QImageReader 的输入设备
    QBuffer buffer;
    buffer.setData(data);
    if (!buffer.open(QIODevice::ReadOnly)) {
        qCWarning(QGCTileCompositorLog) << "Failed to open buffer for reading";
        return QImage();
    }
    
    QImageReader reader(&buffer);
    if (!format.isEmpty()) {
        reader.setFormat(format.toUtf8());
    }
    
    if (reader.canRead()) {
        image = reader.read();
    }
    
    // 如果读取失败，尝试使用 QImage::loadFromData 自动检测格式
    if (image.isNull()) {
        if (!image.loadFromData(data)) {
            qCWarning(QGCTileCompositorLog) << "Failed to load image from data, format:" << format;
            return QImage();
        }
    }

    // 验证图像有效性
    if (image.isNull() || image.width() == 0 || image.height() == 0) {
        qCWarning(QGCTileCompositorLog) << "Invalid image dimensions";
        return QImage();
    }

    // 确保图像格式支持透明度
    if (image.format() != QImage::Format_ARGB32 && 
        image.format() != QImage::Format_ARGB32_Premultiplied) {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        // 检查转换后的图像是否有效
        if (image.isNull() || image.width() == 0 || image.height() == 0) {
            qCWarning(QGCTileCompositorLog) << "Failed to convert image format";
            return QImage();
        }
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

