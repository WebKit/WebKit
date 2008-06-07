# JavaScriptCore - Qt4 build info
VPATH += $$PWD

INCLUDEPATH += tmp
INCLUDEPATH += $$PWD $$PWD/kjs $$PWD/wtf $$PWD/wtf/unicode $$PWD/VM $$PWD/profiler
DEPENDPATH += $$PWD $$PWD/kjs $$PWD/wtf $$PWD/wtf/unicode $$PWD/VM $$PWD/profiler
DEFINES -= KJS_IDENTIFIER_HIDE_GLOBALS 
DEFINES += BUILDING_QT__

win32-msvc*: INCLUDEPATH += $$PWD/os-win32

isEmpty(GENERATED_SOURCES_DIR):GENERATED_SOURCES_DIR = tmp

include(pcre/pcre.pri)

LUT_FILES += \
    kjs/date_object.cpp \
    kjs/number_object.cpp \
    kjs/string_object.cpp \
    kjs/array_object.cpp \
    kjs/math_object.cpp \
    kjs/regexp_object.cpp

KEYWORDLUT_FILES += \
    kjs/keywords.table

KJSBISON += \
    kjs/grammar.y

SOURCES += \
    wtf/Assertions.cpp \
    wtf/HashTable.cpp \
    wtf/MainThread.cpp \
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
    kjs/JSVariableObject.cpp \
    kjs/JSActivation.cpp \
    kjs/JSNotAnObject.cpp \
    VM/CodeBlock.cpp \
    VM/CodeGenerator.cpp \
    VM/ExceptionHelpers.cpp \
    VM/Instruction.cpp \
    VM/JSPropertyNameIterator.cpp \
    VM/LabelID.cpp \
    VM/Machine.cpp \
    VM/Opcode.cpp \
    VM/Register.cpp \
    VM/RegisterFile.cpp \
    VM/RegisterFileStack.cpp \
    VM/RegisterID.cpp

# AllInOneFile.cpp helps gcc analize and optimize code
# Other compilers may be able to do this at link time
SOURCES += \
    kjs/function.cpp \
    kjs/debugger.cpp \
    kjs/array_instance.cpp \
    kjs/array_object.cpp \
    kjs/bool_object.cpp \
    kjs/collector.cpp \
    kjs/CommonIdentifiers.cpp \
    kjs/date_object.cpp \
    kjs/DateMath.cpp \
    kjs/DebuggerCallFrame.cpp \
    kjs/dtoa.cpp \
    kjs/error_object.cpp \
    kjs/ExecState.cpp \
    kjs/function_object.cpp \
    kjs/identifier.cpp \
    kjs/internal.cpp \
    kjs/interpreter.cpp \
    kjs/JSImmediate.cpp \
    kjs/JSLock.cpp \
    kjs/JSWrapperObject.cpp \
    kjs/lexer.cpp \
    kjs/list.cpp \
    kjs/lookup.cpp \
    kjs/math_object.cpp \
    kjs/nodes.cpp \
    kjs/nodes2string.cpp \
    kjs/number_object.cpp \
    kjs/object.cpp \
    kjs/object_object.cpp \
    kjs/operations.cpp \
    kjs/Parser.cpp \
    kjs/property_map.cpp \
    kjs/property_slot.cpp \
    kjs/PropertyNameArray.cpp \
    kjs/regexp.cpp \
    kjs/regexp_object.cpp \
    kjs/scope_chain.cpp \
    kjs/string_object.cpp \
    kjs/ustring.cpp \
    kjs/value.cpp \
    profiler/ProfileNode.cpp \
    profiler/Profile.cpp \
    profiler/Profiler.cpp \
    wtf/FastMalloc.cpp \
    wtf/ThreadingQt.cpp \
    wtf/qt/MainThreadQt.cpp

!CONFIG(QTDIR_build) {
    defineTest(addExtraCompiler) {
        QMAKE_EXTRA_COMPILERS += $$1
        generated_files.depends += compiler_$${1}_make_all
        export(QMAKE_EXTRA_COMPILERS)
        export(generated_files.depends)
        return(true)
    }
}

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
kjsbison.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_GENERATED_SOURCES_DIR}${QMAKE_FILE_BASE}.h
addExtraCompiler(kjsbison)
