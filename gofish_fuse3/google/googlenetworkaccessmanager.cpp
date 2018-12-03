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
}

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
