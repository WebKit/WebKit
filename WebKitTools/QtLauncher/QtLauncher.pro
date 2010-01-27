TEMPLATE = app

SOURCES += \
    main.cpp \
    webpage.cpp \
    urlloader.cpp \
    utils.cpp \

HEADERS += \
    webinspector.h \
    webpage.h \
    urlloader.h \
    utils.h \

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
