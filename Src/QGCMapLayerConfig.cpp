/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "QGCMapLayerConfig.h"
#include "QGCMapUrlEngine.h"
#include "MapProvider.h"

#include <QtCore/QHash>
#include <QtCore/QCryptographicHash>
#include <QtCore/QStringList>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QVariantMap>
#include <algorithm>

Q_LOGGING_CATEGORY(QGCMapLayerConfigLog, "qgc.qtlocationplugin.qgcmaplayerconfig")

void MapLayerStack::addLayer(const MapLayer &layer) {
    // 移除已存在的相同 mapId 的图层
    removeLayer(layer.mapId());
    m_layers.append(layer);
    sort();
}

void MapLayerStack::removeLayer(int mapId) {
    m_layers.removeIf([mapId](const MapLayer &layer) {
        return layer.mapId() == mapId;
    });
}

void MapLayerStack::clear() {
    m_layers.clear();
}

MapLayer MapLayerStack::layer(int index) const {
    if (index >= 0 && index < m_layers.count()) {
        return m_layers.at(index);
    }
    return MapLayer();
}

MapLayer MapLayerStack::layerByMapId(int mapId) const {
    for (const MapLayer &layer : m_layers) {
        if (layer.mapId() == mapId) {
            return layer;
        }
    }
    return MapLayer();
}

void MapLayerStack::sort() {
    std::stable_sort(m_layers.begin(), m_layers.end(),
              [](const MapLayer &a, const MapLayer &b) {
                  if (a.zOrder() != b.zOrder()) {
                      return a.zOrder() < b.zOrder();
                  }
                  return a.mapId() < b.mapId();
              });
}

QString MapLayerStack::generateCacheKey() const {
    QByteArray canonical;
    for (const MapLayer &layer : m_layers) {
        if (layer.visible()) {
            canonical.append(QByteArray::number(layer.mapId()));
            canonical.append(':');
            canonical.append(QByteArray::number(layer.zOrder()));
            canonical.append(':');
            canonical.append(QByteArray::number(layer.opacity(), 'g', 17));
            canonical.append(';');
        }
    }
    return QString::fromLatin1(
        QCryptographicHash::hash(canonical, QCryptographicHash::Sha256).toHex());
}

int MapLayerStack::generateMapId() const {
    if (m_layers.isEmpty()) {
        return -1;
    }

    bool ok = false;
    const quint32 hashPrefix = generateCacheKey().left(8).toUInt(&ok, 16);
    if (!ok) {
        return -1;
    }

    // 保持为正整数，并避开普通 provider 使用的低位 mapId。
    return 10000 + static_cast<int>(hashPrefix & 0x3fffffffU);
}

MapLayerStack MapLayerStack::fromParameters(const QVariantMap &parameters) {
    MapLayerStack stack;

    // 检查是否启用多图层
    if (!parameters.contains("multiLayer") && 
        !parameters.contains("multiLayerEnabled")) {
        return stack;  // 未启用多图层
    }

    bool enabled = false;
    if (parameters.contains("multiLayer")) {
        enabled = parameters["multiLayer"].toBool();
    } else if (parameters.contains("multiLayerEnabled")) {
        enabled = parameters["multiLayerEnabled"].toString().toLower() == "true";
    }

    if (!enabled) {
        return stack;
    }

    // 方式1: 通过 baseLayer 和 overlayLayers 配置
    if (parameters.contains("baseLayer") && parameters.contains("overlayLayers")) {
        QString baseLayerName = parameters["baseLayer"].toString();
        QStringList overlayNames = parameters["overlayLayers"].toString().split(",", Qt::SkipEmptyParts);
        QStringList overlayOpacities;
        
        if (parameters.contains("overlayOpacities")) {
            overlayOpacities = parameters["overlayOpacities"].toString().split(",", Qt::SkipEmptyParts);
        }

        // 添加基础图层
        SharedMapProvider baseProvider = UrlFactory::getMapProviderFromProviderType(baseLayerName);
        if (baseProvider) {
            stack.addLayer(MapLayer(baseProvider->getMapId(), 0, 1.0, true));
        }

        // 添加叠加图层
        for (int i = 0; i < overlayNames.count(); ++i) {
            QString overlayName = overlayNames.at(i).trimmed();
            qreal opacity = 1.0;
            if (i < overlayOpacities.count()) {
                bool ok = false;
                opacity = overlayOpacities.at(i).trimmed().toDouble(&ok);
                if (!ok || opacity < 0.0 || opacity > 1.0) {
                    opacity = 1.0;
                }
            }

            SharedMapProvider overlayProvider = UrlFactory::getMapProviderFromProviderType(overlayName);
            if (overlayProvider) {
                stack.addLayer(MapLayer(overlayProvider->getMapId(), i + 1, opacity, true));
            }
        }
    }
    // 方式2: 通过 layers 和 opacities 配置（逗号分隔）
    else if (parameters.contains("layers")) {
        QStringList layerNames = parameters["layers"].toString().split(",", Qt::SkipEmptyParts);
        QStringList opacities;
        
        if (parameters.contains("opacities")) {
            opacities = parameters["opacities"].toString().split(",", Qt::SkipEmptyParts);
        }

        for (int i = 0; i < layerNames.count(); ++i) {
            QString layerName = layerNames.at(i).trimmed();
            qreal opacity = 1.0;
            if (i < opacities.count()) {
                bool ok = false;
                opacity = opacities.at(i).trimmed().toDouble(&ok);
                if (!ok || opacity < 0.0 || opacity > 1.0) {
                    opacity = 1.0;
                }
            }

            SharedMapProvider provider = UrlFactory::getMapProviderFromProviderType(layerName);
            if (provider) {
                stack.addLayer(MapLayer(provider->getMapId(), i, opacity, true));
            }
        }
    }
    // 方式3: 通过 layerConfig JSON 配置
    else if (parameters.contains("layerConfig")) {
        QString configStr = parameters["layerConfig"].toString();
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(configStr.toUtf8(), &error);
        
        if (error.error == QJsonParseError::NoError && doc.isArray()) {
            QJsonArray array = doc.array();
            for (const QJsonValue &value : array) {
                if (value.isObject()) {
                    QJsonObject obj = value.toObject();
                    QString layerName = obj["name"].toString();
                    qreal opacity = obj["opacity"].toDouble(1.0);
                    int zOrder = obj["zOrder"].toInt(-1);
                    bool visible = obj["visible"].toBool(true);

                    SharedMapProvider provider = UrlFactory::getMapProviderFromProviderType(layerName);
                    if (provider) {
                        if (zOrder < 0) {
                            zOrder = stack.count();
                        }
                        stack.addLayer(MapLayer(provider->getMapId(), zOrder, opacity, visible));
                    }
                }
            }
        }
    }

    return stack;
}

bool MapLayerStack::operator==(const MapLayerStack &other) const {
    if (m_layers.count() != other.m_layers.count()) {
        return false;
    }
    for (int i = 0; i < m_layers.count(); ++i) {
        if (!(m_layers.at(i) == other.m_layers.at(i))) {
            return false;
        }
    }
    return true;
}

