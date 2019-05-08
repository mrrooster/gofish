#ifndef DEFAULTS_H
#define DEFAULTS_H

// Setup some values for performance tuning (it's an fs after all..)
// NB: READ_CHUNK_SIZE must be the same or an exact multiple of CACHE_CHUNK_SIZE
#define READ_CHUNK_SIZE (1024*1024*10) // 2MB for now.
#define UPLOAD_CHUNK_SIZE READ_CHUNK_SIZE
#define CACHE_CHUNK_SIZE (1024*128)   // Cache 128k 'blocks'

#define OP_TIMEOUT_MSEC (20*1000)
// The initial request timer starts at.,,,,,
#define REQUEST_TIMER_TICK_MSEC 2
// multiple requests will slowly back off to a constant rate of....
#define REQUEST_TIMER_TICK_MSEC_MAX 100
// or if there's an error to...
#define REQUEST_TIMER_MAX_MSEC 30000
#define REQUEST_MAX_ERROR_RETRIES 5
#define REQUEST_MAX_INFLIGHT 2

#define DEFAULT_CACHE_SIZE (1024*1024*32) // 32MB
#define DEFAULT_REFRESH_SECS 600

//#include <QString>
//QString byteCountString(qint64 bytes);

#endif // DEFAULTS_H

