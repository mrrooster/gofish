#include "googledrive.h"
#include <QtNetworkAuth>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonValue>
#include <iostream>
#include "googledriveobject.h"
#include "googlenetworkaccessmanager.h"
#include "goauth2authorizationcodeflow.h"
#include "defaults.h"

#define DEBUG_GOOGLEDRIVE
#ifdef DEBUG_GOOGLEDRIVE
#define D(x) qDebug() << QString("[GoogleDrive::%1]").arg(QString::number((quint64)this,16)).toUtf8().data() << x
#else
#define D(x)
#endif

const QUrl auth_url("https://accounts.google.com/o/oauth2/v2/auth");
const QUrl access_token_url("https://www.googleapis.com/oauth2/v4/token");

const QUrl files_list("https://www.googleapis.com/drive/v3/files");
const QUrl files_info("https://www.googleapis.com/drive/v3/files/");

GoogleDrive::GoogleDrive(QObject *parent) : QObject(parent),auth(nullptr),state(Disconnected)
{

    //Operation timer
    connect(&this->operationTimer,&QTimer::timeout,[=]() {
        if (this->queuedOps.isEmpty()) {
            this->operationTimer.stop();
        } else {
            QMutexLocker lock(&this->oAuthLock);
            GoogleDriveOperation *op = this->queuedOps.takeFirst();
            if (!op->headers.isEmpty()) {
                dynamic_cast<GoogleNetworkAccessManager*>(this->auth->networkAccessManager())->setNextHeaders(op->headers);
            }
            auto response = this->auth->get(op->url);
            connect(response,&QNetworkReply::finished,[=]{
                QByteArray responseData = response->readAll();
                if (response->error()==QNetworkReply::NoError && !responseData.isEmpty()) {
                    op->handler(responseData);
                    delete op;
                } else {
                    D("Error: "<<response->errorString());
                    op->retryCount++;
                    if (op->retryCount>10) {
                        D("Abandoning request due to >10 retries.");
                        delete op;
                    } else {
                        this->queuedOps.append(op);
                    }
            	    this->operationTimer.start();
                }
                response->deleteLater();
            });
            this->operationTimer.start();
        }
    });

    connect(&this->refreshTokenTimer,&QTimer::timeout,[=]() {
        D("Calling token refresh...");
        authenticate();
    });
    authenticate();
}

GoogleDrive::~GoogleDrive()
{
    auto keys = this->blockingLocks.keys();
    while(!keys.isEmpty()) {
        delete this->blockingLocks.take(keys.first());
    }
    keys = this->fileLocks.keys();
    while(!keys.isEmpty()) {
        delete this->fileLocks.take(keys.first());
    }
}

void GoogleDrive::readRemoteFolder(QString path, QString parentId, GoogleDriveObject *target)
{
    D("read remote folder."<<path);
    if (this->inflightPaths.contains(path)) {
        D("Another request in progress.");
        return;
    }
    if (this->inflightValues.contains(path)) {
        this->inflightValues.value(path)->clear();
    } else {
        this->inflightValues.insert(path,new QVector<QVariantMap>());
    }
    QMutex *mtex = getBlockingLock(path);
    mtex->lock();
    this->inflightPaths.append(path);
    D("Removing path from preflight: "<<path<<mtex);
    this->preflightPaths.removeOne(path);
    D("read remote folder.. locked."<<path);
    readFolder(path,"",parentId,target);
}

QMutex *GoogleDrive::getBlockingLock(QString folder)
{
    if (!this->blockingLocks.contains(folder)) {
        this->blockingLocks.insert(folder,new QMutex());
    }
    return this->blockingLocks.value(folder);
}

QMutex *GoogleDrive::getFileLock(QString fileId)
{
    if (!this->fileLocks.contains(fileId)) {
        this->fileLocks.insert(fileId,new QMutex());
    }
    return this->fileLocks.value(fileId);
}

void GoogleDrive::addPathToPreFlightList(QString path)
{
    QMutexLocker lock(&this->preflightLock);
    if (!this->preflightPaths.contains(path)) {
        D("Adding path to preflight: "<<path);
        this->preflightPaths.append(path);
    }
}

bool GoogleDrive::pathInPreflight(QString path)
{
    QMutexLocker lock(&this->preflightLock);
    return this->preflightPaths.contains(path);
}

bool GoogleDrive::pathInFlight(QString path)
{
    return this->inflightPaths.contains(path);
}

QByteArray GoogleDrive::getPendingSegment(QString fileId, quint64 start, quint64 length)
{
    QString id = QString("%1:%2:%3").arg(fileId).arg(start).arg(length);
    if (this->pendingSegments.contains(id)) {
        return this->pendingSegments.take(id);
    }
    return QByteArray();
}

unsigned int GoogleDrive::getRefreshSeconds()
{
    QSettings settings;
    settings.beginGroup("googledrive");
    return settings.value("refresh_seconds",3600).toUInt();
}

quint64 GoogleDrive::getInMemoryCacheSizeBytes()
{
    QSettings settings;
    settings.beginGroup("googledrive");
    return settings.value("in_memory_cache_bytes",DEFAULT_CACHE_SIZE).toULongLong();
}

void GoogleDrive::getFileContents(QString fileId, quint64 start, quint64 length)
{
    QString id = QString("%1:%2:%3").arg(fileId).arg(start).arg(length);
    D("read remote file."<<id);


    getBlockingLock(id)->lock();
    this->inflightPaths.append(id);
    D("Removing path from preflight: "<<id);
    this->preflightPaths.removeOne(id);
    D("read remote file.. locked."<<id);
    //readFolder(path,path,"","");
    readFileSection(fileId,start,length);
    return;
}

void GoogleDrive::readFolder(QString startPath, QString nextPageToken, QString parentId, GoogleDriveObject *target)
{
    D("readFolder: startPath:"<<startPath<<", parentId: "<<parentId<<", Next page token:"<<nextPageToken);
    QUrl url = files_list;

//    QString query = (dir.isEmpty())?
    QString queryString = QString("'%1' in parents").arg(parentId);
    if (startPath=="/") {
        queryString = "('root' in parents or sharedWithMe = true)";
    }

    QString query = QString("q=%1").arg(QString(QUrl::toPercentEncoding(queryString," '")).replace(" ","+"));
    if (!nextPageToken.isEmpty()) {
        query += QString("&pageToken=%1").arg(QString(QUrl::toPercentEncoding(nextPageToken," '")).replace(" ","+"));
    }
    query += "&fields=files(name,size,mimeType,id,kind,createdTime,modifiedTime)";
    url.setQuery(query);
    queueOp(QPair<QUrl,QVariantMap>(url,QVariantMap()),[=](QByteArray responseData){
        //D("Read file response data:"<<responseData);
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        //D("Got response:"<<doc);

        if (doc.isObject()) {
            if (doc["files"].isArray()) {
                auto files = doc["files"].toArray();

                for(int idx=0;idx<files.size();idx++) {
                    const QJsonValue file = files[idx];
                    QVariantMap fileInfo;
                    fileInfo.insert("id",file["id"].toVariant());
                    fileInfo.insert("name",file["name"].toVariant());
                    fileInfo.insert("mimeType",file["mimeType"].toVariant());
                    fileInfo.insert("size",file["size"].toVariant());
                    fileInfo.insert("createdTime",file["createdTime"].toVariant());
                    fileInfo.insert("modifiedTime",file["modifiedTime"].toVariant());
                    this->inflightValues.value(startPath)->append(fileInfo);
                }
                if (!doc["nextPageToken"].isString()) {
                    this->inflightPaths.removeAll(startPath);
                    QVector<GoogleDriveObject*> newChildren;
                    for(auto it = this->inflightValues.value(startPath)->begin(),end = this->inflightValues.value(startPath)->end(); it!=end; it++) {
                        QVariantMap file = (*it);
        //                D("File: "<<file);

                        GoogleDriveObject *newObj = new GoogleDriveObject(
                                    this,
                                    target->getRefreshSecs(),
                                    file["id"].toString(),
                                    startPath,
                                    file["name"].toString(),
                                    file["mimeType"].toString(),
                                    file["size"].toULongLong(),
                                    QDateTime::fromString(file["createdTime"].toString(),"yyyy-MM-dd'T'hh:mm:ss.z'Z'"),
                                    QDateTime::fromString(file["modifiedTime"].toString(),"yyyy-MM-dd'T'hh:mm:ss.z'Z'"),
                                    target->getCache()
                                   );
                        newChildren.append(newObj);
                    }
                    target->setChildren(newChildren);
                    QMutex *mtex = getBlockingLock(startPath);
                    mtex->unlock();
                    D("Releasing lock (drive)"<<startPath<<mtex);
                    delete this->inflightValues.take(startPath);
                    return;
                } else {
                    getBlockingLock(startPath)->unlock();
                }
            }
            if (doc["nextPageToken"].isString()) {
                D("Has next page! :"<<doc["nextPageToken"].toString());
                this->readFolder(startPath,doc["nextPageToken"].toString(),parentId,target);
            } else {
                getBlockingLock(startPath)->unlock();
            }
        }
    });
}
void GoogleDrive::readFileSection(QString fileId, quint64 start, quint64 length)
{
    QString id = QString("%1:%2:%3").arg(fileId).arg(start).arg(length);
    QUrl url = files_info.toString()+fileId;
    QVariantMap extraHeaders;

    url.setQuery("alt=media");

    D("Get remote file :"<<url);

    extraHeaders.insert("Range",QString("bytes=%1-%2").arg(start).arg(start+length-1));

    queueOp(QPair<QUrl,QVariantMap>(url,extraHeaders),[=](QByteArray responseData){
        this->pendingSegments.insert(id,responseData);
        D("Got file section response."<<responseData.size()<<responseData.left(200));
        this->inflightPaths.removeOne(id);
        D("Releasing lock (file)"<<id);
        getBlockingLock(id)->unlock();
    });
}

void GoogleDrive::queueOp(QPair<QUrl, QVariantMap> urlAndHeaders, std::function<void(QByteArray)> handler)
{
    GoogleDriveOperation *operation = new GoogleDriveOperation();

    operation->url        = urlAndHeaders.first;
    operation->headers    = urlAndHeaders.second;
    operation->handler    = handler;
    operation->retryCount = 0;
    this->queuedOps.append(operation);
    if (!this->operationTimer.isActive()) {
        this->operationTimer.start(10);
    }
}

quint64 GoogleDrive::getInodeForFileId(QString id)
{
    if (!this->inodeMap.contains(id)) {
        this->inodeMap.insert(id,this->inode++);
    }
    return this->inodeMap.value(id);
}

QString GoogleDrive::getRefreshToken()
{
    QSettings settings;
    settings.beginGroup("googledrive");
    return settings.value("refresh_token").toString();
}

void GoogleDrive::authenticate()
{
    QMutexLocker lock(&this->oAuthLock);
    QSettings settings;
    settings.beginGroup("googledrive");

    QString clientId = settings.value("client_id").toString();
    QString clientSecret = settings.value("client_secret").toString();

    if (this->auth) {
        QTimer::singleShot(600000,this->auth,&QObject::deleteLater);
    }
    this->auth    = new GOAuth2AuthorizationCodeFlow(new GoogleNetworkAccessManager(this), this);
    auto *handler = new QOAuthHttpServerReplyHandler(this);
    this->auth->setReplyHandler(handler);
    this->auth->setAuthorizationUrl(auth_url);
    this->auth->setAccessTokenUrl(access_token_url);
    this->auth->setClientIdentifier(clientId);
    this->auth->setClientIdentifierSharedKey(clientSecret);
    this->auth->setScope("https://www.googleapis.com/auth/drive.readonly");

    // Handle a refreshed token....
    connect(handler,&QOAuthHttpServerReplyHandler::tokensReceived,[=](QVariantMap tokens){
        this->auth->setToken(tokens.value("access_token").toString());
        setState(Connected);
        setRefreshToken(tokens.value("refresh_token").toString());
        this->auth->resetStatus(QAbstractOAuth::Status::Granted);
    });

    // Rewriting the refresh token request for google
    this->auth->setModifyParametersFunction([&](QAbstractOAuth::Stage stage, QVariantMap *parameters) {
        if (stage == QAbstractOAuth::Stage::RefreshingAccessToken) {
            parameters->insert("client_id",clientId);
            parameters->insert("client_secret",clientSecret);
            parameters->remove("redirect_uri");
        }
    });

    connect(this->auth, &QAbstractOAuth::statusChanged, [=](
            QAbstractOAuth::Status status) {

        if (status==QAbstractOAuth::Status::Granted) {
           D( "Authorised. Starting token refresh timer.");

           quint64 msecs = 600000;

           D("Setting token refresh timer for"<<msecs<<"ms.");

           this->refreshTokenTimer.start(msecs);

           setState(Connected);
           //emit stateChanged(this->state);
        }
    });

    connect(this->auth, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, [](const QUrl url) {
        qInfo() << QString("To use this app you must allow it access to your Google drive. To do this plese visit the following URL:\n\n\t%1\n\nIdeally you should visit this URL on a browser on the machine you are running gofish from. This will allow the callback to work correctly. If you are accessing the machine remotely then see the documentation for the additonal steps required.").arg(url.toString());
    });

    this->refreshTokenTimer.setSingleShot(true);

    QString refreshToken = getRefreshToken();
    if (refreshToken.isEmpty()) {
        this->auth->grant();
    } else {
        this->auth->setRefreshToken(refreshToken);
        this->auth->refreshAccessToken();
    }

}

void GoogleDrive::setRefreshToken(QString token)
{
    if (!token.isEmpty()) {
        QSettings settings;
        settings.beginGroup("googledrive");
        settings.setValue("refresh_token",token);
    }
}

void GoogleDrive::setState(GoogleDrive::ConnectionState newState)
{
    if (newState!=this->state) {
        this->state = newState;
        emit stateChanged(newState);
    }
}
