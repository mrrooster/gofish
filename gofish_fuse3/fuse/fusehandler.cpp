#include "fusehandler.h"
#include <poll.h>
#include <errno.h>
#include <QSettings>
#include <QCoreApplication>
#include <QThread>

#include "defaults.h"

#define DEBUG_FUSE
#ifdef DEBUG_FUSE
#define D(x) qDebug() << QString("[FuseHandler %1]").arg(this->pollDelay)   .toUtf8().data() << x
#define SD(x) qDebug() << QString("[FuseHandler::static]") << x
#else
#define D(x)
#define SD(x)
#endif

FuseHandler::FuseHandler(int argc, char *argv[],GoogleDrive *gofish, QObject *parent) : QObject(parent),
	root(nullptr),
    cache(nullptr),
    pollDelay(0)
{
    memset(&this->ops,0,sizeof(this->ops));
    //this->ops.init    = FuseHandler::fuse_init;
    //this->ops.destroy = FuseHandler::fuse_destroy;

    this->ops.lookup  = FuseHandler::fuse_lookup;
    this->ops.readdir = FuseHandler::fuse_read_dir;
    this->ops.getattr = FuseHandler::fuse_get_attr;
    this->ops.open    = FuseHandler::fuse_open;
    this->ops.read    = FuseHandler::fuse_read;
    this->ops.create  = FuseHandler::fuse_create;

    this->gofish = gofish;

    initRoot();
    initFuse(argc,argv);

    connect(&this->eventTickTimer,&QTimer::timeout,this,&FuseHandler::eventTick);
    this->eventTickTimer.setSingleShot(true);
    this->eventTickTimer.start(0);
    connect(&this->timeOutTimer,&QTimer::timeout,this,&FuseHandler::timeOutTick);
    this->timeOutTimer.setSingleShot(false);
    this->timeOutTimer.start(OP_TIMEOUT_MSEC);
}

void FuseHandler::initRoot()
{
    D("Refreshing root folder.");

    if (this->cache == nullptr) {
        QSettings settings;
        settings.beginGroup("googledrive");
        quint64 cacheSize = settings.value("in_memory_cache_bytes",DEFAULT_CACHE_SIZE).toULongLong()/1024;

        if (cacheSize<(CACHE_CHUNK_SIZE/1024)) {
            cacheSize = (CACHE_CHUNK_SIZE/1024);
        }
        qInfo().noquote() << QString("Using in memory cache of %1MiB").arg(cacheSize/1024);
        this->cache = new QCache<QString,QByteArray>(cacheSize);
    }

    if (this->root) {
        QTimer::singleShot(600000,this->root,&QObject::deleteLater);
    }
    this->inodeToDir.clear();
    this->root  = new GoogleDriveObject(
                this->gofish,
                this->cache
    );
    this->root->setInode(1);
    addObjectForInode(this->root);
}

void FuseHandler::initFuse(int argc, char *argv[])
{
    struct fuse_args args;

    args.argc=argc-1;
    args.argv=argv;
    args.allocated=0;

    D("Creating fuse session...");
    this->session = fuse_session_new(&args,&this->ops,sizeof(this->ops),this);

    Q_ASSERT(this->session);
    D("Mounting: "<<argv[argc-1]);
    int ret = fuse_session_mount(this->session,argv[argc-1]);
    if (ret!=0) {
        D("Mount failed!"<<ret);
        QCoreApplication::exit(-ret);
    }
    D("Mout returned: "<<ret);
}

void FuseHandler::addObjectForInode(GoogleDriveObject *obj)
{
    connect(obj,&GoogleDriveObject::destroyed,[=](){
        if (this->inodeToDir.value(obj->getInode())==obj) {
            D("Removing inode"<<obj->getInode()<<obj);
            this->inodeToDir.remove(obj->getInode());
        }
        this->connectedObjects.removeOne(obj);
    });
//    D("Adding obj with inode:"<<obj->getInode());
    this->inodeToDir.remove(obj->getInode());
    this->inodeToDir.insert(obj->getInode(),obj);
    if (!this->connectedObjects.contains(obj)) {
        this->connectedObjects.append(obj);
        connect(obj,&GoogleDriveObject::children,this,[=](QVector<GoogleDriveObject*> children,quint64 requestToken){
            D("In children handler, token:"<<requestToken<<obj->getPath());
            if (this->inflightOpMap.contains(requestToken)) {
                InflightOp op = this->inflightOpMap.take(requestToken);
                this->inflightOpList.removeOne(op);

                for(GoogleDriveObject *child : children) {
                    this->addObjectForInode(child);
                }
                if (op.op==ReadDir) {
                    D("ReadDir op"<<op);
                    qint64 size = 0;
                    struct stat stbuf;
                    size += fuse_add_direntry(op.req,nullptr,0,".",nullptr,0);
                    size += fuse_add_direntry(op.req,nullptr,0,"..",nullptr,0);
                    for(GoogleDriveObject *child : children) {
                        QByteArray nameData = child->getName().toUtf8();
                        size += fuse_add_direntry(op.req,nullptr,0,nameData.data(),nullptr,0);
                    }
                    char *dirBuff = new char[size];
                    memset(dirBuff,0,size);
                    quint64 off=0;
                    stbuf.st_ino = obj->getInode();
                    // FIX ALL THIS
                    off += fuse_add_direntry(op.req,dirBuff+off,size-off,".",&stbuf,1);
                    off += fuse_add_direntry(op.req,dirBuff+off,size-off,"..",&stbuf,2);
                    for(GoogleDriveObject *child : children) {
                        QByteArray nameData = child->getName().toUtf8();
                        setupStat(child,&stbuf);
                        int len = fuse_add_direntry(op.req,nullptr,0,nameData.data(),nullptr,0);
                        //D("Fuse_add_dir: off:"<<off<<"size:"<<size<<"len:"<<len<<"name:"<<nameData);
                        off += fuse_add_direntry(op.req,dirBuff+off,size-off,nameData.data(),&stbuf,off+len);
                    }
                    if (op.off>=size) {
                        fuse_reply_buf(op.req,nullptr,0);
                       // D("Sending empty entry.");
                    } else {
                        fuse_reply_buf(op.req,dirBuff+op.off,((size-op.off)<op.size?size-op.off:op.size));
                       // D("fuse_reply_buf: "<<"buff+"<<op.off<<((size-op.off)<op.size?(size-op.off):op.size));
                    }
                    delete[] dirBuff;
                } else if (op.op==Lookup) {
                    D("Lookup for op:"<<op);
                    for(GoogleDriveObject *child : children) {
                        if (child->getName()==op.name) {
                            D("Entry found");
                            struct fuse_entry_param ent;
                            memset(&ent,0,sizeof(ent));
                            ent.ino = child->getInode();
                            ent.generation = 1;
                            setupStat(child,&ent.attr);
                            ent.attr_timeout=60;
                            ent.entry_timeout=60;
                            fuse_reply_entry(op.req,&ent);
                            return;
                        }
                    }
                    fuse_reply_err(op.req, ENOENT);
                }
            }
            D("Leaving child handler");
        });
        connect(obj,&GoogleDriveObject::readResponse,[=](QByteArray data,quint64 requestToken){
            D("Read resopnse, token:"<<requestToken);
            if (this->inflightOpMap.contains(requestToken)) {
                D("Found inflight response.");
                InflightOp op = this->inflightOpMap.take(requestToken);
                this->inflightOpList.removeOne(op);
                D("Reply with data"<<data.size());
                fuse_reply_buf(op.req,data.data(),data.size());
            }
        });
    }
}

void FuseHandler::addOp(FuseHandler::InflightOp &op)
{
    D("Add op:"<<op);
    this->inflightOpList.append(op);
    this->inflightOpMap.insert(op.token,op);
}

void FuseHandler::setupStat(GoogleDriveObject *item, struct stat *stbuf)
{
    ::memset(stbuf,0,sizeof(struct stat));

    stbuf->st_mode= 0555;
    if (item->isFolder()) {
//        D("Entry is directory:"<<item->getName()<<", children:"<<item->getChildFolderCount());
        stbuf->st_mode |= S_IFDIR;
        stbuf->st_size = 512;
        stbuf->st_nlink = 2 + item->getChildFolderCount();
    } else {
      //  D("Thingy is regular file.");
        stbuf->st_mode |= S_IFREG;
        stbuf->st_size = item->getSize();
        stbuf->st_nlink = 1;
    }
    //stbuf->st_rdev = S_IFCHR;
    stbuf->st_ctime= item->getCreatedTime().toTime_t();
    stbuf->st_mtime= item->getCreatedTime().toTime_t();
    stbuf->st_ino  = item->getInode();
    stbuf->st_uid = ::getuid();
    stbuf->st_gid = ::getgid();
//    D("setupStat:"<<*stbuf);
}

void FuseHandler::lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    D("In fuse lookup."<<parent<<name);
    InflightOp op;
    op.req = req;
    op.time = QDateTime::currentMSecsSinceEpoch();
    op.name = name;
    op.inode = parent;
    GoogleDriveObject *dir = this->inodeToDir.value(parent);
    if (!dir) {
        D("Lookup fail!");
        fuse_reply_err(req, ENOENT);
        return;
    }
    op.token = dir->getChildren();
    op.op = Lookup;
    addOp(op);
}

void FuseHandler::readDir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, fuse_file_info *fi)
{
    D("In fuse readdir"<<ino<<size<<off);
    InflightOp op;
    op.req = req;
    op.time = QDateTime::currentMSecsSinceEpoch();
    op.size = size;
    op.off = off;
    op.inode = ino;
    GoogleDriveObject *dir = this->inodeToDir.value(ino);
    if (!dir) {
        D("Error!");
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    op.token = dir->getChildren();
    op.op = ReadDir;
    addOp(op);
}

void FuseHandler::getAttr(fuse_req_t req, fuse_ino_t ino, fuse_file_info *fi)
{
    D("getAttr, Inode:"<<ino);

    GoogleDriveObject *item = this->inodeToDir.value(ino);
    if (!item) {
        D("Returning ENOENT, for inode:"<<ino);
        fuse_reply_err(req, ENOENT);
        return;
    }

    struct stat stbuf;
    setupStat(item,&stbuf);
    fuse_reply_attr(req,&stbuf,DEFAULT_REFRESH_SECS);
    D("Leaving getattr:"<<ino);
}

void FuseHandler::open(fuse_req_t req, fuse_ino_t ino, fuse_file_info *fi)
{
    GoogleDriveObject *item = this->inodeToDir.value(ino);
    bool write = (fi->flags & O_WRONLY) | (fi->flags & O_RDWR);

    D("Open inode"<<ino<<write);
    if (!item && !write) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    fuse_reply_open(req,fi);
    qint64 off = item->getSize() - (CACHE_CHUNK_SIZE*3);
    if (off<0) {
        off=0;
    }
    //item->read(off,item->getSize()-off);
    //item->read(0,(CACHE_CHUNK_SIZE*3));
}

void FuseHandler::read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, fuse_file_info *fi)
{
    GoogleDriveObject *item = this->inodeToDir.value(ino);
    if (!item) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    D("Read inode"<<ino<<",size:"<<size<<",off:"<<off<<",name :"<<item->getName());
    InflightOp op;
    op.req = req;
    op.time = QDateTime::currentMSecsSinceEpoch();
    op.size = size;
    op.off = off;
    op.inode = ino;
    op.token = item->read(off,size);
    op.op = Read;
    addOp(op);
}

void FuseHandler::write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, fuse_file_info *fi)
{
    D("In write");
}

void FuseHandler::create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, fuse_file_info *fi)
{
    D("In create");
    GoogleDriveObject *parentItem = this->inodeToDir.value(parent);
    if (!parentItem) {
        fuse_reply_err(req,ENOENT);
        return;
    }
    //GoogleDriveObject(GoogleDrive *gofish, QString id, QString path, QString name, QString mimeType, quint64 size, QDateTime ctime, QDateTime mtime, QCache<QString,QByteArray> *cache,QObject *parent=nullptr);
    GoogleDriveObject *item = parentItem->create(name);
    item->open();
    addObjectForInode(item);
    struct fuse_entry_param ent;
    memset(&ent,0,sizeof(ent));
    ent.ino = item->getInode();
    ent.generation = 1;
    setupStat(item,&ent.attr);
    ent.attr_timeout=60;
    ent.entry_timeout=60;
    fuse_reply_create(req,&ent,fi);
}

void FuseHandler::fuse_init(void *userdata,struct fuse_conn_info *conn)
{
    SD("In fuse_init");
}

void FuseHandler::fuse_destroy(void *userdata)
{
    SD("In fuse destroy");
}

void FuseHandler::fuse_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    reinterpret_cast<FuseHandler*>(fuse_req_userdata(req))->lookup(req,parent,name);
}

void FuseHandler::fuse_read_dir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, fuse_file_info *fi)
{
    reinterpret_cast<FuseHandler*>(fuse_req_userdata(req))->readDir(req,ino,size,off,fi);
}

void FuseHandler::fuse_get_attr(fuse_req_t req, fuse_ino_t ino, fuse_file_info *fi)
{
    reinterpret_cast<FuseHandler*>(fuse_req_userdata(req))->getAttr(req,ino,fi);
}

void FuseHandler::fuse_open(fuse_req_t req, fuse_ino_t ino, fuse_file_info *fi)
{
    reinterpret_cast<FuseHandler*>(fuse_req_userdata(req))->open(req,ino,fi);
}

void FuseHandler::fuse_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, fuse_file_info *fi)
{
    reinterpret_cast<FuseHandler*>(fuse_req_userdata(req))->read(req,ino,size,off,fi);
}

void FuseHandler::fuse_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, fuse_file_info *fi)
{
    reinterpret_cast<FuseHandler*>(fuse_req_userdata(req))->write(req,ino,buf,size,off,fi);
}

void FuseHandler::fuse_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, fuse_file_info *fi)
{
    reinterpret_cast<FuseHandler*>(fuse_req_userdata(req))->create(req,parent,name,mode,fi);
}

void FuseHandler::eventTick()
{
    int fd = fuse_session_fd(this->session);
    struct pollfd fds[1];

    fds[0].fd = fd;
    fds[0].events = POLLIN;
    fds[0].revents= 0;
    this->pollDelay += (this->pollDelay)<1000?1:0;
    if (::poll((struct pollfd*)&fds,1,0)>0) {
        struct fuse_buf buff;

        memset(&buff,0,sizeof(buff));

        int ret = fuse_session_receive_buf(this->session,&buff);
        fuse_session_process_buf(this->session,&buff);
        ::free(buff.mem);
        this->pollDelay=0;
    }
    QThread::yieldCurrentThread();
    this->eventTickTimer.start(this->pollDelay/10);
}

void FuseHandler::timeOutTick()
{
    qint64 time = QDateTime::currentMSecsSinceEpoch()-OP_TIMEOUT_MSEC;
    QVector<InflightOp> toDelete;
    for(auto it=this->inflightOpList.begin();it!=this->inflightOpList.end();it++) {
        if ((*it).time<time) {
            toDelete.append((*it));
            fuse_reply_err((*it).req, EIO);
        }
    }
    for(auto it=toDelete.begin();it!=toDelete.end();it++) {
        InflightOp op = this->inflightOpMap.take((*it).token);
        this->inflightOpList.removeOne(op);
    }
}

bool operator==(const FuseHandler::InflightOp &a, const FuseHandler::InflightOp &b)
{
    bool ret = (a.op==b.op && a.time==b.time && a.token==b.token);
    if (ret) {
        if (a.op==FuseHandler::ReadDir) {
            ret =  (a.inode == b.inode && a.off == b.off && a.size == b.size);
        }
    }
    return ret;
}

QDebug operator<<(QDebug debug, const FuseHandler::InflightOp &o)
{
    QDebugStateSaver s(debug);

    /*
     *         Operation op;
        qint64 time;
        fuse_req_t req;
        QString name;
        fuse_ino_t inode;
        size_t size;
        off_t off;
        quint64 token;
        */
    debug.nospace()
            << "[Op:" << (o.op == FuseHandler::ReadDir?"ReadDir": o.op==FuseHandler::Lookup ? "Lookup" : "Read")
            << ", Time: "<< o.time
            << ", Name: "<< o.name
            << ", Inode: "<< o.inode
            << ", Size: "<<o.size
            << ", off: "<< o.off
            << ", Token: "<<o.token << "]";
    return debug;
}

QDebug operator<<(QDebug debug, const struct stat &o)
{
    QDebugStateSaver s(debug);
    debug.nospace()
            << "[Stat:"
            << ", inode:"<<o.st_ino
            << ", mode:"<<o.st_mode
            << ", size:"<<o.st_size
            << ", links:"<<o.st_nlink
            << "]";
    return debug;
}
