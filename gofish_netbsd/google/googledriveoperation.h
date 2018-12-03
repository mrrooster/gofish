#ifndef GOOGLEDRIVEOPERATION_H
#define GOOGLEDRIVEOPERATION_H

#include <QUrl>
#include <QVariantMap>
#include <QNetworkReply>

class GoogleDriveOperation
{
public:
    GoogleDriveOperation();

    QUrl url;
    QVariantMap headers;
    std::function<void(QByteArray)> handler;
    int retryCount;
};

#endif // GOOGLEDRIVEOPERATION_H
