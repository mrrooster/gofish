#include "fusethread.h"
#include <QDebug>
#include <sys/stat.h>
#include <string.h>
#include "defaults.h"

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
    this->root  = GoogleDriveObject(this->gofish);

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

    GoogleDriveObject item = getObjectForPath(path);

    if (!item.isValid()) {
        return -ENOENT;
    }

    ::memset(stbuf,0,sizeof(struct stat));

    stbuf->st_mode = 0666;
    stbuf->st_mode= 0555;
    if (item.isFolder()) {
        D("Thingy is dir.");
        stbuf->st_mode |= S_IFDIR;
        stbuf->st_size = 512;
        stbuf->st_nlink = 2 + item.getChildFolderCount();
    } else {
        D("Thingy is regular file.");
        stbuf->st_mode |= S_IFREG;
        stbuf->st_size = item.getSize();
        stbuf->st_nlink = 1;
    }
    stbuf->st_rdev = S_IFCHR;
    stbuf->st_ctime= item.getCreatedTime().toTime_t();
    stbuf->st_mtime= item.getCreatedTime().toTime_t();
    stbuf->st_ino  = item.getInode();
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

    D("Leaving getattr:"<<path);
    return 0;
}

int FuseThread::openDir(const char *path, struct fuse_file_info *fi)
{
    D("In opendir"<<path);
    validatePath(path);
    return 0;
}

int FuseThread::readDir(const char *path, void *buf,fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    D("In readDir... "<<path);
    GoogleDriveObject object = getObjectForPath(path);
    if (!object.isValid()) {
        D("reddir - Did not get object for path:"<<path);
        return -ENOENT;
    }

    QVector<GoogleDriveObject> children = object.getChildren();
    for(int idx=0;idx<children.size();idx++) {
        if(filler(buf,children.at(idx).getName().toUtf8().constData(),0,0)) {
            return -ENOMEM;
        }
    }
    return 0;
}

int FuseThread::closeDir(const char *path, struct fuse_file_info *fi)
{
    D("In closedir:"<<path);
    //this->directories.remove(path);
    return 0;
}

int FuseThread::open(const char *path, struct fuse_file_info *fi)
{
    D("In open"<<path);
    return getObjectForPath(path).isValid();
}

int FuseThread::read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi)
{
    D("In read"<<path<<", offset:"<<offset<<", size:"<<size);

    GoogleDriveObject item = getObjectForPath(path);

    quint64 blockSize = item.getBlockSize();
    quint64 start = offset;
    quint64 readCount = 0;
    char *ptr=buf;
    quint64 chunkStart = (start/blockSize) * blockSize;

    if (item.isValid()) {
        while (size>0) {
            D("Start: "<<start<<", chunkstart: "<<chunkStart<<", size: "<<size);

            // read a chunk
            QByteArray data = item.read(chunkStart,blockSize);

            Q_ASSERT(!data.isEmpty());

            D("Got returned data, size: "<<data.size());

            int copySize = size;
            if (data.size()<copySize) {
                copySize = data.size();
            }
            int chunkAvailable = blockSize - (start-chunkStart);
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

    D("Returning: "<<readCount);

    return readCount;
}

GoogleDriveObject FuseThread::getObjectForPath(QString path)
{
    D("Get object for path: "<<path);

    GoogleDriveObject &ret = this->root;

    path = path.mid(1); // Strip off the initial slash

    while(!path.isEmpty() && ret.isValid()) {
        QString item = path.left(path.indexOf('/'));
        if (path.contains('/')) {
            path = path.mid(path.indexOf('/')+1);
        } else {
            path = "";
        }

        QVector<GoogleDriveObject> children = ret.getChildren();
        D("Got "<<children.size()<<"children");

        GoogleDriveObject childItem;
        for(int idx=0;idx<children.size();idx++) {
//            D("Checking:"<<children.at(idx).getName()<<idx);
            if (children.at(idx).getName()==item) {
//                D("Found!");
                childItem = children.at(idx);
                break;
            }
        }
        ret = childItem;
    }
    D("Returning item:"<<(ret.isValid()?ret.getName():"--NONE--"));
    return ret;
}

void FuseThread::validatePath(QString path)
{
    getObjectForPath(path);
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
    qDebug()<<"In close"<<path;
    return 0;
}

int FuseThread::fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    return static_cast<FuseThread*>(fuse_get_context()->private_data)->read(path,buf,size,offset,fi);
}


