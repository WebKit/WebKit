{
  'includes': [
    'JavaScriptGlue.gypi',
  ],
  'xcode_config_file': '../Configurations/DebugRelease.xcconfig',
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
      'xcode_config_file': '../Configurations/JavaScriptGlue.xcconfig',
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
