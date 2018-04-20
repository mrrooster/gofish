#include <stdlib.h>

#include <QCoreApplication>
#include <QString>
#include <QThread>
#include <QSettings>
#include <QCommandLineParser>

#include "google/googledrive.h"
#include "fuse/fusethread.h"
#include "fuse/fusehandler.h"

#include <QDebug>

void messageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);

qint64 g_startTime;
bool g_debug = false;

int main(int argc, char *argv[])
{
    g_startTime = QDateTime::currentMSecsSinceEpoch();
#ifdef QT_DEBUG
    g_debug = true;
#endif
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
    QCommandLineOption clientIdOpt("id","Set the google client ID, you must do this at least once.","Google client ID");
    QCommandLineOption clientSecretOpt("secret","Set the google client secret, you must do this at least once.","Google client secret");
    QCommandLineOption refreshSecondsOpt("refresh-secs","Set the number of seconds between refreshes, the longer this value is the better performance will be, however remote changes may not become visible.","Seconds");
    QCommandLineOption cacheSizeOpt("cache-bytes","Set the size of the in memory block cache in bytes. More memory good.","Bytes");
    QCommandLineOption dloadOpt("download-size","How much data to download in each request, this should be roughly a quater of your total download speed.","Bytes");
    QCommandLineOption foregroundOpt(QStringList({"f","foreground"}),"Run in the foreground");
    QCommandLineOption singleThreadedOpt(QStringList({"m","multi-threaded"}),"Multi threaded fuse.");
    QCommandLineOption optionsOpt(QStringList({"o","options"}),"mount options for fuse, eg: ro,allow_other","Options");
    QCommandLineOption debugOpt(QStringList({"d","debug"}),"Turn on debugging output, implies -f");
    QCommandLineOption helpOpt(QStringList({"h","help"}),"Help. Show this help.");

    parser.setApplicationDescription("Gofish is a fuse filesystem for read only access to a google drive. The refresh-secs, cache-bytes, id, secret and download-bytes options are saved to the settings file, and therefore only need to be specified once.");
    //parser.addHelpOption();
    parser.addOption(helpOpt);
    parser.addVersionOption();
    parser.addOption(optionsOpt);
    parser.addOption(singleThreadedOpt);
    parser.addOption(foregroundOpt);
    parser.addOption(debugOpt);
    parser.addOption(clientIdOpt);
    parser.addOption(clientSecretOpt);
    parser.addOption(cacheSizeOpt);
    parser.addOption(dloadOpt);
    parser.addOption(refreshSecondsOpt);
    parser.addPositionalArgument("mountpoint","The mountpoint to use");

    QStringList args;
    for(int x=0;x<argc;x++) {
        args.append(argv[x]);
    }
    parser.process(args);

    if (!parser.value(clientIdOpt).isEmpty()) {
        QSettings settings;
        settings.beginGroup("googledrive");
        settings.setValue("client_id",parser.value(clientIdOpt));
        settings.setValue("client_secret",parser.value(clientSecretOpt));
    }
    if (!parser.value(refreshSecondsOpt).isNull()) {
        QSettings settings;
        settings.beginGroup("googledrive");
        settings.setValue("refresh_seconds",parser.value(refreshSecondsOpt));
    }
    if (!parser.value(cacheSizeOpt).isNull()) {
        QSettings settings;
        settings.beginGroup("googledrive");
        settings.setValue("in_memory_cache_bytes",parser.value(cacheSizeOpt));
    }
    if (!parser.value(dloadOpt).isNull()) {
        QSettings settings;
        settings.beginGroup("googledrive");
        settings.setValue("download_chunk_bytes",parser.value(dloadOpt));
    }
    if (parser.isSet(debugOpt)) {
        g_debug = true;
    }

    QSettings settings;
    settings.beginGroup("googledrive");

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
    if (fuseArgs.size()!=1 || parser.isSet(helpOpt)) {
        showHelp = true;
    } else {
        fuseArgsData.append("-f");

        if (!parser.isSet(singleThreadedOpt)) {
            qInfo() << "Single threaded FUSE.";
            fuseArgsData.append("-s");
        }

        if (parser.isSet(optionsOpt)) {
            fuseArgsData.append("-o");
            fuseArgsData.append(parser.value(optionsOpt).toLocal8Bit());
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

    GoogleDrive googledrive;
    FuseHandler *fuse;
    QObject::connect(&googledrive,&GoogleDrive::stateChanged,[&](GoogleDrive::ConnectionState state){
        if (state==GoogleDrive::Connected) {
            fuse = new FuseHandler(fuse_argc,fuse_argv,&googledrive);
            /*
            qDebug() << "Drive connected, starting fuse thread...";
            FuseThread *thread = new FuseThread(fuse_argc,fuse_argv,&googledrive);
            QObject::connect(thread,&FuseThread::finished,thread,&FuseThread::deleteLater);
            QObject::connect(thread,&FuseThread::finished,&googledrive,&FuseThread::deleteLater);
            QObject::connect(thread,&FuseThread::finished,&a,&QCoreApplication::quit);
            thread->start();
            */
        }
    });

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
