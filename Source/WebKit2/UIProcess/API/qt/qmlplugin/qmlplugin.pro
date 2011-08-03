TEMPLATE = lib
TARGET = webkit2qmlplugin
TARGETPATH = QtWebKit/experimental
CONFIG += qt plugin

SOURCES += plugin.cpp

include(../../../../../WebKit.pri)
QT += declarative

QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR


# From WK1 qml module. Copies the qmldir file to the build directory,
# so we can use it in place without installing.
QMLDIRFILE = $${_PRO_FILE_PWD_}/qmldir
copy2build.input = QMLDIRFILE
CONFIG(QTDIR_build) {
    copy2build.output = $$QT_BUILD_TREE/imports/$$TARGETPATH/qmldir
} else {
    copy2build.output = $$OUTPUT_DIR/imports/$$TARGETPATH/qmldir
}
!contains(TEMPLATE_PREFIX, vc):copy2build.variable_out = PRE_TARGETDEPS
copy2build.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
copy2build.name = COPY ${QMAKE_FILE_IN}
copy2build.CONFIG += no_link
QMAKE_EXTRA_COMPILERS += copy2build


CONFIG(QTDIR_build) {
    DESTDIR = $$QT_BUILD_TREE/imports/$$TARGETPATH
} else {
    DESTDIR = $$OUTPUT_DIR/imports/$$TARGETPATH
}
target.path = $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

qmldir.files += $$PWD/qmldir
qmldir.path +=  $$[QT_INSTALL_IMPORTS]/$$TARGETPATH

INSTALLS += target qmldir
