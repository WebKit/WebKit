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
                        "EPOCHEAPSIZE  0x40000 0x6000000 // Min 256kB, Max 96MB" \
                    "$${LITERAL_HASH}endif"
                    MMP_RULES += heapSizeRule
                }
            }
        }
    }
    DEPENDPATH += $$PWD/WebKit/qt/Api
}

!mac:!unix|symbian {
    DEFINES += USE_SYSTEM_MALLOC
}

CONFIG(release, debug|release) {
    DEFINES += NDEBUG
}

BASE_DIR = $$PWD

symbian {
    INCLUDEPATH += $$PWD/include/QtWebKit
} else {
    INCLUDEPATH += $$OUTPUT_DIR/include/QtWebKit
}

CONFIG -= warn_on
*-g++*:QMAKE_CXXFLAGS += -Wall -Wextra -Wreturn-type -fno-strict-aliasing -Wcast-align -Wchar-subscripts -Wformat-security -Wreturn-type -Wno-unused-parameter -Wno-sign-compare -Wno-switch -Wno-switch-enum -Wundef -Wmissing-noreturn -Winit-self

# Treat warnings as errors on x86/Linux/GCC
linux-g++*:!isEqual(QT_ARCH,arm): QMAKE_CXXFLAGS += -Werror

# Enable GNU compiler extensions to the ARM compiler for all Qt ports using RVCT
symbian|*-armcc {
    RVCT_COMMON_CFLAGS = --gnu --diag_suppress 68,111,177,368,830,1293
    RVCT_COMMON_CXXFLAGS = $$RVCT_COMMON_CFLAGS --no_parse_templates
}

*-armcc {
    QMAKE_CFLAGS += $$RVCT_COMMON_CFLAGS
    QMAKE_CXXFLAGS += $$RVCT_COMMON_CXXFLAGS
}

symbian {
    QMAKE_CXXFLAGS.ARMCC += $$RVCT_COMMON_CXXFLAGS
}

##### Defaults for some mobile platforms
symbian|maemo5|maemo6 {
    CONFIG += disable_uitools
    CONFIG += enable_fast_mobile_scrolling
    CONFIG += use_qt_mobile_theme
} else {
    CONFIG += include_webinspector
}

embedded: CONFIG += enable_fast_mobile_scrolling

####

disable_uitools: DEFINES *= QT_NO_UITOOLS

contains(DEFINES, QT_NO_UITOOLS): CONFIG -= uitools

# Disable a few warnings on Windows. The warnings are also
# disabled in WebKitLibraries/win/tools/vsprops/common.vsprops
win32-msvc*: QMAKE_CXXFLAGS += -wd4291 -wd4344 -wd4396 -wd4503 -wd4800 -wd4819 -wd4996

