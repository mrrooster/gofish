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
    quint64 addPathToPreFlightList(QString path);
    bool pathInPreflight(quint64 token);
    bool pathInFlight(QString path);
    QByteArray getPendingSegment(QString fileId, quint64 start, quint64 length);
    unsigned int getRefreshSeconds();
    quint64 getInMemoryCacheSizeBytes();
    quint64 getInodeForFileId(QString id);

signals:
    void stateChanged(ConnectionState newState);

public slots:
    void readRemoteFolder(QString path, QString parentId, GoogleDriveObject *target,quint64 token);
    void getFileContents(QString fileId, quint64 start, quint64 length,quint64 token);

private:
    quint64 inode;
    quint64 requestToken;
    GOAuth2AuthorizationCodeFlow *auth;
    QTimer refreshTokenTimer;
    QTimer operationTimer;
    ConnectionState state;
    QMap<QString,QVector<QVariantMap>*> inflightValues;
    QMap<QString,QByteArray> pendingSegments;
    QMap<QString,quint64> inodeMap;
    QVector<QString> inflightPaths;
    QVector<quint64> preflightPaths;
    QVector<GoogleDriveOperation*> queuedOps;
    QMutex preflightLock;
    QMutex oAuthLock;
    QMutex blockingLocksLock;

    QMap<QString,QMutex*> blockingLocks;
    QMap<QNetworkReply*,GoogleDriveOperation*> inprogressOps;

    QString getRefreshToken();
    void authenticate();
    void setRefreshToken(QString token);
    void setState(ConnectionState newState);
    void readFolder(QString startPath, QString path,QString nextPageToken,QString currentFolderId);
    void readFolder(QString startPath, QString nextPageToken, QString parentId, GoogleDriveObject *target);
    void readFileSection(QString fileId, quint64 start, quint64 length);

    void queueOp(QPair<QUrl,QVariantMap> urlAndHeaders,std::function<void(QByteArray)> handler);
private slots:
    void operationTimerFired();
    void requestFinished(QNetworkReply *response);
};

#endif // GOOGLEDRIVE_H
