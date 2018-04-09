#ifndef GOOGLENETWORKACCESSMANAGER_H
#define GOOGLENETWORKACCESSMANAGER_H

#include <QNetworkAccessManager>

class GoogleNetworkAccessManager : public QNetworkAccessManager
{
public:
    GoogleNetworkAccessManager(QObject *parent = nullptr);
//    ~GoogleNetworkAccessManager();

    virtual QNetworkReply *createRequest(Operation op, const QNetworkRequest &originalReq, QIODevice *outgoingData = Q_NULLPTR) Q_DECL_OVERRIDE;

    void setNextHeaders(QVariantMap headers);
//    QNetworkConfiguration 	activeConfiguration() const;
//    void 	addStrictTransportSecurityHosts(const QVector<QHstsPolicy> &knownHosts);
//    QAbstractNetworkCache *	cache() const;
//    void 	clearAccessCache();
//    void 	clearConnectionCache();
//    QNetworkConfiguration 	configuration() const;
//    void 	connectToHost(const QString &hostName, quint16 port = 80);
//    void 	connectToHostEncrypted(const QString &hostName, quint16 port = 443, const QSslConfiguration &sslConfiguration = QSslConfiguration::defaultConfiguration());
//    QNetworkCookieJar *	cookieJar() const;
//    QNetworkReply *	deleteResource(const QNetworkRequest &request);
//    void 	enableStrictTransportSecurityStore(bool enabled, const QString &storeDir = QString());
//    QNetworkReply *	get(const QNetworkRequest &request);
//    QNetworkReply *	head(const QNetworkRequest &request);
//    bool 	isStrictTransportSecurityEnabled() const;
//    bool 	isStrictTransportSecurityStoreEnabled() const;
//    NetworkAccessibility 	networkAccessible() const;
//    QNetworkReply *	post(const QNetworkRequest &request, QIODevice *data);
//    QNetworkReply *	post(const QNetworkRequest &request, const QByteArray &data);
//    QNetworkReply *	post(const QNetworkRequest &request, QHttpMultiPart *multiPart);
//    QNetworkProxy 	proxy() const;
//    QNetworkProxyFactory *	proxyFactory() const;
//    QNetworkReply *	put(const QNetworkRequest &request, QIODevice *data);
//    QNetworkReply *	put(const QNetworkRequest &request, const QByteArray &data);
//    QNetworkReply *	put(const QNetworkRequest &request, QHttpMultiPart *multiPart);
//    QNetworkRequest::RedirectPolicy 	redirectPolicy() const;
//    QNetworkReply *	sendCustomRequest(const QNetworkRequest &request, const QByteArray &verb, QIODevice *data = Q_NULLPTR);
//    QNetworkReply *	sendCustomRequest(const QNetworkRequest &request, const QByteArray &verb, const QByteArray &data);
//    QNetworkReply *	sendCustomRequest(const QNetworkRequest &request, const QByteArray &verb, QHttpMultiPart *multiPart);
//    void 	setCache(QAbstractNetworkCache *cache);
//    void 	setConfiguration(const QNetworkConfiguration &config);
//    void 	setCookieJar(QNetworkCookieJar *cookieJar);
//    void 	setNetworkAccessible(NetworkAccessibility accessible);
//    void 	setProxy(const QNetworkProxy &proxy);
//    void 	setProxyFactory(QNetworkProxyFactory *factory);
//    void 	setRedirectPolicy(QNetworkRequest::RedirectPolicy policy);
//    void 	setStrictTransportSecurityEnabled(bool enabled);
//    QVector<QHstsPolicy> 	strictTransportSecurityHosts() const;
//    QStringList 	supportedSchemes() const;

//private:
//    QNetworkAccessManager *target;
    QVariantMap nextHeaders;
};

#endif // GOOGLENETWORKACCESSMANAGER_H
