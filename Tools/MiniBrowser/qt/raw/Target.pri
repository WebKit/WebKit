TEMPLATE = app
TARGET = MiniBrowserRaw

HEADERS += \
    View.h

SOURCES += \
    View.cpp

DESTDIR = $${ROOT_BUILD_DIR}/bin

QT = core gui network webkit

load(features)

WEBKIT += wtf javascriptcore webkit2
