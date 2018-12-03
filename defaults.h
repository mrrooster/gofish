#ifndef DEFAULTS_H
#define DEFAULTS_H

// Setup some values for performance tuning (it's an fs after all..)
// NB: READ_CHUNK_SIZE must be the same or an exact multiple of CACHE_CHUNK_SIZE
#define READ_CHUNK_SIZE (1024*1024*10) // 2MB for now.
#define CACHE_CHUNK_SIZE (1024*128)   // Cache 64k 'blocks'

#define OP_TIMEOUT_MSEC (20*1000)
#define REQUEST_TIMER_TICK_MSEC 20
#define REQUEST_MAX_INFLIGHT 5

#define DEFAULT_CACHE_SIZE (1024*1024*128) // 128mb
#define DEFAULT_REFRESH_SECS 600


#endif // DEFAULTS_H
