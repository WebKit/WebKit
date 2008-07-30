# JavaScriptCore - Qt4 build info
VPATH += $$PWD

INCLUDEPATH += tmp
INCLUDEPATH += $$PWD $$PWD/kjs $$PWD/wtf $$PWD/wtf/unicode $$PWD/VM $$PWD/profiler $$PWD/API $$PWD/.. \
               $$PWD/ForwardingHeaders
DEFINES += BUILDING_QT__

isEmpty(GENERATED_SOURCES_DIR):GENERATED_SOURCES_DIR = tmp
GENERATED_SOURCES_DIR_SLASH = $$GENERATED_SOURCES_DIR/
win32-*: GENERATED_SOURCES_DIR_SLASH ~= s|/|\|


include(pcre/pcre.pri)

LUT_FILES += \
    kjs/DatePrototype.cpp \
    kjs/NumberConstructor.cpp \
    kjs/StringPrototype.cpp \
    kjs/ArrayPrototype.cpp \
    kjs/MathObject.cpp \
    kjs/RegExpConstructor.cpp \
    kjs/RegExpObject.cpp

KEYWORDLUT_FILES += \
    kjs/keywords.table

KJSBISON += \
    kjs/grammar.y

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
    kjs/InitializeThreading.cpp \
    kjs/JSGlobalData.cpp \
    kjs/JSGlobalObject.cpp \
    kjs/JSStaticScopeObject.cpp \
    kjs/JSVariableObject.cpp \
    kjs/JSActivation.cpp \
    kjs/JSNotAnObject.cpp \
    VM/CodeBlock.cpp \
    VM/CodeGenerator.cpp \
    VM/ExceptionHelpers.cpp \
    VM/JSPropertyNameIterator.cpp \
    VM/Machine.cpp \
    VM/Opcode.cpp \
    VM/SamplingTool.cpp \
    VM/RegisterFile.cpp

# AllInOneFile.cpp helps gcc analize and optimize code
# Other compilers may be able to do this at link time
SOURCES += \
    kjs/ArgList.cpp \
    kjs/Arguments.cpp \
    kjs/ArrayConstructor.cpp \
    kjs/ArrayPrototype.cpp \
    kjs/BooleanConstructor.cpp \
    kjs/BooleanObject.cpp \
    kjs/BooleanPrototype.cpp \
    kjs/CallData.cpp \
    kjs/collector.cpp \
    kjs/CommonIdentifiers.cpp \
    kjs/ConstructData.cpp \
    kjs/DateConstructor.cpp \
    kjs/DateInstance.cpp \
    kjs/DateMath.cpp \
    kjs/DatePrototype.cpp \
    kjs/debugger.cpp \
    kjs/DebuggerCallFrame.cpp \
    kjs/dtoa.cpp \
    kjs/Error.cpp \
    kjs/ErrorConstructor.cpp \
    kjs/ErrorInstance.cpp \
    kjs/ErrorPrototype.cpp \
    kjs/ExecState.cpp \
    kjs/FunctionConstructor.cpp \
    kjs/FunctionPrototype.cpp \
    kjs/GetterSetter.cpp \
    kjs/GlobalEvalFunction.cpp \
    kjs/identifier.cpp \
    kjs/IndexToNameMap.cpp \
    kjs/InternalFunction.cpp \
    kjs/interpreter.cpp \
    kjs/JSArray.cpp \
    kjs/JSCell.cpp \
    kjs/JSFunction.cpp \
    kjs/JSGlobalObjectFunctions.cpp \
    kjs/JSImmediate.cpp \
    kjs/JSLock.cpp \
    kjs/JSNumberCell.cpp \
    kjs/JSObject.cpp \
    kjs/JSString.cpp \
    kjs/JSValue.cpp \
    kjs/JSWrapperObject.cpp \
    kjs/LabelStack.cpp \
    kjs/lexer.cpp \
    kjs/lookup.cpp \
    kjs/MathObject.cpp \
    kjs/NativeErrorConstructor.cpp \
    kjs/NativeErrorPrototype.cpp \
    kjs/nodes.cpp \
    kjs/nodes2string.cpp \
    kjs/NumberConstructor.cpp \
    kjs/NumberObject.cpp \
    kjs/NumberPrototype.cpp \
    kjs/ObjectConstructor.cpp \
    kjs/ObjectPrototype.cpp \
    kjs/operations.cpp \
    kjs/Parser.cpp \
    kjs/PropertyMap.cpp \
    kjs/PropertyNameArray.cpp \
    kjs/PropertySlot.cpp \
    kjs/PrototypeFunction.cpp \
    kjs/regexp.cpp \
    kjs/RegExpConstructor.cpp \
    kjs/RegExpObject.cpp \
    kjs/RegExpPrototype.cpp \
    kjs/ScopeChain.cpp \
    kjs/StringConstructor.cpp \
    kjs/StringObject.cpp \
    kjs/StringPrototype.cpp \
    kjs/ustring.cpp \
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
lut.commands = perl $$PWD/kjs/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
lut.depend = ${QMAKE_FILE_NAME}
lut.input = LUT_FILES
lut.CONFIG += no_link
addExtraCompiler(lut)

# GENERATOR 1-B: particular LUT creator (for 1 file only)
keywordlut.output = $$GENERATED_SOURCES_DIR/lexer.lut.h
keywordlut.commands = perl $$PWD/kjs/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
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
