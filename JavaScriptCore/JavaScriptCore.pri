# JavaScriptCore - Qt4 build info
VPATH += $$PWD

INCLUDEPATH += tmp
INCLUDEPATH += $$PWD $$PWD/parser $$PWD/bytecompiler $$PWD/debugger $$PWD/runtime $$PWD/wtf $$PWD/wtf/unicode $$PWD/interpreter $$PWD/jit $$PWD/profiler $$PWD/wrec $$PWD/API $$PWD/.. \
               $$PWD/ForwardingHeaders $$PWD/bytecode $$PWD/assembler
DEFINES += BUILDING_QT__

isEmpty(GENERATED_SOURCES_DIR):GENERATED_SOURCES_DIR = tmp
GENERATED_SOURCES_DIR_SLASH = $$GENERATED_SOURCES_DIR/
win32-* {
    GENERATED_SOURCES_DIR_SLASH ~= s|/|\|
    LIBS += -lwinmm
}

# Disable the JIT due to numerous observed miscompilations :(
CONFIG(release):isEqual(QT_ARCH,i386) {
     JIT_DEFINES = ENABLE_JIT ENABLE_WREC ENABLE_JIT_OPTIMIZE_CALL ENABLE_JIT_OPTIMIZE_PROPERTY_ACCESS ENABLE_JIT_OPTIMIZE_ARITHMETIC
     # Require gcc >= 4.1
     linux-g++*:greaterThan(QT_GCC_MAJOR_VERSION,3):greaterThan(QT_GCC_MINOR_VERSION,0) {
         DEFINES += $$JIT_DEFINES WTF_USE_JIT_STUB_ARGUMENT_VA_LIST
         QMAKE_CXXFLAGS += -fno-stack-protector
         QMAKE_CFLAGS += -fno-stack-protector
     }
     win32-msvc* {
         DEFINES += $$JIT_DEFINES WTF_USE_JIT_STUB_ARGUMENT_REGISTER
     }
}

include(pcre/pcre.pri)

LUT_FILES += \
    runtime/DatePrototype.cpp \
    runtime/NumberConstructor.cpp \
    runtime/StringPrototype.cpp \
    runtime/ArrayPrototype.cpp \
    runtime/MathObject.cpp \
    runtime/RegExpConstructor.cpp \
    runtime/RegExpObject.cpp

KEYWORDLUT_FILES += \
    parser/Keywords.table

JSCBISON += \
    parser/Grammar.y

SOURCES += \
    wtf/Assertions.cpp \
    wtf/ByteArray.cpp \
    wtf/HashTable.cpp \
    wtf/MainThread.cpp \
    wtf/RandomNumber.cpp \
    wtf/RefCountedLeakCounter.cpp \
    wtf/TypeTraits.cpp \
    wtf/unicode/CollatorDefault.cpp \
    wtf/unicode/icu/CollatorICU.cpp \
    wtf/unicode/UTF8.cpp \
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
    runtime/InitializeThreading.cpp \
    runtime/JSGlobalData.cpp \
    runtime/JSGlobalObject.cpp \
    runtime/JSStaticScopeObject.cpp \
    runtime/JSVariableObject.cpp \
    runtime/JSActivation.cpp \
    runtime/JSNotAnObject.cpp \
    runtime/TimeoutChecker.cpp \
    bytecode/CodeBlock.cpp \
    bytecode/StructureStubInfo.cpp \
    bytecode/JumpTable.cpp \
    jit/JIT.cpp \
    jit/JITCall.cpp \
    jit/JITArithmetic.cpp \
    jit/JITPropertyAccess.cpp \
    jit/ExecutableAllocator.cpp \
    jit/JITStubs.cpp \
    bytecompiler/BytecodeGenerator.cpp \
    runtime/ExceptionHelpers.cpp \
    runtime/JSPropertyNameIterator.cpp \
    interpreter/Interpreter.cpp \
    bytecode/Opcode.cpp \
    bytecode/SamplingTool.cpp \
    wrec/CharacterClass.cpp \
    wrec/CharacterClassConstructor.cpp \
    wrec/WREC.cpp \
    wrec/WRECFunctors.cpp \
    wrec/WRECGenerator.cpp \
    wrec/WRECParser.cpp \
    interpreter/RegisterFile.cpp

win32-*: SOURCES += jit/ExecutableAllocatorWin.cpp
else: SOURCES += jit/ExecutableAllocatorPosix.cpp

# AllInOneFile.cpp helps gcc analize and optimize code
# Other compilers may be able to do this at link time
SOURCES += \
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
    runtime/ConstructData.cpp \
    wtf/CurrentTime.cpp \
    runtime/DateConstructor.cpp \
    runtime/DateInstance.cpp \
    runtime/DateMath.cpp \
    runtime/DatePrototype.cpp \
    debugger/Debugger.cpp \
    debugger/DebuggerCallFrame.cpp \
    debugger/DebuggerActivation.cpp \
    wtf/dtoa.cpp \
    runtime/Error.cpp \
    runtime/ErrorConstructor.cpp \
    runtime/ErrorInstance.cpp \
    runtime/ErrorPrototype.cpp \
    interpreter/CallFrame.cpp \
    runtime/FunctionConstructor.cpp \
    runtime/FunctionPrototype.cpp \
    runtime/GetterSetter.cpp \
    runtime/GlobalEvalFunction.cpp \
    runtime/Identifier.cpp \
    runtime/InternalFunction.cpp \
    runtime/Completion.cpp \
    runtime/JSArray.cpp \
    runtime/JSByteArray.cpp \
    runtime/JSCell.cpp \
    runtime/JSFunction.cpp \
    runtime/JSGlobalObjectFunctions.cpp \
    runtime/JSImmediate.cpp \
    runtime/JSLock.cpp \
    runtime/JSNumberCell.cpp \
    runtime/JSObject.cpp \
    runtime/JSString.cpp \
    runtime/JSValue.cpp \
    runtime/JSWrapperObject.cpp \
    parser/Lexer.cpp \
    runtime/Lookup.cpp \
    runtime/MathObject.cpp \
    runtime/NativeErrorConstructor.cpp \
    runtime/NativeErrorPrototype.cpp \
    parser/Nodes.cpp \
    runtime/NumberConstructor.cpp \
    runtime/NumberObject.cpp \
    runtime/NumberPrototype.cpp \
    runtime/ObjectConstructor.cpp \
    runtime/ObjectPrototype.cpp \
    runtime/Operations.cpp \
    parser/Parser.cpp \
    runtime/PropertyNameArray.cpp \
    runtime/PropertySlot.cpp \
    runtime/PrototypeFunction.cpp \
    runtime/RegExp.cpp \
    runtime/RegExpConstructor.cpp \
    runtime/RegExpObject.cpp \
    runtime/RegExpPrototype.cpp \
    runtime/ScopeChain.cpp \
    runtime/SmallStrings.cpp \
    runtime/StringConstructor.cpp \
    runtime/StringObject.cpp \
    runtime/StringPrototype.cpp \
    runtime/Structure.cpp \
    runtime/StructureChain.cpp \
    runtime/UString.cpp \
    profiler/HeavyProfile.cpp \
    profiler/Profile.cpp \
    profiler/ProfileGenerator.cpp \
    profiler/ProfileNode.cpp \
    profiler/Profiler.cpp \
    profiler/TreeProfile.cpp \
    wtf/FastMalloc.cpp \
    wtf/Threading.cpp \
    wtf/ThreadingQt.cpp \
    wtf/qt/MainThreadQt.cpp

# GENERATOR 1-A: LUT creator
lut.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.lut.h
lut.commands = perl $$PWD/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
lut.depend = ${QMAKE_FILE_NAME}
lut.input = LUT_FILES
lut.CONFIG += no_link
addExtraCompiler(lut)

# GENERATOR 1-B: particular LUT creator (for 1 file only)
keywordlut.output = $$GENERATED_SOURCES_DIR/Lexer.lut.h
keywordlut.commands = perl $$PWD/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
keywordlut.depend = ${QMAKE_FILE_NAME}
keywordlut.input = KEYWORDLUT_FILES
keywordlut.CONFIG += no_link
addExtraCompiler(keywordlut)

# GENERATOR 2: bison grammar
jscbison.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.cpp
jscbison.commands = bison -d -p jscyy ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_BASE}.tab.c && $(MOVE) ${QMAKE_FILE_BASE}.tab.c ${QMAKE_FILE_OUT} && $(MOVE) ${QMAKE_FILE_BASE}.tab.h $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.h
jscbison.depend = ${QMAKE_FILE_NAME}
jscbison.input = JSCBISON
jscbison.variable_out = GENERATED_SOURCES
jscbison.dependency_type = TYPE_C
jscbison.CONFIG = target_predeps
addExtraCompilerWithHeader(jscbison)

