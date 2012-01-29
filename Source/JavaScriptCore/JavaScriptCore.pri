# -------------------------------------------------------------------
# This file contains shared rules used both when building
# JavaScriptCore itself, and by targets that use JavaScriptCore.
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

SOURCE_DIR = $${ROOT_WEBKIT_DIR}/Source/JavaScriptCore

JAVASCRIPTCORE_GENERATED_SOURCES_DIR = $${ROOT_BUILD_DIR}/Source/JavaScriptCore/$${GENERATED_SOURCES_DESTDIR}

INCLUDEPATH += \
    $$SOURCE_DIR \
    $$SOURCE_DIR/.. \
    $$SOURCE_DIR/assembler \
    $$SOURCE_DIR/bytecode \
    $$SOURCE_DIR/bytecompiler \
    $$SOURCE_DIR/heap \
    $$SOURCE_DIR/dfg \
    $$SOURCE_DIR/debugger \
    $$SOURCE_DIR/interpreter \
    $$SOURCE_DIR/jit \
    $$SOURCE_DIR/parser \
    $$SOURCE_DIR/profiler \
    $$SOURCE_DIR/runtime \
    $$SOURCE_DIR/tools \
    $$SOURCE_DIR/yarr \
    $$SOURCE_DIR/API \
    $$SOURCE_DIR/ForwardingHeaders \
    $$JAVASCRIPTCORE_GENERATED_SOURCES_DIR

win32-* {
    DEFINES += _HAS_TR1=0
    LIBS += -lwinmm

    win32-g++* {
        LIBS += -lpthreadGC2
    } else:win32-msvc* {
        LIBS += -lpthreadVC2
    }
}

wince* {
    INCLUDEPATH += $$QT_SOURCE_TREE/src/3rdparty/ce-compat
    INCLUDEPATH += $$SOURCE_DIR/os-win32
}
