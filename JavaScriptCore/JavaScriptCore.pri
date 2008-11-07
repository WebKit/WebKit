# JavaScriptCore - Qt4 build info
VPATH += $$PWD

INCLUDEPATH += tmp
INCLUDEPATH += $$PWD $$PWD/parser $$PWD/bytecompiler $$PWD/debugger $$PWD/runtime $$PWD/wtf $$PWD/wtf/unicode $$PWD/VM $$PWD/profiler $$PWD/API $$PWD/.. \
               $$PWD/ForwardingHeaders
DEFINES += BUILDING_QT__

isEmpty(GENERATED_SOURCES_DIR):GENERATED_SOURCES_DIR = tmp
GENERATED_SOURCES_DIR_SLASH = $$GENERATED_SOURCES_DIR/
win32-*: GENERATED_SOURCES_DIR_SLASH ~= s|/|\|
win32-g++: LIBS += -lwinmm


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

KJSBISON += \
    parser/Grammar.y

SOURCES += \
    wtf/Assertions.cpp \
    wtf/HashTable.cpp \
    wtf/MainThread.cpp \
    wtf/RefCountedLeakCounter.cpp \
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
    VM/CodeBlock.cpp \
    bytecompiler/CodeGenerator.cpp \
    VM/ExceptionHelpers.cpp \
    runtime/JSPropertyNameIterator.cpp \
    VM/Machine.cpp \
    VM/Opcode.cpp \
    VM/SamplingTool.cpp \
    VM/RegisterFile.cpp

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
    runtime/DateConstructor.cpp \
    runtime/DateInstance.cpp \
    runtime/DateMath.cpp \
    runtime/DatePrototype.cpp \
    debugger/Debugger.cpp \
    debugger/DebuggerCallFrame.cpp \
    wtf/dtoa.cpp \
    runtime/Error.cpp \
    runtime/ErrorConstructor.cpp \
    runtime/ErrorInstance.cpp \
    runtime/ErrorPrototype.cpp \
    runtime/ExecState.cpp \
    runtime/FunctionConstructor.cpp \
    runtime/FunctionPrototype.cpp \
    runtime/GetterSetter.cpp \
    runtime/GlobalEvalFunction.cpp \
    runtime/Identifier.cpp \
    runtime/InternalFunction.cpp \
    runtime/Interpreter.cpp \
    runtime/JSArray.cpp \
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
    parser/nodes2string.cpp \
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
    runtime/StructureID.cpp \
    runtime/StructureIDChain.cpp \
    runtime/UString.cpp \
    profiler/HeavyProfile.cpp \
    profiler/Profile.cpp \
    profiler/ProfileGenerator.cpp \
    profiler/ProfileNode.cpp \
    profiler/Profiler.cpp \
    profiler/TreeProfile.cpp \
    wtf/FastMalloc.cpp \
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
kjsbison.output = $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.cpp
kjsbison.commands = bison -d -p kjsyy ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_BASE}.tab.c && $(MOVE) ${QMAKE_FILE_BASE}.tab.c ${QMAKE_FILE_OUT} && $(MOVE) ${QMAKE_FILE_BASE}.tab.h $$GENERATED_SOURCES_DIR/${QMAKE_FILE_BASE}.h
kjsbison.depend = ${QMAKE_FILE_NAME}
kjsbison.input = KJSBISON
kjsbison.variable_out = GENERATED_SOURCES
kjsbison.dependency_type = TYPE_C
kjsbison.CONFIG = target_predeps
addExtraCompilerWithHeader(kjsbison)
