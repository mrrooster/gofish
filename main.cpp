#include <QCoreApplication>
#include <QThread>

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
    GoogleDrive googledrive(GOOGLE_DRIVE_CLIENT_ID,GOOGLE_DRIVE_CLIENT_SECRET);

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
