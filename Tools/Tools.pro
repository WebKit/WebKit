# -------------------------------------------------------------------
# Root project file for tools
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = subdirs
CONFIG += ordered

load(features)

SUBDIRS += QtTestBrowser/QtTestBrowser.pro
SUBDIRS += DumpRenderTree/qt/DumpRenderTree.pro
SUBDIRS += DumpRenderTree/qt/ImageDiff.pro

!no_webkit2 {
    SUBDIRS += MiniBrowser/qt/MiniBrowser.pro \
               WebKitTestRunner/WebKitTestRunner.pro
}

# FIXME: the test plugin cause some trouble during layout tests.
# See: https://bugs.webkit.org/show_bug.cgi?id=86620
# Reenable it after we have a fix for this issue.
false {
    !win32:contains(DEFINES, ENABLE_NETSCAPE_PLUGIN_API=1) {
        SUBDIRS += DumpRenderTree/qt/TestNetscapePlugin/TestNetscapePlugin.pro
    }
}

OTHER_FILES = \
    Scripts/* \
    qmake/README \
    qmake/configure.pro \
    qmake/sync.profile \
    qmake/qt_webkit.pri \
    qmake/config.tests/README \
    qmake/config.tests/fontconfig/* \
    qmake/config.tests/gccdepends/* \
    qmake/mkspecs/modules/* \
    qmake/mkspecs/features/*.prf \
    qmake/mkspecs/features/*.pri \
    qmake/mkspecs/features/mac/*.prf \
    qmake/mkspecs/features/unix/*.prf \
    qmake/mkspecs/features/win32/*.prf
