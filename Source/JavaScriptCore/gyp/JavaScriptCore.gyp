{
  'includes': [
    '../../gyp/common.gypi',
    '../JavaScriptCore.gypi',
  ],
  'xcode_config_file': '<(project_dir)/Configurations/DebugRelease.xcconfig',
  'variables': {
    # FIXME: We should use a header map instead of listing these explicitly.
    'javascriptcore_include_dirs': [
      '<(DEPTH)', # Some paths in API include JavaScriptCore/
      '<(project_dir)',
      '<(project_dir)/ForwardingHeaders',
      '<(project_dir)/API',
      '<(project_dir)/assembler',
      '<(project_dir)/collector/handles',
      '<(project_dir)/bytecode',
      '<(project_dir)/bytecompiler',
      '<(project_dir)/debugger',
      '<(project_dir)/icu',
      '<(project_dir)/interpreter',
      '<(project_dir)/jit',
      '<(project_dir)/parser',
      '<(project_dir)/profiler',
      '<(project_dir)/runtime',
      '<(project_dir)/wtf',
      '<(project_dir)/wtf/unicode',
      '<(SHARED_INTERMEDIATE_DIR)',
    ],
    'derived_source_files': [
      '<(SHARED_INTERMEDIATE_DIR)/ArrayPrototype.lut.h',
      '<(SHARED_INTERMEDIATE_DIR)/DatePrototype.lut.h',
      '<(SHARED_INTERMEDIATE_DIR)/HeaderDetection.h',
      '<(SHARED_INTERMEDIATE_DIR)/JSONObject.lut.h',
      '<(SHARED_INTERMEDIATE_DIR)/Lexer.lut.h',
      '<(SHARED_INTERMEDIATE_DIR)/MathObject.lut.h',
      '<(SHARED_INTERMEDIATE_DIR)/NumberConstructor.lut.h',
      '<(SHARED_INTERMEDIATE_DIR)/RegExpConstructor.lut.h',
      '<(SHARED_INTERMEDIATE_DIR)/RegExpJitTables.h',
      '<(SHARED_INTERMEDIATE_DIR)/RegExpObject.lut.h',
      '<(SHARED_INTERMEDIATE_DIR)/StringPrototype.lut.h',
      '<(SHARED_INTERMEDIATE_DIR)/TracingDtrace.h',
      '<(SHARED_INTERMEDIATE_DIR)/ObjectConstructor.lut.h',
    ],
  },
  'targets': [
    {
      'target_name': 'JavaScriptCore',
      'type': 'shared_library',
      'dependencies': [
        'Derived Sources',
        'Update Version',
      ],
      'include_dirs': [
        '<@(javascriptcore_include_dirs)',
      ],
      'sources': [
        '<@(javascriptcore_files)',
        '<@(javascriptcore_publicheader_files)',
        '<@(javascriptcore_privateheader_files)',
        '<@(derived_source_files)',
        '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
        '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        '/usr/lib/libicucore.dylib',
        '/usr/lib/libobjc.dylib',
      ],
      'mac_framework_headers': [
        '<@(javascriptcore_publicheader_files)',
      ],
      'mac_framework_private_headers': [
        '<@(javascriptcore_privateheader_files)',
      ],
      'xcode_config_file': '<(project_dir)/Configurations/JavaScriptCore.xcconfig',
      'sources/': [
        ['exclude', 'qt'],
        ['exclude', 'os-win32'],
        ['exclude', 'wtf/android'],
        ['exclude', 'wtf/brew'],
        ['exclude', 'wtf/efl'],
        ['exclude', 'wtf/gtk'],
        ['exclude', 'wtf/qt'],
        ['exclude', 'wtf/haiku'],
        ['exclude', 'API/tests'],
        ['exclude', 'wtf/url'],
        ['exclude', 'wtf/wince'],
        ['exclude', 'wtf/wx'],
        ['exclude', 'wtf/unicode/brew'],
        ['exclude', 'wtf/unicode/wince'],
        ['exclude', 'wtf/unicode/glib'],
        ['exclude', 'wtf/unicode/qt4'],
        ['exclude', '/(gtk|glib|gobject)/.*\\.(cpp|h)$'],
        ['exclude', '(Default|Gtk|Chromium|None|Qt|Win|Wx|Symbian)\\.(cpp|mm|h)$'],
        ['exclude', 'GCActivityCallback\.cpp'],
        ['exclude', '.*BSTR.*$'],
        ['exclude', 'jsc.cpp$'],
      ],
      'postbuilds': [
        {
          'postbuild_name': 'Check For Global Initializers',
          'action': [
            'sh', '<(DEPTH)/gyp/run-if-exists.sh', '<(DEPTH)/../Tools/Scripts/check-for-global-initializers'
          ],
        },
        {
          'postbuild_name': 'Check For Exit Time Destructors',
          'action': [
            'sh', '<(DEPTH)/gyp/run-if-exists.sh', '<(DEPTH)/../Tools/Scripts/check-for-exit-time-destructors'
          ],
        },
        {
          'postbuild_name': 'Check For Weak VTables and Externals',
          'action': [
            'sh', '<(DEPTH)/gyp/run-if-exists.sh', '<(DEPTH)/../Tools/Scripts/check-for-weak-vtables-and-externals'
          ],
        },
      ],
      'conditions': [
        ['OS=="mac"', {
          'mac_bundle': 1,
          'xcode_settings': {
            # FIXME: Remove these overrides once JavaScriptCore.xcconfig is
            # used only by this project.
            'GCC_PREFIX_HEADER': '<(project_dir)/JavaScriptCorePrefix.h',
            'INFOPLIST_FILE': '<(project_dir)/Info.plist',
            # This setting mirrors the setting in Base.xcconfig, with
            # one difference noted below.
            'WARNING_CFLAGS_BASE': [
              '-Wall',
              '-Wextra',
              '-Wcast-qual',
              '-Wchar-subscripts',
              '-Wextra-tokens',
              '-Wformat=2',
              '-Winit-self',
              # FIXME: For some reason, -Wmissing-format-attribute causes a
              # build error in Assertions.cpp in the GYP build but not in the
              # non-GYP build.
              # '-Wmissing-format-attribute',
              '-Wmissing-noreturn',
              '-Wpacked',
              '-Wpointer-arith',
              '-Wredundant-decls',
              '-Wundef',
              '-Wwrite-strings',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'Derived Sources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'Generate Derived Sources',
          'inputs': [],
          'outputs': [
            '<@(derived_source_files)',
          ],
          'action': [
            'sh', 'generate-derived-sources.sh', '<(SHARED_INTERMEDIATE_DIR)'
          ],
        },
        {
          'action_name': 'Generate DTrace Header',
          'inputs': [],
           'outputs': [],
           'action': [
             'sh', '<(project_dir)/gyp/generate-dtrace-header.sh', '<(project_dir)', '<(SHARED_INTERMEDIATE_DIR)'
            ]
        }
      ],
    },
    {
      'target_name': 'Update Version',
      'type': 'none',
      'actions': [{
        'action_name': 'Update Info.plist with version information',
        'inputs': [],
         'outputs': [],
         'action': [
           'sh', '<(DEPTH)/gyp/update-info-plist.sh', '<(project_dir)/Info.plist'
          ]
      }],
    },
    {
      'target_name': 'minidom',
      'type': 'executable',
      'dependencies': [
        'JavaScriptCore',
      ],
      # FIXME: We should use a header map instead of listing these explicitly.
      'include_dirs': [
        '<@(javascriptcore_include_dirs)',
      ],
      'sources': [
        '<@(minidom_files)',
        '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
      ],
      'copies': [{
        'destination': '<(PRODUCT_DIR)',
        'files': [
          '<@(minidom_support_files)',
        ],
      }],
    },
    {
      'target_name': 'testapi',
      'type': 'executable',
      'dependencies': [
        'JavaScriptCore',
      ],
      # FIXME: We should use a header map instead of listing these explicitly.
      'include_dirs': [
        '<@(javascriptcore_include_dirs)',
      ],
      'sources': [
        '<@(testapi_files)',
        '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
      ],
      'copies': [{
        'destination': '<(PRODUCT_DIR)',
        'files': [
          '<@(testapi_support_files)',
        ],
      }],
    },
    {
      'target_name': 'jsc',
      'type': 'executable',
      'dependencies': [
        'JavaScriptCore',
      ],
      # FIXME: We should use a header map instead of listing these explicitly.
      'include_dirs': [
        '<@(javascriptcore_include_dirs)',
      ],
      'sources': [
        '<@(jsc_files)',
        '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
        '/usr/lib/libedit.dylib',
      ],
    },
  ], # targets
}
