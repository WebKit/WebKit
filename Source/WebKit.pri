# Include file to make it easy to include WebKit into Qt projects

contains(QT_CONFIG, qpa)|contains(QT_CONFIG, embedded): CONFIG += embedded

# For convenience
greaterThan(QT_MAJOR_VERSION, 4): CONFIG += qt5

!CONFIG(release, debug|release) {
    OBJECTS_DIR = obj/debug
} else { # Release
    OBJECTS_DIR = obj/release
    DEFINES *= NDEBUG
}

# Make sure that build_all follows the build_all config in WebCore
mac:contains(QT_CONFIG, qt_framework):!CONFIG(webkit_no_framework):!build_pass:CONFIG += build_all

#We don't want verify and other platform macros to pollute the namespace
mac: DEFINES += __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES=0

CONFIG += depend_includepath
DEPENDPATH += $$OUT_PWD

DEFINES += BUILDING_QT__=1
building-libs {
    win32-msvc*|win32-icc: INCLUDEPATH += $$PWD/JavaScriptCore/os-win32
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
        }
    }
    DEPENDPATH += $$PWD/WebKit/qt/Api
}

CONFIG(release, debug|release) {
    DEFINES += NDEBUG
}

INCLUDEPATH += \
    $$PWD \
    $$OUTPUT_DIR/include/QtWebKit \
    $$OUTPUT_DIR/include \
    $$QT.script.includes

DEFINES += QT_ASCII_CAST_WARNINGS

webkit2:INCLUDEPATH *= $$OUTPUT_DIR/include/WebKit2

# Pick up 3rdparty libraries from INCLUDE/LIB just like with MSVC
win32-g++* {
    TMPPATH            = $$quote($$(INCLUDE))
    QMAKE_INCDIR_POST += $$split(TMPPATH,";")
    TMPPATH            = $$quote($$(LIB))
    QMAKE_LIBDIR_POST += $$split(TMPPATH,";")
}

CONFIG -= warn_on
*-g++*:QMAKE_CXXFLAGS += -Wall -Wextra -Wreturn-type -fno-strict-aliasing -Wchar-subscripts -Wformat-security -Wreturn-type -Wno-unused-parameter -Wno-sign-compare -Wno-switch -Wno-switch-enum -Wundef -Wmissing-noreturn -Winit-self

# Treat warnings as errors on x86/Linux/GCC
linux-g++* {
    isEqual(QT_ARCH,x86_64)|isEqual(QT_ARCH,i386): QMAKE_CXXFLAGS += -Werror

    greaterThan(QT_GCC_MAJOR_VERSION, 3):greaterThan(QT_GCC_MINOR_VERSION, 5) {
        if (!contains(QMAKE_CXXFLAGS, -std=c++0x) && !contains(QMAKE_CXXFLAGS, -std=gnu++0x)) {
            # We need to deactivate those warnings because some names conflicts with upcoming c++0x types (e.g.nullptr).
            QMAKE_CFLAGS_WARN_ON += -Wno-c++0x-compat
            QMAKE_CXXFLAGS_WARN_ON += -Wno-c++0x-compat
            QMAKE_CFLAGS += -Wno-c++0x-compat
            QMAKE_CXXFLAGS += -Wno-c++0x-compat
        }
    }
}

valgrind {
    contains(JAVASCRIPTCORE_JIT,yes): error("'JAVASCRIPTCORE_JIT=yes' not supported with valgrind")
    QMAKE_CXXFLAGS += -g
    QMAKE_LFLAGS += -g
    DEFINES += USE_SYSTEM_MALLOC=1
    DEFINES += ENABLE_JIT=0
    JAVASCRIPTCORE_JIT = no
}

contains(JAVASCRIPTCORE_JIT,yes): DEFINES+=ENABLE_JIT=1
contains(JAVASCRIPTCORE_JIT,no): DEFINES+=ENABLE_JIT=0

linux-g++ {
isEmpty($$(SBOX_DPKG_INST_ARCH)):exists(/usr/bin/ld.gold) {
    message(Using gold linker)
    QMAKE_LFLAGS+=-fuse-ld=gold
}
}


##### Defaults
CONFIG += include_webinspector

*sh4* {
    CONFIG += disable_uitools
}
####

disable_uitools: DEFINES *= QT_NO_UITOOLS

# Disable a few warnings on Windows. The warnings are also
# disabled in WebKitLibraries/win/tools/vsprops/common.vsprops
win32-msvc*|wince*: QMAKE_CXXFLAGS += -wd4291 -wd4344 -wd4396 -wd4503 -wd4800 -wd4819 -wd4996
win32-icc: QMAKE_CXXFLAGS += -wd873

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
