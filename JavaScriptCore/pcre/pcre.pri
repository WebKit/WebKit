# Perl Compatible Regular Expressions - Qt4 build info
VPATH += $$PWD
INCLUDEPATH += $$PWD $$OUTPUT_DIR/JavaScriptCore/kjs/tmp
DEPENDPATH += $$PWD $$OUTPUT_DIR/JavaScriptCore/kjs/tmp

SOURCES += \
    pcre_compile.c \
    pcre_exec.c \
    pcre_ord2utf8.c \
    pcre_tables.c \
    pcre_ucp_searchfuncs.c \
    pcre_xclass.c

CTGENFILE += \
    dftables.c

# GENERATOR: "chartables.c": compile and execute the chartables generator (and add it to sources)
ctgen.output = tmp/chartables.c
ctgen.commands = $$OUTPUT_DIR/JavaScriptCore/pcre/tmp/dftables ${QMAKE_FILE_OUT}
ctgen.input = CTGENFILE
ctgen.CONFIG += target_predeps no_link
ctgen.variable_out = GENERATED_SOURCES
ctgen.dependency_type = TYPE_C
ctgen.clean = ${QMAKE_FILE_OUT} tmp/${QMAKE_FILE_BASE}
QMAKE_EXTRA_COMPILERS += ctgen
