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
    explicit GoogleDriveObject(GoogleDrive *gofish, quint32 cacheSize, quint32 refreshSecs, QObject *parent = nullptr);
    explicit GoogleDriveObject(GoogleDrive *gofish, quint64 refreshSecs, QString id, QString path, QString name, QString mimeType, quint64 size, QDateTime ctime, QDateTime mtime, QCache<QString,QByteArray> *cache, QObject *parent=nullptr);
    ~GoogleDriveObject();

    bool isFolder() const;
    QString getName() const;
    QString getPath() const;
    QString getMimeType() const;
    quint64 getSize() const;
    quint64 getChildFolderCount() const;
    quint64 getBlockSize();
    quint64 getInode() const;
    QDateTime getCreatedTime();
    QDateTime getModifiedTime();
    QVector<GoogleDriveObject*> getChildren();
    void setChildren(QVector<GoogleDriveObject*> newChildren);
    QByteArray read(quint64 start, quint64 totalLength);
    QCache<QString,QByteArray> *getCache() const;
    void release();
    void lock();
    quint32 getRefreshSecs();

//    void operator =(const GoogleDriveObject &other);
signals:
    void readFolder(QString folder, QString parentId, GoogleDriveObject *into);
    void readData(QString fileId, quint64 start, quint64 offset);

private:
    int usageCount;
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
    quint32 refreshSecs;

    GoogleDrive *gofish;
    QCache<QString,QByteArray> *cache;
    QVector<GoogleDriveObject*> contents;

    void setupConnections();
    void clearChildren();
};

QDebug operator<<(QDebug debug, const GoogleDriveObject &o);

#endif // GOOGLEDRIVEOBJECT_H
