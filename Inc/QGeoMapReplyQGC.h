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

#include "QGCMapTasks.h"

Q_DECLARE_LOGGING_CATEGORY(QGeoTiledMapReplyQGCLog)

class QNetworkAccessManager;
class QSslError;

class QGeoTiledMapReplyQGC : public QGeoTiledMapReply
{
    Q_OBJECT

public:
    QGeoTiledMapReplyQGC(QNetworkAccessManager *networkManager, const QNetworkRequest &request, const QGeoTileSpec &spec, QObject *parent = nullptr);
    // 延迟初始化构造函数（用于子类）
    QGeoTiledMapReplyQGC(QNetworkAccessManager *networkManager, const QGeoTileSpec &spec, QObject *parent = nullptr);
    ~QGeoTiledMapReplyQGC();

    void abort();
    
    // 允许子类调用初始化
    void initializeFromCache();

protected:
    // 允许子类访问
    QNetworkAccessManager *_networkManager = nullptr;
    QNetworkRequest _request;

    // 辅助方法：创建网络请求（供子类复用）
    // connectSignals: 是否连接到父类的槽函数（子类通常设为 false，自己连接）
    QNetworkReply* createNetworkRequest(const QNetworkRequest &request, bool connectSignals = true);
    // 辅助方法：处理单个网络回复的验证和解析（供子类复用）
    bool processNetworkReply(QNetworkReply *reply, int mapId, QByteArray &image, QString &format);

protected slots:
    // 允许子类重写
    virtual void _networkReplyFinished();
    virtual void _networkReplyError(QNetworkReply::NetworkError error);
    virtual void _networkReplySslErrors(const QList<QSslError> &errors);
    virtual void _cacheReply(QGCCacheTile *tile);
    virtual void _cacheError(QGCMapTask::TaskType type, QStringView errorString);

private:
    static void _initDataFromResources();

    static QByteArray _bingNoTileImage;
    static QByteArray _badTile;

    enum HTTP_Response {
        SUCCESS_OK = 200,
        REDIRECTION_MULTIPLE_CHOICES = 300
    };
};
