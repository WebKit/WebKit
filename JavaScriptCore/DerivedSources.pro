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

    wkScript = $$eval($${1}.wkScript)
    eval($${1}.depends += $$wkScript)

    export($${1}.CONFIG)
    export($${1}.variable_out)
    export($${1}.dependency_type)
    export($${1}.depends)

    QMAKE_EXTRA_COMPILERS += $$1
    generated_files.depends += compiler_$${1}_make_all
    export(QMAKE_EXTRA_COMPILERS)
    export(generated_files.depends)
    return(true)
}

# GENERATOR 1-A: LUT creator
lut.output = $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.lut.h
lut.input = LUT_FILES
lut.wkScript = $$PWD/create_hash_table
lut.commands = perl $$lut.wkScript ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
lut.depends = ${QMAKE_FILE_NAME}
addExtraCompiler(lut)

# GENERATOR 1-B: particular LUT creator (for 1 file only)
keywordlut.output = $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}Lexer.lut.h
keywordlut.input = KEYWORDLUT_FILES
keywordlut.wkScript = $$PWD/create_hash_table
keywordlut.commands = perl $$keywordlut.wkScript ${QMAKE_FILE_NAME} -i > ${QMAKE_FILE_OUT}
keywordlut.depends = ${QMAKE_FILE_NAME}
addExtraCompiler(keywordlut)

# GENERATOR 2: bison grammar
jscbison.output = $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.cpp
jscbison.input = JSCBISON
jscbison.commands = bison -d -p jscyy ${QMAKE_FILE_NAME} -o $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.c && $(MOVE) $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.c ${QMAKE_FILE_OUT} && $(MOVE) $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.tab.h $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}${QMAKE_FILE_BASE}.h
jscbison.depends = ${QMAKE_FILE_NAME}
addExtraCompiler(jscbison)

# GENERATOR 3: JIT Stub functions for RVCT
rvctstubs.output = $${JSC_GENERATED_SOURCES_DIR}$${QMAKE_DIR_SEP}Generated${QMAKE_FILE_BASE}_RVCT.h
rvctstubs.wkScript = $$PWD/create_jit_stubs
rvctstubs.commands = perl -i $$rvctstubs.wkScript --prefix RVCT ${QMAKE_FILE_NAME} > ${QMAKE_FILE_OUT}
rvctstubs.depends = ${QMAKE_FILE_NAME}
rvctstubs.input = RVCT_STUB_FILES
rvctstubs.CONFIG += no_link
addExtraCompiler(rvctstubs)

# GENERATOR: "chartables.c": compile and execute the chartables generator (and add it to sources)
win32-msvc*|wince*: PREPROCESSOR = "--preprocessor=\"$$QMAKE_CC /E\""
ctgen.output = $$JSC_GENERATED_SOURCES_DIR/chartables.c
ctgen.wkScript = $$PWD/pcre/dftables
ctgen.input = ctgen.wkScript
ctgen.commands = perl $$ctgen.wkScript ${QMAKE_FILE_OUT} $$PREPROCESSOR
ctgen.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_JSC_GENERATED_SOURCES_DIR}${QMAKE_FILE_BASE}
addExtraCompiler(ctgen)

#GENERATOR: "RegExpJitTables.h": tables used by Yarr
retgen.output = $$JSC_GENERATED_SOURCES_DIR/RegExpJitTables.h
retgen.wkScript = $$PWD/create_regex_tables 
retgen.input = retgen.wkScript
retgen.commands = python $$retgen.wkScript > ${QMAKE_FILE_OUT}
addExtraCompiler(retgen)
