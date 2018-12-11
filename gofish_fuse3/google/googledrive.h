#ifndef GOOGLEDRIVE_H
#define GOOGLEDRIVE_H

#include <QObject>
#include "goauth2authorizationcodeflow.h"
#include "googledriveoperation.h"
#include <QTimer>
#include <QVector>
#include <QJsonValue>
#include <QJsonDocument>
class GoogleDriveObject;

class GoogleDrive : public QObject
{
    Q_OBJECT
public:
    enum ConnectionState { Disconnected,Connecting,Connected,ConnectionFailed };

    explicit GoogleDrive(QObject *parent = nullptr);
    ~GoogleDrive();

    bool pathInFlight(QString path);
    QByteArray getPendingSegment(QString fileId, quint64 start, quint64 length);
    quint64 getInodeForFileId(QString id);
    quint64 readRemoteFolder(QString path, QString parentId);
    quint64 getFileContents(QString fileId, quint64 start, quint64 length);
    QTimer *getEmitTimer();

signals:
    void stateChanged(ConnectionState newState);
    void remoteFolder(QString path, QVector<GoogleDriveObject*> children);
    void pendingSegment(QString fileId, quint64 start, quint64 length);

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
    QVector<GoogleDriveOperation*> queuedOps;
    QTimer emitTimer;

    QMap<QNetworkReply*,GoogleDriveOperation*> inprogressOps;

    QString getRefreshToken();
    QString sanitizeFilename(QString path);
    void authenticate();
    void setRefreshToken(QString token);
    void setState(ConnectionState newState);
    void readFolder(QString startPath, QString path,QString nextPageToken,QString currentFolderId);
    void readFolder(QString startPath, QString nextPageToken, QString parentId);
    void readFileSection(QString fileId, quint64 start, quint64 length);

    void queueOp(QPair<QUrl,QVariantMap> urlAndHeaders,std::function<void(QByteArray,bool)> handler);
private slots:
    void operationTimerFired();
    void requestFinished(QNetworkReply *response);
};

#endif // GOOGLEDRIVE_H
