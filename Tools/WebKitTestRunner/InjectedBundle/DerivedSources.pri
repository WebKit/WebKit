# -------------------------------------------------------------------
# Derived sources for WebKitTestRunner's InjectedBundle
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

# This file is both a top level target, and included from Target.pri,
# so that the resulting generated sources can be added to SOURCES.
# We only set the template if we're a top level target, so that we
# don't override what Target.pri has already set.
equals(_FILE_, $$_PRO_FILE_): TEMPLATE = derived

load(features)

IDL_BINDINGS += \
    Bindings/EventSendingController.idl \
    Bindings/GCController.idl \
    Bindings/LayoutTestController.idl \
    Bindings/TextInputController.idl \

# GENERATOR 1: IDL compiler
idl.output = JS${QMAKE_FILE_BASE}.cpp
idl.input = IDL_BINDINGS
idl.script = $${ROOT_WEBKIT_DIR}/Source/WebCore/bindings/scripts/generate-bindings.pl
idl.commands = perl -I$${ROOT_WEBKIT_DIR}/Source/WebCore/bindings/scripts -I$$PWD/Bindings $$idl.script --defines \"$${FEATURE_DEFINES_JAVASCRIPT}\" --generator TestRunner --include $$PWD/Bindings --outputDir ${QMAKE_FUNC_FILE_OUT_PATH} --preprocessor \"$${QMAKE_MOC} -E\" ${QMAKE_FILE_NAME}
idl.depends = $${ROOT_WEBKIT_DIR}/Source/WebCore/bindings/scripts/CodeGenerator.pm \
              $$PWD/Bindings/CodeGeneratorTestRunner.pm \
              $${ROOT_WEBKIT_DIR}/Source/WebCore/bindings/scripts/IDLParser.pm \
              $${ROOT_WEBKIT_DIR}/Source/WebCore/bindings/scripts/IDLStructure.pm \
              $${ROOT_WEBKIT_DIR}/Source/WebCore/bindings/scripts/InFilesParser.pm \
              $${ROOT_WEBKIT_DIR}/Source/WebCore/bindings/scripts/generate-bindings.pl
GENERATORS += idl

INCLUDEPATH += $$buildDirForSource(Tools/WebKitTestRunner/InjectedBundle)/$${GENERATED_SOURCES_DESTDIR}

