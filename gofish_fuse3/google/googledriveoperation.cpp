#include "googledriveoperation.h"

GoogleDriveOperation::GoogleDriveOperation() : mimeData(nullptr)
{

}

QDebug operator<<(QDebug debug, const GoogleDriveOperation &o)
{
    QDebugStateSaver s(debug);

    debug.nospace()
            << "[Url: " << o.url
            << ", op: " << ((o.httpOp==GoogleDriveOperation::Get)?"GET":(o.httpOp==GoogleDriveOperation::Put)?"PUT":(o.httpOp==GoogleDriveOperation::Post)?"POST":(o.httpOp==GoogleDriveOperation::Patch)?"PATCH":"-Unknown-")
            << ", retcount: " << o.retryCount
            << ", datalen: " << o.dataToSend.size()
            << "]"
               ;
    return debug;
}
