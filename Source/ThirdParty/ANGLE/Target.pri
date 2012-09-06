# -------------------------------------------------------------------
# Target file for the ANGLE static library
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = lib
TARGET = ANGLE

include(ANGLE.pri)

CONFIG += staticlib

INCLUDEPATH += \
    $$SOURCE_DIR/src \
    $$SOURCE_DIR/src/compiler/preprocessor/new \
    $$SOURCE_DIR/include

HEADERS += \
    src/compiler/BaseTypes.h \
    src/compiler/BuiltInFunctionEmulator.h \
    src/compiler/Common.h \
    src/compiler/ConstantUnion.h \
    src/compiler/debug.h \
    src/compiler/depgraph/DependencyGraph.h \
    src/compiler/depgraph/DependencyGraphBuilder.h \
    src/compiler/depgraph/DependencyGraphOutput.h \
    src/compiler/DetectDiscontinuity.h \
    src/compiler/DetectRecursion.h \
    src/compiler/Diagnostics.h \
    src/compiler/DirectiveHandler.h \
    src/compiler/ExtensionBehavior.h \
    src/compiler/ForLoopUnroll.h \
    src/compiler/glslang.h \
    src/compiler/InfoSink.h \
    src/compiler/InitializeDll.h \
    src/compiler/InitializeGlobals.h \
    src/compiler/Initialize.h \
    src/compiler/InitializeParseContext.h \
    src/compiler/intermediate.h \
    src/compiler/localintermediate.h \
    src/compiler/MMap.h \
    src/compiler/MapLongVariableNames.h \
    src/compiler/osinclude.h \
    src/compiler/Pragma.h \
    src/compiler/preprocessor/atom.h \
    src/compiler/preprocessor/compile.h \
    src/compiler/preprocessor/cpp.h \
    src/compiler/preprocessor/length_limits.h \
    src/compiler/preprocessor/memory.h \
    src/compiler/preprocessor/new/Diagnostics.h \
    src/compiler/preprocessor/new/DirectiveHandler.h \
    src/compiler/preprocessor/new/DirectiveParser.h \
    src/compiler/preprocessor/new/Input.h \
    src/compiler/preprocessor/new/Lexer.h \
    src/compiler/preprocessor/new/Macro.h \
    src/compiler/preprocessor/new/MacroExpander.h \
    src/compiler/preprocessor/new/Preprocessor.h \
    src/compiler/preprocessor/new/SourceLocation.h \
    src/compiler/preprocessor/new/Token.h \
    src/compiler/preprocessor/new/Tokenizer.h \
    src/compiler/preprocessor/parser.h \
    src/compiler/preprocessor/preprocess.h \
    src/compiler/preprocessor/scanner.h \
    src/compiler/preprocessor/slglobals.h \
    src/compiler/preprocessor/symbols.h \
    src/compiler/preprocessor/tokens.h \
    src/compiler/OutputESSL.h \
    src/compiler/OutputGLSL.h \
    src/compiler/OutputGLSLBase.h \
    src/compiler/OutputHLSL.h \
    src/compiler/ParseHelper.h \
    src/compiler/PoolAlloc.h \
    src/compiler/QualifierAlive.h \
    src/compiler/RemoveTree.h \
    src/compiler/RenameFunction.h \
    src/compiler/SearchSymbol.h \
    src/compiler/ShHandle.h \
    src/compiler/SymbolTable.h \
    src/compiler/timing/RestrictFragmentShaderTiming.h \
    src/compiler/timing/RestrictVertexShaderTiming.h \
    src/compiler/TranslatorESSL.h \
    src/compiler/TranslatorGLSL.h \
    src/compiler/TranslatorHLSL.h \
    src/compiler/Types.h \
    src/compiler/UnfoldShortCircuit.h \
    src/compiler/util.h \
    src/compiler/ValidateLimitations.h \
    src/compiler/VariableInfo.h \
    src/compiler/VersionGLSL.h

SOURCES += \
    src/compiler/BuiltInFunctionEmulator.cpp \
    src/compiler/CodeGenGLSL.cpp \
    src/compiler/Compiler.cpp \
    src/compiler/debug.cpp \
    src/compiler/depgraph/DependencyGraph.cpp \
    src/compiler/depgraph/DependencyGraphBuilder.cpp \
    src/compiler/depgraph/DependencyGraphOutput.cpp \
    src/compiler/depgraph/DependencyGraphTraverse.cpp \
    src/compiler/DetectDiscontinuity.cpp \
    src/compiler/DetectRecursion.cpp \
    src/compiler/Diagnostics.cpp \
    src/compiler/DirectiveHandler.cpp \
    src/compiler/ForLoopUnroll.cpp \
    src/compiler/InfoSink.cpp \
    src/compiler/Initialize.cpp \
    src/compiler/InitializeDll.cpp \
    src/compiler/InitializeParseContext.cpp \
    src/compiler/Intermediate.cpp \
    src/compiler/intermOut.cpp \
    src/compiler/IntermTraverse.cpp \
    src/compiler/MapLongVariableNames.cpp \
    src/compiler/OutputESSL.cpp \
    src/compiler/OutputGLSL.cpp \
    src/compiler/OutputGLSLBase.cpp \
    src/compiler/OutputHLSL.cpp \
    src/compiler/parseConst.cpp \
    src/compiler/ParseHelper.cpp \
    src/compiler/PoolAlloc.cpp \
    src/compiler/QualifierAlive.cpp \
    src/compiler/RemoveTree.cpp \
    src/compiler/SearchSymbol.cpp \
    src/compiler/ShaderLang.cpp \
    src/compiler/SymbolTable.cpp \
    src/compiler/timing/RestrictFragmentShaderTiming.cpp \
    src/compiler/timing/RestrictVertexShaderTiming.cpp \
    src/compiler/TranslatorESSL.cpp \
    src/compiler/TranslatorGLSL.cpp \
    src/compiler/TranslatorHLSL.cpp \
    src/compiler/UnfoldShortCircuit.cpp \
    src/compiler/util.cpp \
    src/compiler/ValidateLimitations.cpp \
    src/compiler/VariableInfo.cpp \
    src/compiler/VersionGLSL.cpp \
    src/compiler/preprocessor/atom.c \
    src/compiler/preprocessor/cpp.c \
    src/compiler/preprocessor/cppstruct.c \
    src/compiler/preprocessor/memory.c \
    src/compiler/preprocessor/new/DiagnosticsBase.cpp \
    src/compiler/preprocessor/new/DirectiveHandlerBase.cpp \
    src/compiler/preprocessor/new/DirectiveParser.cpp \
    src/compiler/preprocessor/new/Input.cpp \
    src/compiler/preprocessor/new/Lexer.cpp \
    src/compiler/preprocessor/new/Macro.cpp \
    src/compiler/preprocessor/new/MacroExpander.cpp \
    src/compiler/preprocessor/new/Preprocessor.cpp \
    src/compiler/preprocessor/new/Token.cpp \
    src/compiler/preprocessor/scanner.c \
    src/compiler/preprocessor/symbols.c \
    src/compiler/preprocessor/tokens.c

win32: SOURCES += src/compiler/ossource_win.cpp
else: SOURCES += src/compiler/ossource_posix.cpp

# Make sure the derived sources are built
include(DerivedSources.pri)

*g++* {
    QMAKE_CXXFLAGS += -Wno-unused-variable -Wno-missing-noreturn -Wno-unused-function -Wno-reorder -Wno-error -Wno-unknown-pragmas -Wno-undef
}

# We do not need anything from Qt
QT =
