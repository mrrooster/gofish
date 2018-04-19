#include <QCoreApplication>
#include <QString>
#include <QThread>
#include <QSettings>
#include <QCommandLineParser>

#include "google/googledrive.h"
#include "fuse/fusethread.h"

#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationDomain("gofish.ohmyno.co.uk");
    QCoreApplication::setApplicationName("GoFiSh");
    QCoreApplication::setApplicationVersion(QString("%1 build date: %2 %3 %4")
                                            .arg("20180409")
                                            .arg(__DATE__)
                                            .arg(QSysInfo::prettyProductName())
                                            .arg(QSysInfo::buildCpuArchitecture())
                                            );

    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    QCommandLineOption clientIdOpt("id","Set the google client ID, you must do this at least once.","Google client ID");
    QCommandLineOption clientSecretOpt("secret","Set the google client secret, you must do this at least once.","Google client secret");
    QCommandLineOption refreshSecondsOpt("refresh-secs","Set the number of seconds between refreshes, the longer this value is the better performance will be, however remote changes may not become visible.","Seconds");
    QCommandLineOption cacheSizeOpt("cache-bytes","Set the size of the in memory block cache in bytes. More memory good.","Bytes");
    QCommandLineOption dloadOpt("download-size","How much data to download in each request, this should be roughly a quater of your total download speed.","Bytes");
    QCommandLineOption foregroundOpt("f","Run in the foreground");
    QCommandLineOption optionsOpt("o","Fuse fs options","Options");

    parser.setApplicationDescription("Gofish is a fuse filesystem for read only access to a google drive. The refresh-secs, cache-bytes, id, secret and download-bytes options are saved to the settings file, and therefore only need to be specified once.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(clientIdOpt);
    parser.addOption(clientSecretOpt);
    parser.addOption(foregroundOpt);
    parser.addOption(cacheSizeOpt);
    parser.addOption(dloadOpt);
    parser.addOption(refreshSecondsOpt);
    parser.addOption(optionsOpt);
    parser.addPositionalArgument("mountpoint","The mountpoint to use");
    parser.process(a);

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
    if (fuseArgs.size()!=1) {
        parser.showHelp(1);
    }
    if (parser.isSet(foregroundOpt)) {
        fuseArgsData.append("-f");
    }
    if (parser.isSet(optionsOpt)) {
        fuseArgsData.append("-o");
        fuseArgsData.append(parser.value(optionsOpt).toLocal8Bit());
    }
    fuseArgsData.append(fuseArgs.first().toLocal8Bit());

    char **fuse_argv = new char*[fuseArgsData.size()];
    int fuse_argc = fuseArgsData.size();
    for(int x=0;x<fuse_argc;x++) {
        fuse_argv[x]=const_cast<char*>(fuseArgsData.at(x).data());
    }

    GoogleDrive googledrive;
    QObject::connect(&googledrive,&GoogleDrive::stateChanged,[&](GoogleDrive::ConnectionState state){
        if (state==GoogleDrive::Connected) {
            qDebug() << "Drive connected, starting fuse thread...";
            FuseThread *thread = new FuseThread(fuse_argc,fuse_argv,&googledrive);
            QObject::connect(thread,&FuseThread::finished,thread,&FuseThread::deleteLater);
            QObject::connect(thread,&FuseThread::finished,&googledrive,&FuseThread::deleteLater);
            thread->start();
        }
    });

    return a.exec();
}
