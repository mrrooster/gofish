#ifndef GOOGLEDRIVE_H
#define GOOGLEDRIVE_H

#include <QObject>
#include "goauth2authorizationcodeflow.h"
#include "googledriveoperation.h"
#include <QTimer>
#include <QVector>
#include <QJsonValue>
#include <QJsonDocument>
class QNetworkReply;
class GoogleDriveObject;

#define GOOGLE_FOLDER "application/vnd.google-apps.folder"

class GoogleDrive : public QObject
{
    Q_OBJECT
public:
    enum ConnectionState { Disconnected,Connecting,Connected,ConnectionFailed };

    explicit GoogleDrive(bool readOnly,QObject *parent = nullptr);
    ~GoogleDrive();

    bool pathInFlight(QString path);
    QByteArray getPendingSegment(QString fileId, qint64 start, qint64 length);
    qint64 getInodeForFileId(QString id);
    void readRemoteFolder(QString path, QString parentId);
    void getFileContents(QString fileId, qint64 start, qint64 length);
    void uploadFile(QIODevice *file, QString path, QString fileId, QString parentId);
    void createFolder(QString path,QString fileId,QString parentId);
    void unlink(QString path,QString fileId);
    void rename(QString fileId,QString oldParentId,QString newParentId,QString newName,QString removeId);
    void updateMetadata(GoogleDriveObject *obj);

signals:
    void stateChanged(ConnectionState newState);
    void remoteFolder(QString path, QVector<GoogleDriveObject*> children);
    void pendingSegment(QString fileId, qint64 start, qint64 length);
    void uploadInProgress(QString path);
    void uploadComplete(QString path,QString fileId);
    void dirCreated(QString path,QString fileId);
    void unlinkComplete(QString path,bool sucess);
    void renameComplete(QString fileId, bool sucess);

private:
    qint64 inode;
    GOAuth2AuthorizationCodeFlow *auth;
    QTimer refreshTokenTimer;
    QTimer operationTimer;
    ConnectionState state;
    QMap<QString,QVector<QVariantMap>*> inflightValues;
    QMap<QString,QByteArray> pendingSegments;
    QMap<QString,qint64> inodeMap;
    QVector<QString> inflightPaths;
    QVector<GoogleDriveOperation*> queuedOps;
    QVector<QString> pregeneratedIds;
    bool readOnly;

    QMap<QNetworkReply*,GoogleDriveOperation*> inprogressOps;

    QString getRefreshToken();
    QString sanitizeFilename(QString path);
    void authenticate();
    void setRefreshToken(QString token);
    void setState(ConnectionState newState);
    void readFolder(QString startPath, QString path,QString nextPageToken,QString currentFolderId);
    void readFolder(QString startPath, QString nextPageToken, QString parentId);
    void readFileSection(QString fileId, qint64 start, qint64 length);

    void unlink(QString path,QString fileId,std::function<void(QNetworkReply *,bool)> handler);
    void updateFileMetadata(QString fileId,QJsonDocument doc,std::function<void(QNetworkReply *,bool)> handler, QString oldParent="",QString newParent="");

    void queueOp(QUrl url, QVariantMap headers, std::function<void(QNetworkReply *, bool)> handler);
    void queueOp(QUrl url, QVariantMap headers, QByteArray data, GoogleDriveOperation::HttpOperation op, std::function<void(QNetworkReply *, bool)> handler);
    void queueOp(QUrl url, QVariantMap headers, QByteArray data, GoogleDriveOperation::HttpOperation op, std::function<void(QNetworkReply *, bool)> handler, std::function<void(QNetworkReply *)> inProgressHandler);
    void uploadFileContents(QIODevice *file, QString path, QString fileId, QString url);
    int modeFromCapabilites(QJsonObject caps,bool folder);
private slots:
    void operationTimerFired();
    void requestFinished(QNetworkReply *response);
};

#endif // GOOGLEDRIVE_H
