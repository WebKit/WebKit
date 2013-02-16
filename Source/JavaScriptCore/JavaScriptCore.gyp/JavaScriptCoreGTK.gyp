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
      'target_name': 'libjavascriptcoregtk',
        'type': 'shared_library',
        'dependencies': [
          '<(Source)/WTF/WTF.gyp/WTFGTK.gyp:wtf',
          'LLIntOffsetExtractor',
          '<(Dependencies):glib',
          '<(Dependencies):icu',
        ],
        'product_extension': 'so.<@(javascriptcore_soname_version)',
        'product_name': 'javascriptcoregtk-<@(api_version)',
        'cflags': [ '-fPIC', ],
        'include_dirs': [ '<@(javascriptcore_includes)' ],
        'sources': [
          '<@(javascriptcore_yarr_files)',
          '<@(javascriptcore_derived_source_files)',
          '<@(javascriptcore_files)',
        ],
        'sources/': [
          ['exclude', '(BlackBerry)\\.(cpp|mm|h)$'],
          ['exclude', 'JSStringRefBSTR\\.(h|cpp)$'],
          ['exclude', 'JSStringRefCF\\.(h|cpp)$'],
        ],
      'actions': [
        {
          'action_name': 'Generate Derived Sources',
          'inputs': [
            '<(JavaScriptCore)/DerivedSources.make',
            'generate-derived-sources.sh',
          ],
          'outputs': [
            '<@(javascriptcore_derived_source_files)',
          ],
          'action': ['sh', 'generate-derived-sources.sh', '<@(project_dir)', '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore'],
        },
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
      ],
    },
    {
      'target_name': 'jsc',
        'dependencies': [ 'libjavascriptcoregtk' ],
        'type': 'executable',
        'sources': [ '<@(jsc_files)' ],
        'include_dirs': [ '<@(javascriptcore_includes)' ],
    },
    {
      'target_name': 'minidom',
        'dependencies': [ 'libjavascriptcoregtk' ],
        'type': 'executable',
        'sources': [ '<@(minidom_files)' ],
        'include_dirs': [ '<@(javascriptcore_includes)', ],
    },
  ]
}

