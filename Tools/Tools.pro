# -------------------------------------------------------------------
# Root project file for tools
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = subdirs
CONFIG += ordered

!no_webkit1 {
    SUBDIRS += QtTestBrowser/QtTestBrowser.pro
    contains(DEFINES, HAVE_QTTESTLIB=1): SUBDIRS += DumpRenderTree/qt/DumpRenderTree.pro
    SUBDIRS += DumpRenderTree/qt/ImageDiff.pro
}

!no_webkit2 {
    # WTR's InjectedBundle depends currently on WK1's DumpRenderTreeSupport
    !no_webkit1:contains(DEFINES, HAVE_QTQUICK=1):contains(DEFINES, HAVE_QTTESTLIB=1): SUBDIRS += WebKitTestRunner/WebKitTestRunner.pro

    contains(DEFINES, HAVE_QTQUICK=1): SUBDIRS += MiniBrowser/qt/MiniBrowser.pro
    SUBDIRS += MiniBrowser/qt/raw/MiniBrowserRaw.pro
}

!win32:contains(DEFINES, ENABLE_NETSCAPE_PLUGIN_API=1) {
    SUBDIRS += DumpRenderTree/qt/TestNetscapePlugin/TestNetscapePlugin.pro
}

OTHER_FILES = \
    Scripts/* \
    $$files(Scripts/webkitpy/*.py, true) \
    $$files(Scripts/webkitperl/*.p[l|m], true) \
    qmake/README \
    qmake/configure.* \
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
