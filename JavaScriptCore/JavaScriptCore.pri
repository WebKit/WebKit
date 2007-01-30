# JavaScriptCore - Qt4 build info
VPATH += $$PWD

INCLUDEPATH += tmp
INCLUDEPATH += $$PWD $$PWD/kjs $$PWD/bindings $$PWD/bindings/c $$PWD/bindings/qt $$PWD/wtf
DEFINES -= KJS_IDENTIFIER_HIDE_GLOBALS 
DEFINES += BUILDING_QT__

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
    wtf/TCSystemAlloc.cpp \
    wtf/Assertions.cpp \
    wtf/HashTable.cpp \
    wtf/FastMalloc.cpp \
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
    bindings/qt/qt_class.cpp \
    bindings/qt/qt_instance.cpp \
    bindings/qt/qt_runtime.cpp \
    kjs/DateMath.cpp \
    kjs/JSWrapperObject.cpp \
    kjs/PropertyNameArray.cpp \
    kjs/array_object.cpp \
    kjs/bool_object.cpp \
    kjs/collector.cpp \
    kjs/Context.cpp \
    kjs/date_object.cpp \
    kjs/debugger.cpp \
    kjs/dtoa.cpp \
    kjs/error_object.cpp \
    kjs/ExecState.cpp \
    kjs/fpconst.cpp \
    kjs/function.cpp \
    kjs/function_object.cpp \
    kjs/identifier.cpp \
    kjs/internal.cpp \
    kjs/interpreter.cpp \
    kjs/JSImmediate.cpp \
    kjs/JSLock.cpp \
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
    kjs/regexp.cpp \
    kjs/regexp_object.cpp \
    kjs/scope_chain.cpp \
    kjs/string_object.cpp \
    kjs/ustring.cpp \
    kjs/value.cpp


# GENERATOR 1-A: LUT creator
lut.output = tmp/${QMAKE_FILE_BASE}.lut.h
lut.commands = perl $$PWD/kjs/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
lut.depend = ${QMAKE_FILE_NAME}
lut.input = LUT_FILES
lut.CONFIG += no_link
QMAKE_EXTRA_COMPILERS += lut

# GENERATOR 1-B: particular LUT creator (for 1 file only)
keywordlut.output = tmp/lexer.lut.h
keywordlut.commands = perl $$PWD/kjs/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
keywordlut.depend = ${QMAKE_FILE_NAME}
keywordlut.input = KEYWORDLUT_FILES
keywordlut.CONFIG += no_link
QMAKE_EXTRA_COMPILERS += keywordlut

# GENERATOR 2: bison grammar
kjsbison.output = tmp/${QMAKE_FILE_BASE}.cpp
kjsbison.commands = bison -d -p kjsyy ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_BASE}.tab.c && mv ${QMAKE_FILE_BASE}.tab.c ${QMAKE_FILE_OUT} && mv ${QMAKE_FILE_BASE}.tab.h tmp/${QMAKE_FILE_BASE}.h
kjsbison.depend = ${QMAKE_FILE_NAME}
kjsbison.input = KJSBISON
kjsbison.variable_out = GENERATED_SOURCES
kjsbison.dependency_type = TYPE_C
kjsbison.CONFIG = target_predeps
kjsbison.clean = ${QMAKE_FILE_OUT} tmp/${QMAKE_FILE_BASE}.h
QMAKE_EXTRA_COMPILERS += kjsbison
