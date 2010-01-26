TARGET     = QtScript
TEMPLATE   = lib
QT         = core

INCLUDEPATH += $$PWD

CONFIG += building-libs

isEmpty(JSC_GENERATED_SOURCES_DIR):JSC_GENERATED_SOURCES_DIR = ../../generated
CONFIG(debug, debug|release) {
    OBJECTS_DIR = obj/debug
} else { # Release
    OBJECTS_DIR = obj/release
}

include($$PWD/../../../WebKit.pri)
include($$PWD/../../JavaScriptCore.pri)

INCLUDEPATH += $$PWD/../../API

SOURCES +=  $$PWD/qscriptengine.cpp \
            $$PWD/qscriptengine_p.cpp \
            $$PWD/qscriptvalue.cpp \

HEADERS +=  $$PWD/qtscriptglobal.h \
            $$PWD/qscriptengine.h \
            $$PWD/qscriptengine_p.h \
            $$PWD/qscriptvalue.h \
            $$PWD/qscriptvalue_p.h \
            $$PWD/qscriptconverter_p.h \


!static: DEFINES += QT_MAKEDLL

DESTDIR = $$OUTPUT_DIR/lib

