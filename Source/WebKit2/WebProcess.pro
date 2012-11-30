# -------------------------------------------------------------------
# Project file for the WebKit2 web process binary
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = app

TARGET = QtWebProcess
DESTDIR = $${ROOT_BUILD_DIR}/bin

SOURCES += qt/MainQt.cpp

QT += network webkit
macx: QT += xml

haveQtModule(widgets): QT += widgets webkitwidgets

build?(webkit1): DEFINES += HAVE_WEBKIT1

INSTALLS += target

isEmpty(INSTALL_BINS) {
    use?(libexecdir): target.path = $$[QT_INSTALL_LIBEXECS]
    else: target.path = $$[QT_INSTALL_BINS]
} else {
    target.path = $$INSTALL_BINS
}


