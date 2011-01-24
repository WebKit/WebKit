# DerivedSources - qmake build info

CONFIG -= debug_and_release

TEMPLATE = lib
TARGET = dummy

QMAKE_EXTRA_TARGETS += generated_files

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../..
SRC_ROOT_DIR = $$replace(PWD, "/Tools/MiniBrowser", "")

!exists($$OUTPUT_DIR/MiniBrowser/qt): system($$QMAKE_MKDIR $$OUTPUT_DIR/MiniBrowser/qt)

ualist_copier.input = $$SRC_ROOT_DIR/Tools/QtTestBrowser/useragentlist.txt
ualist_copier.output = $$OUTPUT_DIR/MiniBrowser/qt/useragentlist.txt
ualist_copier.tempNames = $$ualist_copier.input $$ualist_copier.output
ualist_copier.commands = $$QMAKE_COPY $$replace(ualist_copier.tempNames, "/", $$QMAKE_DIR_SEP)
ualist_copier.depends = $$ualist_copier.input
generated_files.depends += ualist_copier
QMAKE_EXTRA_TARGETS += ualist_copier

# We have to copy the resource file to the build directory
# to use the useragentlist.txt file of QtTestBrowser without
# polluting the source tree.

qrc_copier.input = $$SRC_ROOT_DIR/Tools/MiniBrowser/MiniBrowser.qrc
qrc_copier.output = $$OUTPUT_DIR/MiniBrowser/qt/MiniBrowser.qrc
qrc_copier.tempNames = $$qrc_copier.input $$qrc_copier.output
qrc_copier.commands = $$QMAKE_COPY $$replace(qrc_copier.tempNames, "/", $$QMAKE_DIR_SEP)
qrc_copier.depends = ualist_copier $$qrc_copier.input
generated_files.depends += qrc_copier
QMAKE_EXTRA_TARGETS += qrc_copier
