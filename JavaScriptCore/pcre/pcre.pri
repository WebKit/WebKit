# Perl Compatible Regular Expressions - Qt4 build info
VPATH += $$PWD
INCLUDEPATH += $$PWD $$OUTPUT_DIR/JavaScriptCore/kjs/tmp

SOURCES += \
    pcre_get.c \
    pcre_refcount.c \
    pcre_ucp_findchar.c \
    pcre_compile.c \
    pcre_globals.c \
    pcre_config.c \
    pcre_version.c \
    pcre_info.c \
    pcre_study.c \
    pcre_exec.c \
    pcre_xclass.c \
    pcre_tables.c \
    pcre_maketables.c \
    pcre_try_flipped.c \
    pcre_ord2utf8.c \
    pcre_fullinfo.c

CTGENFILE += \
    dftables.c


# GENERATOR: "chartables.c": compile and execute the chartables generator (and add it to sources)
ctgen.output = tmp/chartables.c
ctgen.commands = gcc ${QMAKE_FILE_NAME} -I$$PWD/../wtf -o tmp/${QMAKE_FILE_BASE} && ./tmp/${QMAKE_FILE_BASE} ${QMAKE_FILE_OUT}
qt-port:ctgen.commands = gcc ${QMAKE_FILE_NAME} -DBUILDING_QT__ -I$$PWD/../wtf -o tmp/${QMAKE_FILE_BASE} && ./tmp/${QMAKE_FILE_BASE} ${QMAKE_FILE_OUT}
gdk-port:ctgen.commands = gcc ${QMAKE_FILE_NAME} -DBUILDING_GDK__ -DBUILDING_CAIRO__ -I$$PWD/../wtf -o tmp/${QMAKE_FILE_BASE} && ./tmp/${QMAKE_FILE_BASE} ${QMAKE_FILE_OUT}
ctgen.input = CTGENFILE
ctgen.CONFIG += target_predeps no_link
ctgen.variable_out = GENERATED_SOURCES
ctgen.dependency_type = TYPE_C
ctgen.clean = ${QMAKE_FILE_OUT} tmp/${QMAKE_FILE_BASE}
QMAKE_EXTRA_COMPILERS += ctgen
