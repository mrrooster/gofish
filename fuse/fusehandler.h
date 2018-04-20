#ifndef FUSEHANDLER_H
#define FUSEHANDLER_H

#include <QObject>
#include <QTimer>
#include "google/googledrive.h"
#ifndef FUSE_USE_VERSION
    #define FUSE_USE_VERSION 26
#endif
typedef unsigned long   vsize_t;
typedef long int        register_t;
#include <fuse.h>
#include <fuse_lowlevel.h>
#include <unistd.h>


class FuseHandler : public QObject
{
    Q_OBJECT
public:
    explicit FuseHandler(int argc, char *argv[], GoogleDrive *gofish, QObject *parent = nullptr);

signals:

public slots:

private:
    struct fuse_lowlevel_ops ops;
    struct fuse_session *session;
    QTimer eventTickTimer;

    void initFuse(int argc, char *argv[]);

    void lookup(fuse_req_t req, fuse_ino_t parent, const char *name);

    // fuse callbacks
    static void fuse_init(void *userdata, struct fuse_conn_info *conn);
    static void fuse_destroy(void *userdata);
    static void fuse_lookup(fuse_req_t req, fuse_ino_t parent, const char *name);

private slots:
    void eventTick();
};

#endif // FUSEHANDLER_H
