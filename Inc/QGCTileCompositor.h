/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "QGCMapLayerConfig.h"
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QLoggingCategory>
#include <QtGui/QImage>

Q_DECLARE_LOGGING_CATEGORY(QGCTileCompositorLog)

/**
 * @brief 瓦片数据（用于合成）
 */
struct TileImageData {
    QByteArray imageData;
    QString format;  // "png", "jpg", etc.
    bool isValid = false;
};

/**
 * @brief 瓦片合成器
 * 负责将多个图层的瓦片合成为一个最终瓦片
 */
class TileCompositor {
public:
    TileCompositor() = default;
    ~TileCompositor() = default;

    /**
     * @brief 合成多个图层的瓦片
     * @param layers 图层配置列表（已按 zOrder 排序）
     * @param tiles 对应图层的瓦片数据（按 layers 顺序）
     * @return 合成后的瓦片数据
     */
    static TileImageData composite(const QList<MapLayer> &layers,
                                   const QList<TileImageData> &tiles);

    /**
     * @brief 合成两个图像（带透明度）
     * @param base 底层图像
     * @param overlay 叠加层图像
     * @param opacity 叠加层透明度
     * @return 合成后的图像
     */
    static QImage compositeImages(const QImage &base, const QImage &overlay, qreal opacity);

private:
    /**
     * @brief 将字节数组转换为 QImage
     */
    static QImage imageFromData(const QByteArray &data, const QString &format);

    /**
     * @brief 将 QImage 转换为字节数组
     */
    static QByteArray imageToData(const QImage &image, const QString &format);
};

