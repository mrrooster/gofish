#include "oauth2handler.h"
#include <QtNetworkAuth>
#include <QDesktopServices>
#include <QDebug>
const QUrl files_list("https://www.googleapis.com/drive/v3/files");

OAuth2Handler::OAuth2Handler(const QString clientId, const QString clientSecret, QObject *parent) : QObject(parent)
{
    this->auth    = new QOAuth2AuthorizationCodeFlow(this);
    auto *handler = new QOAuthHttpServerReplyHandler(this);
    this->auth->setReplyHandler(handler);
    this->auth->setAuthorizationUrl(QUrl("https://accounts.google.com/o/oauth2/v2/auth"));
    this->auth->setAccessTokenUrl(QUrl("https://www.googleapis.com/oauth2/v4/token"));
    this->auth->setClientIdentifier(clientId);
    this->auth->setClientIdentifierSharedKey(clientSecret);
    this->auth->setRefreshToken("1/MSfdBr9qgbUrgtP0okwdlcbNsIOxCr46JUhBF_QvqcY");
    this->auth->setModifyParametersFunction([&](QAbstractOAuth::Stage stage, QVariantMap *parameters) {
        if (stage == QAbstractOAuth::Stage::RefreshingAccessToken) {
            parameters->insert("client_id",clientId);
            parameters->insert("client_secret",clientSecret);
            parameters->remove("redirect_uri");
            qDebug()<<"Rewritten refresh thingy...";
        }
    });

    connect(handler,&QOAuthHttpServerReplyHandler::tokensReceived,[=](QVariantMap tokens){
        qDebug()<<"Got tokens." << tokens;
        this->auth->setToken(tokens.value("access_token").toString());
        auto response = this->auth->get(files_list);
        connect(response,&QNetworkReply::finished,[=](){
            qDebug()<<"Response:"<<response->readAll();
        });
    });

    connect(this->auth, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, [](const QUrl url) {
        qDebug() <<"Go to: "<<url.toString();
    });
    connect(this->auth, &QOAuth2AuthorizationCodeFlow::statusChanged, [=](
            QAbstractOAuth::Status status) {
        qDebug()<<"Oauth status:"<<(int)status;
        if (status==QAbstractOAuth::Status::Granted){
           qDebug() << "Authorised. Starting token refresh timer.";
           qDebug() << "Token expires at:" <<this->auth->expirationAt().toString();
           qDebug() << "Refresh token: "<<this->auth->refreshToken();

           auto response = this->auth->get(files_list);
           connect(response,&QNetworkReply::finished,[=](){
               qDebug()<<"Response:"<<response->readAll();
           });


           //this->refreshTokenTimer.start(10000);
        }
    });

    this->auth->setScope("https://www.googleapis.com/auth/drive.readonly");
    //this->auth->grant();
    this->auth->refreshAccessToken();
}
