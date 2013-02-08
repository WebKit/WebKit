{
  'includes': [
    'Configuration.gypi',
    '../../../JavaScriptCore/JavaScriptCore.gypi',
  ],
  'variables': {
    'javascriptcore_includes': [
      '<(Source)/',
      '<(Source)/JavaScriptCore',
      '<(Source)/JavaScriptCore/API',
      '<(Source)/JavaScriptCore/assembler',
      '<(Source)/JavaScriptCore/bytecode',
      '<(Source)/JavaScriptCore/bytecompiler',
      '<(Source)/JavaScriptCore/dfg',
      '<(Source)/JavaScriptCore/disassembler',
      '<(Source)/JavaScriptCore/heap',
      '<(Source)/JavaScriptCore/debugger',
      '<(Source)/JavaScriptCore/ForwardingHeaders',
      '<(Source)/JavaScriptCore/interpreter',
      '<(Source)/JavaScriptCore/jit',
      '<(Source)/JavaScriptCore/jit',
      '<(Source)/JavaScriptCore/llint',
      '<(Source)/JavaScriptCore/parser',
      '<(Source)/JavaScriptCore/profiler',
      '<(Source)/JavaScriptCore/runtime',
      '<(Source)/JavaScriptCore/tools',
      '<(Source)/JavaScriptCore/yarr',
      '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore',
    ],
  },
  'targets': [
    {
      'target_name': 'LLIntOffsetExtractor',
        'dependencies': [
          'WTF.gyp:wtf',
          'WTF.gyp:glib',
          'WTF.gyp:icu'
        ],
        'type': 'executable',
      'sources': [
        '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore/LLIntDesiredOffsets.h',
        '<@(llintoffsetextractor_files)',
      ],
      'include_dirs': [ '<@(javascriptcore_includes)' ],
      'defines': [ '<@(default_defines)' ],
      'actions': [
        {
          'action_name': 'llint_desired_offsets',
          'inputs': [
            '<(Source)/JavaScriptCore/offlineasm/generate_offset_extractor.rb',
            '<(Source)/JavaScriptCore/llint/LowLevelInterpreter.asm',
            '<@(llintdesiredoffsets_h_files)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore/LLIntDesiredOffsets.h',
          ],
          'action': [
            'ruby',
            '<(Source)/JavaScriptCore/offlineasm/generate_offset_extractor.rb',
            '<(Source)/JavaScriptCore/llint/LowLevelInterpreter.asm',
            '<@(_outputs)',
          ],
        },
      ],
    },
    {
      'target_name': 'libjavascriptcoregtk',
        'type': 'shared_library',
        'dependencies': [
          'WTF.gyp:wtf',
          'LLIntOffsetExtractor'
        ],
        'include_dirs': [ '<@(javascriptcore_includes)' ],
        'product_extension': 'so.<@(javascriptcore_soname_version)',
        'product_name': 'javascriptcoregtk-<@(library_version)',
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
        'direct_dependent_settings': {
          'include_dirs': [
            '<(Source)/WTF',
            '<(Source)/WTF/wtf',
          ],
        },
        'defines': [ '<@(default_defines)' ],
        'cflags': [ '-fPIC' ],
      'actions': [
        {
          'action_name': 'Generate Derived Sources',
          'inputs': [
            '<(Source)/JavaScriptCore/DerivedSources.make',
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
            '<(Source)/JavaScriptCore/offlineasm/asm.rb',
            '<(Source)/JavaScriptCore/llint/LowLevelInterpreter.asm',
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
        'defines': [ '<@(default_defines)' ],
    },
    {
      'target_name': 'minidom',
        'dependencies': [ 'libjavascriptcoregtk' ],
        'type': 'executable',
        'sources': [ '<@(minidom_files)' ],
        'include_dirs': [ '<@(javascriptcore_includes)' ],
        'defines': [ '<@(default_defines)' ],
    },
  ]
}

