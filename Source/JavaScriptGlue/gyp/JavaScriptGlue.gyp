{
  'includes': [
    'JavaScriptGlue.gypi',
  ],
  'targets': [
    {
      'target_name': 'JavaScriptGlue',
      'type': 'shared_library',
      'dependencies': [
        'Update Version'
      ],
      'include_dirs': [
        '..',
        '../ForwardingHeaders',
        '../icu',
        '<(PRODUCT_DIR)/include',
      ],
      'sources': [
        '<@(javascriptglue_files)',
        '<(PRODUCT_DIR)/JavaScriptCore.framework',
        '$(SDKROOT)/System/Library/Frameworks/CoreServices.framework',
        '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
      ],
      'configurations': {
        'Debug': {},
      },
      'default_configuration': 'Debug',
      'defines': [
        'WEBKIT_VERSION_MIN_REQUIRED=WEBKIT_VERSION_LATEST',
      ],
      'postbuilds': [
        {
          'postbuild_name': 'Check For Global Initializers',
          'action': [
            'sh', 'run-if-exists.sh', 'check-for-global-initializers'
          ],
        },
        {
          'postbuild_name': 'Check For Weak VTables and Externals',
          'action': [
            'sh', 'run-if-exists.sh', 'check-for-weak-vtables-and-externals'
          ],
        },
        {
          'postbuild_name': 'Remove Headers If Needed',
          'action': [
            'sh', 'remove-headers-if-needed.sh'
          ],
        },
      ],
      'conditions': [
        ['OS=="mac"', {
          'mac_bundle': 1,
          'xcode_settings': {
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
              '-W',
              '-Wchar-subscripts',
              '-Wformat-security',
              '-Wmissing-format-attribute',
              '-Wpointer-arith',
              '-Wwrite-strings',
              '-Wno-format-y2k',
              '-Wno-unused-parameter',
              '-Wundef',
              '-Wno-strict-aliasing',
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

            ##### From JavaScriptGlue #####

            'EXPORTED_SYMBOLS_FILE': '../JavaScriptGlue.exp',
            'GCC_PREPROCESSOR_DEFINITIONS': '$(DEBUG_DEFINES) WEBKIT_VERSION_MIN_REQUIRED=WEBKIT_VERSION_LATEST $(GCC_PREPROCESSOR_DEFINITIONS)',
            'INFOPLIST_FILE': '../Info.plist',
            'INSTALL_PATH': '$(SYSTEM_LIBRARY_DIR)/PrivateFrameworks',
            'OTHER_CFLAGS': '-Wno-deprecated-declarations',
            'PRODUCT_NAME': 'JavaScriptGlue',

            ##### From Version #####

            'MAJOR_VERSION': '534',
            'MINOR_VERSION': '21',
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
          },
        }],
      ],
    },
    {
      'target_name': 'Update Version',
      'type': 'none',
      'actions': [
        {
          'action_name': 'Update Info.plist with version information',
          'inputs': [],
          'outputs': [],
          'action': [
            'sh', 'update-info-plist.sh'
          ],
        },
      ], # actions
    },
  ], # targets
}
