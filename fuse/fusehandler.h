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


class FuseHandler : public QObject
{
    Q_OBJECT
public:
    explicit FuseHandler(int argc, char *argv[], GoogleDrive *gofish, QObject *parent = nullptr);
    enum Operation { ReadDir,Lookup };
    struct InflightOp {
        Operation op;
        qint64 time;
        fuse_req_t req;
        QString name;
        fuse_ino_t inode;
        size_t size;
        off_t off;
        quint64 token;
    };

signals:

public slots:

private:
   struct fuse_lowlevel_ops ops;
    struct fuse_session *session;
    QTimer eventTickTimer;
    GoogleDrive *gofish;
    GoogleDriveObject *root;
    QMap<fuse_ino_t,GoogleDriveObject *> inodeToDir;
    QVector<InflightOp> inflightOpList;
    QMap<quint64,InflightOp> inflightOpMap;
    QCache<QString,QByteArray> *cache;

    void initRoot();
    void initFuse(int argc, char *argv[]);
    quint64 getObjectForPath(QString path);
    void addObjectForInode(GoogleDriveObject *obj);
    void addOp(InflightOp &op);
    void setupStat(GoogleDriveObject *item,struct stat *stbuf);

    void lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
    void readDir (fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
    void getAttr(fuse_req_t req, fuse_ino_t ino,struct fuse_file_info *fi);

    // fuse callbacks
    static void fuse_init(void *userdata, struct fuse_conn_info *conn);
    static void fuse_destroy(void *userdata);
    static void fuse_lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
    static void fuse_read_dir (fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
    static void fuse_get_attr(fuse_req_t req, fuse_ino_t ino,struct fuse_file_info *fi);

private slots:
    void eventTick();
};

bool operator==(const FuseHandler::InflightOp &a,const FuseHandler::InflightOp &b);

#endif // FUSEHANDLER_H
