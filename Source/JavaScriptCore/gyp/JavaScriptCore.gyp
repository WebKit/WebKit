{
  'includes': [
    '../JavaScriptCore.gypi',
  ],
  'xcode_config_file': '<(DEPTH)/JavaScriptCore/Configurations/DebugRelease.xcconfig',
  'variables': {
    # FIXME: We should use a header map instead of listing these explicitly.
    'javascriptcore_include_dirs': [
      '<(DEPTH)', # Some paths in API include JavaScriptCore/
      '<(DEPTH)/JavaScriptCore',
      '<(DEPTH)/JavaScriptCore/ForwardingHeaders',
      '<(DEPTH)/JavaScriptCore/API',
      '<(DEPTH)/JavaScriptCore/assembler',
      '<(DEPTH)/JavaScriptCore/collector/handles',
      '<(DEPTH)/JavaScriptCore/bytecode',
      '<(DEPTH)/JavaScriptCore/bytecompiler',
      '<(DEPTH)/JavaScriptCore/debugger',
      '<(DEPTH)/JavaScriptCore/icu',
      '<(DEPTH)/JavaScriptCore/interpreter',
      '<(DEPTH)/JavaScriptCore/jit',
      '<(DEPTH)/JavaScriptCore/parser',
      '<(DEPTH)/JavaScriptCore/profiler',
      '<(DEPTH)/JavaScriptCore/runtime',
      '<(DEPTH)/JavaScriptCore/wtf',
      '<(DEPTH)/JavaScriptCore/wtf/unicode',
      '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore',
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
        '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
        '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        'libicucore.dylib',
        'libobjc.dylib',
      ],
      'mac_framework_headers': [
        '<@(javascriptcore_publicheader_files)',
      ],
      'mac_framework_private_headers': [
        '<@(javascriptcore_privateheader_files)',
      ],
      'xcode_config_file': '<(DEPTH)/JavaScriptCore/Configurations/JavaScriptCore.xcconfig',
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
      'configurations': {
        'Debug': {},
        'Release': {},
        'Production': {},
      },
      'default_configuration': 'Debug',
      'defines': [
        'WEBKIT_VERSION_MIN_REQUIRED=WEBKIT_VERSION_LATEST',
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
            'USE_HEADERMAP': 'NO',
            # FIXME: Remove these overrides once JavaScriptCore.xcconfig is
            # used only by this project.
            'GCC_PREFIX_HEADER': '<(DEPTH)/JavaScriptCore/JavaScriptCorePrefix.h',
            'INFOPLIST_FILE': '<(DEPTH)/JavaScriptCore/Info.plist',
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
      'actions': [{
        'action_name': 'generate_derived_sources',
        'inputs': [],
        'outputs': [],
        'action': [
          'sh', 'generate-derived-sources.sh',
        ],
      }],
      'configurations': {
        'Debug': {},
        'Release': {},
        'Production': {},
      },
      'default_configuration': 'Debug',
    },
    {
      'target_name': 'Update Version',
      'type': 'none',
      'actions': [{
        'action_name': 'Update Info.plist with version information',
        'inputs': [],
         'outputs': [],
         'action': [
           'sh', '<(DEPTH)/gyp/update-info-plist.sh', '<(DEPTH)/JavaScriptCore/Info.plist'
          ]
      }],
      'configurations': {
        'Debug': {},
        'Release': {},
        'Production': {},
      },
      'default_configuration': 'Debug',
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
      'configurations': {
        'Debug': {},
        'Release': {},
        'Production': {},
      },
      'default_configuration': 'Debug',
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'USE_HEADERMAP': 'NO',
          }
        }],
      ],
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
      'configurations': {
        'Debug': {},
        'Release': {},
        'Production': {},
      },
      'default_configuration': 'Debug',
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'USE_HEADERMAP': 'NO',
          }
        }],
      ],
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
        'libedit.dylib',
      ],
      'configurations': {
        'Debug': {},
        'Release': {},
        'Production': {},
      },
      'default_configuration': 'Debug',
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'USE_HEADERMAP': 'NO',
          }
        }],
      ],
    },
  ], # targets
}
