# Include file to make it easy to include WebKit into Qt projects


isEmpty(OUTPUT_DIR) {
    CONFIG(release):OUTPUT_DIR=$$PWD/WebKitBuild/Release
    CONFIG(debug):OUTPUT_DIR=$$PWD/WebKitBuild/Debug
}

DEFINES += BUILDING_QT__=1
building-libs {
    win32-msvc*: INCLUDEPATH += $$PWD/JavaScriptCore/os-win32
} else {
    CONFIG(QTDIR_build) {
        QT += webkit
    } else {
        QMAKE_LIBDIR = $$OUTPUT_DIR/lib $$QMAKE_LIBDIR
        mac:!static:contains(QT_CONFIG, qt_framework):!CONFIG(webkit_no_framework) {
            LIBS += -framework QtWebKit
            QMAKE_FRAMEWORKPATH = $$OUTPUT_DIR/lib $$QMAKE_FRAMEWORKPATH
        } else {
            LIBS += -lQtWebKit
        }
    }
    DEPENDPATH += $$PWD/WebKit/qt/Api
}

DEFINES += USE_SYSTEM_MALLOC
CONFIG(release) {
    DEFINES += NDEBUG
}

BASE_DIR = $$PWD
INCLUDEPATH += $$PWD/WebKit/qt/Api

#
# For builds inside Qt we interpret the output rule and the input of each extra compiler manually
# and add the resulting sources to the SOURCES variable, because the build inside Qt contains already
# all the generated files. We do not need to generate any extra compiler rules in that case.
#
# In addition this function adds a new target called 'generated_files' that allows manually calling
# all the extra compilers to generate all the necessary files for the build using 'make generated_files'
#
defineTest(addExtraCompiler) {
    CONFIG(QTDIR_build) {
        outputRule = $$eval($${1}.output)
        outVariable = $$eval($${1}.variable_out)
        !isEqual(outVariable,GENERATED_SOURCES):return(true)

        input = $$eval($${1}.input)
        input = $$eval($$input)

        for(file,input) {
            base = $$basename(file)
            base ~= s/\..+//
            newfile=$$replace(outputRule,\\$\\{QMAKE_FILE_BASE\\},$$base)
            SOURCES += $$newfile
        }

        export(SOURCES)
    } else {
        QMAKE_EXTRA_COMPILERS += $$1
        generated_files.depends += compiler_$${1}_make_all
        export(QMAKE_EXTRA_COMPILERS)
        export(generated_files.depends)
    }
    return(true)
}

defineTest(addExtraCompilerWithHeader) {
    addExtraCompiler($$1)

    eval(headerFile = $${2})
    isEmpty(headerFile) {
        eval($${1}_header.output = $$eval($${1}.output))
        eval($${1}_header.output ~= s/\.cpp/.h/)
        eval($${1}_header.output ~= s/\.c/.h/)
    } else {
        eval($${1}_header.output = $$headerFile)
    }

    eval($${1}_header.input = $$eval($${1}.input))
    eval($${1}_header.commands = @echo -n '')
    eval($${1}_header.depends = compiler_$${1}_make_all)
    eval($${1}_header.variable_out = GENERATED_FILES)

    export($${1}_header.output)
    export($${1}_header.input)
    export($${1}_header.commands)
    export($${1}_header.depends)
    export($${1}_header.variable_out)

    !CONFIG(QTDIR_build): QMAKE_EXTRA_COMPILERS += $${1}_header

    export(QMAKE_EXTRA_COMPILERS)
    export(generated_files.depends)
    export(SOURCES)

    return(true)
}

