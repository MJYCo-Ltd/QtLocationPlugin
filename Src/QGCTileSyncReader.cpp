#include "QGCTileSyncReader.h"

#include <QtCore/QFile>
#include <QtCore/QMutex>
#include <QtCore/QThread>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

namespace QGCTileSyncReader {

namespace {

QMutex s_addDbMutex;

} // namespace

bool tryFetchTile(const QString &databasePath, const QString &hash,
                  QByteArray *imageOut, QString *formatOut)
{
    if (!imageOut || !formatOut || databasePath.isEmpty() || hash.isEmpty()) {
        return false;
    }
    if (!QFile::exists(databasePath)) {
        return false;
    }

    const QString connName =
        QStringLiteral("qgc_tile_ro_%1")
            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));

    {
        QMutexLocker lock(&s_addDbMutex);
        if (!QSqlDatabase::contains(connName)) {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            db.setDatabaseName(databasePath);
            db.setConnectOptions(
                QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=5000"));
            if (!db.open()) {
                QSqlDatabase::removeDatabase(connName);
                return false;
            }
        }
    }

    QSqlDatabase db = QSqlDatabase::database(connName);
    if (!db.isOpen()) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT tile, format FROM Tiles WHERE hash = ?"));
    query.addBindValue(hash);
    if (!query.exec() || !query.next()) {
        return false;
    }

    *imageOut = query.value(0).toByteArray();
    *formatOut = query.value(1).toString();
    return !imageOut->isEmpty() && !formatOut->isEmpty();
}

} // namespace QGCTileSyncReader
