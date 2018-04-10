#ifndef GOOGLEDRIVE_H
#define GOOGLEDRIVE_H

#include <QObject>
#include "goauth2authorizationcodeflow.h"
#include <QTimer>
#include <QVector>
#include <QJsonValue>
#include <QJsonDocument>
#include <QMutex>

class GoogleDrive : public QObject
{
    Q_OBJECT
public:
    enum ConnectionState { Disconnected,Connected };

    explicit GoogleDrive(const QString clientId, const QString clientSecret, QObject *parent = nullptr);
    ~GoogleDrive();

    QMutex *getBlockingLock(QString folder);
    QMutex *getFileLock(QString fileId);
    void addPathToPreFlightList(QString path);
    bool pathInPreflight(QString path);
    bool pathInFlight(QString path);
    QByteArray getPendingSegment(QString fileId, quint64 start, quint64 length);

signals:
    void stateChanged(ConnectionState newState);
    void folderContents(QString path,QVector<QMap<QString,QString>> fileList);
    //void fileInfo(QString fileId,QJsonDocument fileInfo);

public slots:
    void readRemoteFolder(QString path,QString parentId);
    void getFileContents(QString fileId, quint64 start, quint64 length);

private:
    GOAuth2AuthorizationCodeFlow *auth;
    QTimer refreshTokenTimer;
    QTimer operationTimer;
    ConnectionState state;
    QMap<QString,QVector<QMap<QString,QString>>*> inflightValues;
    QMap<QString,QByteArray> pendingSegments;
    QVector<QString> inflightPaths;
    QVector<QString> preflightPaths;
    QVector<QPair<QPair<QUrl,QVariantMap>,std::function<void(QNetworkReply*)>>> queuedOps;
    QMutex preflightLock;

    QMap<QString,QMutex*> blockingLocks;
    QMap<QString,QMutex*> fileLocks;
    QString getRefreshToken();
    void setRefreshToken(QString token);
    void setState(ConnectionState newState);
    void readFolder(QString startPath, QString path,QString nextPageToken,QString currentFolderId);
    void readFolder(QString startPath, QString nextPageToken, QString parentId);
    void readFileSection(QString fileId, quint64 start, quint64 length);

    void queueOp(QPair<QUrl,QVariantMap> urlAndHeaders,std::function<void(QNetworkReply*)> handler);

};

#endif // GOOGLEDRIVE_H
