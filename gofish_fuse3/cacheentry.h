#ifndef CACHEENTRY_H
#define CACHEENTRY_H

#include <QObject>

class CacheEntry
{
public:
    CacheEntry();

    QByteArray data;
    qint64 start;
};

#endif // CACHEENTRY_H
