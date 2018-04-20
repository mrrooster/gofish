#QT -= gui
QT += network networkauth

CONFIG += c++11 console
CONFIG -= app_bundle

unix {
  netbsd-g++ {
    message(Building for NetBSD)
    LIBS += -lrefuse
  } else {
    LIBS += -lfuse
    QMAKE_CXXFLAGS += -D_FILE_OFFSET_BITS=64
  }
}

!CONFIG(debug, debug|release) {
#    DEFINES += QT_NO_DEBUG_OUTPUT #QT_NO_WARNING_OUTPUT
}

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += main.cpp \
    google/oauth2handler.cpp \
    google/googledrive.cpp \
    fuse/fusethread.cpp \
    google/googledriveobject.cpp \
    google/googlenetworkaccessmanager.cpp \
    google/goauth2authorizationcodeflow.cpp \
    google/googledriveoperation.cpp \
    fuse/fusehandler.cpp

HEADERS += \
    google/oauth2handler.h \
    google/googledrive.h \
    fuse/fusethread.h \
    google/googledriveobject.h \
    google/googlenetworkaccessmanager.h \
    defaults.h \
    google/goauth2authorizationcodeflow.h \
    google/googledriveoperation.h \
    fuse/fusehandler.h

DISTFILES +=
