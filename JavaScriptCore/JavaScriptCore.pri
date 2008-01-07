# JavaScriptCore - Qt4 build info
VPATH += $$PWD

INCLUDEPATH += tmp
INCLUDEPATH += $$PWD $$PWD/kjs $$PWD/bindings $$PWD/bindings/c $$PWD/wtf
DEPENDPATH += $$PWD $$PWD/kjs $$PWD/bindings $$PWD/bindings/c $$PWD/wtf
DEFINES -= KJS_IDENTIFIER_HIDE_GLOBALS 
qt-port:INCLUDEPATH += $$PWD/bindings/qt
qt-port:DEFINES += BUILDING_QT__
gtk-port:DEFINES += BUILDING_GTK__

# http://bugs.webkit.org/show_bug.cgi?id=16406
# [Gtk] JavaScriptCore needs -lpthread
gtk-port:!win32-*:LIBS += -lpthread

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
    wtf/unicode/UTF8.cpp \
    bindings/NP_jsobject.cpp \
    bindings/npruntime.cpp \
    bindings/runtime_array.cpp \
    bindings/runtime.cpp \
    bindings/runtime_method.cpp \
    bindings/runtime_object.cpp \
    bindings/runtime_root.cpp \
    bindings/c/c_class.cpp \
    bindings/c/c_instance.cpp \
    bindings/c/c_runtime.cpp \
    bindings/c/c_utility.cpp \
    API/JSBase.cpp \
    API/JSCallbackConstructor.cpp \
    API/JSCallbackFunction.cpp \
    API/JSCallbackObject.cpp \
    API/JSClassRef.cpp \
    API/JSContextRef.cpp \
    API/JSObjectRef.cpp \
    API/JSStringRef.cpp \
    API/JSValueRef.cpp \
    kjs/JSGlobalObject.cpp \
    kjs/JSVariableObject.cpp

# AllInOneFile.cpp helps gcc analize and optimize code
# Other compilers may be able to do this at link time
gtk-port:CONFIG(release) {
SOURCES += \
    kjs/AllInOneFile.cpp
} else {
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
    wtf/FastMalloc.cpp

!qt-port:SOURCES += \
    wtf/TCSystemAlloc.cpp
}

qt-port:SOURCES += \
    bindings/qt/qt_class.cpp \
    bindings/qt/qt_instance.cpp \
    bindings/qt/qt_runtime.cpp

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
