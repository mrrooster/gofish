#include <QDebug>
#include <QSettings>
#include <QDir>
#include <QTemporaryFile>

#include "googledriveobject.h"
#include "defaults.h"

#define DEBUG_GOOGLE_OBJECT
#ifdef DEBUG_GOOGLE_OBJECT
#define D(x) qDebug() << QString("[GoogleDriveObject::%1]").arg(this->instanceId).toUtf8().data() << x
#define SD(x) qDebug() << QString("[GoogleDriveObject::static]") << x
#else
#define D(x)
#define SD(x)
#endif
qint64 GoogleDriveObject::requestToken  = 0l;
qint64 GoogleDriveObject::instanceCount  = 0l;


GoogleDriveObject::GoogleDriveObject(GoogleDriveObject *parentObject,GoogleDrive *gofish, QCache<QString, CacheEntry> *cache,  QObject *parent) :
    GoogleDriveObject::GoogleDriveObject(parentObject,gofish,"root","","",GOOGLE_FOLDER,0,QDateTime::currentDateTimeUtc(),QDateTime::currentDateTimeUtc(),cache,parent)
{
}

GoogleDriveObject::GoogleDriveObject(GoogleDriveObject *parentObject,GoogleDrive *gofish, QString id, QString path, QString name, QString mimeType, qint64 size, QDateTime ctime, QDateTime mtime, QCache<QString,CacheEntry> *cache, QObject *parent) :
    QObject(parent),
    populated(false),
    temporaryFile(nullptr),
    openMode(QIODevice::NotOpen)
{
    this->instanceId = GoogleDriveObject::instanceCount++;
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
    this->inode    = gofish->getInodeForFileId(this->id.isEmpty()? (getPath()+"/"+getName()):this->id);
    this->childFolderCount = 0;
    this->usageCount = 0;
    this->parentObject = parentObject;
    this->fileMode = 0555;
    this->uid      = ::getuid();
    this->gid      = ::getgid();

    QSettings settings;
    settings.beginGroup("googledrive");
    quint32 chunkSize = settings.value("download_chunk_bytes",READ_CHUNK_SIZE).toUInt();
    if (chunkSize>this->cacheChunkSize) {
        int mult = chunkSize/this->cacheChunkSize;
        mult += (chunkSize%this->cacheChunkSize)==0?0:1;
        this->maxReadChunkSize = (mult?mult:1)*this->cacheChunkSize;
        D("Set read size:"<<this->maxReadChunkSize);
    }

    setupReadTimer();

    connect(this->gofish,&GoogleDrive::remoteFolder,this,&GoogleDriveObject::processRemoteFolder);
    connect(this->gofish,&GoogleDrive::pendingSegment,this,&GoogleDriveObject::processPendingSegment);
    connect(this->gofish,&GoogleDrive::uploadInProgress,this,&GoogleDriveObject::processUploadInProgress);
    connect(this->gofish,&GoogleDrive::uploadComplete,this,&GoogleDriveObject::processUploadComplete);
    connect(this->gofish,&GoogleDrive::dirCreated,this,&GoogleDriveObject::processDirCreated);
    connect(this->gofish,&GoogleDrive::renameComplete,this,[=](QString id,bool ok){
        if (this->renameMap.contains(id)) {
            emit this->renameResponse(this->renameMap.take(id),ok);
        }
    });
    connect(&this->metadataTimer,&QTimer::timeout,this,[=](){
        this->gofish->updateMetadata(this);
    });
    connect(this->gofish,&GoogleDrive::unlinkComplete,[=](QString path,bool){
        if (path==this->getPath()) {
            this->decreaseUsageCount();
            emit unlinkResponse(this->unlinkToken,true);
        }
    });

    this->metadataTimer.setSingleShot(true);
    this->metadataTimer.setInterval(3000);
    D("New google object: "<<*this);
}

GoogleDriveObject::~GoogleDriveObject()
{
    if (this->temporaryFile!=nullptr) {
        delete this->temporaryFile;
    }
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

QString GoogleDriveObject::getFileId() const
{
    return this->id;
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

qint64 GoogleDriveObject::getInode() const
{
    return this->inode;
}

void GoogleDriveObject::setInode(qint64 inode)
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

void GoogleDriveObject::setModifiedTime(time_t time)
{
    setModifiedTime(QDateTime::fromTime_t(time));
}

void GoogleDriveObject::setModifiedTime(QDateTime time)
{
    this->mtime = time;
    this->metadataTimer.start();
}

/**
 * @brief GoogleDriveObject::getChildren Gets the children of this folder object.
 * @return A list of children.
 *
 */
qint64 GoogleDriveObject::getChildren()
{
    qint64 token = this->requestToken++;
    qint64 currentSecs = QDateTime::currentSecsSinceEpoch();
    QString fullPath = getPath();
    D("In get children of: "<<fullPath<<", token:"<<token);
    if (this->isFolder()) {
        if (this->populated) {
            D("Returning populated list..");
            //this->contentsToSend.append(QPair<qint64,QVector<GoogleDriveObject*>>(token,this->contents));
            this->tokenList.append(token);
            QTimer::singleShot(1,this,[&](){
                while(!this->tokenList.isEmpty()) {
                    emit this->children(this->contents,this->tokenList.takeFirst());
                }
            });
        } else {
            D("Fetching remotely..."<<fullPath<<this->id);
            this->gofish->readRemoteFolder(fullPath,this->id);
            this->tokenList.append(token);
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
        (*it)->parentObject = this;
        (*it)->usageCount++;
    }
    this->contents = newChildren;
    D("Set children, "<<this->childFolderCount<<"folders:"<<getPath()<<this->contents.size());
    this->populated = true;
}

GoogleDriveObject *GoogleDriveObject::create(QString name)
{
    GoogleDriveObject *newObj = new GoogleDriveObject(
                this,
                this->gofish,
                "",
                this->path,
                name.replace(QRegExp("[/]"),"_"),
                "application/octet-stream",
                0,
                QDateTime::currentDateTime(),
                QDateTime::currentDateTime(),
                nullptr
               );
    newObj->cache=this->cache;
    newObj->parentObject=this;
    newObj->usageCount++;
    this->contents.append(newObj);
    return newObj;
}

qint64 GoogleDriveObject::createDir(QString name)
{
    qint64 token = this->requestToken++;
    auto folder = create(name);
    this->childFolderCount++;
    folder->mimeType = GOOGLE_FOLDER;
    this->gofish->createFolder(folder->getPath(),folder->id,this->id);
    folder->closeToken = token;

    return token;
}

qint64 GoogleDriveObject::read(qint64 start, qint64 totalLength)
{
    qint64 token = this->requestToken++;
    ReadData readData;
    //QString fullPath = getPath();

    readData.requestedLength = totalLength;
    readData.length = totalLength>this->cacheChunkSize ? totalLength:this->cacheChunkSize;
    readData.start = readData.requestedStart = start;

    D("Read:: read insert (read) "<<token<<readData);
    this->readMap.insert(token,readData);
    if (!this->readTimer.isActive()) {
        this->readTimer.setInterval(0);
        this->readTimer.setSingleShot(true);
        this->readTimer.start();
    }
    return token;

}

qint64 GoogleDriveObject::write(QByteArray data, qint64 start)
{
    qint64 token = this->requestToken++;
    if (this->temporaryFile) {
        if (start<=this->temporaryFile->size()) {
            QTimer::singleShot(0,[=]{
                if (this->temporaryFile->open(QIODevice::ReadWrite)) {
                    this->temporaryFile->seek(start);
                    this->temporaryFile->write(data);
                    this->temporaryFile->close();
                    if (this->temporaryFile->size()>this->size) {
                        this->size=this->temporaryFile->size();
                    }
                }
                emit this->writeResponse(token);
            });
        } else if (start<=size) {
            qint64 readToken = read(this->temporaryFile->size(),start-this->temporaryFile->size());
            connect(this,&GoogleDriveObject::readResponse,[=](QByteArray,qint64 rtoken){
                QTimer::singleShot(0,[=](){
                    if (readToken==rtoken) {
                        D("Do actual write after read"<<start<<this->temporaryFile->size()<<this->temporaryFile->fileName());
                        if (this->temporaryFile->open(QIODevice::ReadWrite)) {
                            this->temporaryFile->seek(start);
                            D("Durrent pos"<<this->temporaryFile->pos()<<this->size);
                            this->temporaryFile->write(data);
                            this->temporaryFile->close();
                            if (this->temporaryFile->size()>this->size) {
                                this->size=this->temporaryFile->size();
                            }
                            D("Size now:"<<this->temporaryFile->size()<<this->size);
                        }
                        emit this->writeResponse(token);
                    }
                });
            });
        }
    }
    return token;
}

qint64 GoogleDriveObject::unlinkChild(QString name)
{
    qint64 token = this->requestToken++;
    D("Unlink child: "<<name);
     QTimer::singleShot(0,[=](){

         GoogleDriveObject *child = nullptr;
         for(auto aChild : this->contents) {
            if (aChild->getName()==name) {
                child = aChild;
                break;
            }
         }
         if (child) {
             child->unlink(token);
         }
    });
     return token;
}

qint64 GoogleDriveObject::takeItem(GoogleDriveObject *oldParent, QString name, QString newName)
{
    qint64 token = this->requestToken++;

    for(auto child : oldParent->contents) {
        if (child->getName()==name) {
            oldParent->contents.removeOne(child);
            child->name=newName;
            this->contents.append(child);

            this->renameMap.insert(child->id,token);
            this->gofish->rename(child->id,oldParent->id,this->id,newName);
            break;
        }
    }
    return token;
}

QCache<QString, CacheEntry> *GoogleDriveObject::getCache() const
{
    return this->cache;
}

void GoogleDriveObject::setCache(QCache<QString, CacheEntry> *cache)
{
    this->cache = cache;
}

void GoogleDriveObject::open(bool write)
{
    this->usageCount++;
    if (!this->temporaryFile && write) {
        this->temporaryFile = new QTemporaryFile(QDir::tempPath()+"/gofishtemp");
    }
}

qint64 GoogleDriveObject::close()
{
    qint64 token = this->requestToken++;
    decreaseUsageCount();
    if (this->temporaryFile) {

        if (this->temporaryFile->size()<this->size) {
            qint64 pos = this->temporaryFile->size();
            qint64 token = read(pos,this->size-pos);
            connect(this,&GoogleDriveObject::readResponse,this->temporaryFile,[=](QByteArray data,qint64 reqToken){
                if (token==reqToken) {
                    this->writeTempFileToGoogle(token);
                }
            });
        } else {
            this->writeTempFileToGoogle(token);
        }
    } else {
        QTimer::singleShot(0,[=]{
            emit this->closeResponse(token);
        });
    }
    return token;
}

GoogleDriveObject *GoogleDriveObject::getParentObject()
{
    if (this->parentObject) {
        return this->parentObject;
    }
    return nullptr;
}

int GoogleDriveObject::getFileMode()
{
    return this->fileMode;
}

void GoogleDriveObject::setFileMode(int mode)
{
    this->fileMode = mode;
}

uid_t GoogleDriveObject::getUid()
{
    return this->uid;
}

void GoogleDriveObject::setUid(uid_t uid)
{
    this->uid = uid;
}

gid_t GoogleDriveObject::getGid()
{
    return this->gid;
}

void GoogleDriveObject::setGid(gid_t gid)
{
    this->gid = gid;
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
            qint64 key = this->readMap.firstKey();
            ReadData data = this->readMap.take(key);
            D("Read:"<<data.start<<data.length<<getPath()<<"Filesize:"<<getSize());

            while(data.length>0) {
                if (this->temporaryFile && data.start<this->temporaryFile->size()) {
                    D("Reading from temp file"<<data.start<<this->temporaryFile->size()<<this->temporaryFile->fileName());
                    this->temporaryFile->open(QIODevice::ReadWrite);
                    this->temporaryFile->seek(data.start);
                    int readSize = data.requestedLength;
                    D("Readsize1"<<readSize);
                    if (this->temporaryFile->size()<(data.start+readSize)) {
                        readSize = this->temporaryFile->size()-data.start;
                    }
                    D("Readsize2"<<readSize);
                    QByteArray readData = this->temporaryFile->read(readSize);
                    data.length-=readData.size();
                    data.start+=readData.size();
                    data.data.append(readData);
                    if ((data.requestedLength-data.data.size())==0){
                        data.length=0;
                    }
                    D("Datalen:"<<data.length<<"rds:"<<readData.size());
                    this->temporaryFile->close();
                    if (data.length==0) {
                        emit readResponse(data.data,key);
                    }
                    continue;
                }

                qint64 msecs = this->mtime.toMSecsSinceEpoch();
                int chunkCount = data.start/this->cacheChunkSize;
                QString chunkId = QString("%1:%2:%3:%4").arg(this->id).arg(chunkCount * this->cacheChunkSize).arg(this->cacheChunkSize).arg(msecs);

                D("READ chunk id:"<<chunkId);
                bool done=false;
                if (this->cache->contains(chunkId)) {
                    CacheEntry *cacheEntry = this->cache->object(chunkId);
                    D("Got cache hit for:"<<chunkId<<", read from:"<<(data.start-cacheEntry->start));
                    data.data.append(cacheEntry->data.mid(data.start-cacheEntry->start));
                    data.length-=this->cacheChunkSize;
                    data.start+=this->cacheChunkSize;
                    if (data.length<=0) {
                        if (data.data.length()>data.requestedLength) {
                            data.data.resize(data.requestedLength);
                        }
                        saveRemoteDateToTempFile(data);
                        emit readResponse(data.data,key);
                    }
                    D("Data len now:"<<data.length);
                    done=true;
                }

                if (!done && !this->isFolder()) {
                    qint64 currentReadChunkSize = this->readChunkSize;
                    this->pendingSegments.insert(QPair<qint64,qint64>(data.start,currentReadChunkSize),QPair<qint64,ReadData>(key,data));
                    this->gofish->getFileContents(this->id,data.start,currentReadChunkSize);
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

void GoogleDriveObject::saveRemoteDateToTempFile(GoogleDriveObject::ReadData &readData)
{
    if (!this->temporaryFile) {
        return;
    }
    D("Save data to temp file:"<<readData);
    if (this->temporaryFile->open(QIODevice::ReadWrite)) {
        this->temporaryFile->seek(readData.requestedStart);
        this->temporaryFile->write(readData.data);
        this->temporaryFile->close();
    }
}

void GoogleDriveObject::writeTempFileToGoogle(qint64 token)
{
    D("Upload: " << this->getPath());

    this->closeToken = token;
    auto parent = getParentObject();
    this->gofish->uploadFile(this->temporaryFile,this->getPath(),this->id,(parent==nullptr)?"root":parent->id);
}

void GoogleDriveObject::unlink(qint64 token)
{
    this->unlinkToken = token;
    this->gofish->unlink(getPath(),this->id);
}

void GoogleDriveObject::decreaseUsageCount()
{
    this->usageCount--;
    if (this->usageCount<=0) {
        if (this->parentObject) {
            D("Deleting object:"<<this->instanceId);
            this->parentObject->contents.removeOne(this);
            this->deleteLater();
        }
    }
}

void GoogleDriveObject::processRemoteFolder(QString path, QVector<GoogleDriveObject *> children)
{
    if (path==getPath()) {
        D("Got children of: "<<this->path << this->name);
        for(GoogleDriveObject *child:children) {
            child->setCache(this->getCache());
        }
        this->setChildren(children);
//        if (!this->tokenList.empty()) {

//            QTimer::singleShot(1,this,[&](){
                while(!this->tokenList.isEmpty()) {
                    emit this->children(this->contents,this->tokenList.takeFirst());
                }
//            });
//        }
        //this->contentsToSend.append(QPair<qint64,QVector<GoogleDriveObject*>>(token,children));
    }
}

void GoogleDriveObject::processPendingSegment(QString fileId, qint64 start, qint64 length)
{
    if (this->id!=fileId) {
        return;
    }
    D("Process pending segment:"<<fileId<<start<<length);
    QByteArray receivedData = this->gofish->getPendingSegment(fileId,start,length);
    QPair<qint64,ReadData> dataPair = this->pendingSegments.take(QPair<qint64,qint64>(start,length));
    ReadData myData = dataPair.second;
    qint64 key = dataPair.first;
    qint64 msecs = this->mtime.toMSecsSinceEpoch();
    D("Got data: ,"<<receivedData.size());
Q_ASSERT(myData.start==start);

    if (!receivedData.isEmpty()) {
        while(!receivedData.isEmpty()) {
            CacheEntry *cacheEntry = new CacheEntry();
            cacheEntry->data = receivedData.left(this->cacheChunkSize);
            cacheEntry->start = start;
            QString cacheChunkId = QString("%1:%2:%3:%4").arg(this->id).arg(start).arg(this->cacheChunkSize).arg(msecs);
            start+=this->cacheChunkSize;
            receivedData = receivedData.mid(this->cacheChunkSize);
            int cost = cacheEntry->data.size()/1024;
            if (cost==0) {
                cost=1;
            }
            myData.data.append(cacheEntry->data);
            this->cache->insert(cacheChunkId,cacheEntry,cost);
            D("Inserted "<<cacheChunkId<<",of size: "<<cacheEntry->data.size()<<", start"<<cacheEntry->start<<"into cache.");
            D("Cache: "<<this->cache->totalCost()<<"of"<<this->cache->maxCost()<<", "<<(((qint64)this->cache->totalCost())*100/((qint64)this->cache->maxCost()))<<"%");
        }
    }
    myData.start+=length;
    myData.length -= length;
    if (myData.length>0) {
        D("In pps, inserting into readmap: key="<<key<<", data="<<myData);
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
        if (this->temporaryFile) {
            this->saveRemoteDateToTempFile(myData);
        }
        emit readResponse(myData.data,key);
    }
}

void GoogleDriveObject::processUploadInProgress(QString path)
{
    if (path==getPath()) {
        emit closeInProgress(this->closeToken);
    }
}

void GoogleDriveObject::processUploadComplete(QString path, QString fileId)
{
    if (path==getPath()) {
        if (this->temporaryFile) {
            D("Removed temp file:"<<this->temporaryFile->fileName());
            this->temporaryFile->deleteLater();
            this->temporaryFile=nullptr;
        }
        this->id = fileId;

        // remove any entries in the cache for this file
        for(QString key:this->cache->keys()){
            if (key.startsWith(this->id+":")) {
                this->cache->remove(key);
                D("**** REMOVING CACHE ENTRY:"+key);
            }
        }
        emit closeResponse(this->closeToken);
    }
}

void GoogleDriveObject::processDirCreated(QString path, QString fileId)
{
    if (path==getPath()) {
        D("Got dir created:"<<path);
        this->id = fileId;
        emit this->getParentObject()->createDirResponse(this->closeToken,this);
    }
}

QDebug operator<<(QDebug debug, const GoogleDriveObject &o)
{
    QDebugStateSaver s(debug);

    debug.nospace()
            << "[Ino: " << o.getInode()
            << ", name: " << o.getName()
            << ", Mimetype: " << o.getMimeType()
            << ", Size: " << o.getSize()
            << ", is folder: " << (o.isFolder()?'Y':'N')
            << ", full path: " << o.getPath()
            << "]"
               ;
    return debug;
}
QDebug operator<<(QDebug debug, const GoogleDriveObject::ReadData &o)
{
    QDebugStateSaver s(debug);

    debug.nospace()
            << "[Start: " << o.start
            << ", length: " << o.length
            << ", reqst: " << o.requestedStart
            << ", reqlen: " << o.requestedLength
            << ", datalen: " << o.data.size()
            << "]"
               ;
    return debug;
}
