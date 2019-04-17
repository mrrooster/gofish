#include "fusethread.h"
#include <QDebug>
#include <sys/stat.h>
#include <string.h>
#include "defaults.h"
#include <QSettings>

#define DEBUG_FUSE
#ifdef DEBUG_FUSE
#define D(x) qDebug() << QString("[FuseThread::%1]").arg(QString::number((quint64)QThread::currentThreadId(),16)).toUtf8().data() << x
#define SD(x) qDebug() << QString("[FuseThread::static]") << x
#else
#define D(x)
#define SD(x)
#endif

FuseThread::FuseThread(int argc, char *argv[],GoogleDrive *gofish, QObject *parent) : QThread(parent)
{
    D("Fuse thread created.");
    this->argc   = argc;
    this->argv   = argv;
    this->user   = ::getuid();
    this->group  = ::getgid();
    this->gofish = gofish;
    this->root   = nullptr;
    this->cache  = nullptr;

    QSettings settings;
    settings.beginGroup("googledrive");
    qint64 refreshSecs = settings.value("refresh_seconds",DEFAULT_REFRESH_SECS).toUInt();

    connect(&this->refreshTimer,&QTimer::timeout,this,&FuseThread::setupRoot);
    this->refreshTimer.setSingleShot(false);
    this->refreshTimer.start(refreshSecs*1000);
    qInfo().noquote() << QString("Refreshing directory every %1 seconds.").arg(refreshSecs);
}

void FuseThread::run()
{
    D("In fuse thread...");
    struct fuse_operations *fuse_ops = new fuse_operations();

    fuse_ops->getattr = FuseThread::fuse_getattr;
    fuse_ops->opendir = FuseThread::fuse_opendir;
    fuse_ops->readdir = FuseThread::fuse_readdir;
    fuse_ops->releasedir = FuseThread::fuse_closedir;
    fuse_ops->open = FuseThread::fuse_open;
    fuse_ops->read = FuseThread::fuse_read;
    fuse_ops->release = FuseThread::fuse_close;
    fuse_ops->destroy = FuseThread::fuse_destroy;

    setupRoot();

    fuse_main(this->argc,this->argv,fuse_ops,this);
}

int FuseThread::getAttr(const char *path, struct stat *stbuf)
{
    D("In getattr"<<path);

    GoogleDriveObject *item = getObjectForPath(path);

    if (!item) {
        return -ENOENT;
    }

    setupStat(item,stbuf);
    D("Leaving getattr:"<<path);
    return 0;
}

int FuseThread::openDir(const char *path, struct fuse_file_info *)
{
    D("In opendir"<<path);
    GoogleDriveObject *obj = getObjectForPath(path);
    return obj?0:-ENOENT;
}

int FuseThread::readDir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *)
{
    D("In readDir... "<<path);
    GoogleDriveObject *object = getObjectForPath(path);
    if (!object) {
        D("reddir - Did not get object for path:"<<path);
        return -ENOENT;
    }

    int ret = 0;
    struct stat stbuf;
    QVector<GoogleDriveObject*> children = object->getChildren();
    for(int idx=offset;idx<children.size();idx++) {
        GoogleDriveObject *child = children.at(idx);
        setupStat(child,&stbuf);
        if(ret==0 && filler(buf,child->getName().toUtf8().constData(),&stbuf,0)) {
            ret = -ENOMEM;
        }
    }
    return ret;
}

int FuseThread::closeDir(const char *path, struct fuse_file_info *)
{
    D("In closedir:"<<path);
    GoogleDriveObject *item = getObjectForPath(path);
    return (item)?0:-ENOENT;
}

int FuseThread::open(const char *path, struct fuse_file_info *)
{
    D("In open"<<path);
    GoogleDriveObject *obj = getObjectForPath(path);
    return obj?0:-ENOENT;
}

int FuseThread::read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *)
{
    D("In read"<<path<<", offset:"<<offset<<", size:"<<size);

    GoogleDriveObject *item = getObjectForPath(path);

    qint64 blockSize = item->getBlockSize();
    qint64 start = offset;
    qint64 readCount = 0;
    char *ptr=buf;
    qint64 chunkStart = (start/blockSize) * blockSize;

    if (start>=item->getSize()) {
        return -ENODATA;
    }

    if (item->isFolder()) {
        return -EISDIR;
    }

    if ((start+size)>item->getSize()) {
        size = item->getSize()-start;
    }

    if (item && !item->isFolder()) {
        while (size>0) {
            D("Start: "<<start<<", chunkstart: "<<chunkStart<<", size: "<<size);

            // read a chunk
            QByteArray data = item->read(chunkStart,blockSize);

            D("Got returned data, size: "<<data.size());
            if (data.isEmpty()) {
                return -EIO;
            }

            int copySize = size;
            if (data.size()<copySize) {
                copySize = data.size();
            }
            qint64 chunkAvailable = blockSize - (start-chunkStart);
            if (size > chunkAvailable) {
                copySize = chunkAvailable;
            }
            D("start-chunkstart:"<<(start-chunkStart)<<", copysize:"<<copySize);
            ::memcpy(ptr,data.data() + (start-chunkStart),copySize);
            ptr+=copySize;
            size-=copySize;
            readCount += copySize;
            start += copySize;
            chunkStart = (start/blockSize) * blockSize;
            D("Size:"<<size);
        }
    }

    //item->release();

    D("Returning: "<<readCount);

    return readCount;
}

int FuseThread::close(const char *path,struct fuse_file_info *)
{
    return (getObjectForPath(path))?0:-ENOENT;
}

void FuseThread::destroy()
{
    //terminate();
}

GoogleDriveObject *FuseThread::getObjectForPath(QString path)
{
    QMutexLocker locker(&this->rootSwapLock);
    D("Get object for path: "<<path);

    GoogleDriveObject *ret = this->root;

    path = path.mid(1); // Strip off the initial slash

    while(!path.isEmpty() && ret) {
        QString item = path.left(path.indexOf('/'));
        if (path.contains('/')) {
            path = path.mid(path.indexOf('/')+1);
        } else {
            path = "";
        }

        QVector<GoogleDriveObject*> children = ret->getChildren();
        D("Got "<<children.size()<<"children");

        GoogleDriveObject *childItem=nullptr;
        for(int idx=0;idx<children.size();idx++) {
//            D("Checking:"<<children.at(idx).getName()<<idx);
            GoogleDriveObject *child = children.at(idx);
            if (!childItem && child->getName()==item) {
//                D("Found!");
                childItem = child;
            }
        }
        ret = childItem;
    }
    D("Returning item:"<<(ret?ret->getName():"--NONE--"));
    return ret;
}

void FuseThread::validatePath(QString path)
{
    getObjectForPath(path);
}

void FuseThread::setupStat(GoogleDriveObject *item,struct stat *stbuf)
{
    ::memset(stbuf,0,sizeof(struct stat));

    stbuf->st_mode= 0555;
    if (item->isFolder()) {
        D("Thingy is dir.");
        stbuf->st_mode |= S_IFDIR;
        stbuf->st_size = 0;
        stbuf->st_nlink = 2 + item->getChildFolderCount();
    } else {
        D("Thingy is regular file.");
        stbuf->st_mode |= S_IFREG;
        stbuf->st_size = item->getSize();
        stbuf->st_nlink = 1;
    }
    stbuf->st_rdev = S_IFCHR;
    stbuf->st_ctime= item->getCreatedTime().toTime_t();
    stbuf->st_mtime= item->getCreatedTime().toTime_t();
    stbuf->st_ino  = item->getInode();
    stbuf->st_uid = this->user;
    stbuf->st_gid = this->group;
}

void FuseThread::setupRoot()
{
    QMutexLocker locker(&this->rootSwapLock);
    D("Refreshing root folder.");

    if (this->cache == nullptr) {
        QSettings settings;
        settings.beginGroup("googledrive");
        qint64 cacheSize = settings.value("in_memory_cache_bytes",DEFAULT_CACHE_SIZE).toULongLong()/1024;

        if (cacheSize<(CACHE_CHUNK_SIZE/1024)) {
            cacheSize = (CACHE_CHUNK_SIZE/1024);
        }
        qInfo().noquote() << QString("Using in memory cache of %1MiB").arg(cacheSize/1024);
        this->cache = new QCache<QString,QByteArray>(cacheSize);
    }

    if (this->root) {
        QTimer::singleShot(600000,this->root,&QObject::deleteLater);
    }
    this->root  = new GoogleDriveObject(
                this->gofish,
                this->cache
                );
}

int FuseThread::fuse_getattr(const char *path, struct stat *stbuf)
{
    return static_cast<FuseThread*>(fuse_get_context()->private_data)->getAttr(path,stbuf);
}

int FuseThread::fuse_opendir(const char *path, struct fuse_file_info *fi)
{
    return static_cast<FuseThread*>(fuse_get_context()->private_data)->openDir(path,fi);
}

int FuseThread::fuse_closedir(const char *path, struct fuse_file_info *fi)
{
    return static_cast<FuseThread*>(fuse_get_context()->private_data)->closeDir(path,fi);
}

int FuseThread::fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    return static_cast<FuseThread*>(fuse_get_context()->private_data)->readDir(path,buf,filler,offset,fi);
}

int FuseThread::fuse_open(const char *path, struct fuse_file_info *fi)
{
    return static_cast<FuseThread*>(fuse_get_context()->private_data)->open(path,fi);
}

int FuseThread::fuse_close(const char *path, struct fuse_file_info *fi)
{
    return static_cast<FuseThread*>(fuse_get_context()->private_data)->close(path,fi);
}

int FuseThread::fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    return static_cast<FuseThread*>(fuse_get_context()->private_data)->read(path,buf,size,offset,fi);
}

void FuseThread::fuse_destroy(void *)
{
    static_cast<FuseThread*>(fuse_get_context()->private_data)->destroy();
}


