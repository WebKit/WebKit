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

            'DEAD_CODE_STRIPPING': 'YES',
            'DEBUG_INFORMATION_FORMAT': 'dwarf',
            'GCC_C_LANGUAGE_STANDARD': 'gnu99',
            'GCC_DEBUGGING_SYMBOLS': 'default',
            'GCC_DYNAMIC_NO_PIC': 'NO',
            'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',
            'GCC_ENABLE_CPP_RTTI': 'NO',
            'GCC_ENABLE_OBJC_EXCEPTIONS': 'YES',
            'GCC_ENABLE_OBJC_GC': 'supported',
            'GCC_ENABLE_SYMBOL_SEPARATION': 'NO',
            'GCC_FAST_OBJC_DISPATCH': 'YES',
            'GCC_GENERATE_DEBUGGING_SYMBOLS': 'YES',
            'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES',
            'GCC_MODEL_TUNING': 'G5',
            'GCC_PRECOMPILE_PREFIX_HEADER': 'YES',
            'GCC_STRICT_ALIASING': 'YES',
            'GCC_THREADSAFE_STATICS': 'NO',
            'GCC_TREAT_WARNINGS_AS_ERRORS': 'YES',
            'GCC_WARN_ABOUT_DEPRECATED_FUNCTIONS': 'NO',
            'GCC_WARN_ABOUT_MISSING_NEWLINE': 'YES',
            'GCC_WARN_ABOUT_MISSING_PROTOTYPES': 'NO',
            'GCC_WARN_NON_VIRTUAL_DESTRUCTOR': 'YES',
            'LINKER_DISPLAYS_MANGLED_NAMES': 'YES',
            'PREBINDING': 'NO',
            'VALID_ARCHS': 'i386 ppc x86_64 ppc64',
            'WARNING_CFLAGS': '$(WARNING_CFLAGS_$(CURRENT_ARCH))',
            'WARNING_CFLAGS_BASE': [
              '-Wall',
              '-Wextra',
              '-Wcast-qual',
              '-Wchar-subscripts',
              '-Wextra-tokens',
              '-Wformat=2',
              '-Winit-self',
              # FIXME: Add this warning back.
              # '-Wmissing-format-attribute',
              '-Wmissing-noreturn',
              '-Wpacked',
              '-Wpointer-arith',
              '-Wredundant-decls',
              '-Wundef',
              '-Wwrite-strings',
            ],
            'WARNING_CFLAGS_': '$(WARNING_CFLAGS_BASE) -Wshorten-64-to-32',
            'WARNING_CFLAGS_i386': '$(WARNING_CFLAGS_BASE) -Wshorten-64-to-32',
            'WARNING_CFLAGS_ppc': '$(WARNING_CFLAGS_BASE) -Wshorten-64-to-32',
            # FIXME: JavaScriptGlue 64-bit builds should build with -Wshorten-64-to-32
            'WARNING_CFLAGS_ppc64': '$(WARNING_CFLAGS_BASE)',
            'WARNING_CFLAGS_x86_64': '$(WARNING_CFLAGS_BASE)',

            'TARGET_MAC_OS_X_VERSION_MAJOR': '$(MAC_OS_X_VERSION_MAJOR)',

            # DEBUG_DEFINES, GCC_OPTIMIZATION_LEVEL and STRIP_INSTALLED_PRODUCT vary between the debug and normal variants.
            # We set up the values for each variant here, and have the Debug configuration in the Xcode project use the _debug variant.
            'DEBUG_DEFINES_debug': '',
            'DEBUG_DEFINES_normal': 'NDEBUG',
            'DEBUG_DEFINES': '$(DEBUG_DEFINES_$(CURRENT_VARIANT))',

            'GCC_OPTIMIZATION_LEVEL': '$(GCC_OPTIMIZATION_LEVEL_$(CURRENT_VARIANT))',
            'GCC_OPTIMIZATION_LEVEL_normal': 's',
            'GCC_OPTIMIZATION_LEVEL_debug': '0',

            'STRIP_INSTALLED_PRODUCT': '$(STRIP_INSTALLED_PRODUCT_$(CURRENT_VARIANT))',
            'STRIP_INSTALLED_PRODUCT_normal': 'YES',
            'STRIP_INSTALLED_PRODUCT_debug': 'NO',

            # Use GCC 4.2 with Xcode 3.1, which includes GCC 4.2 but defaults to GCC 4.0.
            # Note that Xcode versions as new as 3.1.2 use XCODE_VERSION_ACTUAL for the minor version
            # number.  Newer versions of Xcode use XCODE_VERSION_MINOR for the minor version, and
            # XCODE_VERSION_ACTUAL for the full version number.
            'TARGET_GCC_VERSION': '$(TARGET_GCC_VERSION_$(TARGET_MAC_OS_X_VERSION_MAJOR))',
            'TARGET_GCC_VERSION_': '$(TARGET_GCC_VERSION_1040)',
            'TARGET_GCC_VERSION_1040': 'GCC_40',
            'TARGET_GCC_VERSION_1050': '$(TARGET_GCC_VERSION_1050_$(XCODE_VERSION_MINOR))',
            'TARGET_GCC_VERSION_1050_': '$(TARGET_GCC_VERSION_1050_$(XCODE_VERSION_ACTUAL))',
            'TARGET_GCC_VERSION_1050_0310': 'GCC_42',
            'TARGET_GCC_VERSION_1050_0320': 'GCC_42',
            'TARGET_GCC_VERSION_1060': 'GCC_42',
            'TARGET_GCC_VERSION_1070': 'LLVM_GCC_42',

            'GCC_VERSION': '$(GCC_VERSION_$(TARGET_GCC_VERSION))',
            'GCC_VERSION_GCC_40': '4.0',
            'GCC_VERSION_GCC_42': '4.2',
            'GCC_VERSION_LLVM_GCC_42': 'com.apple.compilers.llvmgcc42',

            # If the target Mac OS X version does not match the current Mac OS X version then we'll want to build using the target version's SDK.
            'SDKROOT': '$(SDKROOT_$(MAC_OS_X_VERSION_MAJOR)_$(TARGET_MAC_OS_X_VERSION_MAJOR))',
            'SDKROOT_1050_1040': 'macosx10.4',
            'SDKROOT_1060_1040': 'macosx10.4',
            'SDKROOT_1060_1050': 'macosx10.5',
            'SDKROOT_1070_1040': 'macosx10.4',
            'SDKROOT_1070_1050': 'macosx10.5',
            'SDKROOT_1070_1060': 'macosx10.6',

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
