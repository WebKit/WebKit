# DerivedSources - qmake build info

CONFIG -= debug_and_release

TEMPLATE = lib
TARGET = dummy

QMAKE_EXTRA_TARGETS += generated_files

GENERATED_SOURCES_DIR = generated

IDL_BINDINGS += \
    InjectedBundle/Bindings/EventSendingController.idl \
    InjectedBundle/Bindings/GCController.idl \
    InjectedBundle/Bindings/LayoutTestController.idl \

defineTest(addExtraCompiler) {
    eval($${1}.CONFIG = target_predeps no_link)
    eval($${1}.variable_out =)
    eval($${1}.dependency_type = TYPE_C)

    wkScript = $$eval($${1}.wkScript)
    eval($${1}.depends += $$wkScript)

    export($${1}.CONFIG)
    export($${1}.variable_out)
    export($${1}.dependency_type)
    export($${1}.depends)

    QMAKE_EXTRA_COMPILERS += $$1
    generated_files.depends += compiler_$${1}_make_all
    export(QMAKE_EXTRA_COMPILERS)
    export(generated_files.depends)
    return(true)
}

SRC_ROOT_DIR = $$replace(PWD, /Tools/WebKitTestRunner, "")

# Make sure forwarded headers needed by this project are present
fwheader_generator.commands = perl $${SRC_ROOT_DIR}/Source/WebKit2/Scripts/generate-forwarding-headers.pl $${SRC_ROOT_DIR}/Tools/WebKitTestRunner $${OUTPUT_DIR}/include qt
fwheader_generator.depends  = $${SRC_ROOT_DIR}/Source/WebKit2/Scripts/generate-forwarding-headers.pl
generated_files.depends     += fwheader_generator
QMAKE_EXTRA_TARGETS         += fwheader_generator

# GENERATOR 1: IDL compiler
idl.output = $${GENERATED_SOURCES_DIR}/JS${QMAKE_FILE_BASE}.cpp
idl.input = IDL_BINDINGS
idl.wkScript = $$PWD/../../Source/WebCore/bindings/scripts/generate-bindings.pl
idl.commands = perl -I$$PWD/../../Source/WebCore/bindings/scripts -I$$PWD/InjectedBundle/Bindings $$idl.wkScript --defines \"\" --generator TestRunner --include $$PWD/InjectedBundle/Bindings --outputDir $$GENERATED_SOURCES_DIR --preprocessor \"$${QMAKE_MOC} -E\" ${QMAKE_FILE_NAME}
idl.depends = $$PWD/../../Source/WebCore/bindings/scripts/CodeGenerator.pm \
              $$PWD/InjectedBundle/Bindings/CodeGeneratorTestRunner.pm \
              $$PWD/../../Source/WebCore/bindings/scripts/IDLParser.pm \
              $$PWD/../../Source/WebCore/bindings/scripts/IDLStructure.pm \
              $$PWD/../../Source/WebCore/bindings/scripts/InFilesParser.pm \
              $$PWD/../../Source/WebCore/bindings/scripts/generate-bindings.pl
addExtraCompiler(idl)

