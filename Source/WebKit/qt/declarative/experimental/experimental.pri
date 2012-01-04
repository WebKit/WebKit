# -------------------------------------------------------------------
# Project file for the Qt Quick (QML) experimental API plugin
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = lib
TARGET  = qmlwebkitexperimentalplugin

TARGET.module_name = QtWebKit/experimental

CONFIG += qt plugin

QMLDIRFILE = $${_PRO_FILE_PWD_}/qmldir
copy2build.input = QMLDIRFILE
copy2build.output = $${ROOT_BUILD_DIR}/imports/$${TARGET.module_name}/qmldir
!contains(TEMPLATE_PREFIX, vc):copy2build.variable_out = PRE_TARGETDEPS
copy2build.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
copy2build.name = COPY ${QMAKE_FILE_IN}
copy2build.CONFIG += no_link
QMAKE_EXTRA_COMPILERS += copy2build

contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols

wince*:LIBS += $$QMAKE_LIBS_GUI

CONFIG += qtwebkit qtwebkit-private

QT += declarative widgets network

DESTDIR = $${ROOT_BUILD_DIR}/imports/$${TARGET.module_name}

CONFIG += rpath
RPATHDIR_RELATIVE_TO_DESTDIR = ../../lib

SOURCES += plugin.cpp

DEFINES += HAVE_WEBKIT2
INCLUDEPATH += ../../../../WebKit2/UIProcess/API/qt

target.path = $$[QT_INSTALL_IMPORTS]/$${TARGET.module_name}


qmldir.files += $$PWD/qmldir
qmldir.path +=  $$[QT_INSTALL_IMPORTS]/$${TARGET.module_name}

INSTALLS += target qmldir


