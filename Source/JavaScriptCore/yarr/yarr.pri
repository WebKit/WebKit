# -------------------------------------------------------------------
# Project file for YARR
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

SOURCES += \
    $$PWD/YarrInterpreter.cpp \
    $$PWD/YarrPattern.cpp \
    $$PWD/YarrSyntaxChecker.cpp \
    $$PWD/YarrCanonicalizeUCS2.cpp

# For UString.h
v8 {
    INCLUDEPATH += \
        $$PWD/.. \
        $$PWD/../runtime
}
