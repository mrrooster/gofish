#!/bin/sh

set -e
qt_url="http://mirrors.ukfast.co.uk/sites/qt.io/archive/qt/5.13/5.13.0/single/qt-everywhere-src-5.13.0.tar.xz"
qt_opts="-prefix $1/gofish-build/qt -release -static -confirm-license -accessibility -qt-zlib -qt-libpng -qt-libjpeg -qt-xcb -qt-pcre -qt-freetype -no-xcb -no-glib -no-cups -no-sql-sqlite -no-opengl -no-egl -no-sm -no-icu -nomake examples -nomake tests -skip qtactiveqt -skip qtlocation -skip qtmultimedia -skip qtserialport -skip qtquickcontrols -skip qtquickcontrols2 -skip qtsensors -skip qtwebengine -skip qtwebsockets -skip qtxmlpatterns -skip qt3d -skip qtvirtualkeyboard -skip qtserialbus -skip qtspeech -skip qtsvg -skip qtwebchannel -skip qtwebglplugin -skip qtwebview -skip qtxmlpatterns -skip qtpurchasing -skip qtdoc -skip qtgamepad -skip qtgraphicaleffects -skip qtimageformats -skip qtcanvas3d -skip qtcharts -skip qttools -skip qtwayland -skip qtdeclarative -opensource"

if [ -d "$1" ]; then
	cd $1
else 
	exit 1
fi

mkdir gofish-build
cd gofish-build

curl $qt_url | tar -xJf -

mkdir qt-build
cd qt-build
../qt-everywhere-src-5.13.0/configure $qt_opts
make -j8 && make install
cd ..
mkdir gf-build
git clone "https://github.com/mrrooster/gofish.git"
cd gf-build
../qt/bin/qmake ../gofish/gofish_fuse3/gofish.pro
make -j4
