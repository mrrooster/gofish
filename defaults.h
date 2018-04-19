#ifndef DEFAULTS_H
#define DEFAULTS_H

// Setup some values for performance tuning (it's an fs after all..)
// NB: READ_CHUNK_SIZE must be the same or an exact multiple of CACHE_CHUNK_SIZE
#define READ_CHUNK_SIZE (1024*1024*2) // 2MB for now.
#define CACHE_CHUNK_SIZE (1024*64)   // Cache 64k 'blocks'

#define DEFAULT_CACHE_SIZE (1024*1024*128) // 128mb
#define DEFAULT_REFRESH_SECS 600


#endif // DEFAULTS_H
