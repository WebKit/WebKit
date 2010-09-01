TEMPLATE = app
TARGET = MiniBrowser

SOURCES += \
    main.cpp \
    BrowserWindow.cpp \

HEADERS += \
    BrowserWindow.h \

CONFIG += uitools

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../..
include(../../../WebKit.pri)

INCLUDEPATH += \
    $$PWD/../../../WebKit2/ \
    $$PWD/../../../WebKit2/UIProcess/API/cpp \
    $$PWD/../../../WebKit2/UIProcess/API/C \
    $$PWD/../../../WebKit2/UIProcess/API/qt \
    $$OUTPUT_DIR/include


DESTDIR = $$OUTPUT_DIR/bin
!CONFIG(standalone_package): CONFIG -= app_bundle

QT += network
macx:QT+=xml

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
    TARGET.CAPABILITY = ReadUserData WriteUserData NetworkServices
}

contains(QT_CONFIG, opengl) {
    QT += opengl
    DEFINES += QT_CONFIGURED_WITH_OPENGL
}
