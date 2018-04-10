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
    GoogleDriveObject();
    GoogleDriveObject(const GoogleDriveObject &other);
    explicit GoogleDriveObject(GoogleDrive *gofish, quint32 cacheSize = DEFAULT_CACHE_SIZE, QObject *parent = nullptr);
    explicit GoogleDriveObject(GoogleDrive *gofish, QString id, QString path, QString name, QString mimeType, quint64 size, QDateTime ctime, QDateTime mtime, QCache<QString,QByteArray> *cache, QObject *parent=nullptr);
    ~GoogleDriveObject();

    bool isFolder() const;
    bool isValid() const;
    QString getName() const;
    QString getPath() const;
    QString getMimeType() const;
    quint64 getSize() const;
    quint64 getChildFolderCount() const;
    quint64 getBlockSize();
    quint64 getInode() const;
    QDateTime getCreatedTime();
    QDateTime getModifiedTime();
    QVector<GoogleDriveObject> getChildren();
    QByteArray read(quint64 start, quint64 totalLength);

    void operator =(const GoogleDriveObject &other);
signals:
    void readFolder(QString folder, QString parentId);
    void readData(QString fileId, quint64 start, quint64 offset);
public slots:

private:
    int cacheChunkSize;
    int readChunkSize;
    quint64 childFolderCount;
    bool populated;
    QString mimeType;
    QString id;
    quint64 size;
    QString path; // w/o name
    QString name;
    QDateTime ctime;
    QDateTime mtime;
    qint64 updated;

    GoogleDrive *gofish;
    QCache<QString,QByteArray> *cache;
    QVector<GoogleDriveObject> contents;

    void setupConnections();
};

QDebug operator<<(QDebug debug, const GoogleDriveObject &o);

#endif // GOOGLEDRIVEOBJECT_H
