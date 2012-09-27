# -------------------------------------------------------------------
# Project file for the LLIntOffsetsExtractor binary, used to generate
# derived sources for JavaScriptCore.
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = app
TARGET = LLIntOffsetsExtractor
DESTDIR = $$OUT_PWD

QT = core # Needed for qglobal.h

defineTest(addIncludePaths) {
    # Just needed for include paths
    include(../WTF/WTF.pri)
    include(JavaScriptCore.pri)

    export(INCLUDEPATH)
}

addIncludePaths()

INPUT_FILES = $$PWD/llint/LowLevelInterpreter.asm
llint.output = LLIntDesiredOffsets.h
llint.script = $$PWD/offlineasm/generate_offset_extractor.rb
llint.input = INPUT_FILES
llint.commands = ruby $$llint.script ${QMAKE_FILE_NAME} ${QMAKE_FILE_OUT}
llint.CONFIG += no_link
QMAKE_EXTRA_COMPILERS += llint

# Compilation of this file will automatically depend on LLIntDesiredOffsets.h
# due to qmake scanning the source file for header dependencies.
SOURCES = llint/LLIntOffsetsExtractor.cpp

mac: LIBS_PRIVATE += -framework AppKit
