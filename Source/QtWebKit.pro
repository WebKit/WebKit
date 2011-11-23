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
}

include(WebKit/qt/docs/docs.pri)

declarative.file = WebKit/qt/declarative/declarative.pro
declarative.makefile = Makefile.declarative
SUBDIRS += declarative

tests.file = tests.pri
SUBDIRS += tests

examples.file = WebKit/qt/examples/examples.pro
examples.CONFIG += no_default_target
examples.makefile = Makefile
SUBDIRS += examples

