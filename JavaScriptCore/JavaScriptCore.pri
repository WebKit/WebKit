# JavaScriptCore - Qt4 build info
VPATH += $$PWD
CONFIG(debug, debug|release) {
    # Output in JavaScriptCore/<config>
    JAVASCRIPTCORE_DESTDIR = debug
    # Use a config-specific target to prevent parallel builds file clashes on Mac
    JAVASCRIPTCORE_TARGET = jscored
} else {
    JAVASCRIPTCORE_DESTDIR = release
    JAVASCRIPTCORE_TARGET = jscore
}
CONFIG(standalone_package) {
    isEmpty(JSC_GENERATED_SOURCES_DIR):JSC_GENERATED_SOURCES_DIR = $$PWD/generated
} else {
    isEmpty(JSC_GENERATED_SOURCES_DIR):JSC_GENERATED_SOURCES_DIR = generated
}

CONFIG(standalone_package): DEFINES *= NDEBUG

symbian: {
    # Need to guarantee this comes before system includes of /epoc32/include
    MMP_RULES += "USERINCLUDE ../JavaScriptCore/profiler"
    LIBS += -lhal
    # For hal.h
    INCLUDEPATH *= $$MW_LAYER_SYSTEMINCLUDE
}

INCLUDEPATH = \
    $$PWD \
    $$PWD/.. \
    $$PWD/assembler \
    $$PWD/bytecode \
    $$PWD/bytecompiler \
    $$PWD/debugger \
    $$PWD/interpreter \
    $$PWD/jit \
    $$PWD/parser \
    $$PWD/pcre \
    $$PWD/profiler \
    $$PWD/runtime \
    $$PWD/wtf \
    $$PWD/wtf/symbian \
    $$PWD/wtf/unicode \
    $$PWD/yarr \
    $$PWD/API \
    $$PWD/ForwardingHeaders \
    $$JSC_GENERATED_SOURCES_DIR \
    $$INCLUDEPATH

win32-*: DEFINES += _HAS_TR1=0

DEFINES += BUILDING_QT__ BUILDING_JavaScriptCore BUILDING_WTF

contains(JAVASCRIPTCORE_JIT,yes) {
    DEFINES+=ENABLE_JIT=1
    DEFINES+=ENABLE_YARR_JIT=1
    DEFINES+=ENABLE_YARR=1
}
contains(JAVASCRIPTCORE_JIT,no) {
    DEFINES+=ENABLE_JIT=0
    DEFINES+=ENABLE_YARR_JIT=0
    DEFINES+=ENABLE_YARR=0
}

wince* {
    INCLUDEPATH += $$QT_SOURCE_TREE/src/3rdparty/ce-compat
    DEFINES += WINCEBASIC

    INCLUDEPATH += $$PWD/../JavaScriptCore/os-wince
    INCLUDEPATH += $$PWD/../JavaScriptCore/os-win32
}


defineTest(addJavaScriptCoreLib) {
    # Argument is the relative path to JavaScriptCore.pro's qmake output
    pathToJavaScriptCoreOutput = $$ARGS/$$JAVASCRIPTCORE_DESTDIR

    win32-msvc*|wince* {
        LIBS += -L$$pathToJavaScriptCoreOutput
        LIBS += -l$$JAVASCRIPTCORE_TARGET
        POST_TARGETDEPS += $${pathToJavaScriptCoreOutput}$${QMAKE_DIR_SEP}$${JAVASCRIPTCORE_TARGET}.lib
    } else:symbian {
        LIBS += -l$${JAVASCRIPTCORE_TARGET}.lib
        # The default symbian build system does not use library paths at all. However when building with
        # qmake's symbian makespec that uses Makefiles
        QMAKE_LIBDIR += $$pathToJavaScriptCoreOutput
        POST_TARGETDEPS += $${pathToJavaScriptCoreOutput}$${QMAKE_DIR_SEP}$${JAVASCRIPTCORE_TARGET}.lib
    } else {
        # Make sure jscore will be early in the list of libraries to workaround a bug in MinGW
        # that can't resolve symbols from QtCore if libjscore comes after.
        QMAKE_LIBDIR = $$pathToJavaScriptCoreOutput $$QMAKE_LIBDIR
        LIBS += -l$$JAVASCRIPTCORE_TARGET
        POST_TARGETDEPS += $${pathToJavaScriptCoreOutput}$${QMAKE_DIR_SEP}lib$${JAVASCRIPTCORE_TARGET}.a
    }

    win32-* {
        LIBS += -lwinmm
    }

    # The following line is to prevent qmake from adding jscore to libQtWebKit's prl dependencies.
    # The compromise we have to accept by disabling explicitlib is to drop support to link QtWebKit and QtScript
    # statically in applications (which isn't used often because, among other things, of licensing obstacles).
    CONFIG -= explicitlib

    export(QMAKE_LIBDIR)
    export(LIBS)
    export(POST_TARGETDEPS)
    export(CONFIG)

    return(true)
}
