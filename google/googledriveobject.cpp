#include <QDebug>
#include <QThread>
#include <QSettings>

#include "googledriveobject.h"
#include "defaults.h"

#define GOOGLE_FOLDER "application/vnd.google-apps.folder"

#define DEBUG_GOOGLE_OBJECT
#ifdef DEBUG_GOOGLE_OBJECT
#define D(x) qDebug() << QString("[GoogleDriveObject::%1]").arg(QString::number((quint64)QThread::currentThreadId(),16)).toUtf8().data() << x
#define SD(x) qDebug() << QString("[GoogleDriveObject::static]") << x
#else
#define D(x)
#define SD(x)
#endif

GoogleDriveObject::GoogleDriveObject(GoogleDrive *gofish, quint64 cacheSize, quint32 refreshSecs, QObject *parent) :
    GoogleDriveObject::GoogleDriveObject(gofish,refreshSecs,"","","",GOOGLE_FOLDER,0,QDateTime::currentDateTimeUtc(),QDateTime::currentDateTimeUtc(),new QCache<QString,QByteArray>((cacheSize/1024)?(cacheSize/1024):1),parent)
{
    lock();
    qInfo()<<"Using in memory cache of"<<(cacheSize/1024/1024)<<"MiB.";
    qInfo()<<"Refresh folder contents every"<<refreshSecs<<"seconds.";
}

GoogleDriveObject::GoogleDriveObject(GoogleDrive *gofish, quint64 refreshSecs, QString id, QString path, QString name, QString mimeType, quint64 size, QDateTime ctime, QDateTime mtime, QCache<QString,QByteArray> *cache, QObject *parent) :
    QObject(parent),
    populated(false)
{
    Q_ASSERT(READ_CHUNK_SIZE%CACHE_CHUNK_SIZE == 0);
    this->readChunkSize = READ_CHUNK_SIZE;
    this->cacheChunkSize= CACHE_CHUNK_SIZE;

    this->gofish   = gofish;
    this->cache    = cache;
    this->id       = id;
    this->path     = (path=="/")?"":path;
    this->name     = name;
    this->mimeType = mimeType;
    this->size     = size;
    this->ctime    = ctime;
    this->mtime    = mtime;
    this->inode    = gofish->getInodeForFileId(this->id.isEmpty()?getPath():this->id);
    this->childFolderCount = 0;
    this->usageCount = 0;
    this->refreshSecs= refreshSecs;

    QSettings settings;
    settings.beginGroup("googledrive");
    quint32 chunkSize = settings.value("download_chunk_bytes",READ_CHUNK_SIZE).toUInt();
    if (chunkSize>this->cacheChunkSize) {
        int mult = chunkSize/this->cacheChunkSize;
        this->readChunkSize = (mult?mult:1)*this->cacheChunkSize;
        D("Set read size:"<<this->readChunkSize);
    }

    setupConnections();

    D("New google object: "<<*this);
}

GoogleDriveObject::~GoogleDriveObject()
{
    clearChildren();
}

bool GoogleDriveObject::isFolder() const
{
    return (this->mimeType==GOOGLE_FOLDER);
}

QString GoogleDriveObject::getName() const
{
    return this->name;
}

QString GoogleDriveObject::getPath() const
{
    return QString("%1/%2").arg(this->path).arg(this->name);
}

QString GoogleDriveObject::getMimeType() const
{
    return this->mimeType;
}

quint64 GoogleDriveObject::getSize() const
{
    return this->size;
}

quint64 GoogleDriveObject::getChildFolderCount()
{
    lock();
    if (this->isFolder() && !this->populated) {
        getChildren();
    }
    release();
    return this->childFolderCount;
}

/**
 * @brief GoogleDriveObject::getBlockSize Returns the block size.
 * @return The current block size.
 *
 * You should request data in multiples of this value, from an offset that
 * is a multiple of this value, this allows the cacheing to work correctly.
 */
quint64 GoogleDriveObject::getBlockSize()
{
    return this->cacheChunkSize;
}

quint64 GoogleDriveObject::getInode() const
{
    return this->inode;
}

QDateTime GoogleDriveObject::getCreatedTime()
{
    return this->ctime;
}

QDateTime GoogleDriveObject::getModifiedTime()
{
    return this->mtime;
}

/**
 * @brief GoogleDriveObject::getChildren Gets the children of this folder object.
 * @return A list of locked children.
 *
 * YOU MUST RELEASE EACH CHILD.
 */
QVector<GoogleDriveObject *> GoogleDriveObject::getChildren()
{
    lock();
    qint64 currentSecs = QDateTime::currentSecsSinceEpoch();
    QString fullPath = getPath();
    D("In get children of: "<<fullPath);
    if (this->isFolder() && (!this->populated || (currentSecs-this->updated)>getRefreshSecs())) {
        this->gofish->addPathToPreFlightList(fullPath);
        emit readFolder(fullPath,this->id.isEmpty()?"root":this->id,this);
        QThread::yieldCurrentThread();
        while(this->gofish->pathInPreflight(fullPath)) {
            QThread::yieldCurrentThread();
        }
        D("Getting lock...");
        QMutex *mtx = this->gofish->getBlockingLock(fullPath);
        mtx->lock();
        D("Got lock."<<fullPath<<mtx);
        D("Got "<<this->contents.size()<<"children.");
        this->updated = currentSecs;
        mtx->unlock();
    }
    QVector<GoogleDriveObject*> oldContents = this->contents;
    for(auto it = oldContents.begin(),end = oldContents.end();it!=end;it++) {
        (*it)->lock();
    }
    release();
    return oldContents;
}

void GoogleDriveObject::setChildren(QVector<GoogleDriveObject *> newChildren)
{
    lock();
    this->childFolderCount = 0;
    clearChildren();
    for(auto it = newChildren.begin(),end = newChildren.end();it!=end;it++) {
        (*it)->lock();
        if ((*it)->isFolder()) {
            this->childFolderCount+=1;
        }
    }
    this->contents = newChildren;
    D("Set children, "<<this->childFolderCount<<"folders:"<<getPath());
    this->populated = true;
    release();
}

QByteArray GoogleDriveObject::read(quint64 start, quint64 totalLength)
{
    lock();
    QByteArray retData;
    //QString fullPath = getPath();


    quint64 length = totalLength<this->cacheChunkSize ? totalLength:this->cacheChunkSize;
    quint64 readStart = start;

    D("Read:"<<start<<totalLength<<getPath());

    while(totalLength>0) {
        QString chunkId = QString("%1:%2:%3:%4").arg(this->id).arg(start).arg(this->cacheChunkSize).arg(this->mtime.toMSecsSinceEpoch());

        D("READ chunk id:"<<chunkId);

        if (this->cache->contains(chunkId)) {
            D("Got cache hit for:"<<chunkId);
            retData.append(*this->cache->object(chunkId));
        } else if (!this->isFolder()) {
            QString readChunkId = QString("%1:%2:%3").arg(this->id).arg(readStart).arg(this->readChunkSize);
            this->gofish->addPathToPreFlightList(readChunkId);
            emit readData(this->id,readStart,this->readChunkSize);
            QThread::yieldCurrentThread();
            while(this->gofish->pathInPreflight(readChunkId)) {
                QThread::yieldCurrentThread();
            }
            D("Getting lock for file..."<<readChunkId);
            QMutexLocker lock(this->gofish->getBlockingLock(readChunkId));
            D("Got lock for file..."<<readChunkId);

            QByteArray data = this->gofish->getPendingSegment(this->id,readStart,this->readChunkSize);
            D("Got data: ,"<<data.size());

            if (!data.isEmpty()) {
                while(!data.isEmpty()) {
                    QByteArray *cacheEntry = new QByteArray(data.left(this->cacheChunkSize));
                    QString cacheChunkId = QString("%1:%2:%3:%4").arg(this->id).arg(readStart).arg(this->cacheChunkSize).arg(this->mtime.toMSecsSinceEpoch());
                    readStart+=this->cacheChunkSize;
                    data = data.mid(this->cacheChunkSize);
                    int cost = cacheEntry->size()/1024;
                    if (cost==0) {
                        cost=1;
                    }
                    this->cache->insert(cacheChunkId,cacheEntry,cost);
                    D("Inserted "<<cacheChunkId<<",of size: "<<cacheEntry->size()<<"into cache.");
                    D("Cache: "<<this->cache->totalCost()<<"of"<<this->cache->maxCost()<<", "<<(((quint64)this->cache->totalCost())*100/((quint64)this->cache->maxCost()))<<"%");
                }
                retData.append(*this->cache->object(chunkId));
            }
        }

        start+=length;
        totalLength -= length;
        length = totalLength<this->cacheChunkSize ? totalLength:this->cacheChunkSize;
    }

    D("Returning data size:"<<retData.size()<<getPath());
    release();
    return retData;
}

QCache<QString, QByteArray> *GoogleDriveObject::getCache() const
{
    return this->cache;
}

//void GoogleDriveObject::operator =(const GoogleDriveObject &other)
//{
//    this->cache = other.cache;
//    this->cacheChunkSize = other.cacheChunkSize;
//    this->readChunkSize = other.readChunkSize;
//    this->populated = other.populated;
//    this->contents = other.contents;
//    this->updated = other.updated;
//    this->gofish = other.gofish;
//    this->id = other.id;
//    this->size = other.size;
//    this->path = other.path;
//    this->mimeType = other.mimeType;
//    this->name = other.name;
//    this->mtime = other.mtime;
//    this->ctime = other.ctime;
//    setupConnections();
//}

void GoogleDriveObject::setupConnections()
{
//    connect(this->gofish,&GoogleDrive::folderContents,[=](QString path, QVector<QVariantMap> files){
//        QString myPath = this->getPath();

//        if (myPath==path) {
//            D("Got dir info::"<<path<<myPath);
//            this->contents.clear();
//            this->childFolderCount=0;
//            for(int idx=0;idx<files.size();idx++) {
//                QVariantMap file = files.at(idx);
////                D("File: "<<file);

//                GoogleDriveObject newObj(
//                            this->gofish,
//                            file["id"].toString(),
//                            path,
//                            file["name"].toString(),
//                            file["mimeType"].toString(),
//                            file["size"].toULongLong(),
//                            QDateTime::fromString(file["createdTime"].toString(),"yyyy-MM-dd'T'hh:mm:ss.z'Z'"),
//                            QDateTime::fromString(file["modifiedTime"].toString(),"yyyy-MM-dd'T'hh:mm:ss.z'Z'"),
//                            this->cache
//                           );

//                if (newObj.isFolder()) {
//                    this->childFolderCount++;
//                }

//                this->contents.append(newObj);
//            }
//            this->updated = QDateTime::currentSecsSinceEpoch();
//            this->populated=true;
//        }

//    });

    connect(this,&GoogleDriveObject::readFolder,this->gofish,&GoogleDrive::readRemoteFolder);
    connect(this,&GoogleDriveObject::readData,this->gofish,&GoogleDrive::getFileContents);
}

void GoogleDriveObject::lock()
{
    this->usageCount++;
    //    D("Locked item:"<<this->usageCount);
}

quint32 GoogleDriveObject::getRefreshSecs()
{
   return this->refreshSecs;
}

void GoogleDriveObject::release()
{
    Q_ASSERT(this->usageCount>0);
    this->usageCount--;
//    D("Release item: "<<this->usageCount);
    if (this->usageCount == 0) {
        this->deleteLater();
    }
}

void GoogleDriveObject::clearChildren()
{
    this->childFolderCount = 0;
    QVector<GoogleDriveObject*> oldContents = this->contents;
    this->contents.clear();
    for(auto it = oldContents.begin(),end = oldContents.end();it!=end;it++) {
        (*it)->release();
    }
}

QDebug operator<<(QDebug debug, const GoogleDriveObject &o)
{
    QDebugStateSaver s(debug);

    debug.nospace()
            << "[name: " << o.getName()
            << ", Mimetype: " << o.getMimeType()
            << ", Size: " << o.getSize()
            << ", is folder: " << (o.isFolder()?'Y':'N')
            << ", full path: " << o.getPath()
            << "]"
               ;
    return debug;
}
