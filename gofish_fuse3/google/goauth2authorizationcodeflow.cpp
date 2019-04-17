#include "goauth2authorizationcodeflow.h"
#include <QNetworkReply>

GOAuth2AuthorizationCodeFlow::GOAuth2AuthorizationCodeFlow(QNetworkAccessManager *manager, QObject *parent) : QOAuth2AuthorizationCodeFlow(manager,parent)
{

}

void GOAuth2AuthorizationCodeFlow::resetStatus(QAbstractOAuth::Status status)
{
    setStatus(status);
}

QNetworkReply *GOAuth2AuthorizationCodeFlow::patch(const QUrl &url, const QByteArray &data)
{

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    QString auth = QString("Bearer %1").arg(token());
    request.setRawHeader("Authorization", auth.toUtf8());
    QNetworkReply *reply = this->networkAccessManager()->sendCustomRequest(request,"PATCH", data);
    connect(reply, &QNetworkReply::finished, std::bind(&QAbstractOAuth::finished, this, reply));
    return reply;
}
