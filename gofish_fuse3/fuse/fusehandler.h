#ifndef FUSEHANDLER_H
#define FUSEHANDLER_H

#include <QObject>
#include <QTimer>
#include <QMap>
#include "google/googledrive.h"
#include "google/googledriveobject.h"
#ifndef FUSE_USE_VERSION
    #define FUSE_USE_VERSION 31
#endif
typedef unsigned long   vsize_t;
typedef long int        register_t;

#include <fuse3/fuse_lowlevel.h>
#include <unistd.h>
#include "cacheentry.h"

class FuseHandler : public QObject
{
    Q_OBJECT
public:
    explicit FuseHandler(int argc, char *argv[], GoogleDrive *gofish, QObject *parent = nullptr);
    enum Operation { ReadDir,Lookup,Read,Write,Release };
    struct InflightOp {
        Operation op;
        qint64 time;
        fuse_req_t req;
        QString name;
        fuse_ino_t inode;
        size_t size;
        off_t off;
        qint64 token;
    };

signals:

public slots:

private:
   struct fuse_lowlevel_ops ops;
    struct fuse_session *session;
    QTimer eventTickTimer;
    QTimer timeOutTimer;
    GoogleDrive *gofish;
    GoogleDriveObject *root;
    QMap<fuse_ino_t,GoogleDriveObject *> inodeToDir;
    QVector<GoogleDriveObject *> connectedObjects;
    QVector<InflightOp> inflightOpList;
    QMap<qint64,InflightOp> inflightOpMap;
    QCache<QString,CacheEntry> *cache;
    int pollDelay;

    void initRoot();
    void initFuse(int argc, char *argv[]);
    qint64 getObjectForPath(QString path);
    void addObjectForInode(GoogleDriveObject *obj);
    void addOp(InflightOp &op);
    void setupStat(GoogleDriveObject *item,struct stat *stbuf);

    void lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
    void readDir (fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
    void getAttr(fuse_req_t req, fuse_ino_t ino,struct fuse_file_info *fi);
    void open(fuse_req_t req, fuse_ino_t ino,struct fuse_file_info *fi);
    void release(fuse_req_t req, fuse_ino_t ino,struct fuse_file_info *fi);
    void read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
    void write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi);
    void create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi);
    // fuse callbacks
    static void fuse_init(void *userdata, struct fuse_conn_info *conn);
    static void fuse_destroy(void *userdata);
    static void fuse_lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
    static void fuse_read_dir (fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
    static void fuse_get_attr(fuse_req_t req, fuse_ino_t ino,struct fuse_file_info *fi);
    static void fuse_open(fuse_req_t req, fuse_ino_t ino,struct fuse_file_info *fi);
    static void fuse_release(fuse_req_t req, fuse_ino_t ino,struct fuse_file_info *fi);
    static void fuse_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
    static void fuse_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi);
    static void fuse_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi);
private slots:
    void eventTick();
    void timeOutTick();
};

bool operator==(const FuseHandler::InflightOp &a,const FuseHandler::InflightOp &b);
QDebug operator<<(QDebug debug, const FuseHandler::InflightOp &o);
QDebug operator<<(QDebug debug, const struct stat &o);

#endif // FUSEHANDLER_H
