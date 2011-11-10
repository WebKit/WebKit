# -------------------------------------------------------------------
# Project file for the WebKit2 MiniBrowser binary
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = app

SOURCES += \
    BrowserWindow.cpp \
    main.cpp \
    MiniBrowserApplication.cpp \
    UrlLoader.cpp \
    utils.cpp \

HEADERS += \
    BrowserWindow.h \
    MiniBrowserApplication.h \
    UrlLoader.h \
    utils.h \

TARGET = MiniBrowser
DESTDIR = $${ROOT_BUILD_DIR}/bin

CONFIG += qtwebkit

# FIXME: When webkit-private works let's use it.
load(javascriptcore)
load(webcore)
load(webkit2)

QT += network declarative widgets
macx: QT += xml

RESOURCES += MiniBrowser.qrc

OTHER_FILES += \
    qml/BrowserWindow.qml \
    qml/DesktopView.qml \
    qml/TouchView.qml
