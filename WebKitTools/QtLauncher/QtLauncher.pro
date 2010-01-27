TEMPLATE = app

SOURCES += \
    main.cpp \
    urlloader.cpp \
    utils.cpp \
    webpage.cpp \
    webview.cpp \

HEADERS += \
    urlloader.h \
    utils.h \
    webinspector.h \
    webpage.h \
    webview.h \

CONFIG -= app_bundle
CONFIG += uitools
DESTDIR = ../../bin

include(../../WebKit.pri)

QT += network
macx:QT+=xml
QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR

symbian {
    TARGET.UID3 = 0xA000E543
    TARGET.CAPABILITY = ReadUserData WriteUserData NetworkServices
}
