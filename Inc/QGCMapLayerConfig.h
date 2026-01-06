/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QVariantMap>
#include <QtCore/QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(QGCMapLayerConfigLog)

/**
 * @brief 单个图层配置
 */
class MapLayer {
public:
    MapLayer() = default;
    MapLayer(int mapId, int zOrder, qreal opacity, bool visible = true)
        : m_mapId(mapId)
        , m_zOrder(zOrder)
        , m_opacity(opacity)
        , m_visible(visible)
    {}

    int mapId() const { return m_mapId; }
    void setMapId(int mapId) { m_mapId = mapId; }

    int zOrder() const { return m_zOrder; }
    void setZOrder(int zOrder) { m_zOrder = zOrder; }

    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal opacity) { m_opacity = qBound(0.0, opacity, 1.0); }

    bool visible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }

    QString layerName() const { return m_layerName; }
    void setLayerName(const QString &name) { m_layerName = name; }

    bool operator==(const MapLayer &other) const {
        return m_mapId == other.m_mapId &&
               m_zOrder == other.m_zOrder &&
               qFuzzyCompare(m_opacity, other.m_opacity) &&
               m_visible == other.m_visible;
    }

private:
    int m_mapId = -1;
    int m_zOrder = 0;
    qreal m_opacity = 1.0;
    bool m_visible = true;
    QString m_layerName;
};

/**
 * @brief 图层组合配置（图层栈）
 */
class MapLayerStack {
public:
    MapLayerStack() = default;

    void addLayer(const MapLayer &layer);
    void removeLayer(int mapId);
    void clear();
    bool isEmpty() const { return m_layers.isEmpty(); }
    int count() const { return m_layers.count(); }

    QList<MapLayer> layers() const { return m_layers; }
    MapLayer layer(int index) const;
    MapLayer layerByMapId(int mapId) const;

    // 按 zOrder 排序
    void sort();

    // 生成缓存键的哈希值
    QString generateCacheKey() const;

    // 从参数解析
    static MapLayerStack fromParameters(const QVariantMap &parameters);

    bool operator==(const MapLayerStack &other) const;

private:
    QList<MapLayer> m_layers;
};

