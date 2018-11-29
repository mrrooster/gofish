#ifndef GOOGLEDRIVEOBJECT_H
#define GOOGLEDRIVEOBJECT_H

#include <QObject>
#include <QDateTime>
#include <QCache>
#include "googledrive.h"
#include "defaults.h"

class GoogleDriveObject : public QObject
{
    Q_OBJECT
public:
    struct ReadData {
        QByteArray data;
        quint64 start,length;
    };
    explicit GoogleDriveObject(GoogleDrive *gofish, QCache<QString,QByteArray> *cache, QObject *parent = nullptr);
    explicit GoogleDriveObject(GoogleDrive *gofish, QString id, QString path, QString name, QString mimeType, quint64 size, QDateTime ctime, QDateTime mtime, QCache<QString,QByteArray> *cache, QObject *parent=nullptr);
    ~GoogleDriveObject();

    bool isFolder() const;
    QString getName() const;
    QString getPath() const;
    QString getMimeType() const;
    quint64 getSize() const;
    quint64 getChildFolderCount();
    quint64 getBlockSize();
    quint64 getInode() const;
    void setInode(quint64 inode);
    QDateTime getCreatedTime();
    QDateTime getModifiedTime();
    quint64 getChildren();
    void setChildren(QVector<GoogleDriveObject*> newChildren);
    quint64 read(quint64 start, quint64 totalLength);
    QCache<QString,QByteArray> *getCache() const;
    quint32 getRefreshSecs();
    void setCache(QCache<QString,QByteArray> *cache);

//    void operator =(const GoogleDriveObject &other);
signals:
    void readFolder(QString folder, QString parentId, GoogleDriveObject *into, quint64 requestToken);
    void readData(QString fileId, quint64 start, quint64 offset, quint64 requestToken);

    void children(QVector<GoogleDriveObject*> children,quint64 requestToken);
    void read(QByteArray data,quint64 requestToken);

private:
    int usageCount;
    quint64 requestToken;
    quint64 cacheChunkSize;
    quint64 readChunkSize;
    quint64 childFolderCount;
    bool populated;
    bool shouldDelete;
    QString mimeType;
    QString id;
    quint64 size;
    QString path; // w/o name
    QString name;
    QDateTime ctime;
    QDateTime mtime;
    qint64 updated;
    quint64 inode;

    GoogleDrive *gofish;
    QCache<QString,QByteArray> *cache;
    QVector<GoogleDriveObject*> contents;
    QMap<quint64,ReadData> readMap;
    QTimer readTimer;

    void setupConnections();
    void clearChildren();
    void setupReadTimer();
};

QDebug operator<<(QDebug debug, const GoogleDriveObject &o);

#endif // GOOGLEDRIVEOBJECT_H
