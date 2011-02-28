{
  'includes': [
    '../JavaScriptCore.gypi',
  ],
  'xcode_config_file': '../Configurations/DebugRelease.xcconfig',
  'targets': [
    {
      'target_name': 'JavaScriptCore',
      'type': 'shared_library',
      'include_dirs': [
        '../..', # Some paths in API include JavaScriptCore/
        '..',
        '../ForwardingHeaders',
        '../API',
        '../assembler',
        '../collector/handles',
        '../bytecode',
        '../bytecompiler',
        '../debugger',
        '../icu',
        '../interpreter',
        '../jit',
        '../parser',
        '../profiler',
        '../runtime',
        '../wtf',
        '../wtf/unicode',
        '<(PRODUCT_DIR)/DerivedSources/JavaScriptCore',
      ],
      'sources': [
        '<@(javascriptcore_files)',
        '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
        '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        'libedit.dylib',
        'libicucore.dylib',
        'libobjc.dylib',
      ],
      'xcode_config_file': '../Configurations/JavaScriptCore.xcconfig',
      'sources/': [
        ['exclude', 'qt'],
        ['exclude', 'os-win32'],
        ['exclude', 'wtf/android'],
        ['exclude', 'wtf/brew'],
        ['exclude', 'wtf/efl'],
        ['exclude', 'wtf/gtk'],
        ['exclude', 'wtf/qt'],
        ['exclude', 'wtf/haiku'],
        ['exclude', 'wtf/url'],
        ['exclude', 'wtf/wince'],
        ['exclude', 'wtf/wx'],
        ['exclude', 'wtf/unicode/brew'],
        ['exclude', 'wtf/unicode/wince'],
        ['exclude', 'wtf/unicode/glib'],
        ['exclude', 'wtf/unicode/qt4'],
        ['exclude', '/(gtk|glib|gobject)/.*\\.(cpp|h)$'],
        ['exclude', '(Default|Gtk|Chromium|None|Qt|Win|Wx|Symbian)\\.(cpp|mm)$'],
        ['exclude', 'GCActivityCallback\.cpp'],
        ['exclude', '.*BSTR.*$'],
        ['exclude', 'jsc.cpp$'],
      ],
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
        'Relase': {},
        'Production': {},
      },
      'default_configuration': 'Debug',
      'defines': [
        'WEBKIT_VERSION_MIN_REQUIRED=WEBKIT_VERSION_LATEST',
      ],
      'conditions': [
        ['OS=="mac"', {
          'mac_bundle': 1,
          'xcode_settings': {
            'USE_HEADERMAP': 'NO', # FIXME: Do we need this?
          }
        }],
      ],
    },
  ], # targets
}
