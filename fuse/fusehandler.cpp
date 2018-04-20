#include "fusehandler.h"
#include <poll.h>

#define DEBUG_FUSE
#ifdef DEBUG_FUSE
#define D(x) qDebug() << QString("[FuseHandler::%1]").arg(QString::number((quint64)QThread::currentThreadId(),16)).toUtf8().data() << x
#define SD(x) qDebug() << QString("[FuseHandler::static]") << x
#else
#define D(x)
#define SD(x)
#endif

FuseHandler::FuseHandler(int argc, char *argv[],GoogleDrive *gofish, QObject *parent) : QObject(parent)
{
    this->ops.init    = FuseHandler::fuse_init;
    this->ops.destroy = FuseHandler::fuse_destroy;

    this->ops.lookup  = FuseHandler::fuse_lookup;

    initFuse(argc,argv);

    connect(&this->eventTickTimer,&QTimer::timeout,this,&FuseHandler::eventTick);
    this->eventTickTimer.setSingleShot(false);
    this->eventTickTimer.start(0);
}

void FuseHandler::initFuse(int argc, char *argv[])
{
    struct fuse_args args;

    args.argc=argc;
    args.argv=argv;
    args.allocated=0;

    D("Creating fuse session...");
    this->session = fuse_session_new(&args,&this->ops,sizeof(struct fuse_lowlevel_ops),this);

    D("Mounting: "<<argv[argc-1]);
    fuse_session_mount(this->session,argv[argc-1]);
}

void FuseHandler::lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    D("In fuse lookup."<<parent<<name);
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
    SD("In fuse lookup");
    reinterpret_cast<FuseHandler*>(fuse_req_userdata(req))->lookup(req,parent,name);
}

void FuseHandler::eventTick()
{
    int fd = fuse_session_fd(this->session);
    struct pollfd fds[1];

    fds[0].fd = fd;
    fds[0].events = POLLIN;
    fds[0].revents= 0;
    if (::poll(&fds,1,0)>0) {
        struct fuse_buf buff;
        fuse_session_receive_buff(this->session,&buff);
        fuse_session_process_buff(this->session,&buff);
    }
}
