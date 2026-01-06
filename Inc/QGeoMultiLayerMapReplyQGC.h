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
#include <QtLocation/private/qgeotiledmapreply_p.h>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtCore/QHash>
#include "QGCMapLayerConfig.h"
#include "QGCTileCompositor.h"
#include "QGCMapTasks.h"
#include "QGCCacheTile.h"

Q_DECLARE_LOGGING_CATEGORY(QGeoMultiLayerMapReplyQGCLog)

class QNetworkAccessManager;
class QSslError;

/**
 * @brief 多图层瓦片回复类
 * 负责并行获取多个图层的瓦片，并在全部完成后进行合成
 */
class QGeoMultiLayerMapReplyQGC : public QGeoTiledMapReply
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
    void _networkReplyFinished();
    void _networkReplyError(QNetworkReply::NetworkError error);
    void _networkReplySslErrors(const QList<QSslError> &errors);
    void _cacheReply(QGCCacheTile *tile);
    void _cacheError(QGCMapTask::TaskType type, QStringView errorString);

private:
    void _startFetching();
    void _startFetchingLayers();
    void _compositeTiles();
    void _initDataFromResources();
    QNetworkRequest _createRequest(int mapId, int x, int y, int zoom);

    QNetworkAccessManager *_networkManager = nullptr;
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

    static QByteArray _bingNoTileImage;
    static QByteArray _badTile;

    enum HTTP_Response {
        SUCCESS_OK = 200,
        REDIRECTION_MULTIPLE_CHOICES = 300
    };
};

