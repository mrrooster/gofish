#include <stdlib.h>

#include <QCoreApplication>
#include <QString>
#include <QThread>
#include <QSettings>
#include <QCommandLineParser>

#include "google/googledrive.h"
//include "fuse/fusethread.h"
#include "fuse/fusehandler.h"

#include <QDebug>
#include <QDir>

void messageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);
QString byteCountString(qint64 bytes);
QString byteCountString(QString bytes);
qint64 stringToNumber(QString string);

qint64 g_startTime;
bool g_debug = false;

int main(int argc, char *argv[])
{
    g_startTime = QDateTime::currentMSecsSinceEpoch();
    QCoreApplication::setOrganizationDomain("gofish.ohmyno.co.uk");
    QCoreApplication::setApplicationName("GoFiSh");
    QCoreApplication::setApplicationVersion(QString("%1 build date: %2 %3 %4")
                                            .arg("20180424")
                                            .arg(__DATE__)
                                            .arg(QSysInfo::prettyProductName())
                                            .arg(QSysInfo::buildCpuArchitecture())
                                            );

    qInstallMessageHandler(messageOutput);

    QCommandLineParser parser;
    QSettings settings;
    settings.beginGroup("googledrive");
    QString currentTemp = settings.value("temp_dir",QDir::tempPath()).toString();
    QString currentDownloadSize = settings.value("download_chunk_bytes",READ_CHUNK_SIZE).toString();
    QString currentCacheSize = settings.value("in_memory_cache_bytes",DEFAULT_CACHE_SIZE).toString();
    QString currentRefreshSecs = settings.value("refresh_seconds",DEFAULT_REFRESH_SECS).toString();

    QCommandLineOption clientIdOpt("id","Set the google client ID, you must do this at least once.","Google client ID");
    QCommandLineOption clientSecretOpt("secret","Set the google client secret, you must do this at least once.","Google client secret");
    QCommandLineOption refreshSecondsOpt("refresh-secs",QString("Set the number of seconds between directory information refreshes, the longer this value is the better performance will be, however remote changes may not become visible. IF you use the precache-dirs option then setting this option too low will result in constant traffic. Currently: %1 seconds").arg(currentRefreshSecs),"Seconds");
    QCommandLineOption cacheSizeOpt("cache-bytes",QString("Set the size of the in memory block cache in bytes. Genearlly more memory is good. Currently: %1 bytes (%2)").arg(currentCacheSize).arg(byteCountString(currentCacheSize)),"Bytes");
    QCommandLineOption dloadOpt("download-size",QString("How much data to download in each request. Currently: %1 bytes (%2)").arg(currentDownloadSize).arg(byteCountString(currentDownloadSize)),"Bytes");
    QCommandLineOption precacheOpt("precache-dirs","Causes gofish to fetch the enitre directory tree, minimises the initial pause when accessing a directory at the expense of some initial bandwith usage.");
    QCommandLineOption foregroundOpt(QStringList({"f","foreground"}),"Run in the foreground");
    QCommandLineOption optionsOpt(QStringList({"o","options"}),"mount options for fuse, eg: ro,allow_other","Options");
    QCommandLineOption debugOpt(QStringList({"d","debug"}),"Turn on debugging output, implies -f");
    QCommandLineOption tmpOpt(QStringList({"t","temp-dir"}),QString("Sets the location uploaded files are saved to before they are sent to Google, if you do a lot of upload this folder should be large enough to hold the uploaded files. Currently: '%1'").arg(currentTemp),"temp dir");
    QCommandLineOption helpOpt(QStringList({"h","help"}),"Help. Show this help.");

    parser.setApplicationDescription("Gofish is a fuse filesystem for read only access to a google drive. The refresh-secs, cache-bytes, id, secret and download-bytes options are saved to the settings file, and therefore only need to be specified once.\n\nOptions that take a byte value can use a k,m or g suffix for KiB,MiB and GiB respectivly.");
    //parser.addHelpOption();
    parser.addOption(helpOpt);
    parser.addVersionOption();
    parser.addOption(optionsOpt);
    parser.addOption(foregroundOpt);
    parser.addOption(debugOpt);
    parser.addOption(clientIdOpt);
    parser.addOption(clientSecretOpt);
    parser.addOption(cacheSizeOpt);
    parser.addOption(dloadOpt);
    parser.addOption(tmpOpt);
    parser.addOption(refreshSecondsOpt);
    parser.addOption(precacheOpt);
    parser.addPositionalArgument("mountpoint","The mountpoint to use");

    QStringList args;
    for(int x=0;x<argc;x++) {
        args.append(argv[x]);
    }
    parser.process(args);
    bool precacheDirs=parser.isSet(precacheOpt);


    if (!parser.value(clientIdOpt).isEmpty()) {
        settings.setValue("client_id",parser.value(clientIdOpt));
        settings.setValue("client_secret",parser.value(clientSecretOpt));
    }
    if (!parser.value(refreshSecondsOpt).isNull()) {
        settings.setValue("refresh_seconds",stringToNumber(parser.value(refreshSecondsOpt)));
    }
    if (!parser.value(cacheSizeOpt).isNull()) {
        settings.setValue("in_memory_cache_bytes",stringToNumber(parser.value(cacheSizeOpt)));
    }
    if (!parser.value(dloadOpt).isNull()) {
        settings.setValue("download_chunk_bytes",stringToNumber(parser.value(dloadOpt)));
    }
    if (!parser.value(tmpOpt).isNull()) {
        settings.setValue("temp_dir",parser.value(tmpOpt));
    }  
    if (parser.isSet(debugOpt)) {
        g_debug = true;
    }

    if (settings.value("client_id").isNull()) {
        qInfo() << "To use this you need a google API access key that has the 'Google Drive'\n\
API permission. These should be passed to the application using the 'id' \n\
and 'secret' options.";
        return 1;
    }

    QStringList fuseArgs = parser.positionalArguments();
    QVector<QByteArray> fuseArgsData;
    fuseArgsData.append(argv[0]);
    bool showHelp = false;
    bool readOnly = false;
    if (fuseArgs.size()!=1 || parser.isSet(helpOpt)) {
        showHelp = true;
    } else {
     //   fuseArgsData.append("-f");
        if (parser.isSet(optionsOpt)) {
            fuseArgsData.append("-o");
            QString mountOptions = parser.value(optionsOpt);
            fuseArgsData.append(mountOptions.toLocal8Bit());
            readOnly = mountOptions.split(',').contains("ro");
        }
        fuseArgsData.append(fuseArgs.first().toLocal8Bit());
        if (!parser.isSet(foregroundOpt) && !g_debug ) {
            ::daemon(0,0);
        }
    }

    char **fuse_argv = new char*[fuseArgsData.size()];
    int fuse_argc = fuseArgsData.size();
    for(int x=0;x<fuse_argc;x++) {
        fuse_argv[x]=const_cast<char*>(fuseArgsData.at(x).data());
    }

    QCoreApplication a(argc, argv);

    if (showHelp) {
        parser.showHelp(1);
        return 1;
    }

    GoogleDrive googledrive(readOnly,settings.value("temp_dir",QDir::tempPath()).toString());
    FuseHandler *fuse=nullptr;
    QObject::connect(&googledrive,&GoogleDrive::stateChanged,[&](GoogleDrive::ConnectionState state){
        if (state==GoogleDrive::Connected) {
            if (!fuse) {
                fuse = new FuseHandler(fuse_argc,fuse_argv,&googledrive,settings.value("refresh_seconds",DEFAULT_REFRESH_SECS).toLongLong());
            }
        } else if (state==GoogleDrive::ConnectionFailed) {
            QCoreApplication::exit(-1);
        }
    });

    googledrive.setPrecacheDirs(precacheDirs);

    return a.exec();
}

void messageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QByteArray localMsg = msg.toLocal8Bit();

    switch (type) {
    case QtDebugMsg:
        if (g_debug) {
            ::fprintf(stdout, "%s\n",localMsg.constData());
        }
        break;
    case QtInfoMsg:
        ::fprintf(stdout, "%s\n",localMsg.constData());
        break;
    case QtWarningMsg:
        ::fprintf(stderr, "Warning: %s\n", localMsg.constData());
        break;
    case QtCriticalMsg:
        ::fprintf(stderr, "Critical: %s\n", localMsg.constData());
        break;
    case QtFatalMsg:
        ::fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        abort();
    }
    fflush(stderr);
    fflush(stdout);
}

QString byteCountString(qint64 bytes)
{
    QString suffix="B";

    if (bytes>1024*1024*1024) {
        suffix="GiB";
        bytes=bytes/1024/1024/1024;
    } else if (bytes>1024*1024*4) {
        suffix="MiB";
        bytes=bytes/1024/1024;
    } else if (bytes>1234) {
        suffix="KiB";
        bytes=bytes/1024;
    }

    return QString("%0 %1").arg(bytes).arg(suffix);
}

QString byteCountString(QString bytes)
{
    return byteCountString(bytes.toLongLong());
}

qint64 stringToNumber(QString string) {
    qint64 ret=0;
    if (!string.isEmpty()) {
        QString suffix = string.right(1).toLower();
        if (QString("kmgt").contains(suffix)) {
            ret = string.left(string.length()-1).toLongLong() * 1024; // k
            if (suffix=="m") {
                ret *= 1024;
            } else if (suffix=="g") {
                ret *= 1024 * 1024;
            } else if (suffix=="t") {
                ret *= 1024 * 1024 * 1024;
            }
        } else {
            ret = string.toLongLong();
        }
    }
    return ret;
}
