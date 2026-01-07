/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QLoggingCategory>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QHash>
#include "QGeoMapReplyQGC.h"
#include "QGCMapLayerConfig.h"
#include "QGCTileCompositor.h"

Q_DECLARE_LOGGING_CATEGORY(QGeoMultiLayerMapReplyQGCLog)

class QNetworkAccessManager;
class QSslError;

/**
 * @brief 多图层瓦片回复类
 * 负责并行获取多个图层的瓦片，并在全部完成后进行合成
 * 继承自 QGeoTiledMapReplyQGC 以复用缓存、错误处理等逻辑
 */
class QGeoMultiLayerMapReplyQGC : public QGeoTiledMapReplyQGC
{
    Q_OBJECT

public:
    QGeoMultiLayerMapReplyQGC(QNetworkAccessManager *networkManager, 
                               const QGeoTileSpec &spec,
                               const MapLayerStack &layerStack,
                               QObject *parent = nullptr);
    ~QGeoMultiLayerMapReplyQGC();

    void abort() final;

private slots:
    // 重写父类方法以处理多图层逻辑
    void _networkReplyFinished() override;
    void _networkReplyError(QNetworkReply::NetworkError error) override;
    void _networkReplySslErrors(const QList<QSslError> &errors) override;
    void _cacheReply(QGCCacheTile *tile) override;
    void _cacheError(QGCMapTask::TaskType type, QStringView errorString) override;

private:
    void _startFetching();
    void _startFetchingLayers();
    void _compositeTiles();
    void _handleSingleLayerReply(int mapId, const QByteArray &image, const QString &format);

    MapLayerStack _layerStack;
    QList<MapLayer> _visibleLayers;
    
    // 存储每个图层的网络回复
    QHash<int, QNetworkReply*> _replies;
    // 存储每个图层的瓦片数据
    QHash<int, TileImageData> _tiles;
    // 存储每个图层的缓存任务
    QHash<int, QGCFetchTileTask*> _cacheTasks;
    
    int _pendingReplies = 0;
    bool _compositing = false;

    enum HTTP_Response {
        SUCCESS_OK = 200,
        REDIRECTION_MULTIPLE_CHOICES = 300
    };
};

