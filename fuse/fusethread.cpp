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
}

void FuseThread::run()
{
    D("In fuse thread...");

    QSettings settings;
    settings.beginGroup("googledrive");

    this->root  = new GoogleDriveObject(
                this->gofish,
                settings.value("in_memory_cache_bytes",DEFAULT_CACHE_SIZE).toULongLong(),
                settings.value("refresh_seconds",600).toUInt()
                );

    struct fuse_operations *fuse_ops = new fuse_operations();

    fuse_ops->getattr = FuseThread::fuse_getattr;
    fuse_ops->opendir = FuseThread::fuse_opendir;
    fuse_ops->readdir = FuseThread::fuse_readdir;
    fuse_ops->releasedir = FuseThread::fuse_closedir;
    fuse_ops->open = FuseThread::fuse_open;
    fuse_ops->read = FuseThread::fuse_read;
    fuse_ops->release = FuseThread::fuse_close;

    fuse_main(this->argc,this->argv,fuse_ops,this);
//    static struct fuse_operations fuse_ops = {
//        .getattr	,
//        .opendir    = FuseThread::fuse_opendir,
//        .readdir    = FuseThread::fuse_readdir,
//        .releasedir = FuseThread::fuse_closedir,
//        .open		= FuseThread::fuse_open,
//        .read		= FuseThread::fuse_read,
//        .release    = FuseThread::fuse_close
    //    };
}

int FuseThread::getAttr(const char *path, struct stat *stbuf)
{
    QString pathString(path);
    D("In getattr"<<path);

    GoogleDriveObject *item = getObjectForPath(path);

    if (!item) {
        return -ENOENT;
    }

    ::memset(stbuf,0,sizeof(struct stat));

 //   stbuf->st_mode = 0666;
    stbuf->st_mode= 0555;
    if (item->isFolder()) {
        D("Thingy is dir.");
        stbuf->st_mode |= S_IFDIR;
        stbuf->st_size = 512;
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
    /*
    if (this->paths.contains(path)) {
        QJsonDocument doc = this->paths.value(path);
        stbuf->st_size = doc["size"].toDouble(0);
        stbuf->st_ctime = 3431;
        stbuf->st_atime = 3542325;
        stbuf->st_mtime = stbuf->st_atime;
        stbuf->st_ino   = doc["inode"].toDouble(0);
    }
    */
    stbuf->st_uid = this->user;
    stbuf->st_gid = this->group;
    //stbuf->st

    item->release();

    D("Leaving getattr:"<<path);
    return 0;
}

int FuseThread::openDir(const char *path, struct fuse_file_info *fi)
{
    D("In opendir"<<path);
    GoogleDriveObject *obj = getObjectForPath(path);
    if (obj) {
        obj->lock();
        obj->getChildFolderCount();
    }
    fi->fh = reinterpret_cast<quint64>(obj);
    return obj?0:-ENOENT;
    return 0;
}

int FuseThread::readDir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    D("In readDir... "<<path);
    GoogleDriveObject *object = reinterpret_cast<GoogleDriveObject*>(fi->fh);
    if (!object) {
        D("reddir - Did not get object for path:"<<path);
        return -ENOENT;
    }

    int ret = 0;
    QVector<GoogleDriveObject*> children = object->getChildren();
    for(int idx=offset;idx<children.size();idx++) {
        GoogleDriveObject *child = children.at(idx);
        if(ret==0 && filler(buf,child->getName().toUtf8().constData(),0,0)) {
            ret = -ENOMEM;
        }
        child->release();
    }
    return ret;
}

int FuseThread::closeDir(const char *path, struct fuse_file_info *fi)
{
    D("In closedir:"<<path);
    GoogleDriveObject *item = reinterpret_cast<GoogleDriveObject*>(fi->fh);
    if (item) {
        item->release();
    }
    return (item)?0:-ENOENT;
}

int FuseThread::open(const char *path, struct fuse_file_info *fi)
{
    D("In open"<<path);
    GoogleDriveObject *obj = getObjectForPath(path);
    fi->fh = reinterpret_cast<quint64>(obj);
    return obj?0:-ENOENT;
}

int FuseThread::read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    D("In read"<<path<<", offset:"<<offset<<", size:"<<size);

    GoogleDriveObject *item = reinterpret_cast<GoogleDriveObject*>(fi->fh);

    quint64 blockSize = item->getBlockSize();
    quint64 start = offset;
    quint64 readCount = 0;
    char *ptr=buf;
    quint64 chunkStart = (start/blockSize) * blockSize;

    if (item && !item->isFolder()) {
        while (size>0) {
            D("Start: "<<start<<", chunkstart: "<<chunkStart<<", size: "<<size);

            // read a chunk
            QByteArray data = item->read(chunkStart,blockSize);
            QByteArray where = QString("%1 _ %2 _ %3").arg(path).arg(chunkStart).arg(blockSize).toLocal8Bit();
            Q_ASSERT_X(!data.isEmpty(),"read",where.data());

            D("Got returned data, size: "<<data.size());

            int copySize = size;
            if (data.size()<copySize) {
                copySize = data.size();
            }
            quint64 chunkAvailable = blockSize - (start-chunkStart);
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

int FuseThread::close(const char *, fuse_file_info *fi)
{
    GoogleDriveObject *item = reinterpret_cast<GoogleDriveObject*>(fi->fh);
    if (item) {
        item->release();
    }
    return (item)?0:-ENOENT;
}

GoogleDriveObject *FuseThread::getObjectForPath(QString path)
{
    D("Get object for path: "<<path);

    GoogleDriveObject *ret = this->root;

    path = path.mid(1); // Strip off the initial slash

    ret->lock();

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
            } else {
                child->release();
            }
        }
        ret->release();
        ret = childItem;
    }
    D("Returning item:"<<(ret?ret->getName():"--NONE--"));
    return ret;
}

void FuseThread::validatePath(QString path)
{
    auto *ptr = getObjectForPath(path);
    if (ptr) {
        ptr->release();
    }
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


