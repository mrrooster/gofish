# GoFiSh

Gofish is a *read only* fuse fs for mounting your Google drive (See *write support* below), currently developed against libfuse3.

## Write support

If gofish is mounted rw it will enable write support. This is currently quite new so use it at your own risk.

Currently files are written to google once they are closed. Permissions and uid/gids are also written if set. Deleting remote files caused them to be 'trashed'.

### Issues

Because of the way gofish refreshes directory info new files may dissapear from directory listings if they're still uploading over a refresh. They'll come back eventually.

### Temporary files

By default files are staged locally in the default system temp folder, this can be overridden with the -t option (eg if you have /tmp memory backed and are uploading large files). The -t option persists.

## Running

### Dependencies

Gofish requires libfuse3. You should probably go and get that first. ;)

### First run

gofish requires a goodle API ID and secret before it is allowed to connect to your google drive. These are used to identify an application to google. You can use the same api key for many instances of gofish, as the first run process ties it to a particular user. So you can easly generate an API key for your entire organisation.

As I don't supply these with gofish you have to register for your own API key. This proces isn't hard and more information can be found here:

https://developers.google.com/drive/v3/web/about-auth

If you want write support the application needs to be given **./auth/drive** scope, You should always add **./oath/drive.readonly** as this is used if the fs is mounted read only.

Once you have a google client ID and client secret you can run gofish for the first time like this:

```
./gofish --id <client id> --secret <client secret> <mountpoint>
```

The first time you run it you will be given a URL to visit. You should ideally visit this using a browser on the machine you're running gofish from, as the URL has a callback that tells gofish it's authorised.

However, if this isn't possible, visit the URL on another machine, and when you have given 'gofish' access to your google drive you'll get redirected to a localhost URL that will 404.

At this point change the 'localhost' section of the URL in your browser to the IP or hostname of the machine you're running gofish on. This should cause the callback to work as expected.

### Running normally

Once you have registered and authorised with google the --id and --secret parameters are not required anymore.

I currently run it with the following on Linux, the -d option causes plenty of debugging output:

```
sudo ./gofish -d -o ro,allow_other /path/to/mountpoint
```

This will mount your google drive readonly with permission for everyone to read it.

Use 'ro' to mount the drive read only. This is recommended.


### Killing

On linux you may find it doesn't respond to a CTRL-C. If you do `sudo fusermount -u /path/to/mountpoint` it should then exit cleanly.

### Options

There are a few options you can tune to improve performance:

|Option|Description|
|:-----|:----------|
|_refresh-secs_|This controls how long directory information is cached for. If your drive contents don't change that often you can set this to a high value to reduce the number of reads from Google. The default is 600 seconds.|
|_cache-bytes_|This controls how many bytes the in memory block cache uses. The default is 128MB.|
|_download-size_|This sets the amount of data that is downloaded in one request. You can probably leave this at it's default of 2MiB, however if you have a much faster or slower connection you may wish to change this. A good rule of thumb is about a quarter of whatever your internet speed is. So if you can download at 1MiB/sec you should probably set this to 256K. The value is in bytes. For example 256K this would be 262144.|
|_-f_|Don't go to the background.|
|_-d_|Enable debug output. Implies -f|
|_-t &lt;temp folder&gt;_|Set the temporary file location for uploaded files. You can use -h to see the current value|
|_-o &lt;options&gt;_|This allows you to pass mount options to FUSE.|

## Building

Gofish is written using Qt, and requires at least version 5.8. Qt requires SSL support too.

Currently it's developed on NetBSD against refuse, although it's also tested occasionally on Debian and Ubuntu.

To build, get a copy of the source code:

```
$ git clone ..../gofish.git
```

Then create a build folder:

```
$ mkdir gofish-build
```

Then cd to the build folder, run qmake against the .pro file and make as usual:

```
$ cd gofish-build
$ qmake ../gofish/gofish.pro
$ make
```

### Building statically

You can build a static version too, this requires a static build of Qt, and optionally (and not recommended) a static build of libssl.

First create a tools folder:

```
$ mkdir gofish-build-tools
$ cd gofish-build-tools
```

#### OpenSSL

If you want a static build that doesn't depend on libssl, you need a static build of openssl. To build one you can do the following:

```
$ fetch the open ssl source from www.openssl.org, For this example I use 1.1.0h.
$ mkdir openssl-static
$ cd openssl-static
$ ../openssl-1.1.0h/config no-shared
$ make
```

#### Qt

You will also need a static build of Qt.

```
$ cd <gofish-build-tools folder>
$ fetch the Qt sources, I used 5.10.1, and unpack them.
$ mkdir qt-static
$ cd qt-static
$ ../qt-everywhere-src-5.10.1/configure -prefix $PWD -release -confirm-license -static -accessibility -qt-zlib -qt-libpng -qt-libjpeg -qt-xcb -qt-pcre -qt-freetype -no-glib -no-cups -no-sql-sqlite -no-qml-debug -no-opengl -no-egl -no-xinput2 -no-sm -no-icu -nomake examples -nomake tests -skip qtactiveqt -skip qtlocation -skip qtmultimedia -skip qtserialport -skip qtquickcontrols -skip qtquickcontrols2 -skip qtsensors -skip qtwebengine -skip qtwebsockets -skip qtxmlpatterns -skip qt3d -skip qtvirtualkeyboard -skip qtserialbus -skip qtspeech -skip qtsvg -skip qtwebchannel -skip qtwebglplugin -skip qtwebview -skip qtxmlpatterns -skip qtpurchasing -skip qtdoc -skip qtgamepad -skip qtgraphicaleffects -skip qtimageformats -skip qtcanvas3d -skip qtcharts -skip qttools -skip qtwayland -skip qtdeclarative -openssl-linked OPENSSL_LIBDIR=../openssl-static OPENSSL_INCDIR=../openssl-static/include
$ make
$ make install
```

Then you can use the qmake in <gofish-build-tools>/qt-static/bin/qmake to build gofish.
