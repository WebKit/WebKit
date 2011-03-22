{
  'includes': [
    '../../gyp/common.gypi',
    'JavaScriptGlue.gypi',
  ],
  'xcode_config_file': '<(DEPTH)/JavaScriptGlue/Configurations/DebugRelease.xcconfig',
  'targets': [
    {
      'target_name': 'JavaScriptGlue',
      'type': 'shared_library',
      'dependencies': [
        'Update Version'
      ],
      'include_dirs': [
        '<(DEPTH)/JavaScriptGlue/ForwardingHeaders',
        '<(DEPTH)/JavaScriptGlue/icu',
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
            # FIXME: Remove these overrides once JavaScriptGlue.xcconfig is
            # used only by this project.
            'INFOPLIST_FILE': '<(DEPTH)/JavaScriptGlue/Info.plist',
            'EXPORTED_SYMBOLS_FILE': '<(DEPTH)/JavaScriptGlue/JavaScriptGlue.exp', 
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
           'sh', '<(DEPTH)/gyp/update-info-plist.sh', '<(DEPTH)/JavaScriptGlue/Info.plist'
          ]
      }],
    },
  ], # targets
}
