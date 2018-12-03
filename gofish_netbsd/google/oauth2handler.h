#ifndef OAUTH2HANDLER_H
#define OAUTH2HANDLER_H

#include <QObject>
#include <QOAuth2AuthorizationCodeFlow>
#include <QTimer>

class OAuth2Handler : public QObject
{
    Q_OBJECT
public:
    explicit OAuth2Handler(const QString clientId, const QString clientSecret, QObject *parent = nullptr);

signals:

public slots:

private:
    QOAuth2AuthorizationCodeFlow *auth;
};

#endif // OAUTH2HANDLER_H
