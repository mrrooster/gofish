#ifndef GOOGLEDRIVEOBJECT_H
#define GOOGLEDRIVEOBJECT_H

#include <unistd.h>
#include <QObject>
#include <QDateTime>
#include <QCache>
#include <QFile>
#include "googledrive.h"
#include "defaults.h"
#include "cacheentry.h"

class   GoogleDriveObject : public QObject
{
    Q_OBJECT
public:
    struct ReadData {
        QByteArray data;
        qint64 start,length,requestedStart,requestedLength;
    };
    explicit GoogleDriveObject(GoogleDriveObject *parentObject,GoogleDrive *gofish, QCache<QString, CacheEntry> *cache, QObject *parent = nullptr);
    explicit GoogleDriveObject(GoogleDriveObject *parentObject,GoogleDrive *gofish, QString id, QString path, QString name, QString mimeType, qint64 size, QDateTime ctime, QDateTime mtime, QCache<QString, CacheEntry> *cache, QObject *parent=nullptr);
    ~GoogleDriveObject();

    bool isFolder() const;
    QString getName() const;
    QString getPath() const;
    QString getMimeType() const;
    QString getFileId() const;
    qint64 getSize() const;
    qint64 getChildFolderCount();
    qint64 getBlockSize();
    qint64 getInode() const;
    void setInode(qint64 inode);
    QDateTime getCreatedTime();
    QDateTime getModifiedTime();
    void setModifiedTime(time_t time);
    void setModifiedTime(QDateTime time);
    qint64 getChildren();
    void setChildren(QVector<GoogleDriveObject*> newChildren);
    GoogleDriveObject *create(QString name);
    qint64 createDir(QString name);
    qint64 read(qint64 start, qint64 totalLength);
    qint64 write(QByteArray data,qint64 start);
    qint64 unlinkChild(QString name);
    qint64 takeItem(GoogleDriveObject *oldParent,QString name, QString newName); // takes a child object from another folder, used in rename
    QCache<QString,CacheEntry> *getCache() const;
    quint32 getRefreshSecs();
    void setCache(QCache<QString,CacheEntry> *cache);
    void open(bool write);
    qint64 close();
    GoogleDriveObject *getParentObject();
    int getFileMode();
    void setFileMode(int mode);
    uid_t getUid();
    void setUid(uid_t uid);
    gid_t getGid();
    void setGid(gid_t gid);

//    void operator =(const GoogleDriveObject &other);
signals:
    void readFolder(QString folder, QString parentId, GoogleDriveObject *into, qint64 requestToken);
    void readData(QString fileId, qint64 start, qint64 offset, qint64 requestToken);

    void children(QVector<GoogleDriveObject*> children,qint64 requestToken);
    void readResponse(QByteArray data,qint64 requestToken);
    void writeResponse(qint64 requestToken);
    void closeInProgress(qint64 requestToken);
    void closeResponse(qint64 requestToken);
    void createDirResponse(qint64 requestToken,GoogleDriveObject *dir);
    void unlinkResponse(qint64 requestToken,bool found);
    void renameResponse(qint64 requestToken,bool ok);

private:
    static qint64 requestToken;
    static qint64 instanceCount;
    int usageCount;
    qint64 instanceId;
    qint64 closeToken;
    qint64 cacheChunkSize;
    qint64 readChunkSize;
    qint64 maxReadChunkSize;
    qint64 childFolderCount;
    bool populated;
    bool shouldDelete;
    QString mimeType;
    QString id;
    qint64 size;
    QString path; // w/o name
    QString name;
    QDateTime ctime;
    QDateTime mtime;
    qint64 updated;
    qint64 inode;
    qint64 unlinkToken;
    int fileMode;
    uid_t uid;
    gid_t gid;
    QFile *temporaryFile; // If not empty, file is disc backed
    QIODevice::OpenMode openMode; // Only used for open for write

    GoogleDriveObject *parentObject;
    GoogleDrive *gofish;
    QCache<QString,CacheEntry> *cache;
    QVector<GoogleDriveObject*> contents;
    QMap<qint64,ReadData> readMap;
    QTimer readTimer;
    QVector<qint64> tokenList;
    QMap<QPair<qint64,qint64>,QPair<qint64,ReadData>> pendingSegments;
    QMap<QString,qint64> renameMap;
    QTimer metadataTimer;

    void clearChildren(QVector<GoogleDriveObject*> except=QVector<GoogleDriveObject*>());
    void setupReadTimer();
    void saveRemoteDateToTempFile(ReadData &readData);
    void writeTempFileToGoogle(qint64 token);
    void unlink(qint64 token);
    void decreaseUsageCount();
private slots:
    void processRemoteFolder(QString path, QVector<GoogleDriveObject*> children);
    void processPendingSegment(QString fileId, qint64 start, qint64 length);
    void processUploadInProgress(QString path);
    void processUploadComplete(QString path,QString fileId);
    void processDirCreated(QString path,QString fileId);
};

QDebug operator<<(QDebug debug, const GoogleDriveObject &o);
QDebug operator<<(QDebug debug, const GoogleDriveObject::ReadData &o);

#endif // GOOGLEDRIVEOBJECT_H
