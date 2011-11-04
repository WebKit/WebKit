# -------------------------------------------------------------------
# Project file for the QtTestBrowser binary
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = app

SOURCES += \
    locationedit.cpp \
    launcherwindow.cpp \
    main.cpp \
    mainwindow.cpp \
    urlloader.cpp \
    utils.cpp \
    webpage.cpp \
    webview.cpp \
    fpstimer.cpp \
    cookiejar.cpp

HEADERS += \
    locationedit.h \
    launcherwindow.h \
    mainwindow.h \
    urlloader.h \
    utils.h \
    webinspector.h \
    webpage.h \
    webview.h \
    fpstimer.h \
    cookiejar.h

greaterThan(QT_MAJOR_VERSION, 4):isEmpty(QT.uitools.name) {
    message("QtUiTools library not found. QWidget plugin loading will be disabled")
    DEFINES += QT_NO_UITOOLS
} else {
    CONFIG += uitools
}

load(webcore)

CONFIG += qtwebkit

DESTDIR = $$ROOT_BUILD_DIR/bin

QT += network
macx:QT += xml
haveQt(5): QT += printsupport

!embedded: PKGCONFIG += fontconfig

contains(QT_CONFIG, opengl): QT += opengl

RESOURCES += \
    QtTestBrowser.qrc
