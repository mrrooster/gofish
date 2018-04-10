#ifndef GOOGLENETWORKACCESSMANAGER_H
#define GOOGLENETWORKACCESSMANAGER_H

#include <QNetworkAccessManager>

class GoogleNetworkAccessManager : public QNetworkAccessManager
{
public:
    GoogleNetworkAccessManager(QObject *parent = nullptr);

    virtual QNetworkReply *createRequest(Operation op, const QNetworkRequest &originalReq, QIODevice *outgoingData = Q_NULLPTR) Q_DECL_OVERRIDE;

    void setNextHeaders(QVariantMap headers);

private:
    QVariantMap nextHeaders;
};

#endif // GOOGLENETWORKACCESSMANAGER_H
