{
  'includes': [
    '../JavaScriptCore.gypi',
  ],

  'variables': {
    'JavaScriptCore': '..',
    'Source': '../..',
    'Dependencies': '<(Source)/WebKit/gtk/gyp/Dependencies.gyp',
    'javascriptcore_includes': [
      '<(Source)',
      '<(JavaScriptCore)',
      '<(JavaScriptCore)/API',
      '<(JavaScriptCore)/assembler',
      '<(JavaScriptCore)/bytecode',
      '<(JavaScriptCore)/bytecompiler',
      '<(JavaScriptCore)/dfg',
      '<(JavaScriptCore)/disassembler',
      '<(JavaScriptCore)/heap',
      '<(JavaScriptCore)/debugger',
      '<(JavaScriptCore)/ForwardingHeaders',
      '<(JavaScriptCore)/interpreter',
      '<(JavaScriptCore)/jit',
      '<(JavaScriptCore)/jit',
      '<(JavaScriptCore)/llint',
      '<(JavaScriptCore)/parser',
      '<(JavaScriptCore)/profiler',
      '<(JavaScriptCore)/runtime',
      '<(JavaScriptCore)/tools',
      '<(JavaScriptCore)/yarr',
      '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore',
    ],
  },

  'target_defaults' : {
      'cflags' : [ '<@(global_cflags)', ],
      'defines': [ '<@(global_defines)' ],
  },

  'targets': [
    {
      'target_name': 'LLIntOffsetExtractor',
        'dependencies': [
          '<(Source)/WTF/WTF.gyp/WTFGTK.gyp:wtf',
        ],
        'type': 'executable',
      'sources': [
        '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore/LLIntDesiredOffsets.h',
        '<@(llintoffsetextractor_files)',
      ],
      'include_dirs': [ '<@(javascriptcore_includes)' ],
      'actions': [
        {
          'action_name': 'llint_desired_offsets',
          'inputs': [
            '<(JavaScriptCore)/offlineasm/generate_offset_extractor.rb',
            '<(JavaScriptCore)/llint/LowLevelInterpreter.asm',
            '<@(llintdesiredoffsets_h_files)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore/LLIntDesiredOffsets.h',
          ],
          'action': [
            'ruby',
            '<(JavaScriptCore)/offlineasm/generate_offset_extractor.rb',
            '<(JavaScriptCore)/llint/LowLevelInterpreter.asm',
            '<@(_outputs)',
          ],
        },
      ],
    },
    {
      'target_name': 'GenerateLUTs',
      'type': 'none',
      'sources': [
          '../runtime/ArrayConstructor.cpp',
          '../runtime/ArrayPrototype.cpp',
          '../runtime/BooleanPrototype.cpp',
          '../runtime/DateConstructor.cpp',
          '../runtime/DatePrototype.cpp',
          '../runtime/ErrorPrototype.cpp',
          '../runtime/JSONObject.cpp',
          '../runtime/JSGlobalObject.cpp',
          '../runtime/MathObject.cpp',
          '../runtime/NamePrototype.cpp',
          '../runtime/NumberConstructor.cpp',
          '../runtime/NumberPrototype.cpp',
          '../runtime/ObjectConstructor.cpp',
          '../runtime/ObjectPrototype.cpp',
          '../runtime/RegExpConstructor.cpp',
          '../runtime/RegExpPrototype.cpp',
          '../runtime/RegExpObject.cpp',
          '../runtime/StringConstructor.cpp',
          '../runtime/StringPrototype.cpp',
      ],
      'rules' : [
        {
          'rule_name': 'GenerateLUT',
          'extension': 'cpp',
          'inputs': [ '<(JavaScriptCore)/create_hash_table', ],
          'outputs': [
            '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore/<(RULE_INPUT_ROOT).lut.h',
          ],
          'action': [
            './redirect-stdout.sh',
            '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore/<(RULE_INPUT_ROOT).lut.h',
            'perl', '<@(_inputs)', '<(RULE_INPUT_PATH)', '-i',
          ],
        }
      ],
      'actions': [
        {
          'action_name': 'GenerateLexerLUT',
          'inputs': [
              '<(JavaScriptCore)/create_hash_table',
              '<(JavaScriptCore)/parser/Keywords.table',
           ],
          'outputs': [ '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore/Lexer.lut.h', ],
          'action': [
            './redirect-stdout.sh',
            '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore/Lexer.lut.h',
            'perl', '<@(_inputs)',
          ],
        },
      ],
    },
    {
      'target_name': 'libjavascriptcoregtk',
      'type': 'shared_library',
      'dependencies': [
        '<(Dependencies):glib',
        '<(Dependencies):icu',
        '<(Source)/WTF/WTF.gyp/WTFGTK.gyp:wtf',
        'GenerateLUTs',
        'LLIntOffsetExtractor',
      ],
      'product_extension': 'so.<@(javascriptcore_soname_version)',
      'product_name': 'javascriptcoregtk-<@(api_version)',
      'cflags': [ '-fPIC', ],
      'include_dirs': [ '<@(javascriptcore_includes)' ],
      'sources': [
        '<@(javascriptcore_derived_source_files)',
        '<@(javascriptcore_files)',
      ],
      'sources/': [
        ['exclude', '(BlackBerry)\\.(cpp|mm|h)$'],
        ['exclude', 'JSStringRefBSTR\\.(h|cpp)$'],
        ['exclude', 'JSStringRefCF\\.(h|cpp)$'],
      ],
      'direct_dependent_settings': {
        'include_dirs': [ '<@(javascriptcore_includes)' ],
      },
      'actions': [
        {
          'action_name': 'llintassembly_header_generation',
          'inputs': [
            '<(JavaScriptCore)/offlineasm/asm.rb',
            '<(JavaScriptCore)/llint/LowLevelInterpreter.asm',
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)LLIntOffsetExtractor<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore/LLIntAssembly.h',
          ],
          'action': ['ruby', '<@(_inputs)', '<@(_outputs)'],
        },
        {
          'action_name': 'GenerateRegExpJitTables',
          'inputs': [ '<(JavaScriptCore)/create_regex_tables', ],
          'outputs': [ '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore/RegExpJitTables.h', ],
          'action': [
            './redirect-stdout.sh',
            '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore/RegExpJitTables.h',
            'python', '<@(_inputs)'
          ],
        },
        {
          'action_name': 'GenerateKeywordLookup',
          'inputs': [
            '<(JavaScriptCore)/KeywordLookupGenerator.py',
            '<(JavaScriptCore)/parser/Keywords.table',
          ],
          'outputs': [ '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore/KeywordLookup.h', ],
          'action': [
            './redirect-stdout.sh',
            '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore/KeywordLookup.h',
            'python', '<@(_inputs)'
          ],
        },
      ],
    },
    {
      'target_name': 'jsc',
        'dependencies': [ 'libjavascriptcoregtk' ],
        'type': 'executable',
        'sources': [ '<@(jsc_files)' ],
    },
    {
      'target_name': 'minidom',
        'dependencies': [ 'libjavascriptcoregtk' ],
        'type': 'executable',
        'sources': [ '<@(minidom_files)' ],
    },
  ]
}

