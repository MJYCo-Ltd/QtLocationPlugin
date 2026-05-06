#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace QGCTileSyncReader {

/// 在当前线程用只读连接查询 Tiles 表；命中则立即返回，避免经 worker 队列排队。
bool tryFetchTile(const QString &databasePath, const QString &hash,
                  QByteArray *imageOut, QString *formatOut);

} // namespace QGCTileSyncReader
