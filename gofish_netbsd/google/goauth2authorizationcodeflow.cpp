#include "goauth2authorizationcodeflow.h"

GOAuth2AuthorizationCodeFlow::GOAuth2AuthorizationCodeFlow(QNetworkAccessManager *manager, QObject *parent) : QOAuth2AuthorizationCodeFlow(manager,parent)
{

}

void GOAuth2AuthorizationCodeFlow::resetStatus(QAbstractOAuth::Status status)
{
    setStatus(status);
}
