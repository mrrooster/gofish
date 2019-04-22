#ifndef GOAUTH2AUTHORIZATIONCODEFLOW_H
#define GOAUTH2AUTHORIZATIONCODEFLOW_H

#include <QOAuth2AuthorizationCodeFlow>

class GOAuth2AuthorizationCodeFlow : public QOAuth2AuthorizationCodeFlow
{
public:
    GOAuth2AuthorizationCodeFlow(QNetworkAccessManager *manager, QObject *parent = nullptr);
public:
    void resetStatus(QAbstractOAuth::Status status);

    virtual QNetworkReply *patch(const QUrl &url, const QByteArray &data);
    virtual QNetworkReply *patch(const QUrl &url,  QHttpMultiPart *data);
};

#endif // GOAUTH2AUTHORIZATIONCODEFLOW_H
