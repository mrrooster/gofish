#include <QDebug>
#include <QSettings>

#include "googledriveobject.h"
#include "defaults.h"

#define GOOGLE_FOLDER "application/vnd.google-apps.folder"

#define DEBUG_GOOGLE_OBJECT
#ifdef DEBUG_GOOGLE_OBJECT
#define D(x) qDebug() << QString("[GoogleDriveObject::]").toUtf8().data() << x
#define SD(x) qDebug() << QString("[GoogleDriveObject::static]") << x
#else
#define D(x)
#define SD(x)
#endif
quint64 GoogleDriveObject::requestToken  = 0l;

GoogleDriveObject::GoogleDriveObject(GoogleDrive *gofish, QCache<QString, QByteArray> *cache, QTimer *emitTimer, QObject *parent) :
    GoogleDriveObject::GoogleDriveObject(gofish,"","","",GOOGLE_FOLDER,0,QDateTime::currentDateTimeUtc(),QDateTime::currentDateTimeUtc(),cache,emitTimer,parent)
{
}

GoogleDriveObject::GoogleDriveObject(GoogleDrive *gofish, QString id, QString path, QString name, QString mimeType, quint64 size, QDateTime ctime, QDateTime mtime, QCache<QString,QByteArray> *cache, QTimer *emitTimer, QObject *parent) :
    QObject(parent),
    populated(false)
{
    Q_ASSERT(READ_CHUNK_SIZE%CACHE_CHUNK_SIZE == 0);
    this->maxReadChunkSize = READ_CHUNK_SIZE;
    this->cacheChunkSize   = this->readChunkSize = CACHE_CHUNK_SIZE;

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
    this->emitTimer = emitTimer;

    QSettings settings;
    settings.beginGroup("googledrive");
    quint32 chunkSize = settings.value("download_chunk_bytes",READ_CHUNK_SIZE).toUInt();
    if (chunkSize>this->cacheChunkSize) {
        int mult = chunkSize/this->cacheChunkSize;
        this->maxReadChunkSize = (mult?mult:1)*this->cacheChunkSize;
        D("Set read size:"<<this->maxReadChunkSize);
    }

    setupConnections();
    setupReadTimer();

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

qint64 GoogleDriveObject::getSize() const
{
    return this->size;
}

qint64 GoogleDriveObject::getChildFolderCount()
{
    //if (this->isFolder() && !this->populated) {
    //    getChildren();
    //}
    return this->childFolderCount;
}

/**
 * @brief GoogleDriveObject::getBlockSize Returns the block size.
 * @return The current block size.
 *
 * You should request data in multiples of this value, from an offset that
 * is a multiple of this value, this allows the cacheing to work correctly.
 */
qint64 GoogleDriveObject::getBlockSize()
{
    return this->cacheChunkSize;
}

quint64 GoogleDriveObject::getInode() const
{
    return this->inode;
}

void GoogleDriveObject::setInode(quint64 inode)
{
    this->inode = inode;
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
 * @return A list of children.
 *
 */
quint64 GoogleDriveObject::getChildren()
{
    quint64 token = this->requestToken++;
    qint64 currentSecs = QDateTime::currentSecsSinceEpoch();
    QString fullPath = getPath();
    D("In get children of: "<<fullPath<<", token:"<<token);
    if (this->isFolder()) {
        if (this->populated) {
            D("Returning populated list..");
            this->contentsToSend.append(QPair<quint64,QVector<GoogleDriveObject*>>(token,this->contents));
        } else {
            D("Fetching remotely..."<<fullPath<<this->id);
            this->gofish->readRemoteFolder(fullPath,this->id);

            connect(this->gofish,&GoogleDrive::remoteFolder,[=](QString path,QVector<GoogleDriveObject*> children){
                D("Got children of: "<<fullPath)<<path;
                if (path==fullPath) {
                    for(GoogleDriveObject *child:children) {
                        child->setCache(this->getCache());
                    }
                    this->setChildren(children);
                    //emit this->children(children,token);
                    this->contentsToSend.append(QPair<quint64,QVector<GoogleDriveObject*>>(token,children));
                }
            });


        }
    }
    return token;
}

void GoogleDriveObject::setChildren(QVector<GoogleDriveObject *> newChildren)
{
    this->childFolderCount = 0;
    D("In setchildren");
    clearChildren(newChildren);
    for(auto it = newChildren.begin(),end = newChildren.end();it!=end;it++) {
        if ((*it)->isFolder()) {
            this->childFolderCount+=1;
        }
    }
    this->contents = newChildren;
    D("Set children, "<<this->childFolderCount<<"folders:"<<getPath()<<this->contents.size());
    this->populated = true;
}

qint64 GoogleDriveObject::read(qint64 start, qint64 totalLength)
{
    quint64 token = this->requestToken++;
    ReadData readData;
    //QString fullPath = getPath();

    readData.requestedLength = totalLength;
    readData.length = totalLength>this->cacheChunkSize ? totalLength:this->cacheChunkSize;
    readData.start = start;

    this->readMap.insert(token,readData);
    if (!this->readTimer.isActive()) {
        this->readTimer.setInterval(0);
        this->readTimer.setSingleShot(true);
        this->readTimer.start();
    }
    return token;

}

QCache<QString, QByteArray> *GoogleDriveObject::getCache() const
{
    return this->cache;
}

void GoogleDriveObject::setCache(QCache<QString, QByteArray> *cache)
{
    this->cache = cache;
}

void GoogleDriveObject::setupConnections()
{
    connect(this->emitTimer,&QTimer::timeout,this,&GoogleDriveObject::childEmitTImer);
//    connect(this,&GoogleDriveObject::readFolder,this->gofish,&GoogleDrive::readRemoteFolder);
//    connect(this,&GoogleDriveObject::readData,this->gofish,&GoogleDrive::getFileContents);
}

void GoogleDriveObject::clearChildren(QVector<GoogleDriveObject*> except)
{
    this->childFolderCount = 0;
    QVector<GoogleDriveObject*> oldContents = this->contents;
    this->contents.clear();
    for(auto it = oldContents.begin(),end = oldContents.end();it!=end;it++) {
        if (!except.contains((*it))) {
            (*it)->deleteLater();
        }
    }
}

void GoogleDriveObject::setupReadTimer()
{
    connect(&this->readTimer,&QTimer::timeout,[=](){
        if (!this->readMap.isEmpty()) {
            quint64 key = this->readMap.firstKey();
            ReadData data = this->readMap.take(key);
            D("Read:"<<data.start<<data.length<<getPath()<<"Filesize:"<<getSize());

            while(data.length>0) {
                qint64 msecs = this->mtime.toMSecsSinceEpoch();
                QString chunkId = QString("%1:%2:%3:%4").arg(this->id).arg(data.start).arg(this->cacheChunkSize).arg(msecs);

                D("READ chunk id:"<<chunkId);
                bool done=false;
                if (this->cache->contains(chunkId)) {
                    D("Got cache hit for:"<<chunkId);
		    QByteArray *cacheEntry = this->cache->object(chunkId);
                    data.data.append(*cacheEntry);
                    data.length-=this->cacheChunkSize;
                    data.start+=this->cacheChunkSize;
                    if (data.length<=0) {
                        if (data.data.length()>data.requestedLength) {
                            data.data.resize(data.requestedLength);
                        }
                        emit readResponse(data.data,key);
                    }
		    D("Data len now:"<<data.length);
                    done=true;
                }

                if (!done && !this->isFolder()) {
                    qint64 currentReadChunkSize = this->readChunkSize;
                    this->gofish->getFileContents(this->id,data.start,currentReadChunkSize);
                    connect(this->gofish,&GoogleDrive::pendingSegment,[=](QString fileId, quint64 start, quint64 length){
                        if (fileId==this->id && start==data.start && length==currentReadChunkSize) {
                            QByteArray receivedData = this->gofish->getPendingSegment(this->id,data.start,currentReadChunkSize);
                            ReadData myData = data;
                            D("Got data: ,"<<receivedData.size());
			    Q_ASSERT(myData.start==start);

                            if (!receivedData.isEmpty()) {
                                while(!receivedData.isEmpty()) {
                                    QByteArray *cacheEntry = new QByteArray(receivedData.left(this->cacheChunkSize));
                                    QString cacheChunkId = QString("%1:%2:%3:%4").arg(this->id).arg(start).arg(this->cacheChunkSize).arg(msecs);
                                    start+=this->cacheChunkSize;
                                    receivedData = receivedData.mid(this->cacheChunkSize);
                                    int cost = cacheEntry->size()/1024;
                                    if (cost==0) {
                                        cost=1;
                                    }
                                    myData.data.append(*cacheEntry);
                                    this->cache->insert(cacheChunkId,cacheEntry,cost);
                                    D("Inserted "<<cacheChunkId<<",of size: "<<cacheEntry->size()<<"into cache.");
                                    D("Cache: "<<this->cache->totalCost()<<"of"<<this->cache->maxCost()<<", "<<(((quint64)this->cache->totalCost())*100/((quint64)this->cache->maxCost()))<<"%");
                                }
                            }
                            myData.start+=length;
                            myData.length -= length;
                            if (myData.length>0) {
                                this->readMap.insert(key,myData);
                                if (!this->readTimer.isActive()) {
                                    this->readTimer.setSingleShot(true);
                                    this->readTimer.setInterval(0);
                                    this->readTimer.start();
                                }
                            } else {
                                if (myData.data.length()>myData.requestedLength) {
				    D("Truncating data from"<<myData.data.length()<<"to"<<myData.requestedLength);
                                    myData.data.resize(myData.requestedLength);
                                }
                                emit readResponse(myData.data,key);
                            }
                        }
                    });
                    if (this->readChunkSize<this->maxReadChunkSize) {
                        int multiplier=1;
                        for(;(this->cacheChunkSize*multiplier)<(this->maxReadChunkSize-this->readChunkSize)&&multiplier<7;multiplier++) ;
                        this->readChunkSize += (this->cacheChunkSize*multiplier);
                        //D("read chunk size now:"<<::byteCountString(this->readChunkSize));
                    }
                    data.length=0;
                }
            }
            this->readTimer.start();
        }
    });
}

void GoogleDriveObject::childEmitTImer()
{
    while(!this->contentsToSend.isEmpty()) {
        QPair<quint64,QVector<GoogleDriveObject*>> signal = this->contentsToSend.takeFirst();
        emit this->children(signal.second,signal.first);
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
