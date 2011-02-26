{
  'includes': [
    '../JavaScriptCore.gypi',
  ],
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
      'xcode_config_file': '../Configurations/Base.xcconfig',
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
      },
      'default_configuration': 'Debug',
      'defines': [
        'WEBKIT_VERSION_MIN_REQUIRED=WEBKIT_VERSION_LATEST',
      ],
      'conditions': [
        ['OS=="mac"', {
          'mac_bundle': 1,
          'xcode_settings': {
            #### From Chromium #####
            'USE_HEADERMAP': 'NO',

            ##### From DebugRelease #####

            'ARCHS': '$(ARCHS_$(TARGET_MAC_OS_X_VERSION_MAJOR))',
            'ARCHS_': '$(ARCHS_1040)',
            'ARCHS_1040': '$(NATIVE_ARCH)',
            'ARCHS_1050': '$(NATIVE_ARCH)',
            'ARCHS_1060': '$(ARCHS_STANDARD_32_64_BIT)',
            'ARCHS_1070': '$(ARCHS_STANDARD_32_64_BIT)',

            'ONLY_ACTIVE_ARCH': 'YES',

            'MACOSX_DEPLOYMENT_TARGET': '$(MACOSX_DEPLOYMENT_TARGET_$(TARGET_MAC_OS_X_VERSION_MAJOR))',
            'MACOSX_DEPLOYMENT_TARGET_': '10.4',
            'MACOSX_DEPLOYMENT_TARGET_1040': '10.4',
            'MACOSX_DEPLOYMENT_TARGET_1050': '10.5',
            'MACOSX_DEPLOYMENT_TARGET_1060': '10.6',
            'MACOSX_DEPLOYMENT_TARGET_1070': '10.7',

            'GCC_WARN_ABOUT_DEPRECATED_FUNCTIONS': 'YES',

            ##### From JavaScriptCore #####

            'JSVALUE_MODEL': '$(JSVALUE_MODEL_$(CURRENT_ARCH))',
            'JSVALUE_MODEL_': 'UNKNOWN_JSVALUE_MODEL',
            'JSVALUE_MODEL_armv6': '32_64',
            'JSVALUE_MODEL_armv7': '32_64',
            'JSVALUE_MODEL_i386': '32_64',
            'JSVALUE_MODEL_ppc': '32_64',
            'JSVALUE_MODEL_ppc64': '64',
            'JSVALUE_MODEL_x86_64': '64',

            'EXPORTED_SYMBOLS_FILE': '$(BUILT_PRODUCTS_DIR)/DerivedSources/JavaScriptCore/JavaScriptCore.JSVALUE$(JSVALUE_MODEL).exp',
            'OTHER_LDFLAGS_BASE': '-lobjc -Wl,-Y,3',
            'OTHER_LDFLAGS': '$(OTHER_LDFLAGS_$(REAL_PLATFORM_NAME))',
            'OTHER_LDFLAGS_iphoneos': '$(OTHER_LDFLAGS_BASE)',
            'OTHER_LDFLAGS_iphonesimulator': '$(OTHER_LDFLAGS_iphoneos)',
            'OTHER_LDFLAGS_macosx': '$(OTHER_LDFLAGS_BASE) -sub_library libobjc -framework CoreServices $(OTHER_LDFLAGS_macosx_$(TARGET_MAC_OS_X_VERSION_MAJOR))',
            'OTHER_LDFLAGS_macosx_1070': '-Xlinker -objc_gc_compaction',
            'GCC_PREFIX_HEADER': '../JavaScriptCorePrefix.h',
            'INFOPLIST_FILE': '../Info.plist',
            'INSTALL_PATH': '$(JAVASCRIPTCORE_FRAMEWORKS_DIR)',
            'PRODUCT_NAME': 'JavaScriptCore',

            'OTHER_CFLAGS': '$(OTHER_CFLAGS_$(CONFIGURATION)_$(CURRENT_VARIANT))',
            'OTHER_CFLAGS_Release_normal': '$(OTHER_CFLAGS_normal_$(TARGET_GCC_VERSION))',
            'OTHER_CFLAGS_Production_normal': '$(OTHER_CFLAGS_normal_$(TARGET_GCC_VERSION))',
            'OTHER_CFLAGS_normal_GCC_42': '-fomit-frame-pointer -funwind-tables',
            'OTHER_CFLAGS_normal_LLVM_GCC_42': '$(OTHER_CFLAGS_normal_GCC_42)',

            ##### From Version #####

            'MAJOR_VERSION': '534',
            'MINOR_VERSION': '22',
            'TINY_VERSION': '0',
            'FULL_VERSION': '$(MAJOR_VERSION).$(MINOR_VERSION)',

            # The bundle version and short version string are set based on the current build configuration, see below.
            'BUNDLE_VERSION': '$(BUNDLE_VERSION_$(CONFIGURATION))',
            'SHORT_VERSION_STRING': '$(SHORT_VERSION_STRING_$(CONFIGURATION))',

            # The system version prefix is based on the current system version.
            'SYSTEM_VERSION_PREFIX': '$(SYSTEM_VERSION_PREFIX_$(TARGET_MAC_OS_X_VERSION_MAJOR))',
            'SYSTEM_VERSION_PREFIX_': '4', # Some Tiger versions of Xcode don't set MAC_OS_X_VERSION_MAJOR.
            'SYSTEM_VERSION_PREFIX_1040': '4',
            'SYSTEM_VERSION_PREFIX_1050': '5',
            'SYSTEM_VERSION_PREFIX_1060': '6',
            'SYSTEM_VERSION_PREFIX_1070': '7',

            # The production build always uses the full version with a system version prefix.
            'BUNDLE_VERSION_Production': '$(SYSTEM_VERSION_PREFIX)$(FULL_VERSION)',
            'BUNDLE_VERSION_': '$(BUNDLE_VERSION_Production)',

            # The production build always uses the major version with a system version prefix
            'SHORT_VERSION_STRING_Production': '$(SYSTEM_VERSION_PREFIX)$(MAJOR_VERSION)',
            'SHORT_VERSION_STRING_': '$(SHORT_VERSION_STRING_Production)',

            # Local builds are the full version with a plus suffix.
            'BUNDLE_VERSION_Release': '$(FULL_VERSION)+',
            'BUNDLE_VERSION_Debug': '$(BUNDLE_VERSION_Release)',

            # Local builds use the major version with a plus suffix
            'SHORT_VERSION_STRING_Release': '$(MAJOR_VERSION)+',
            'SHORT_VERSION_STRING_Debug': '$(SHORT_VERSION_STRING_Release)',

            'DYLIB_COMPATIBILITY_VERSION': '1',
            'DYLIB_CURRENT_VERSION': '$(FULL_VERSION)',
          }
        }],
      ],
    },
  ], # targets
}
