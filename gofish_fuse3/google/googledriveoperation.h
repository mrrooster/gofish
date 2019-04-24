#ifndef GOOGLEDRIVEOPERATION_H
#define GOOGLEDRIVEOPERATION_H

#include <QUrl>
#include <QVariantMap>
#include <QNetworkReply>
#include <QDebug>

class GoogleDriveOperation
{
public:
    enum HttpOperation { Get,Put,Post,Patch };
    GoogleDriveOperation();

    QUrl url;
    QVariantMap headers;
    std::function<void(QNetworkReply*,bool)> handler;
    std::function<void(QNetworkReply*)> inProgressHandler;
    QByteArray dataToSend;
    int retryCount;
    HttpOperation httpOp;
    QHttpMultiPart *mimeData;
};

QDebug operator<<(QDebug debug, const GoogleDriveOperation &o);

#endif // GOOGLEDRIVEOPERATION_H
