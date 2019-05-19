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
QMap<QString,QString> GoogleDriveObject::typeMap{
    {"ai","application/postscript"},
    {"aif","audio/x-aiff"},
    {"aifc","audio/x-aiff"},
    {"aiff","audio/x-aiff"},
    {"asc","text/plain"},
    {"atom","application/atom+xml"},
    {"au","audio/basic"},
    {"avi","video/x-msvideo"},
    {"bcpio","application/x-bcpio"},
    {"bin","application/octet-stream"},
    {"bmp","image/bmp"},
    {"cdf","application/x-netcdf"},
    {"cgm","image/cgm"},
    {"class","application/octet-stream"},
    {"cpio","application/x-cpio"},
    {"cpt","application/mac-compactpro"},
    {"csh","application/x-csh"},
    {"css","text/css"},
    {"dcr","application/x-director"},
    {"dif","video/x-dv"},
    {"dir","application/x-director"},
    {"djv","image/vnd.djvu"},
    {"djvu","image/vnd.djvu"},
    {"dll","application/octet-stream"},
    {"dmg","application/octet-stream"},
    {"dms","application/octet-stream"},
    {"doc","application/msword"},
    {"dtd","application/xml-dtd"},
    {"dv","video/x-dv"},
    {"dvi","application/x-dvi"},
    {"dxr","application/x-director"},
    {"eps","application/postscript"},
    {"etx","text/x-setext"},
    {"exe","application/octet-stream"},
    {"ez","application/andrew-inset"},
    {"gif","image/gif"},
    {"gram","application/srgs"},
    {"grxml","application/srgs+xml"},
    {"gtar","application/x-gtar"},
    {"hdf","application/x-hdf"},
    {"hqx","application/mac-binhex40"},
    {"htm","text/html"},
    {"html","text/html"},
    {"ice","x-conference/x-cooltalk"},
    {"ico","image/x-icon"},
    {"ics","text/calendar"},
    {"ief","image/ief"},
    {"ifb","text/calendar"},
    {"iges","model/iges"},
    {"igs","model/iges"},
    {"jnlp","application/x-java-jnlp-file"},
    {"jp2","image/jp2"},
    {"jpe","image/jpeg"},
    {"jpeg","image/jpeg"},
    {"jpg","image/jpeg"},
    {"js","application/x-javascript"},
    {"kar","audio/midi"},
    {"latex","application/x-latex"},
    {"lha","application/octet-stream"},
    {"lzh","application/octet-stream"},
    {"m3u","audio/x-mpegurl"},
    {"m4a","audio/mp4a-latm"},
    {"m4b","audio/mp4a-latm"},
    {"m4p","audio/mp4a-latm"},
    {"m4u","video/vnd.mpegurl"},
    {"m4v","video/x-m4v"},
    {"mac","image/x-macpaint"},
    {"man","application/x-troff-man"},
    {"mathml","application/mathml+xml"},
    {"me","application/x-troff-me"},
    {"mesh","model/mesh"},
    {"mid","audio/midi"},
    {"midi","audio/midi"},
    {"mif","application/vnd.mif"},
    {"mkv","video/x-matroska"},
    {"mov","video/quicktime"},
    {"movie","video/x-sgi-movie"},
    {"mp2","audio/mpeg"},
    {"mp3","audio/mpeg"},
    {"mp4","video/mp4"},
    {"mpe","video/mpeg"},
    {"mpeg","video/mpeg"},
    {"mpg","video/mpeg"},
    {"mpga","audio/mpeg"},
    {"ms","application/x-troff-ms"},
    {"msh","model/mesh"},
    {"mxu","video/vnd.mpegurl"},
    {"nc","application/x-netcdf"},
    {"oda","application/oda"},
    {"ogg","application/ogg"},
    {"pbm","image/x-portable-bitmap"},
    {"pct","image/pict"},
    {"pdb","chemical/x-pdb"},
    {"pdf","application/pdf"},
    {"pgm","image/x-portable-graymap"},
    {"pgn","application/x-chess-pgn"},
    {"pic","image/pict"},
    {"pict","image/pict"},
    {"png","image/png"},
    {"pnm","image/x-portable-anymap"},
    {"pnt","image/x-macpaint"},
    {"pntg","image/x-macpaint"},
    {"ppm","image/x-portable-pixmap"},
    {"ppt","application/vnd.ms-powerpoint"},
    {"ps","application/postscript"},
    {"qt","video/quicktime"},
    {"qti","image/x-quicktime"},
    {"qtif","image/x-quicktime"},
    {"ra","audio/x-pn-realaudio"},
    {"ram","audio/x-pn-realaudio"},
    {"ras","image/x-cmu-raster"},
    {"rdf","application/rdf+xml"},
    {"rgb","image/x-rgb"},
    {"rm","application/vnd.rn-realmedia"},
    {"roff","application/x-troff"},
    {"rtf","text/rtf"},
    {"rtx","text/richtext"},
    {"sgm","text/sgml"},
    {"sgml","text/sgml"},
    {"sh","application/x-sh"},
    {"shar","application/x-shar"},
    {"silo","model/mesh"},
    {"sit","application/x-stuffit"},
    {"skd","application/x-koan"},
    {"skm","application/x-koan"},
    {"skp","application/x-koan"},
    {"skt","application/x-koan"},
    {"smi","application/smil"},
    {"smil","application/smil"},
    {"snd","audio/basic"},
    {"so","application/octet-stream"},
    {"spl","application/x-futuresplash"},
    {"src","application/x-wais-source"},
    {"sv4cpio","application/x-sv4cpio"},
    {"sv4crc","application/x-sv4crc"},
    {"svg","image/svg+xml"},
    {"swf","application/x-shockwave-flash"},
    {"t","application/x-troff"},
    {"tar","application/x-tar"},
    {"tcl","application/x-tcl"},
    {"tex","application/x-tex"},
    {"texi","application/x-texinfo"},
    {"texinfo","application/x-texinfo"},
    {"tif","image/tiff"},
    {"tiff","image/tiff"},
    {"tr","application/x-troff"},
    {"tsv","text/tab-separated-values"},
    {"txt","text/plain"},
    {"ustar","application/x-ustar"},
    {"vcd","application/x-cdlink"},
    {"vrml","model/vrml"},
    {"vxml","application/voicexml+xml"},
    {"wav","audio/x-wav"},
    {"wbmp","image/vnd.wap.wbmp"},
    {"wbmxl","application/vnd.wap.wbxml"},
    {"wml","text/vnd.wap.wml"},
    {"wmlc","application/vnd.wap.wmlc"},
    {"wmls","text/vnd.wap.wmlscript"},
    {"wmlsc","application/vnd.wap.wmlscriptc"},
    {"wrl","model/vrml"},
    {"xbm","image/x-xbitmap"},
    {"xht","application/xhtml+xml"},
    {"xhtml","application/xhtml+xml"},
    {"xls","application/vnd.ms-excel"},
    {"xml","application/xml"},
    {"xpm","image/x-xpixmap"},
    {"xsl","application/xml"},
    {"xslt","application/xslt+xml"},
    {"xul","application/vnd.mozilla.xul+xml"},
    {"xwd","image/x-xwindowdump"},
    {"xyz","chemical/x-xyz"},
    {"zip","application/zip"}
};

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
    this->closeToken = 0;
    this->openCount  = 0;

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
    connect(this->gofish,&GoogleDrive::unlinkComplete,this,[=](QString path,bool){
        if (path==this->getPath()) {
            this->decreaseUsageCount();
            emit unlinkResponse(this->unlinkToken,true);
        }
    });

    this->metadataTimer.setSingleShot(true);
    this->metadataTimer.setInterval(3000);
    D("New google object: "<<*this);

    if (this->gofish->getPrecacheDirs() && isFolder()) {
        this->gofish->addObjectToScan(this);
    }
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

void GoogleDriveObject::setSize(qint64 size)
{
    this->size = size;
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
    QMap<qint64,GoogleDriveObject*> finalObjects;
    QVector<GoogleDriveObject*> objectsToRefresh;

    D("In setchildren, with "<<newChildren.size()<<"entries.");
    for(auto child : newChildren) {
        finalObjects.insert(child->getInode(),child);
    }
    for(auto child : this->contents) {
        if (finalObjects.contains(child->getInode())) {
            GoogleDriveObject *newChild = finalObjects.take(child->getInode());
            child->updateUsing(newChild);
            newChild->decreaseUsageCount();
            finalObjects.insert(child->getInode(),child);
            objectsToRefresh.append(child);
        } else {
            D("Release stale item:"<<child->getPath()<<child->id);
            child->decreaseUsageCount();
        }
    }
    D("Final object count: "<<finalObjects.size());

    this->childFolderCount = 0;
    this->contents.clear();

    for(auto child : finalObjects.values()) {
        if (child->isFolder()) {
            this->childFolderCount++;
        }
        child->parentObject = this;
        child->usageCount++;
        this->contents.append(child);
    }

    D("Set children, "<<this->childFolderCount<<"folders:"<<getPath()<<this->contents.size());
    this->populated = true;
    D("Refreshing "<<objectsToRefresh.size()<<" child objects.");
    this->gofish->addObjectsToScan(objectsToRefresh);
}

GoogleDriveObject *GoogleDriveObject::create(QString name)
{
    GoogleDriveObject *newObj = new GoogleDriveObject(
                this,
                this->gofish,
                "",
                getPath(),
                name.replace(QRegExp("[/]"),"_"),
                mimetypeFromName(name),
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

qint64 GoogleDriveObject::createDir(QString name,mode_t mode)
{
    qint64 token = this->requestToken++;
    auto folder = create(name);
    folder->setFileMode(mode);
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
    write(data,start,token);
    return token;
}

void GoogleDriveObject::write(QByteArray data, qint64 start, qint64 token)
{
    if (this->closeToken>0) {
        // Writing data to google
        QTimer::singleShot(500,[=](){
            D("Write blocked due to upload!!");
            this->write(data,start,token);
        });
        emit this->opInProgress(token);
        return;
    }
    if (this->temporaryFile) {
        this->fileUploadedToGoogle = false;
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
            emit this->opInProgress(token);
            connect(this,&GoogleDriveObject::opInProgress,this->temporaryFile,[=](qint64 rtoken){
                if (rtoken == readToken) {
                    emit this->opInProgress(token);
                }
            });
            connect(this,&GoogleDriveObject::readResponse,this->temporaryFile,[=](QByteArray,qint64 rtoken){
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
            QString removeId = "";
            for(auto child2 : this->contents) {
                if (child2->getName()==newName) {
                    removeId=child2->id;
                    break;
                }
            }
            this->contents.append(child);

            this->renameMap.insert(child->id,token);
            this->gofish->rename(child->id,oldParent->id,this->id,newName,removeId);
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
    this->openCount++;
    if (!this->temporaryFile && write) {
        this->temporaryFile = new QTemporaryFile(this->gofish->getTempDir()+"/gofishtemp");
    }
}

qint64 GoogleDriveObject::close(bool write)
{
    qint64 token = this->requestToken++;
    decreaseUsageCount();
    this->openCount--;
    if (write && this->temporaryFile) {

        if (this->temporaryFile->size()<this->size) {
            qint64 pos = this->temporaryFile->size();
            qint64 readToken = read(pos,this->size-pos);
            this->closeToken = token;
            connect(this,&GoogleDriveObject::readResponse,this->temporaryFile,[=](QByteArray ,qint64 reqToken){
                if (readToken==reqToken) {
                    this->writeTempFileToGoogle(token);
                }
            });
        } else {
            if (!this->fileUploadedToGoogle) {
                this->writeTempFileToGoogle(token);
            } else {
                QTimer::singleShot(0,[=]{
                    emit this->closeResponse(token);
                });
            }
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
    this->metadataTimer.start();
}

uid_t GoogleDriveObject::getUid()
{
    return this->uid;
}

void GoogleDriveObject::setUid(uid_t uid)
{
    this->uid = uid;
    this->metadataTimer.start();
}

gid_t GoogleDriveObject::getGid()
{
    return this->gid;
}

void GoogleDriveObject::setGid(gid_t gid)
{
    this->gid = gid;
    this->metadataTimer.start();
}

void GoogleDriveObject::stopMetadataUpdate()
{
    this->metadataTimer.stop();
}

QString GoogleDriveObject::mimetypeFromName(QString name)
{
    QString nameToTest = name.toLower();
    QString mimeType = "application/octet-stream";

    if (nameToTest.contains('.')) {
        nameToTest = nameToTest.mid(nameToTest.lastIndexOf('.')+1);
        if (GoogleDriveObject::typeMap.contains(nameToTest)) {
            return GoogleDriveObject::typeMap[nameToTest];
        }
    }
    return mimeType;
}

void GoogleDriveObject::refresh()
{
    if (isFolder()) {
        this->populated = false; // Will cause next 'getChildren' to update dir from google.
    }
}

void GoogleDriveObject::clearChildren(QVector<GoogleDriveObject*> except)
{
    this->childFolderCount = 0;
    QVector<GoogleDriveObject*> oldContents = this->contents;
    this->contents.clear();
    for(auto it = oldContents.begin(),end = oldContents.end();it!=end;it++) {
        if (!except.contains((*it))) {
            (*it)->decreaseUsageCount();
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
                    if (data.start>=getSize()) {
                        data.length=0;
                    }
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
                    emit this->opInProgress(key);
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
        D("Deleting object:"<<this->instanceId<<", usage count:"<<this->usageCount);
        if (this->parentObject) {
            this->parentObject->contents.removeOne(this);
        }
        this->deleteLater();
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
    if (!this->pendingSegments.contains(QPair<qint64,qint64>(start,length))) {
        D("Warning: Got unexpected pending segment. id:"<<fileId<<", start:"<<start<<", len:"<<length);
        return;
    }
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
            int chunkCount = start/this->cacheChunkSize;
            QString cacheChunkId = QString("%1:%2:%3:%4").arg(this->id).arg(chunkCount * this->cacheChunkSize).arg(this->cacheChunkSize).arg(msecs);
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
        D("Upload complete:"<<path<<fileId);
        if (this->temporaryFile && this->openCount==0) {
            D("Removed temp file:"<<this->temporaryFile->fileName());
            this->temporaryFile->deleteLater();
            this->temporaryFile=nullptr;
        }
        this->id = fileId;
        this->fileUploadedToGoogle = true;

        // remove any entries in the cache for this file
        for(QString key:this->cache->keys()){
            if (key.startsWith(this->id+":")) {
                this->cache->remove(key);
            }
        }
        int token = this->closeToken;
        this->closeToken=0;
        emit closeResponse(token);
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

void GoogleDriveObject::updateUsing(GoogleDriveObject *other)
{
    this->mimeType = other->mimeType;
    this->size = other->size;
    this->path = other->path;
    this->name = other->name;
    this->ctime = other->ctime;
    this->mtime = other->mtime;
    this->fileMode = other->fileMode;
    this->uid = other->uid;
    this->gid = other->gid;
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
