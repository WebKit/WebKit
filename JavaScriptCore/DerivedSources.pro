# DerivedSources - qmake build info

CONFIG -= debug_and_release

TEMPLATE = lib
TARGET = dummy

QMAKE_EXTRA_TARGETS += generated_files

CONFIG(standalone_package) {
    isEmpty(JSC_GENERATED_SOURCES_DIR):JSC_GENERATED_SOURCES_DIR = $$PWD/generated
} else {
    isEmpty(JSC_GENERATED_SOURCES_DIR):JSC_GENERATED_SOURCES_DIR = generated
}

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

RVCT_STUB_FILES += \
    jit/JITStubs.cpp

defineTest(addExtraCompiler) {
    eval($${1}.CONFIG = target_predeps no_link)
    eval($${1}.variable_out =)
    eval($${1}.dependency_type = TYPE_C)

    export($${1}.CONFIG)
    export($${1}.variable_out)
    export($${1}.dependency_type)

    QMAKE_EXTRA_COMPILERS += $$1
    generated_files.depends += compiler_$${1}_make_all
    export(QMAKE_EXTRA_COMPILERS)
    export(generated_files.depends)
    return(true)
}

# GENERATOR 1-A: LUT creator
lut.output = $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.lut.h
lut.input = LUT_FILES
lut.commands = perl $$PWD/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
lut.depend = ${QMAKE_FILE_NAME}
addExtraCompiler(lut)

# GENERATOR 1-B: particular LUT creator (for 1 file only)
keywordlut.output = $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}Lexer.lut.h
keywordlut.input = KEYWORDLUT_FILES
keywordlut.commands = perl $$PWD/create_hash_table ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
keywordlut.depend = ${QMAKE_FILE_NAME}
addExtraCompiler(keywordlut)

# GENERATOR 2: bison grammar
jscbison.output = $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.cpp
jscbison.input = JSCBISON
jscbison.commands = bison -d -p jscyy ${QMAKE_FILE_NAME} -o $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.c && $(MOVE) $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.c ${QMAKE_FILE_OUT} && $(MOVE) $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.h $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.h
jscbison.depend = ${QMAKE_FILE_NAME}
addExtraCompiler(jscbison)

# GENERATOR 3: JIT Stub functions for RVCT
rvctstubs.output = $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}Generated${QMAKE_FILE_BASE}_RVCT.h
rvctstubs.commands = perl $$PWD/create_rvct_stubs ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
rvctstubs.depend = ${QMAKE_FILE_NAME}
rvctstubs.input = RVCT_STUB_FILES
rvctstubs.CONFIG += no_link
addExtraCompiler(rvctstubs)

# GENERATOR: "chartables.c": compile and execute the chartables generator (and add it to sources)
win32-msvc*|wince*: PREPROCESSOR = "--preprocessor=\"$$QMAKE_CC /E\""
DFTABLES = $$PWD/pcre/dftables
ctgen.output = $$JSC_GENERATED_SOURCES_DIR/chartables.c
ctgen.input = DFTABLES
ctgen.commands = perl $$DFTABLES ${QMAKE_FILE_OUT} $$PREPROCESSOR
ctgen.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_JSC_GENERATED_SOURCES_DIR}${QMAKE_FILE_BASE}
addExtraCompiler(ctgen)

