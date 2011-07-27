# DerivedSources - qmake build info

TEMPLATE = lib
TARGET = dummy

CONFIG -= debug_and_release

QMAKE_EXTRA_TARGETS += generated_files

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

mac {
    SRC_ROOT_DIR = $$replace(PWD, /Source/WebCore, "")
    fwheader_generator.commands = perl $${SRC_ROOT_DIR}/Source/WebKit2/Scripts/generate-forwarding-headers.pl $${SRC_ROOT_DIR}/Source/WebKit2 ../include mac
    fwheader_generator.depends  = $${SRC_ROOT_DIR}/Source/WebKit2/Scripts/generate-forwarding-headers.pl
    generated_files.depends     += fwheader_generator
    QMAKE_EXTRA_TARGETS         += fwheader_generator
}

include(CodeGenerators.pri)
