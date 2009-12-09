# JavaScriptCore - Qt4 build info
VPATH += $$PWD

CONFIG(debug, debug|release) {
    isEmpty(GENERATED_SOURCES_DIR):GENERATED_SOURCES_DIR = generated$${QMAKE_DIR_SEP}debug
    OBJECTS_DIR = obj/debug
} else { # Release
    isEmpty(GENERATED_SOURCES_DIR):GENERATED_SOURCES_DIR = generated$${QMAKE_DIR_SEP}release
    OBJECTS_DIR = obj/release
}

symbian: {
    # Need to guarantee this comes before system includes of /epoc32/include
    MMP_RULES += "USERINCLUDE ../JavaScriptCore/profiler"
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
    $$PWD/profiler \
    $$PWD/runtime \
    $$PWD/wrec \
    $$PWD/wtf \
    $$PWD/wtf/unicode \
    $$PWD/yarr \
    $$PWD/API \
    $$PWD/ForwardingHeaders \
    $$GENERATED_SOURCES_DIR \
    $$INCLUDEPATH

DEFINES += BUILDING_QT__ BUILDING_JavaScriptCore BUILDING_WTF

GENERATED_SOURCES_DIR_SLASH = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}
win32-* {
    LIBS += -lwinmm
}
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

# In debug mode JIT disabled until crash fixed
win32-* {
    CONFIG(debug):!contains(DEFINES, ENABLE_JIT=1): DEFINES+=ENABLE_JIT=0
}

# Rules when JIT enabled (not disabled)
!contains(DEFINES, ENABLE_JIT=0) {
    linux*-g++*:greaterThan(QT_GCC_MAJOR_VERSION,3):greaterThan(QT_GCC_MINOR_VERSION,0) {
        QMAKE_CXXFLAGS += -fno-stack-protector
        QMAKE_CFLAGS += -fno-stack-protector
    }
}

wince* {
    INCLUDEPATH += $$QT_SOURCE_TREE/src/3rdparty/ce-compat
    SOURCES += $$QT_SOURCE_TREE/src/3rdparty/ce-compat/ce_time.c
    DEFINES += WINCEBASIC
}

include(pcre/pcre.pri)

LUT_FILES += \
    runtime/ArrayPrototype.cpp \
    runtime/DatePrototype.cpp \
    runtime/JSONObject.cpp \
    runtime/MathObject.cpp \
    runtime/NumberConstructor.cpp \
    runtime/RegExpConstructor.cpp \
    runtime/RegExpObject.cpp \
    runtime/StringPrototype.cpp

KEYWORDLUT_FILES += \
    parser/Keywords.table

JSCBISON += \
    parser/Grammar.y

SOURCES += \
    API/JSBase.cpp \
    API/JSCallbackConstructor.cpp \
    API/JSCallbackFunction.cpp \
    API/JSCallbackObject.cpp \
    API/JSClassRef.cpp \
    API/JSContextRef.cpp \
    API/JSObjectRef.cpp \
    API/JSStringRef.cpp \
    API/JSValueRef.cpp \
    API/OpaqueJSString.cpp \
    assembler/ARMAssembler.cpp \
    assembler/MacroAssemblerARM.cpp \
    bytecode/CodeBlock.cpp \
    bytecode/JumpTable.cpp \
    bytecode/Opcode.cpp \
    bytecode/SamplingTool.cpp \
    bytecode/StructureStubInfo.cpp \
    bytecompiler/BytecodeGenerator.cpp \
    bytecompiler/NodesCodegen.cpp \
    debugger/DebuggerActivation.cpp \
    debugger/DebuggerCallFrame.cpp \
    debugger/Debugger.cpp \
    interpreter/CallFrame.cpp \
    interpreter/Interpreter.cpp \
    interpreter/RegisterFile.cpp \
    jit/ExecutableAllocator.cpp \
    jit/JITArithmetic.cpp \
    jit/JITCall.cpp \
    jit/JIT.cpp \
    jit/JITOpcodes.cpp \
    jit/JITPropertyAccess.cpp \
    jit/JITStubs.cpp \
    parser/Lexer.cpp \
    parser/Nodes.cpp \
    parser/ParserArena.cpp \
    parser/Parser.cpp \
    profiler/HeavyProfile.cpp \
    profiler/Profile.cpp \
    profiler/ProfileGenerator.cpp \
    profiler/ProfileNode.cpp \
    profiler/Profiler.cpp \
    profiler/TreeProfile.cpp \
    runtime/ArgList.cpp \
    runtime/Arguments.cpp \
    runtime/ArrayConstructor.cpp \
    runtime/ArrayPrototype.cpp \
    runtime/BooleanConstructor.cpp \
    runtime/BooleanObject.cpp \
    runtime/BooleanPrototype.cpp \
    runtime/CallData.cpp \
    runtime/Collector.cpp \
    runtime/CommonIdentifiers.cpp \
    runtime/Completion.cpp \
    runtime/ConstructData.cpp \
    runtime/DateConstructor.cpp \
    runtime/DateConversion.cpp \
    runtime/DateInstance.cpp \
    runtime/DatePrototype.cpp \
    runtime/ErrorConstructor.cpp \
    runtime/Error.cpp \
    runtime/ErrorInstance.cpp \
    runtime/ErrorPrototype.cpp \
    runtime/ExceptionHelpers.cpp \
    runtime/Executable.cpp \
    runtime/FunctionConstructor.cpp \
    runtime/FunctionPrototype.cpp \
    runtime/GetterSetter.cpp \
    runtime/GlobalEvalFunction.cpp \
    runtime/Identifier.cpp \
    runtime/InitializeThreading.cpp \
    runtime/InternalFunction.cpp \
    runtime/JSActivation.cpp \
    runtime/JSAPIValueWrapper.cpp \
    runtime/JSArray.cpp \
    runtime/JSByteArray.cpp \
    runtime/JSCell.cpp \
    runtime/JSFunction.cpp \
    runtime/JSGlobalData.cpp \
    runtime/JSGlobalObject.cpp \
    runtime/JSGlobalObjectFunctions.cpp \
    runtime/JSImmediate.cpp \
    runtime/JSLock.cpp \
    runtime/JSNotAnObject.cpp \
    runtime/JSNumberCell.cpp \
    runtime/JSObject.cpp \
    runtime/JSONObject.cpp \
    runtime/JSPropertyNameIterator.cpp \
    runtime/JSStaticScopeObject.cpp \
    runtime/JSString.cpp \
    runtime/JSValue.cpp \
    runtime/JSVariableObject.cpp \
    runtime/JSWrapperObject.cpp \
    runtime/LiteralParser.cpp \
    runtime/Lookup.cpp \
    runtime/MarkStack.cpp \
    runtime/MathObject.cpp \
    runtime/NativeErrorConstructor.cpp \
    runtime/NativeErrorPrototype.cpp \
    runtime/NumberConstructor.cpp \
    runtime/NumberObject.cpp \
    runtime/NumberPrototype.cpp \
    runtime/ObjectConstructor.cpp \
    runtime/ObjectPrototype.cpp \
    runtime/Operations.cpp \
    runtime/PropertyDescriptor.cpp \
    runtime/PropertyNameArray.cpp \
    runtime/PropertySlot.cpp \
    runtime/PrototypeFunction.cpp \
    runtime/RegExpConstructor.cpp \
    runtime/RegExp.cpp \
    runtime/RegExpObject.cpp \
    runtime/RegExpPrototype.cpp \
    runtime/ScopeChain.cpp \
    runtime/SmallStrings.cpp \
    runtime/StringConstructor.cpp \
    runtime/StringObject.cpp \
    runtime/StringPrototype.cpp \
    runtime/StructureChain.cpp \
    runtime/Structure.cpp \
    runtime/TimeoutChecker.cpp \
    runtime/UString.cpp \
    wtf/Assertions.cpp \
    wtf/ByteArray.cpp \
    wtf/CurrentTime.cpp \
    wtf/DateMath.cpp \
    wtf/dtoa.cpp \
    wtf/FastMalloc.cpp \
    wtf/HashTable.cpp \
    wtf/MainThread.cpp \
    wtf/qt/MainThreadQt.cpp \
    wtf/RandomNumber.cpp \
    wtf/RefCountedLeakCounter.cpp \
    wtf/Threading.cpp \
    wtf/TypeTraits.cpp \
    wtf/unicode/CollatorDefault.cpp \
    wtf/unicode/icu/CollatorICU.cpp \
    wtf/unicode/UTF8.cpp \
    yarr/RegexCompiler.cpp \
    yarr/RegexInterpreter.cpp \
    yarr/RegexJIT.cpp

symbian {
    SOURCES += jit/ExecutableAllocatorSymbian.cpp \
              runtime/MarkStackSymbian.cpp
} else {
    win32-*|wince* {
        SOURCES += jit/ExecutableAllocatorWin.cpp \
                  runtime/MarkStackWin.cpp
    } else {
        SOURCES += jit/ExecutableAllocatorPosix.cpp \
                  runtime/MarkStackPosix.cpp
    }
}

!contains(DEFINES, USE_SYSTEM_MALLOC) {
    SOURCES += wtf/TCSystemAlloc.cpp
}

!contains(DEFINES, ENABLE_SINGLE_THREADED=1) {
    SOURCES += wtf/qt/ThreadingQt.cpp
} else {
    DEFINES += ENABLE_JSC_MULTIPLE_THREADS=0
    SOURCES += wtf/ThreadingNone.cpp
}

# GENERATOR 1-A: LUT creator
lut.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.lut.h
lut.commands = perl $$PWD/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
lut.depend = ${QMAKE_FILE_NAME}
lut.input = LUT_FILES
lut.CONFIG += no_link
addExtraCompiler(lut)

# GENERATOR 1-B: particular LUT creator (for 1 file only)
keywordlut.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}Lexer.lut.h
keywordlut.commands = perl $$PWD/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
keywordlut.depend = ${QMAKE_FILE_NAME}
keywordlut.input = KEYWORDLUT_FILES
keywordlut.CONFIG += no_link
addExtraCompiler(keywordlut)

# GENERATOR 2: bison grammar
jscbison.output = $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.cpp
jscbison.commands = bison -d -p jscyy ${QMAKE_FILE_NAME} -o $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.c && $(MOVE) $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.c ${QMAKE_FILE_OUT} && $(MOVE) $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.h $${GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.h
jscbison.depend = ${QMAKE_FILE_NAME}
jscbison.input = JSCBISON
jscbison.variable_out = GENERATED_SOURCES
jscbison.dependency_type = TYPE_C
jscbison.CONFIG = target_predeps
addExtraCompilerWithHeader(jscbison)

