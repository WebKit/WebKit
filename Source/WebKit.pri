# Include file to make it easy to include WebKit into Qt projects

# Detect that we are building as a standalone package by the presence of
# either the generated files directory or as part of the Qt package through
# QTDIR_build
CONFIG(QTDIR_build): CONFIG += standalone_package
else:exists($$PWD/WebCore/generated): CONFIG += standalone_package

CONFIG += depend_includepath
DEPENDPATH += $$OUT_PWD

DEFINES += BUILDING_QT__=1
building-libs {
    win32-msvc*|win32-icc: INCLUDEPATH += $$PWD/JavaScriptCore/os-win32
} else {
    CONFIG(QTDIR_build) {
        QT += webkit
    } else {
        QMAKE_LIBDIR = $$OUTPUT_DIR/lib $$QMAKE_LIBDIR
        QTWEBKITLIBNAME = QtWebKit
        mac:!static:contains(QT_CONFIG, qt_framework):!CONFIG(webkit_no_framework) {
            LIBS += -framework $$QTWEBKITLIBNAME
            QMAKE_FRAMEWORKPATH = $$OUTPUT_DIR/lib $$QMAKE_FRAMEWORKPATH
        } else {
            build_pass: win32-*|wince* {
                !CONFIG(release, debug|release): QTWEBKITLIBNAME = $${QTWEBKITLIBNAME}d
                QTWEBKITLIBNAME = $${QTWEBKITLIBNAME}$${QT_MAJOR_VERSION}
                win32-g++*: LIBS += -l$$QTWEBKITLIBNAME
                else: LIBS += $${QTWEBKITLIBNAME}.lib
            } else {
                LIBS += -lQtWebKit
                symbian {
                    TARGET.EPOCSTACKSIZE = 0x14000 // 80 kB
                    # For EXEs only: set heap to usable value
                    TARGET.EPOCHEAPSIZE =
                    heapSizeRule = \
                    "$${LITERAL_HASH}ifdef WINSCW" \
                        "EPOCHEAPSIZE  0x40000 0x2000000 // Min 256kB, Max 32MB" \
                    "$${LITERAL_HASH}else" \
                        "EPOCHEAPSIZE  0x40000 0x10000000 // Min 256kB, Max 256MB" \
                    "$${LITERAL_HASH}endif"
                    MMP_RULES += heapSizeRule
                }
            }
        }
    }
    DEPENDPATH += $$PWD/WebKit/qt/Api
}

CONFIG(release, debug|release) {
    DEFINES += NDEBUG
}

INCLUDEPATH += \
    $$OUTPUT_DIR/include/QtWebKit \
    $$OUTPUT_DIR/include
INCLUDEPATH += $$QT.script.includes

webkit2 {
    INCLUDEPATH += $$OUTPUT_DIR/include/WebKit2
    # FIXME: Once the public header are well defined for WebKit2, this must go away.
    INCLUDEPATH += $$PWD/WebKit2/
}

CONFIG -= warn_on
*-g++*:QMAKE_CXXFLAGS += -Wall -Wextra -Wreturn-type -fno-strict-aliasing -Wcast-align -Wchar-subscripts -Wformat-security -Wreturn-type -Wno-unused-parameter -Wno-sign-compare -Wno-switch -Wno-switch-enum -Wundef -Wmissing-noreturn -Winit-self

# Treat warnings as errors on x86/Linux/GCC
linux-g++* {
    isEqual(QT_ARCH,x86_64)|isEqual(QT_ARCH,i386): QMAKE_CXXFLAGS += -Werror
}

symbian|*-armcc {
    # Enable GNU compiler extensions to the ARM compiler for all Qt ports using RVCT
    RVCT_COMMON_CFLAGS = --gnu --diag_suppress 68,111,177,368,830,1293
    RVCT_COMMON_CXXFLAGS = $$RVCT_COMMON_CFLAGS --no_parse_templates
    # Make debug symbols leaner in RVCT4.x. Ignored by compiler for release builds
    QMAKE_CXXFLAGS.ARMCC_4_0 += --remove_unneeded_entities
}

*-armcc {
    QMAKE_CFLAGS += $$RVCT_COMMON_CFLAGS
    QMAKE_CXXFLAGS += $$RVCT_COMMON_CXXFLAGS
}

symbian {
    QMAKE_CXXFLAGS.ARMCC += $$RVCT_COMMON_CXXFLAGS
}

valgrind {
    contains(JAVASCRIPTCORE_JIT,yes): error("'JAVASCRIPTCORE_JIT=yes' not supported with valgrind")
    QMAKE_CXXFLAGS += -g
    QMAKE_LFLAGS += -g
    DEFINES += USE_SYSTEM_MALLOC=1
    DEFINES += ENABLE_JIT=0
    JAVASCRIPTCORE_JIT = no
}

# Disable dependency to a specific version of a Qt package for non-production builds
symbian:!CONFIG(production):default_deployment.pkg_prerules -= pkg_depends_qt

##### Defaults for some mobile platforms
symbian|maemo5|maemo6 {
    CONFIG += disable_uitools
    CONFIG += enable_fast_mobile_scrolling
    CONFIG += use_qt_mobile_theme
    maemo6: CONFIG += include_webinspector
} else {
    CONFIG += include_webinspector
}

####

contains(QT_CONFIG, modular):!contains(QT_CONFIG, uitools)|disable_uitools: DEFINES *= QT_NO_UITOOLS

!contains(QT_CONFIG, modular) {
    $$QT.phonon.includes = $QMAKE_INCDIR_QT/phonon
    $$QT.phonon.libs = $$QMAKE_LIBDIR_QT
}

# Disable a few warnings on Windows. The warnings are also
# disabled in WebKitLibraries/win/tools/vsprops/common.vsprops
win32-msvc*|wince*: QMAKE_CXXFLAGS += -wd4291 -wd4344 -wd4396 -wd4503 -wd4800 -wd4819 -wd4996

CONFIG(qt_minimal) {
    DEFINES *= QT_NO_ANIMATION
    DEFINES *= QT_NO_BEARERMANAGEMENT
    DEFINES *= QT_NO_CLIPBOARD
    DEFINES *= QT_NO_COMBOBOX
    DEFINES *= QT_NO_CONCURRENT
    DEFINES *= QT_NO_CRASHHANDLER
    DEFINES *= QT_NO_CURSOR
    DEFINES *= QT_NO_DESKTOPSERVICES
    DEFINES *= QT_NO_FILEDIALOG
    DEFINES *= QT_NO_GRAPHICSEFFECT
    DEFINES *= QT_NO_IM
    DEFINES *= QT_NO_INPUTDIALOG
    DEFINES *= QT_NO_LINEEDIT
    DEFINES *= QT_NO_MESSAGEBOX
    DEFINES *= QT_NO_OPENSSL
    DEFINES *= QT_NO_PRINTER
    DEFINES *= QT_NO_QUUID_STRING
    DEFINES *= QT_NO_SHORTCUT
    DEFINES *= QT_NO_STYLE_STYLESHEET
    DEFINES *= QT_NO_SYSTEMTRAYICON
    DEFINES *= QT_NO_TEMPORARYFILE
    DEFINES *= QT_NO_TOOLTIP
    DEFINES *= QT_NO_UITOOLS
    DEFINES *= QT_NO_UNDOCOMMAND
    DEFINES *= QT_NO_UNDOSTACK
    DEFINES *= QT_NO_XRENDER
}

contains(DEFINES, QT_NO_UITOOLS): CONFIG -= uitools
