# -------------------------------------------------------------------
# Project file for the WebKit2 web process binary
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = app

TARGET = QtWebProcess
DESTDIR = $${ROOT_BUILD_DIR}/bin

SOURCES += qt/MainQt.cpp

QT += network webkitwidgets
macx: QT += xml

haveQtModule(widgets): QT += widgets

INSTALLS += target

isEmpty(INSTALL_BINS) {
    target.path = $$[QT_INSTALL_BINS]
} else {
    target.path = $$INSTALL_BINS
}


