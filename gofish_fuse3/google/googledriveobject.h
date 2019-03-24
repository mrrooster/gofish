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
        qint64 start,length,requestedLength;
    };
    explicit GoogleDriveObject(GoogleDrive *gofish, QCache<QString,QByteArray> *cache, QObject *parent = nullptr);
    explicit GoogleDriveObject(GoogleDrive *gofish, QString id, QString path, QString name, QString mimeType, quint64 size, QDateTime ctime, QDateTime mtime, QCache<QString,QByteArray> *cache,QObject *parent=nullptr);
    ~GoogleDriveObject();

    bool isFolder() const;
    QString getName() const;
    QString getPath() const;
    QString getMimeType() const;
    qint64 getSize() const;
    qint64 getChildFolderCount();
    qint64 getBlockSize();
    quint64 getInode() const;
    void setInode(quint64 inode);
    QDateTime getCreatedTime();
    QDateTime getModifiedTime();
    quint64 getChildren();
    void setChildren(QVector<GoogleDriveObject*> newChildren);
    qint64 read(qint64 start, qint64 totalLength);
    QCache<QString,QByteArray> *getCache() const;
    quint32 getRefreshSecs();
    void setCache(QCache<QString,QByteArray> *cache);

//    void operator =(const GoogleDriveObject &other);
signals:
    void readFolder(QString folder, QString parentId, GoogleDriveObject *into, quint64 requestToken);
    void readData(QString fileId, quint64 start, quint64 offset, quint64 requestToken);

    void children(QVector<GoogleDriveObject*> children,quint64 requestToken);
    void readResponse(QByteArray data,quint64 requestToken);

private:
    int usageCount;
    static quint64 requestToken;
    qint64 cacheChunkSize;
    qint64 readChunkSize;
    qint64 maxReadChunkSize;
    qint64 childFolderCount;
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
    QString temporaryFile; // If not empty, file is disc backed
    QIODevice::OpenMode openMode; // Only used for open for write
    qint64 pos;

    GoogleDrive *gofish;
    QCache<QString,QByteArray> *cache;
    QVector<GoogleDriveObject*> contents;
    QMap<quint64,ReadData> readMap;
    QTimer readTimer;
    QVector<quint64> tokenList;

    void clearChildren(QVector<GoogleDriveObject*> except=QVector<GoogleDriveObject*>());
    void setupReadTimer();
private slots:
    void processRemoteFolder(QString path, QVector<GoogleDriveObject*> children);
};

QDebug operator<<(QDebug debug, const GoogleDriveObject &o);

#endif // GOOGLEDRIVEOBJECT_H
