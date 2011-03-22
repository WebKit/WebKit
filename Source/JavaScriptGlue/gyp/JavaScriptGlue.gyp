{
  'includes': [
    '../../gyp/common.gypi',
    'JavaScriptGlue.gypi',
  ],
  'configurations': {
    'Production': {
      'xcode_config_file': '<(project_dir)/Configurations/Base.xcconfig',
      'xcode_settings': {
        'BUILD_VARIANTS': 'normal',
        'SECTORDER_FLAGS': [
          '-sectorder',
          '__TEXT',
          '__text',
          '$(APPLE_INTERNAL_DIR)/OrderFiles/JavaScriptGlue.order',
        ],
      },
    },
    'Release': {
      'xcode_config_file': '<(project_dir)/Configurations/DebugRelease.xcconfig',
      'xcode_settings': {
        'COPY_PHASE_STRIP': 'YES',
        'GCC_ENABLE_FIX_AND_CONTINUE': 'NO',
        'ZERO_LINK': 'NO',
        'STRIP_INSTALLED_PRODUCT': 'NO',
        'INSTALL_PATH': '$(BUILT_PRODUCTS_DIR)',
      },
    },
    'Debug': {
      'xcode_config_file': '<(project_dir)/Configurations/DebugRelease.xcconfig',
      'xcode_settings': {
        'COPY_PHASE_STRIP': 'NO',
        'GCC_DYNAMIC_NO_PIC': 'NO',
        'DEBUG_DEFINES': '$(DEBUG_DEFINES_debug)',
        'GCC_OPTIMIZATION_LEVEL': '$(GCC_OPTIMIZATION_LEVEL_debug)',
        'STRIP_INSTALLED_PRODUCT': '$(STRIP_INSTALLED_PRODUCT_debug)',
        'INSTALL_PATH': '$(BUILT_PRODUCTS_DIR)',
      },
    },
  },
  'targets': [
    {
      'target_name': 'JavaScriptGlue',
      'type': 'shared_library',
      'dependencies': [
        'Update Version'
      ],
      'include_dirs': [
        '<(project_dir)/ForwardingHeaders',
        '<(project_dir)/icu',
        '<(PRODUCT_DIR)/include',
      ],
      'sources': [
        '<@(javascriptglue_files)',
        '<(PRODUCT_DIR)/JavaScriptCore.framework',
        '$(SDKROOT)/System/Library/Frameworks/CoreServices.framework',
        '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
      ],
      'xcode_config_file': '<(project_dir)/Configurations/JavaScriptGlue.xcconfig',
      'postbuilds': [
        {
          'postbuild_name': 'Check For Global Initializers',
          'action': [
            'sh', '<(DEPTH)/gyp/run-if-exists.sh', '<(DEPTH)/../Tools/Scripts/check-for-global-initializers'
          ],
        },
        {
          'postbuild_name': 'Check For Weak VTables and Externals',
          'action': [
            'sh', '<(DEPTH)/gyp/run-if-exists.sh', '<(DEPTH)/../Tools/Scripts/check-for-weak-vtables-and-externals'
          ],
        },
        {
          'postbuild_name': 'Remove Headers If Needed',
          'action': [
            'sh', '<(DEPTH)/gyp/remove-headers-if-needed.sh'
          ],
        },
      ],
      'conditions': [
        ['OS=="mac"', {
          'mac_bundle': 1,
          'xcode_settings': {
            'OTHER_CFLAGS': '-Wno-deprecated-declarations',
            # FIXME: Remove these overrides once JavaScriptGlue.xcconfig is
            # used only by this project.
            'INFOPLIST_FILE': '<(project_dir)/Info.plist',
            'EXPORTED_SYMBOLS_FILE': '<(project_dir)/JavaScriptGlue.exp', 
          },
        }],
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
  ], # targets
}
