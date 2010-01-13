# DerivedSources - qmake build info

TEMPLATE = lib
TARGET = dummy

QMAKE_EXTRA_TARGETS += generated_files

defineTest(addExtraCompiler) {
    eval($${1}.CONFIG = target_predeps no_link)
    eval($${1}.variable_out =)
    eval($${1}.dependency_type = TYPE_C)

    export($${1}.CONFIG)
    export($${1}.variable_out)
    export($${1}.dependency_type)

    QMAKE_EXTRA_COMPILERS += $$1
    generated_files.depends += compiler_$${1}_make_all
    export(QMAKE_EXTRA_COMPILERS)
    export(generated_files.depends)
    return(true)
}

include(WebCore.pri)

