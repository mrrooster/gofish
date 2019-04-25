# GoFiSh 

Gofish is a fuse fs for mounting your Google drive.

##

gofish_fuse3 is the current version, gofish_netbsd is the last version based on the high level fuse interface.

### gofish_fuse3

This is built on libfuse3 and has (beta) support for writing.

### gofish_netbsd

Gofish was originaly developed on NetBSD, this version is in the gofish_netbsd folder. This will work against the netbsd fuse.h using puffs but still has some thread locking issues; so is prone to locking up occasionally. it is designed using a dual thread aproach with the Qt event loop being run in the main thread and the fuse event loop in a secondary fuse thread. It has issues and is unlikely to be updated.

