#include <QCoreApplication>
#include <QThread>
#include <QSettings>
#include <QCommandLineParser>

#include "google/googledrive.h"
#include "fuse/fusethread.h"
#include "clientid.h"

#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationDomain("gofish.ohmyno.co.uk");
    QCoreApplication::setApplicationName("GoFiSh");
    QCoreApplication::setApplicationVersion(QString("gofish %1 build date: %2 %3 %4")
                                            .arg("20180409")
                                            .arg(__DATE__)
                                            .arg(QSysInfo::prettyProductName())
                                            .arg(QSysInfo::buildCpuArchitecture())
                                            );

    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    QCommandLineOption clientIdOpt("id","Set the google client ID, you must do this at least once.");
    QCommandLineOption clientSecretOpt("secret","Set the google client secret, you must do this at least once.");
    QCommandLineOption refreshSecondsOpt("r","Set the number of seconds between refreshes, the longer this value is the better performance will be, however remote changes may not become visible.");
    QCommandLineOption cacheSizeOpt("c","Set the size of the in memory block cache in bytes. More memory good.");

    parser.setApplicationDescription("Gofish is a fuse filesystem for read only access to a google drive.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(clientIdOpt);
    parser.addOption(clientSecretOpt);

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

    GoogleDrive googledrive();

    QObject::connect(&googledrive,&GoogleDrive::stateChanged,[&](GoogleDrive::ConnectionState state){
        if (state==GoogleDrive::Connected) {
            qDebug() << "Drive connected, starting fuse thread...";
            FuseThread *thread = new FuseThread(argc,argv,&googledrive);
            QObject::connect(thread,&FuseThread::finished,thread,&FuseThread::deleteLater);
            QObject::connect(thread,&FuseThread::finished,&googledrive,&FuseThread::deleteLater);
            thread->start();
        }
    });

    return a.exec();
}
