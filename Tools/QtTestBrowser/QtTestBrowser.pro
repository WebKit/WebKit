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

!isEqual(QT_ARCH,sh4): CONFIG += uitools

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../..
include(../../Source/WebKit.pri)
INCLUDEPATH += ../../Source/WebKit/qt/WebCoreSupport

DESTDIR = $$OUTPUT_DIR/bin
!CONFIG(standalone_package): CONFIG -= app_bundle

QT += network
macx:QT+=xml

unix:!mac:!symbian:!embedded {
    CONFIG += link_pkgconfig
    PKGCONFIG += fontconfig
}

linux-* {
    # From Creator's src/rpath.pri:
    # Do the rpath by hand since it's not possible to use ORIGIN in QMAKE_RPATHDIR
    # this expands to $ORIGIN (after qmake and make), it does NOT read a qmake var.
    QMAKE_RPATHDIR = \$\$ORIGIN/../lib $$QMAKE_RPATHDIR
    MY_RPATH = $$join(QMAKE_RPATHDIR, ":")

    QMAKE_LFLAGS += -Wl,-z,origin \'-Wl,-rpath,$${MY_RPATH}\'
    QMAKE_RPATHDIR =
} else {
    QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
}

symbian {
    TARGET.UID3 = 0xA000E543
    TARGET.CAPABILITY = ReadUserData WriteUserData NetworkServices Location
    MMP_RULES *= pageddata
}

contains(QT_CONFIG, opengl) {
    QT += opengl
    DEFINES += QT_CONFIGURED_WITH_OPENGL
}

DEFINES -= QT_ASCII_CAST_WARNINGS

RESOURCES += \
    QtTestBrowser.qrc
