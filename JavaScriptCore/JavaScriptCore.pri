# JavaScriptCore - Qt4 build info
VPATH += $$PWD

CONFIG(standalone_package) {
    isEmpty(JSC_GENERATED_SOURCES_DIR):JSC_GENERATED_SOURCES_DIR = $$PWD/generated
} else {
    isEmpty(JSC_GENERATED_SOURCES_DIR):JSC_GENERATED_SOURCES_DIR = generated
}

CONFIG(debug, debug|release) {
    OBJECTS_DIR = obj/debug
} else { # Release
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
    $$PWD/pcre \
    $$PWD/profiler \
    $$PWD/runtime \
    $$PWD/wrec \
    $$PWD/wtf \
    $$PWD/wtf/unicode \
    $$PWD/yarr \
    $$PWD/API \
    $$PWD/ForwardingHeaders \
    $$JSC_GENERATED_SOURCES_DIR \
    $$INCLUDEPATH

DEFINES += BUILDING_QT__ BUILDING_JavaScriptCore BUILDING_WTF

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
    jit/ExecutableAllocatorPosix.cpp \
    jit/ExecutableAllocatorSymbian.cpp \
    jit/ExecutableAllocatorWin.cpp \
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
    profiler/Profile.cpp \
    profiler/ProfileGenerator.cpp \
    profiler/ProfileNode.cpp \
    profiler/Profiler.cpp \
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
    runtime/MarkStackPosix.cpp \
    runtime/MarkStackSymbian.cpp \
    runtime/MarkStackWin.cpp \
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
    runtime/UStringImpl.cpp \
    wtf/Assertions.cpp \
    wtf/ByteArray.cpp \
    wtf/CurrentTime.cpp \
    wtf/DateMath.cpp \
    wtf/dtoa.cpp \
    wtf/FastMalloc.cpp \
    wtf/HashTable.cpp \
    wtf/MainThread.cpp \
    wtf/qt/MainThreadQt.cpp \
    wtf/qt/ThreadingQt.cpp \
    wtf/RandomNumber.cpp \
    wtf/RefCountedLeakCounter.cpp \
    wtf/ThreadingNone.cpp \
    wtf/Threading.cpp \
    wtf/TypeTraits.cpp \
    wtf/unicode/CollatorDefault.cpp \
    wtf/unicode/icu/CollatorICU.cpp \
    wtf/unicode/UTF8.cpp \
    yarr/RegexCompiler.cpp \
    yarr/RegexInterpreter.cpp \
    yarr/RegexJIT.cpp

# Generated files, simply list them for JavaScriptCore
SOURCES += \
    $${JSC_GENERATED_SOURCES_DIR}/Grammar.cpp

!contains(DEFINES, USE_SYSTEM_MALLOC) {
    SOURCES += wtf/TCSystemAlloc.cpp
}

