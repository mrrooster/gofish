#include "googledrive.h"
#include <QtNetworkAuth>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonValue>
#include <iostream>
#include "googlenetworkaccessmanager.h"
#include "goauth2authorizationcodeflow.h"

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

GoogleDrive::GoogleDrive(const QString clientId, const QString clientSecret, QObject *parent) : QObject(parent),state(Disconnected)
{

    //Operation timer
    connect(&this->operationTimer,&QTimer::timeout,[=]() {
        if (this->queuedOps.isEmpty()) {
            this->operationTimer.stop();
        } else {
            QPair<QPair<QUrl,QVariantMap>,std::function<void(QNetworkReply*)>> op = this->queuedOps.takeFirst();
            if (!op.first.second.isEmpty()) {
                dynamic_cast<GoogleNetworkAccessManager*>(this->auth->networkAccessManager())->setNextHeaders(op.first.second);
            }
            auto response = this->auth->get(op.first.first);
            connect(response,&QNetworkReply::finished,[=]{
                if (response->error()==QNetworkReply::NoError) {
                    op.second(response);
                } else {
                    D("Error: "<<response->errorString());
                    this->queuedOps.append(op);
            	    this->operationTimer.start();
                }
            });
            this->operationTimer.start();
        }
    });

    this->auth    = new GOAuth2AuthorizationCodeFlow(new GoogleNetworkAccessManager(this), this);
    auto *handler = new QOAuthHttpServerReplyHandler(this);
    this->auth->setReplyHandler(handler);
    this->auth->setAuthorizationUrl(auth_url);
    this->auth->setAccessTokenUrl(access_token_url);
    this->auth->setClientIdentifier(clientId);
    this->auth->setClientIdentifierSharedKey(clientSecret);
    this->auth->setScope("https://www.googleapis.com/auth/drive.readonly");

    connect(&this->refreshTokenTimer,&QTimer::timeout,this->auth,&QOAuth2AuthorizationCodeFlow::refreshAccessToken);

    // Handle a refreshed token....
    connect(handler,&QOAuthHttpServerReplyHandler::tokensReceived,[=](QVariantMap tokens){
        this->auth->setToken(tokens.value("access_token").toString());
        setState(Connected);
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
           D( "Token expires at:" <<this->auth->expirationAt().toString());

           quint64 msecs = 120000;

           this->refreshTokenTimer.start(msecs);

           setRefreshToken(this->auth->refreshToken());
           setState(Connected);
           //emit stateChanged(this->state);
        }
    });

    connect(this->auth, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, [](const QUrl url) {
        qInfo() << "Go to: "<<url.toString();
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

void GoogleDrive::readRemoteFolder(QString path, QString parentId)
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
    getBlockingLock(path)->lock();
    this->inflightPaths.append(path);
    D("Removing path from preflight: "<<path);
    this->preflightPaths.removeOne(path);
    D("read remote folder.. locked."<<path);
    readFolder(path,"",parentId);
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
    if (!this->inflightPaths.contains(path)) {
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

void GoogleDrive::getFileContents(QString fileId, quint64 start, quint64 length)
{
    QString id = QString("%1:%2:%3").arg(fileId).arg(start).arg(length);
    D("read remote file."<<id);
    if (this->inflightPaths.contains(id)) {
        D("Another file request in progress.");
        return;
    }

    getBlockingLock(id)->lock();
    this->inflightPaths.append(id);
    D("Removing path from preflight: "<<id);
    this->preflightPaths.removeOne(id);
    D("read remote file.. locked."<<id);
    //readFolder(path,path,"","");
    readFileSection(fileId,start,length);
    return;
}

void GoogleDrive::readFolder(QString startPath, QString nextPageToken, QString parentId)
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
    queueOp(QPair<QUrl,QVariantMap>(url,QVariantMap()),[=](QNetworkReply *response){
        QByteArray responseData = response->readAll();
        //D("Read file response data:"<<responseData);
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        //D("Got response:"<<doc);

        if (doc.isObject()) {
            if (doc["files"].isArray()) {
                auto files = doc["files"].toArray();

                for(int idx=0;idx<files.size();idx++) {
                    const QJsonValue file = files[idx];
                    QMap<QString,QString> fileInfo;
                    fileInfo.insert("id",file["id"].toVariant());
                    fileInfo.insert("name",file["name"].toVariant());
                    fileInfo.insert("mimeType",file["mimeType"].toVariant());
                    fileInfo.insert("size",file["size"].toVariant());
                    fileInfo.insert("createdTime",file["createdTime"].toVariant());
                    fileInfo.insert("modifiedTime",file["modifiedTime"].toVariant());
                    this->inflightValues.value(startPath)->append(fileInfo);
                }
                if (!doc["nextPageToken"].isString()) {
                    emit folderContents(startPath,*this->inflightValues.value(startPath));
                    this->inflightPaths.removeOne(startPath);
                    D("Releasing lock (drive)"<<startPath);
                    getBlockingLock(startPath)->unlock();
                    delete this->inflightValues.take(startPath);
                    return;
                } else {
                    emit folderContents(startPath,QVector<QVariantMap>());
                }
            }
            if (doc["nextPageToken"].isString()) {
                D("Has next page! :"<<doc["nextPageToken"].toString());
                this->readFolder(startPath,doc["nextPageToken"].toString(),parentId);
            } else {
                emit folderContents(startPath,QVector<QVariantMap>());
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

    queueOp(QPair<QUrl,QVariantMap>(url,extraHeaders),[=](QNetworkReply *response){
        QByteArray responseData = response->readAll();
        this->pendingSegments.insert(id,responseData);
        D("Got file section response."<<responseData.size());
        this->inflightPaths.removeOne(id);
        D("Releasing lock (file)"<<id);
        getBlockingLock(id)->unlock();
    });
}

void GoogleDrive::queueOp(QPair<QUrl, QVariantMap> urlAndHeaders, std::function<void(QNetworkReply *)> handler)
{
    this->queuedOps.append(QPair<QPair<QUrl,QVariantMap>,std::function<void(QNetworkReply*)>>(urlAndHeaders,handler));
    this->operationTimer.start(10);
}

QString GoogleDrive::getRefreshToken()
{
    QSettings settings;
    settings.beginGroup("googledrive");
    return settings.value("refresh_token").toString();
}

void GoogleDrive::setRefreshToken(QString token)
{
    QSettings settings;
    settings.beginGroup("googledrive");
    settings.setValue("refresh_token",token);
}

void GoogleDrive::setState(GoogleDrive::ConnectionState newState)
{
    if (newState!=this->state) {
        this->state = newState;
        emit stateChanged(newState);
    }
}
