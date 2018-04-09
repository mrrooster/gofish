#include "googlenetworkaccessmanager.h"
#include <QNetworkConfiguration>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QAuthenticator>
#include <QNetworkProxy>
#include <QHstsPolicy>
#include <QThread>

#define DEBUG_GOOGLE_NET
#ifdef DEBUG_GOOGLE_NET
#define D(x) qDebug() << QString("[GoogleNetworkAccessManager::%1]").arg(QString::number((quint64)QThread::currentThreadId(),16)).toUtf8().data() << x
#define SD(x) qDebug() << QString("[GoogleNetworkAccessManager::static]") << x
#else
#define D(x)
#define SD(x)
#endif

GoogleNetworkAccessManager::GoogleNetworkAccessManager(QObject *parent) : QNetworkAccessManager(parent)
{
//    this->target = new QNetworkAccessManager(this);

//    connect(target,&QNetworkAccessManager::authenticationRequired,this,&QNetworkAccessManager::authenticationRequired);
//    connect(target,&QNetworkAccessManager::encrypted,this,&QNetworkAccessManager::encrypted);
//    connect(target,&QNetworkAccessManager::finished,this,&QNetworkAccessManager::finished);
//    connect(target,&QNetworkAccessManager::networkAccessibleChanged,this,&QNetworkAccessManager::networkAccessibleChanged);
//    connect(target,&QNetworkAccessManager::preSharedKeyAuthenticationRequired,this,&QNetworkAccessManager::preSharedKeyAuthenticationRequired);
//    connect(target,&QNetworkAccessManager::proxyAuthenticationRequired,this,&QNetworkAccessManager::proxyAuthenticationRequired);
//    connect(target,&QNetworkAccessManager::sslErrors,this,&QNetworkAccessManager::sslErrors);
//    connect(target,&QNetworkAccessManager::objectNameChanged,this,&QNetworkAccessManager::objectNameChanged);
//    connect(target,&QNetworkAccessManager::destroyed,this,&QNetworkAccessManager::destroyed);
}

//GoogleNetworkAccessManager::~GoogleNetworkAccessManager()
//{
//    this->target->deleteLater();
//}

QNetworkReply *GoogleNetworkAccessManager::createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest &originalReq, QIODevice *outgoingData)
{
    D("In createRequest, this->nextHeaders.size:"<<this->nextHeaders.size());
    if (!this->nextHeaders.isEmpty()) {
        QNetworkRequest modifiedRequest(originalReq);
        for (auto it=this->nextHeaders.begin(),end=this->nextHeaders.end();it!=end;it++) {
            D("Adding header:"<<it.key()<<", val:"<<it.value());
            modifiedRequest.setRawHeader(it.key().toUtf8(),it.value().toByteArray());
        }
        this->nextHeaders.clear();
        return QNetworkAccessManager::createRequest(op,modifiedRequest,outgoingData);
    }
    return QNetworkAccessManager::createRequest(op,originalReq,outgoingData);
}

/**
 * @brief GoogleNetworkAccessManager::setNextHeaders
 * @param headers
 *
 * Sets the headers to use for the next get. Used to set the Range header.
 */
void GoogleNetworkAccessManager::setNextHeaders(QVariantMap headers)
{
    D("Setting next headers..");
    this->nextHeaders.clear();
    this->nextHeaders = headers;
}

//QNetworkConfiguration GoogleNetworkAccessManager::activeConfiguration() const
//{
//    return this->target->activeConfiguration();
//}

//void GoogleNetworkAccessManager::addStrictTransportSecurityHosts(const QVector<QHstsPolicy> &knownHosts)
//{
//    this->target->addStrictTransportSecurityHosts(knownHosts);
//}

//QAbstractNetworkCache *GoogleNetworkAccessManager::cache() const
//{
//    return this->target->cache();
//}

//void GoogleNetworkAccessManager::clearAccessCache()
//{
//    this->target->clearAccessCache();
//}

//void GoogleNetworkAccessManager::clearConnectionCache()
//{
//    this->target->clearConnectionCache();
//}

//QNetworkConfiguration GoogleNetworkAccessManager::configuration() const
//{
//    return this->target->configuration();
//}

//void GoogleNetworkAccessManager::connectToHost(const QString &hostName, quint16 port)
//{
//    this->target->connectToHost(hostName,port);
//}

//void GoogleNetworkAccessManager::connectToHostEncrypted(const QString &hostName, quint16 port, const QSslConfiguration &sslConfiguration)
//{
//    this->target->connectToHostEncrypted(hostName,port,sslConfiguration);
//}

//QNetworkCookieJar *GoogleNetworkAccessManager::cookieJar() const
//{
//    return this->target->cookieJar();
//}

//QNetworkReply *GoogleNetworkAccessManager::deleteResource(const QNetworkRequest &request)
//{
//    return this->target->deleteResource(request);
//}

//void GoogleNetworkAccessManager::enableStrictTransportSecurityStore(bool enabled, const QString &storeDir)
//{
//    this->target->enableStrictTransportSecurityStore(enabled,storeDir);
//}

//QNetworkReply *GoogleNetworkAccessManager::get(const QNetworkRequest &request)
//{
//    D("In modified get, this->nextHeaders.size:"<<this->nextHeaders.size());
//    if (!this->nextHeaders.isEmpty()) {
//        QNetworkRequest modifiedRequest(request);
//        QList<QString> keys = this->nextHeaders.keys();
//        for (int idx=0;idx<keys.size();idx++) {
//            QString key = keys.at(idx);
//            modifiedRequest.setRawHeader(key.toUtf8(),this->nextHeaders.take(key).toByteArray());
//        }
//        return this->target->get(modifiedRequest);
//    }
//    return this->target->get(request);
//}

//QNetworkReply *GoogleNetworkAccessManager::head(const QNetworkRequest &request)
//{
//    D("In head.");
//    return this->target->head(request);
//}

//bool GoogleNetworkAccessManager::isStrictTransportSecurityEnabled() const
//{
//    return this->target->isStrictTransportSecurityEnabled();
//}

//bool GoogleNetworkAccessManager::isStrictTransportSecurityStoreEnabled() const
//{
//    return this->target->isStrictTransportSecurityStoreEnabled();
//}

//QNetworkAccessManager::NetworkAccessibility GoogleNetworkAccessManager::networkAccessible() const
//{
//    return this->target->networkAccessible();
//}

//QNetworkReply *GoogleNetworkAccessManager::post(const QNetworkRequest &request, QIODevice *data)
//{
//    D("DFASDFASFASDFASDFASDFASDFASDFASDF");
//    return this->target->post(request,data);
//}

//QNetworkReply *GoogleNetworkAccessManager::post(const QNetworkRequest &request, const QByteArray &data)
//{
//    D("DFASDFASFASDFASDFASDFASDFASDFASDF");
//    return this->target->post(request,data);
//}

//QNetworkReply *GoogleNetworkAccessManager::post(const QNetworkRequest &request, QHttpMultiPart *multiPart)
//{
//    D("DFASDFASFASDFASDFASDFASDFASDFASDF");
//    return this->target->post(request,multiPart);
//}

//QNetworkProxy GoogleNetworkAccessManager::proxy() const
//{
//    return this->target->proxy();
//}

//QNetworkProxyFactory *GoogleNetworkAccessManager::proxyFactory() const
//{
//    return this->target->proxyFactory();
//}

//QNetworkReply *GoogleNetworkAccessManager::put(const QNetworkRequest &request, QIODevice *data)
//{
//    D("DFASDFASFASDFASDFASDFASDFASDFASDF");
//    return this->target->put(request,data);
//}

//QNetworkReply *GoogleNetworkAccessManager::put(const QNetworkRequest &request, const QByteArray &data)
//{
//    D("DFASDFASFASDFASDFASDFASDFASDFASDF");
//    return this->target->put(request,data);
//}

//QNetworkReply *GoogleNetworkAccessManager::put(const QNetworkRequest &request, QHttpMultiPart *multiPart)
//{
//    D("DFASDFASFASDFASDFASDFASDFASDFASDF");
//    return this->target->put(request,multiPart);
//}

//QNetworkRequest::RedirectPolicy GoogleNetworkAccessManager::redirectPolicy() const
//{
//    return this->target->redirectPolicy();
//}

//QNetworkReply *GoogleNetworkAccessManager::sendCustomRequest(const QNetworkRequest &request, const QByteArray &verb, QIODevice *data)
//{
//    D("In sendCustomRequest 1");
//    return this->target->sendCustomRequest(request,verb,data);
//}

//QNetworkReply *GoogleNetworkAccessManager::sendCustomRequest(const QNetworkRequest &request, const QByteArray &verb, const QByteArray &data)
//{
//    D("In sendCustomRequest 2");
//    return this->target->sendCustomRequest(request,verb,data);
//}

//QNetworkReply *GoogleNetworkAccessManager::sendCustomRequest(const QNetworkRequest &request, const QByteArray &verb, QHttpMultiPart *multiPart)
//{
//    D("In sendCustomRequest 3");
//    return this->target->sendCustomRequest(request,verb,multiPart);
//}

//void GoogleNetworkAccessManager::setCache(QAbstractNetworkCache *cache)
//{
//    this->target->setCache(cache);
//}

//void GoogleNetworkAccessManager::setConfiguration(const QNetworkConfiguration &config)
//{
//    this->target->setConfiguration(config);
//}

//void GoogleNetworkAccessManager::setCookieJar(QNetworkCookieJar *cookieJar)
//{
//    this->target->setCookieJar(cookieJar);
//}

//void GoogleNetworkAccessManager::setNetworkAccessible(QNetworkAccessManager::NetworkAccessibility accessible)
//{
//    this->target->setNetworkAccessible(accessible);
//}

//void GoogleNetworkAccessManager::setProxy(const QNetworkProxy &proxy)
//{
//    this->target->setProxy(proxy);
//}

//void GoogleNetworkAccessManager::setProxyFactory(QNetworkProxyFactory *factory)
//{
//    this->target->setProxyFactory(factory);
//}

//void GoogleNetworkAccessManager::setRedirectPolicy(QNetworkRequest::RedirectPolicy policy)
//{
//    this->target->setRedirectPolicy(policy);
//}

//void GoogleNetworkAccessManager::setStrictTransportSecurityEnabled(bool enabled)
//{
//    this->target->setStrictTransportSecurityEnabled(enabled);
//}

//QVector<QHstsPolicy> GoogleNetworkAccessManager::strictTransportSecurityHosts() const
//{
//    return this->target->strictTransportSecurityHosts();
//}

//QStringList GoogleNetworkAccessManager::supportedSchemes() const
//{
//    return this->target->supportedSchemes();
//}
