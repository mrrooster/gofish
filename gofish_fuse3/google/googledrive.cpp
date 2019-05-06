#include "googledrive.h"
#include <QtNetworkAuth>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonValue>
#include <iostream>
#include <QNetworkReply>
#include <sys/stat.h>
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
const QUrl files_upload("https://www.googleapis.com/upload/drive/v3/files");
const QUrl files_get_ids("https://www.googleapis.com/drive/v3/files/generateIds");
const QUrl files_metadata("https://www.googleapis.com/drive/v3/files");

GoogleDrive::GoogleDrive(bool readOnly, QString tempDir, QObject *parent) : QObject(parent),auth(nullptr),state(Disconnected),readOnly(readOnly),tempDir(tempDir)
{
    //Operation timer
    connect(&this->operationTimer,&QTimer::timeout,this,&GoogleDrive::operationTimerFired);

    connect(&this->refreshTokenTimer,&QTimer::timeout,[=]() {
        D("Requiring reauth at next request.");
        setState(Disconnected);
        this->refreshTokenTimer.stop();
    });
    this->inode=1;
    this->bytesRead = 0;
    this->bytesWritten = 0;
    this->fuseRead = 0;
    this->fuseWritten = 0;
    this->requestCount = 0;
    this->maxQueuedRequests = 0;
    authenticate();

    connect(&this->statsTimer,&QTimer::timeout,this,[=](){
        D("------------------------------------------------");
        QString data = QString("Bytes read      : %1 (%2)").arg(this->bytesRead).arg(this->byteCountString(this->bytesRead));
        D(data);
        data = QString("Bytes sent      : %1 (%2)").arg(this->bytesWritten).arg(this->byteCountString(this->bytesWritten));
        D(data);
        data = QString("Request count   : %1").arg(this->requestCount);
        D(data);
        data = QString("Max req queue   : %1").arg(this->maxQueuedRequests);
        D(data);
        data = QString("Curr req queue  : %1").arg(this->queuedOps.size());
        D(data);
        data = QString("FS bytes read   : %1 (%2)").arg(this->fuseRead).arg(this->byteCountString(this->fuseRead));
        D(data);
        data = QString("FS bytes written: %1 (%2)").arg(this->fuseWritten).arg(this->byteCountString(this->fuseWritten));
        D(data);
        D("------------------------------------------------");
    });
    this->statsTimer.setSingleShot(false);
    this->statsTimer.start(300000);
    if (this->tempDir=="") {
        this->tempDir = QDir::tempPath();
    }
    D("Initialised google drive, readonly:"<<readOnly);
    D("Using temp dir                    :"<<this->tempDir);
}

GoogleDrive::~GoogleDrive()
{
}

void GoogleDrive::readRemoteFolder(QString path, QString parentId)
{
    D("read remote folder."<<path);
    if (this->inflightPaths.contains(path)) {
        D("Another request in progress.");
    }
    if (this->inflightValues.contains(path)) {
        this->inflightValues.value(path)->clear();
    } else {
        this->inflightValues.insert(path,new QVector<QVariantMap>());
    }
    this->inflightPaths.append(path);
    readFolder(path,"",parentId);
}

bool GoogleDrive::pathInFlight(QString path)
{
    return this->inflightPaths.contains(path);
}

QByteArray GoogleDrive::getPendingSegment(QString fileId, qint64 start, qint64 length)
{
    QString id = QString("%1:%2:%3").arg(fileId).arg(start).arg(length);
    if (this->pendingSegments.contains(id)) {
        return this->pendingSegments.take(id);
    }
    return QByteArray();
}

void GoogleDrive::getFileContents(QString fileId, qint64 start, qint64 length)
{
    QString id = QString("%1:%2:%3").arg(fileId).arg(start).arg(length);
    D("read remote file."<<id);


    this->inflightPaths.append(id);
    D("read remote file.. locked."<<id);
    //readFolder(path,path,"","");
    readFileSection(fileId,start,length);
}

void GoogleDrive::uploadFile(QIODevice *file, QString path, QString fileId, QString parentId)
{
    QUrl url = files_upload;
    GoogleDriveOperation::HttpOperation op = GoogleDriveOperation::Post;
    bool isNewFile = true;
    bool isFolder = (file==nullptr);
    if (fileId!="") {
        op = GoogleDriveOperation::Patch;
        url.setPath(url.path()+"/"+fileId);
        isNewFile = false;
    } else {
        if (this->pregeneratedIds.isEmpty()) {
            url = files_get_ids;
            url.setQuery("count=1&space=drive");
            queueOp(url,QVariantMap(),[=](QNetworkReply *response,bool){
                if(response->error()==QNetworkReply::NoError) {
                    QJsonDocument doc = QJsonDocument::fromJson(response->readAll());
                    if (doc["ids"].isArray()) {
                        auto ids = doc["ids"].toArray();
                        for(int idx=0;idx<ids.size();idx++) {
                            this->pregeneratedIds.append(ids[idx].toString());
                        }
                        this->uploadFile(file,path,fileId,parentId);
                    }
                }
            });
            return;
        } else {
            fileId = this->pregeneratedIds.takeFirst();
        }
    }
    url.setQuery("uploadType=resumable");
    D("Upload URL:"<<url);

    QJsonObject obj;
    QString fileName = path.mid(path.lastIndexOf("/")+1);
    obj.insert("name",QJsonValue(fileName));
    obj.insert("mimeType",(isFolder)?GOOGLE_FOLDER:"application/octet-stream");
    if (isNewFile) {
        obj.insert("id",fileId);
        obj.insert("parents",QJsonArray({parentId}));
    }

    QJsonDocument doc(obj);
    QByteArray bodyContent = doc.toJson();
    QVariantMap headers;
    qint64 size = isFolder?0:file->size();
    headers.insert("Content-Type","application/json; charset=UTF-8");
    headers.insert("X-Upload-Content-Length",size);
    headers.insert("Content-Length",bodyContent.size());
    D("Metadata for upload:"+bodyContent);

    queueOp(url,headers,doc.toJson(),op,[=](QNetworkReply *response,bool){
        //if (responseData.isEmpty()) {
        //    D("Error'd/empty response from upload.");
        //    return;
        //}
        if (response->error()==QNetworkReply::NoError) {
            QString redirect = response->header(QNetworkRequest::LocationHeader).toString();
            QUrl redirectUrl(redirect);
            redirectUrl.setPath("/upload/drive/v3/files");
            redirect = redirectUrl.toString();
            QString body = response->readAll();
            D("Redirect URL: "<<redirect);
//            D("Body        : "<<body);
            if (!isFolder) {
                emit this->uploadInProgress(path);
                file->open(QIODevice::ReadOnly);
            }
            this->uploadFileContents(file,path,fileId,redirect);
        } else {
            D("Error uploading:"+response->errorString());
        }
    });
}

void GoogleDrive::createFolder(QString path, QString fileId, QString parentId)
{
    uploadFile(nullptr,path,fileId,parentId);
}

void GoogleDrive::unlink(QString path, QString fileId)
{
    unlink(path,fileId,[=](QNetworkReply *response,bool ){
        //if (responseData.isEmpty()) {
        //    D("Error'd/empty response from upload.");
        //    return;
        //}
        if (response->error()==QNetworkReply::NoError) {
            emit this->unlinkComplete(path,true);
        } else {
            D("Error uploading:"+response->errorString());
            emit this->unlinkComplete(path,false);
        }
    });
}

void GoogleDrive::rename(QString fileId, QString oldParentId, QString newParentId, QString newName, QString removeId)
{
    QJsonObject obj;
    obj.insert("name",QJsonValue(newName));
    QJsonDocument doc(obj);

    D("Rename file:"<<fileId<<", old:"<<oldParentId<<", new:"<<newParentId<<", newname:"<<newName);
    if (oldParentId==newParentId) {
        oldParentId = newParentId = "";
    }

    std::function<void(QNetworkReply *,bool)> handler = [=](QNetworkReply *response,bool ){
        //if (responseData.isEmpty()) {
        //    D("Error'd/empty response from upload.");
        //    return;
        //}
        D("Rename response");
        if (response->error()==QNetworkReply::NoError) {
            emit this->renameComplete(fileId,true);
        } else {
            D("Error updating metadata:"+response->errorString());
            emit this->renameComplete(fileId,false);
        }
    };
    if (removeId=="") {
        updateFileMetadata(fileId,doc,handler,oldParentId,newParentId);
    } else {
        unlink(removeId,removeId,[=](QNetworkReply* reply,bool){
            if (reply->error()==QNetworkReply::NoError) {
                updateFileMetadata(fileId,doc,handler,oldParentId,newParentId);
            }
        });
    }

}

void GoogleDrive::updateMetadata(GoogleDriveObject *obj)
{
    D("Updating metadata for:"<<obj->getInode());
    QDateTime timestamp;
    QJsonObject json;
    QJsonObject appProperties;

    appProperties.insert("gid",QJsonValue(static_cast<int>(obj->getGid())));
    appProperties.insert("uid",QJsonValue(static_cast<int>(obj->getUid())));
    appProperties.insert("fileMode",QJsonValue(obj->getFileMode()));
    json.insert("appProperties",appProperties);
    json.insert("modifiedTime",QJsonValue(obj->getModifiedTime().toString(Qt::ISODate)+"Z"));
    QJsonDocument doc(json);
    updateFileMetadata(obj->getFileId(),doc,[=](QNetworkReply*,bool){});
}

QString GoogleDrive::getTempDir()
{
    return this->tempDir;
}

void GoogleDrive::uploadFileContents(QIODevice *fileArg, QString path, QString fileId,QString url)
{
    QVariantMap headers;
    qint64 pos = (fileArg)?fileArg->pos():0;
    QByteArray data = (fileArg)?fileArg->read(UPLOAD_CHUNK_SIZE):QByteArray();

    headers.insert("X-Upload-Content-Length",(fileArg)?fileArg->size():0);
    headers.insert("Content-Type",(fileArg)?"application/octet-stream;":GOOGLE_FOLDER);
    headers.insert("Content-Length",data.size());
    if (fileArg && fileArg->size()>0) {
        headers.insert("Content-Range",(fileArg) ? (QString("bytes %1-%2/%3").arg(pos).arg(fileArg->pos()-1).arg(fileArg->size())) : "bytes 0-0/0");
    }
    GoogleDriveOperation::HttpOperation op = GoogleDriveOperation::Put;
    bool isDir = (fileArg==nullptr);
    QPointer<QIODevice> file = fileArg;

    queueOp(url,headers,data,op,[=](QNetworkReply *,bool ok){
        D("UFC: in response"<<(file)<<ok<<path<<fileId<<url<<pos);
        if (ok) {
            if (isDir || !file || file->atEnd()) {
                if (file) {
                    file->close();
                    this->fuseWritten += file->size();
                    emit this->uploadComplete(path,fileId);
                }
                if (isDir) {
                    emit this->dirCreated(path,fileId);
                }
            } else {
                this->uploadFileContents(file,path,fileId,url);
            }
        }
    },[=](QNetworkReply*){emit this->uploadInProgress(path);});

    emit this->uploadInProgress(path);
}

int GoogleDrive::modeFromCapabilites(QJsonObject caps,bool folder)
{
    int mode=0;
    int writeUser = (readOnly)?0:S_IWUSR;
    if (folder) {
        mode |= caps["canAddChildren"].toBool()?writeUser:0;
        mode |= caps["canListChildren"].toBool()?(S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IXOTH|S_IROTH):0;
    } else {
        mode |= caps["canEdit"].toBool()?writeUser:0;
        mode |= caps["canDownload"].toBool()?(S_IRUSR|S_IXUSR|S_IRGRP|S_IROTH):0;
    }
    return mode;
}

QString GoogleDrive::byteCountString(qint64 bytes)
{
    QString suffix="B";

    if (bytes>1024*1024*1024) {
        suffix="GiB";
        bytes=bytes/1024/1024/1024;
    } else if (bytes>1024*1024*4) {
        suffix="MiB";
        bytes=bytes/1024/1024;
    } else if (bytes>1234) {
        suffix="KiB";
        bytes=bytes/1024;
    }

    return QString("%0 %1").arg(bytes).arg(suffix);
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
    queryString += " and not trashed";

    QString query = QString("q=%1").arg(QString(QUrl::toPercentEncoding(queryString," '")).replace(" ","+"));
    if (!nextPageToken.isEmpty()) {
        query += QString("&pageToken=%1").arg(QString(QUrl::toPercentEncoding(nextPageToken," '")).replace(" ","+"));
    }
    query += "&fields=nextPageToken,files(name,size,mimeType,id,kind,createdTime,modifiedTime,capabilities(canDownload,canEdit,canAddChildren,canListChildren),appProperties(uid,gid,fileMode))&pageSize=1000";
    url.setQuery(query);
    queueOp(url,QVariantMap(),[=](QNetworkReply *response,bool found){
        QByteArray responseData = response->readAll();
        //D("Read file response data:"<<responseData);
        if (responseData.isEmpty()||!found) {
            D("Error'd/empty response.");
            return;
        }
//        D("Got response data:"+responseData);
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
//        D("Got response:"<<doc);

        if (doc.isObject()) {
            if (doc["files"].isArray()) {
                QVector<QString> nameList;

                for(auto it = this->inflightValues.value(startPath)->begin(),end = this->inflightValues.value(startPath)->end(); it!=end; it++) {
                    QVariantMap file = (*it);
                    nameList.append(file["name"].toString());
                }
                auto files = doc["files"].toArray();

                for(int idx=0;idx<files.size();idx++) {
                    const QJsonValue file = files[idx];
                    QVariantMap fileInfo;
                    QString localName = file["name"].toVariant().toString();
                    while(nameList.contains(localName)) {
                        localName += ".1";
                    }
                    nameList.append(localName);

                    fileInfo.insert("id",file["id"].toVariant());
                    fileInfo.insert("name",localName);
                    fileInfo.insert("mimeType",file["mimeType"].toVariant());
                    fileInfo.insert("size",file["size"].toVariant());
                    fileInfo.insert("createdTime",file["createdTime"].toVariant());
                    fileInfo.insert("modifiedTime",file["modifiedTime"].toVariant());
                    fileInfo.insert("fileMode",QVariant(this->modeFromCapabilites(file["capabilities"].toObject(),(file["mimeType"].toString()==GOOGLE_FOLDER)) ));

                    if(file.toObject().contains("appProperties")) {
                        QJsonObject appProperties = file["appProperties"].toObject();
                        if (appProperties.contains("gid")) {
                            fileInfo.insert("gid",appProperties["gid"].toVariant());
                        }
                        if (appProperties.contains("uid")) {
                            fileInfo.insert("uid",appProperties["uid"].toVariant());
                        }
                        if (appProperties.contains("fileMode")) {
                            fileInfo.insert("fileMode",appProperties["fileMode"].toVariant());
                        }
                    }

                    this->inflightValues.value(startPath)->append(fileInfo);
                }
                if (!doc["nextPageToken"].isString()) {
                    this->inflightPaths.removeAll(startPath);
                    QVector<GoogleDriveObject*> newChildren;
                    for(auto it = this->inflightValues.value(startPath)->begin(),end = this->inflightValues.value(startPath)->end(); it!=end; it++) {
                        QVariantMap file = (*it);
        //                D("File: "<<file);

                        GoogleDriveObject *newObj = new GoogleDriveObject(
                                    nullptr,
                                    this,
                                    file["id"].toString(),
                                    startPath,
                                    sanitizeFilename(file["name"].toString()),
                                    file["mimeType"].toString(),
                                    file["size"].toULongLong(),
                                    QDateTime::fromString(file["createdTime"].toString(),"yyyy-MM-dd'T'hh:mm:ss.z'Z'"),
                                    QDateTime::fromString(file["modifiedTime"].toString(),"yyyy-MM-dd'T'hh:mm:ss.z'Z'"),
                                    nullptr
                                   );
                        newObj->setFileMode(file["fileMode"].toInt());
                        newObj->setUid(file.contains("uid") ? file["uid"].toUInt() : ::getuid());
                        newObj->setGid(file.contains("gid") ? file["gid"].toUInt() : ::getgid());
                        newObj->setFileMode(file.contains("fileMode") ? file["fileMode"].toUInt() : newObj->getFileMode());
                        newObj->stopMetadataUpdate(); // setuid/gid calls have triggered deffered metadata update to google. Don't do this when creating.
                        newChildren.append(newObj);
                    }
                    emit remoteFolder(startPath,newChildren);

                    delete this->inflightValues.take(startPath);
                    return;
                }
            }
            if (doc["nextPageToken"].isString()) {
                D("Has next page! :"<<doc["nextPageToken"].toString());
                this->readFolder(startPath,doc["nextPageToken"].toString(),parentId);
            }
        }
    });
}
void GoogleDrive::readFileSection(QString fileId, qint64 start, qint64 length)
{
    QString id = QString("%1:%2:%3").arg(fileId).arg(start).arg(length);
    QUrl url = files_info.toString()+fileId;
    QVariantMap extraHeaders;

    url.setQuery("alt=media");

    D("Get remote file :"<<url<<",id:"<<id);
    D("Get remote file :"<<id<<start<<length);

    extraHeaders.insert("Range",QString("bytes=%1-%2").arg(start).arg(start+length-1));

    queueOp(url,extraHeaders,[=](QNetworkReply *response,bool ){
        QByteArray responseData = response->readAll();
        this->pendingSegments.insert(id,responseData);
        D("Got file section response, size:"<<responseData.size());
        this->inflightPaths.removeOne(id);
        this->fuseRead+=responseData.size();
        emit pendingSegment(fileId,start,length);
    });
}

void GoogleDrive::unlink(QString path, QString fileId, std::function<void (QNetworkReply *, bool)> handler)
{
    QJsonObject obj;
    QString fileName = path.mid(path.lastIndexOf("/")+1);
    obj.insert("trashed",true);
    QJsonDocument doc(obj);
    updateFileMetadata(fileId,doc,handler);
}

void GoogleDrive::updateFileMetadata(QString fileId, QJsonDocument doc, std::function<void(QNetworkReply *,bool)> handler, QString oldParent, QString newParent)
{
    QUrl url = files_metadata;
    GoogleDriveOperation::HttpOperation op = GoogleDriveOperation::Patch;

    if (fileId=="") {
        return;
    }

    url.setPath(url.path()+"/"+fileId);

    QString query="";
    if (oldParent!="") {
        query += "removeParents="+oldParent;
    }
    if (newParent!="") {
        if (query!="") {
            query+="&";
        }
        query += "addParents="+newParent;
    }

    if (query!="") {
        url.setQuery(query);
    }

    D("Upload metadata URL:"<<url);

    QByteArray bodyContent = doc.toJson();
    QVariantMap headers;
    headers.insert("Content-Type","application/json; charset=UTF-8");
    headers.insert("Content-Length",bodyContent.size());
    D("Metadata for upload:"+bodyContent);

    queueOp(url,headers,doc.toJson(),op,handler);
}

void GoogleDrive::queueOp(QUrl url, QVariantMap headers, std::function<void(QNetworkReply*,bool)> handler)
{
    queueOp(url,headers,QByteArray(),GoogleDriveOperation::Get,handler);
}

void GoogleDrive::queueOp(QUrl url, QVariantMap headers, QByteArray data, GoogleDriveOperation::HttpOperation op, std::function<void (QNetworkReply *, bool)> handler)
{
    queueOp(url,headers,data,op,handler,[=](QNetworkReply*){});
}

void GoogleDrive::queueOp(QUrl url, QVariantMap headers, QByteArray data, GoogleDriveOperation::HttpOperation op, std::function<void (QNetworkReply*, bool)> handler, std::function<void(QNetworkReply *)> inProgressHandler)
{
    GoogleDriveOperation *operation = new GoogleDriveOperation();

    operation->url        = url;
    operation->headers    = headers;
    operation->handler    = handler;
    operation->inProgressHandler = inProgressHandler;
    operation->dataToSend = data;
    operation->retryCount = 0;
    operation->httpOp     = op;
    this->queuedOps.append(operation);
    if (this->queuedOps.size()>this->maxQueuedRequests) {
        this->maxQueuedRequests = this->queuedOps.size();
    }
    if (!this->operationTimer.isActive()) {
        this->operationTimer.start(REQUEST_TIMER_TICK_MSEC);
    }
}

void GoogleDrive::operationTimerFired()
{
    if (this->queuedOps.isEmpty()) {
        D("Operation queue empty.");
        this->operationTimer.setInterval(REQUEST_TIMER_TICK_MSEC);
        this->operationTimer.stop();
    } else {
        D("Current state: "<<this->state);
        if (this->state == Connected) {
            quint64 interval = this->operationTimer.interval() * 2;
            this->operationTimer.setInterval( (interval>REQUEST_TIMER_TICK_MSEC_MAX) ? REQUEST_TIMER_TICK_MSEC_MAX : interval );
            //D("************"<<this->inprogressOps.size());
            if (this->inprogressOps.size()>REQUEST_MAX_INFLIGHT) {
                D("More than "<<REQUEST_MAX_INFLIGHT<<"inflight.");
            } else {
                this->requestCount++;
                GoogleDriveOperation *op = this->queuedOps.takeFirst();
                D("Sending op: "<<*op);
                if (!op->headers.isEmpty()) {
                    dynamic_cast<GoogleNetworkAccessManager*>(this->auth->networkAccessManager())->setNextHeaders(op->headers);
                }
                QNetworkReply *response = nullptr;
                if (op->httpOp==GoogleDriveOperation::Put) {
                    D("Doing put");
                    response = this->auth->put(op->url,op->dataToSend);
                } else if (op->httpOp==GoogleDriveOperation::Post) {
                    D("Doing post");
                    response = this->auth->post(op->url,op->dataToSend);
                } else if (op->httpOp==GoogleDriveOperation::Patch) {
                    D("Doing patch");
                    response = this->auth->patch(op->url,op->dataToSend);
                } else {
                    response = this->auth->get(op->url);
                }
                D("Inflight response: "<<response);
                connect(response,&QNetworkReply::uploadProgress,response,[=](qint64 bytesWritten, qint64 total){
                    op->inProgressHandler(response);
                    if (total>0 && bytesWritten==total) {
                        this->bytesWritten += total;
                    }
                });
                connect(response,&QNetworkReply::downloadProgress,response,[=](qint64 bytesRead,qint64 total){
                    if (total>0 && bytesRead==total) {
                        this->bytesRead += total;
                    }
                });
                this->inprogressOps.insert(response,op);
            }
        } else if (this->state==Disconnected) {
            D("Reauthenticating...");
            authenticate();
        } else {
            auto interval = operationTimer.interval();
            interval *=2;
            if (interval>REQUEST_TIMER_MAX_MSEC) {
                interval = REQUEST_TIMER_MAX_MSEC;
            }
            this->operationTimer.setInterval(interval);
        }
        this->operationTimer.start();
    }
}

void GoogleDrive::requestFinished(QNetworkReply *response)
{
//    D("REQUEST FIN:"<<response<<response->isFinished());
    if (this->inprogressOps.contains(response)) {
        auto op = this->inprogressOps.take(response);

        if (response->error()==QNetworkReply::NoError) {
            op->handler(response,true);
            delete op;
        } else {
            int msec = REQUEST_TIMER_TICK_MSEC;
            if (response->error()==QNetworkReply::ContentNotFoundError) {
                D("Not found.");
                op->handler(response,false);
                delete op;
                this->operationTimer.start();
                return;
            }
            if (response->error()==QNetworkReply::AuthenticationRequiredError) {
                D("Authentication error, reauthing...");
                setState(Disconnected);
                this->refreshTokenTimer.stop();
                authenticate();
            }
            D("Error: "<<response->errorString()<<response->error());
            D("Error body:"<<response->readAll());
            op->retryCount++;
            if (op->retryCount>10) {
                D("Abandoning request due to >10 retries.");
                op->handler(response,false); // Call the response with no data to prevent locks
                delete op;
            } else {
                this->queuedOps.append(op);
                int factor = msec;
                for(int x=0;x<op->retryCount;x++) {
                    msec *= factor;
                }
                if (msec>REQUEST_TIMER_MAX_MSEC) {
                    msec = REQUEST_TIMER_MAX_MSEC;
                }
            }
            this->operationTimer.start(msec);
        }

        response->deleteLater();
    }
}

qint64 GoogleDrive::getInodeForFileId(QString id)
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

QString GoogleDrive::sanitizeFilename(QString path)
{
    return path.replace(QRegExp("[/]"),"_");
}

void GoogleDrive::authenticate()
{
    QSettings settings;
    settings.beginGroup("googledrive");

    QString clientId = settings.value("client_id").toString();
    QString clientSecret = settings.value("client_secret").toString();

    if (this->auth) {
        QTimer::singleShot(600000,this->auth,&QObject::deleteLater);
    }
    setState(Connecting);
    this->auth    = new GOAuth2AuthorizationCodeFlow(new GoogleNetworkAccessManager(this), this);
    auto *handler = new QOAuthHttpServerReplyHandler(this);
    this->auth->setReplyHandler(handler);
    this->auth->setAuthorizationUrl(auth_url);
    this->auth->setAccessTokenUrl(access_token_url);
    this->auth->setClientIdentifier(clientId);
    this->auth->setClientIdentifierSharedKey(clientSecret);
    if (this->readOnly) {
        this->auth->setScope("https://www.googleapis.com/auth/drive.readonly");
    } else {
        this->auth->setScope("https://www.googleapis.com/auth/drive");
    }

    // Connect the finished signal
    connect(this->auth->networkAccessManager(),&QNetworkAccessManager::finished,this,&GoogleDrive::requestFinished);

    // Handle a refreshed token....
    connect(handler,&QOAuthHttpServerReplyHandler::tokensReceived,[=](QVariantMap tokens){
        this->auth->setToken(tokens.value("access_token").toString());
        setState(Connected);
        setRefreshToken(tokens.value("refresh_token").toString());
        this->auth->resetStatus(QAbstractOAuth::Status::Granted);
    });

    connect(this->auth,&QOAuth2AuthorizationCodeFlow::finished,[=](QNetworkReply *reply){
        if (this->state == Connecting && reply->isFinished() && reply->error()==QNetworkReply::ContentAccessDenied) {
            this->setState(ConnectionFailed);
        }
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

           qint64 msecs = 600000;

           D("Setting token refresh timer for"<<msecs<<"ms.");

           this->refreshTokenTimer.start(msecs);

           setState(Connected);
           //emit stateChanged(this->state);
        }
    });

    connect(this->auth, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, [](const QUrl url) {
        qInfo().noquote() << QString("To use this app you must allow it access to your Google drive. To do this plese visit the following URL:\n\n\t%1\n\nIdeally you should visit this URL on a browser on the machine you are running gofish from. This will allow the callback to work correctly. If you are accessing the machine remotely then see the documentation for the additonal steps required.").arg(url.toString());
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
