TEMPLATE = lib
TARGET = dummy

CONFIG -= debug_and_release

SRC_ROOT_DIR = $$replace(PWD, /WebKitTools/WebKitTestRunner/qt, /)

wtr_fwheader_generator.commands = perl $${SRC_ROOT_DIR}/WebKitTools/Scripts/generate-forwarding-headers.pl $${SRC_ROOT_DIR}/WebKitTools/WebKitTestRunner $${OUTPUT_DIR}/include qt
wtr_fwheader_generator.depends  = $${SRC_ROOT_DIR}/WebKitTools/Scripts/generate-forwarding-headers.pl
generated_files.depends     += wtr_fwheader_generator
QMAKE_EXTRA_TARGETS         += wtr_fwheader_generator

jsc_fwheader_generator.commands = perl $${SRC_ROOT_DIR}/WebKitTools/Scripts/generate-forwarding-headers.pl $${SRC_ROOT_DIR}/JavaScriptCore/API $${OUTPUT_DIR}/include qt
jsc_fwheader_generator.depends  = $${SRC_ROOT_DIR}/WebKitTools/Scripts/generate-forwarding-headers.pl
generated_files.depends     += jsc_fwheader_generator
QMAKE_EXTRA_TARGETS         += jsc_fwheader_generator

QMAKE_EXTRA_TARGETS        += generated_files
