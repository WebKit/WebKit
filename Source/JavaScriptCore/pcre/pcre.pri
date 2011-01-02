# Perl Compatible Regular Expressions - Qt4 build info
VPATH += $$PWD
INCLUDEPATH += $$PWD $$OUTPUT_DIR/JavaScriptCore/tmp
DEPENDPATH += $$PWD

SOURCES += \
    pcre_compile.cpp \
    pcre_exec.cpp \
    pcre_tables.cpp \
    pcre_ucp_searchfuncs.cpp \
    pcre_xclass.cpp

