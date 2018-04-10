#ifndef FUSETHREAD_H
#define FUSETHREAD_H

#include <QThread>
#include <QMap>
#include "google/googledrive.h"
#include "google/googledriveobject.h"

#define FUSE_USE_VERSION 26
typedef unsigned long   vsize_t;
typedef long int        register_t;
#include <fuse.h>
#include <unistd.h>

class FuseThread : public QThread
{
    Q_OBJECT
public:
    explicit FuseThread(int argc, char *argv[], GoogleDrive *gofish, QObject *parent = nullptr);

    void run() Q_DECL_OVERRIDE;

    int getAttr(const char *path, struct stat *stbuf);

    int openDir(const char *path, struct fuse_file_info *fi);
    int readDir(const char *path, void *buf,fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
    int closeDir(const char *path, struct fuse_file_info *fi);

    int open(const char *path, struct fuse_file_info *fi);
    int read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi);


public slots:

private:
    int inode;
    int argc;
    char **argv;
    uid_t user;
    gid_t group;
    GoogleDrive *gofish;
    GoogleDriveObject root;

    GoogleDriveObject getObjectForPath(QString path);
    void validatePath(QString path);

    static int fuse_getattr(const char *path, struct stat *stbuf);

    static int fuse_opendir(const char *path, struct fuse_file_info *fi);
    static int fuse_closedir(const char *path, struct fuse_file_info *fi);
    static int fuse_readdir(const char *path, void *buf,fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);

    static int fuse_open(const char *path, struct fuse_file_info *fi);
    static int fuse_close(const char *path, struct fuse_file_info *fi);
    static int fuse_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi);
};

#endif // FUSETHREAD_H
