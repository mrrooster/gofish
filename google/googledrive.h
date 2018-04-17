#ifndef GOOGLEDRIVE_H
#define GOOGLEDRIVE_H

#include <QObject>
#include "goauth2authorizationcodeflow.h"
#include "googledriveoperation.h"
#include <QTimer>
#include <QVector>
#include <QJsonValue>
#include <QJsonDocument>
#include <QMutex>
class GoogleDriveObject;

class GoogleDrive : public QObject
{
    Q_OBJECT
public:
    enum ConnectionState { Disconnected,Connected };

    explicit GoogleDrive(QObject *parent = nullptr);
    ~GoogleDrive();

    QMutex *getBlockingLock(QString folder);
    QMutex *getFileLock(QString fileId);
    void addPathToPreFlightList(QString path);
    bool pathInPreflight(QString path);
    bool pathInFlight(QString path);
    QByteArray getPendingSegment(QString fileId, quint64 start, quint64 length);
    unsigned int getRefreshSeconds();
    quint64 getInMemoryCacheSizeBytes();
    quint64 getInodeForFileId(QString id);

signals:
    void stateChanged(ConnectionState newState);

public slots:
    void readRemoteFolder(QString path, QString parentId, GoogleDriveObject *target);
    void getFileContents(QString fileId, quint64 start, quint64 length);

private:
    quint64 inode;
    GOAuth2AuthorizationCodeFlow *auth;
    QTimer refreshTokenTimer;
    QTimer operationTimer;
    ConnectionState state;
    QMap<QString,QVector<QVariantMap>*> inflightValues;
    QMap<QString,QByteArray> pendingSegments;
    QMap<QString,quint64> inodeMap;
    QVector<QString> inflightPaths;
    QVector<QString> preflightPaths;
    QVector<GoogleDriveOperation*> queuedOps;
    QMutex preflightLock;
    QMutex oAuthLock;

    QMap<QString,QMutex*> blockingLocks;
    QMap<QString,QMutex*> fileLocks;

    QString getRefreshToken();
    void authenticate();
    void setRefreshToken(QString token);
    void setState(ConnectionState newState);
    void readFolder(QString startPath, QString path,QString nextPageToken,QString currentFolderId);
    void readFolder(QString startPath, QString nextPageToken, QString parentId, GoogleDriveObject *target);
    void readFileSection(QString fileId, quint64 start, quint64 length);

    void queueOp(QPair<QUrl,QVariantMap> urlAndHeaders,std::function<void(QNetworkReply*)> handler);
};

#endif // GOOGLEDRIVE_H
