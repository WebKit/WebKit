# Perl Compatible Regular Expressions - Qt4 build info
VPATH += $$PWD
INCLUDEPATH += $$PWD $$OUTPUT_DIR/JavaScriptCore/kjs/tmp
DEPENDPATH += $$PWD

SOURCES += \
    pcre_compile.cpp \
    pcre_exec.cpp \
    pcre_ord2utf8.cpp \
    pcre_tables.cpp \
    pcre_ucp_searchfuncs.cpp \
    pcre_xclass.cpp

CTGENFILE += \
    dftables.cpp

# GENERATOR: "chartables.c": compile and execute the chartables generator (and add it to sources)
ctgen.output = tmp/chartables.c
ctgen.commands = $$OUTPUT_DIR/JavaScriptCore/pcre/tmp/dftables ${QMAKE_FILE_OUT}
ctgen.input = CTGENFILE
ctgen.CONFIG += target_predeps no_link
ctgen.variable_out = GENERATED_SOURCES
ctgen.dependency_type = TYPE_C
ctgen.clean = ${QMAKE_FILE_OUT} ${QMAKE_VAR_OBJECTS_DIR_WTR}${QMAKE_FILE_BASE}
QMAKE_EXTRA_COMPILERS += ctgen
