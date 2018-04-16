== GoFiSh ==

Gofish is a *read only* fuse fs for mounting your Google drive.

== Building ==

Gofish is written using Qt, and requires at least version 5.8. Qt requires SSL support too.

Currently it's developed on NetBSD against refuse, however it should build and run on Linux too.

To build, get a copy of the source code:

$ git clone ..../gofish.git

Then creat a build folder:

$ mkdir gofish-build

Then cd to the build folder, run qmake against the .pro file and make as usual:

$ cd gofish-build
$ qmake ../gofish/gofish.pro
$ make

=== Building statically === 

You can build a static version too, this requires a static build of Qt, and optionally (and not recommended) a static build of libssl.

First create a tools folder:

$ mkdir gofish-build-tools
$ cd gofish-build-tools

==== openssl ====

If you want a static build that doesn't depend on libssl, you need a static build of openssl. To build one you can do the following:

$ fetch the open ssl source from www.openssl.org, For this example I use 1.1.0h.
$ mkdir openssl-static
$ cd openssl-static 
$ ../openssl-1.1.0h/config no-shared
$ make

==== Qt ====

You will also need a static build of Qt. 

$ cd <gofish-build-tools folder>
$ fetch the Qt sources, I used 5.10.1, and unpack them.
$ mkdir qt-static
$ cd qt-static
$ ../qt-everywhere-src-5.10.1/configure -prefix /home/ian/software/qt/static -release -confirm-license -static -accessibility -qt-zlib -qt-libpng -qt-libjpeg -qt-xcb -qt-pcre -qt-freetype -no-glib -no-cups -no-sql-sqlite -no-qml-debug -no-opengl -no-egl -no-xinput2 -no-sm -no-icu -nomake examples -nomake tests -skip qtactiveqt -skip qtlocation -skip qtmultimedia -skip qtserialport -skip qtquickcontrols -skip qtquickcontrols2 -skip qtsensors -skip qtwebengine -skip qtwebsockets -skip qtxmlpatterns -skip qt3d -skip qtvirtualkeyboard -skip qtserialbus -skip qtspeech -skip qtsvg -skip qtwebchannel -skip qtwebglplugin -skip qtwebview -skip qtxmlpatterns -skip qtpurchasing -skip qtdoc -skip qtgamepad -skip qtgraphicaleffects -skip qtimageformats -skip qtcanvas3d -skip qtcharts -skip qttools -skip qtwayland -skip qtdeclarative -openssl-linked OPENSSL_LIBDIR=../openssl-static OPENSSL_INCDIR=../openssl-static/include
$ make 
$ make install

Then you can use the qmake in <gofish-build-tools>/qt-static/bin/qmake to build gofish.
