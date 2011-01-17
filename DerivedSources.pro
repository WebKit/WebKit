TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
        Source/JavaScriptCore/DerivedSources.pro \
        Source/WebCore/DerivedSources.pro \
        Source/WebKit/qt/Api/DerivedSources.pro

webkit2 {
    SUBDIRS += Source/WebKit2/DerivedSources.pro \
            Tools/WebKitTestRunner/DerivedSources.pro \
            Tools/MiniBrowser/DerivedSources.pro
}

for(subpro, SUBDIRS) {
    subdir = $${dirname(subpro)}
    subtarget = $$replace(subpro, [^a-zA-Z0-9_], -)
    eval($${subtarget}.makefile = "Makefile.DerivedSources")
    eval(generated_files-$${subtarget}.commands = (cd $$subdir && $(MAKE) -f Makefile.DerivedSources generated_files))
    QMAKE_EXTRA_TARGETS += generated_files-$${subtarget}
    generated_files.depends += generated_files-$${subtarget}
}

QMAKE_EXTRA_TARGETS += generated_files
