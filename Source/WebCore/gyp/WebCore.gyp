{
  'includes': [
    '../../gyp/common.gypi',
    '../WebCore.gypi',
  ],
  'xcode_config_file': '../Configurations/DebugRelease.xcconfig',
  'targets': [
    {
      'target_name': 'WebCore',
      'type': 'shared_library',
      'dependencies': [
        'Derived Sources',
        'Update Version',
        # FIXME: Add 'Copy Generated Headers',
        # FIXME: Add 'Copy Forwarding and ICU Headers',
        # FIXME: Add 'Copy Inspector Resources',
      ],
      'include_dirs': [
        '<@(webcore_include_dirs)',
      ],
      'sources': [
        '<@(webcore_files)',
        '<@(webcore_publicheader_files)',
        '<@(webcore_privateheader_files)',
        '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
        '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        'libicucore.dylib',
        'libobjc.dylib',
      ],
      'mac_framework_headers': [
        '<@(webcore_publicheader_files)',
      ],
      'mac_framework_private_headers': [
        '<@(webcore_privateheader_files)',
      ],
      'xcode_config_file': '../Configurations/WebCore.xcconfig',
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
        # FIXME: Add "Check fo Inappropriate Files in Framework".
      ],
      'conditions': [
        ['OS=="mac"', {
          'mac_bundle': 1,
          'xcode_settings': {
            # FIXME: Remove these overrides once WebCore.xcconfig is
            # used only by this project.
            'GCC_PREFIX_HEADER': '<(DEPTH)/WebCore/WebCorePrefix.h',
            'INFOPLIST_FILE': '<(DEPTH)/WebCore/Info.plist',
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
          # FIXME: Generate derived sources.
        ],
      }],
    },
    {
      'target_name': 'Update Version',
      'type': 'none',
      'actions': [{
        'action_name': 'Update Info.plist with version information',
        'inputs': [],
         'outputs': [],
         'action': [
           'sh', '<(DEPTH)/gyp/update-info-plist.sh', '<(DEPTH)/WebCore/Info.plist'
          ]
      }],
    },
    # FIXME: Add WebCoreExportFileGenerator
  ], # targets
}
