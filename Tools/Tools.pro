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
    SUBDIRS += MiniBrowser/qt/MiniBrowser.pro
    linux-g++*: SUBDIRS += WebKitTestRunner/WebKitTestRunner.pro
}

!win32:contains(DEFINES, ENABLE_NETSCAPE_PLUGIN_API=1) {
    SUBDIRS += DumpRenderTree/qt/TestNetscapePlugin/TestNetscapePlugin.pro
}

OTHER_FILES = Scripts/*
