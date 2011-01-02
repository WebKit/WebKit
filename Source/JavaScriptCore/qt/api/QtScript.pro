TARGET     = QtScript
TEMPLATE   = lib
QT         = core

INCLUDEPATH += $$PWD

CONFIG += building-libs

isEmpty(JSC_GENERATED_SOURCES_DIR):JSC_GENERATED_SOURCES_DIR = ../../generated
!CONFIG(release, debug|release) {
    OBJECTS_DIR = obj/debug
} else { # Release
    OBJECTS_DIR = obj/release
}

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../..
include($$PWD/../../../WebKit.pri)

include($$PWD/../../JavaScriptCore.pri)
addJavaScriptCoreLib(../..)

INCLUDEPATH += $$PWD/../../API

SOURCES +=  $$PWD/qscriptengine.cpp \
            $$PWD/qscriptengine_p.cpp \
            $$PWD/qscriptvalue.cpp \
            $$PWD/qscriptvalueiterator.cpp \
            $$PWD/qscriptstring.cpp \
            $$PWD/qscriptprogram.cpp \
            $$PWD/qscriptsyntaxcheckresult.cpp \
            $$PWD/qscriptfunction.cpp

HEADERS +=  $$PWD/qtscriptglobal.h \
            $$PWD/qscriptengine.h \
            $$PWD/qscriptengine_p.h \
            $$PWD/qscriptvalue.h \
            $$PWD/qscriptvalue_p.h \
            $$PWD/qscriptvalueiterator.h \
            $$PWD/qscriptvalueiterator_p.h \
            $$PWD/qscriptconverter_p.h \
            $$PWD/qscriptstring.h \
            $$PWD/qscriptstring_p.h \
            $$PWD/qscriptprogram.h \
            $$PWD/qscriptprogram_p.h \
            $$PWD/qscriptsyntaxcheckresult.h \
            $$PWD/qscriptoriginalglobalobject_p.h \
            $$PWD/qscriptfunction_p.h

!static: DEFINES += QT_MAKEDLL

DESTDIR = $$OUTPUT_DIR/lib
