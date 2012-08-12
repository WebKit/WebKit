# -------------------------------------------------------------------
# Root project file for QtWebKit
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = subdirs
CONFIG += ordered

api.file = api.pri
SUBDIRS += api

!no_webkit2 {
    webprocess.file = WebKit2/WebProcess.pro
    SUBDIRS += webprocess
    contains(DEFINES, ENABLE_PLUGIN_PROCESS=1) {
        pluginprocess.file = WebKit2/PluginProcess.pro
        SUBDIRS += pluginprocess
    }
}

include(WebKit/qt/docs/docs.pri)

declarative.file = WebKit/qt/declarative/declarative.pro
declarative.makefile = Makefile.declarative
contains(DEFINES, HAVE_QTQUICK=1): SUBDIRS += declarative

!no_webkit1 {
    contains(DEFINES, HAVE_QTTESTLIB=1) {
        tests.file = tests.pri
        SUBDIRS += tests
    }

    examples.file = WebKit/qt/examples/examples.pro
    examples.CONFIG += no_default_target
    examples.makefile = Makefile
    SUBDIRS += examples
}
