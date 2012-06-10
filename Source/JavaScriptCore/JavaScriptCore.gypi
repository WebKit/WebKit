{
    'variables': {
        'project_dir': ['.'],
        'javascriptcore_yarr_files': [
            'yarr/YarrCanonicalizeUCS2.cpp',
            'yarr/YarrInterpreter.cpp',
            'yarr/YarrPattern.cpp',
            'yarr/YarrSyntaxChecker.cpp',
        ],
        'javascriptcore_derived_source_files': [
            '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore/RegExpJitTables.h',
        ],
    }
}
