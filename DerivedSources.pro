TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
        JavaScriptCore/DerivedSources.pro \
        WebCore/DerivedSources.pro

for(subpro, SUBDIRS) {
    subdir = $${dirname(subpro)}
    subtarget = $$replace(subdir, [^A-Za-z0-9], _)
    eval(generated_files-$${subtarget}.commands = (cd $$subdir && $(MAKE) -f $(MAKEFILE).DerivedSources generated_files))
    QMAKE_EXTRA_TARGETS += generated_files-$${subtarget}
    generated_files.depends += generated_files-$${subtarget}
}

QMAKE_EXTRA_TARGETS += generated_files
