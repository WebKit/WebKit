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
        '<(DEPTH)/WebCore/icu',
        '<(DEPTH)/WebCore/ForwardingHeaders',
        '<(PRODUCT_DIR)/usr/local/include',
        '/usr/include/libxml2',
      ],
      'sources': [
        '<@(webcore_files)',
        '<@(webcore_publicheader_files)',
        '<@(webcore_privateheader_files)',
        '<@(webcore_derived_source_files)',
        '$(SDKROOT)/System/Library/Frameworks/Accelerate.framework',
        '$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework',
        '$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework',
        '$(SDKROOT)/System/Library/Frameworks/AudioUnit.framework',
        '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
        '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
        '$(SDKROOT)/System/Library/Frameworks/CoreAudio.framework',
        '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
        '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
        '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
        '$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework',
        '<(PRODUCT_DIR)/JavaScriptCore.framework',
        'libicucore.dylib',
        'libobjc.dylib',
        'libxml2.dylib',
        'libz.dylib',
      ],
      'sources/': [
        ['exclude', 'accessibility/[^/]+/'],
        ['include', 'accessibility/mac/'],
        ['exclude', 'bindings/[^/]+/'],
        ['include', 'bindings/generic/'],
        ['include', 'bindings/js/'],
        ['include', 'bindings/objc/'],
        ['exclude', 'bridge/jni/v8/'],
        ['exclude', 'bridge/qt/'],
        # FIXME: These files shouldn't be in this directory.
        ['exclude', 'bridge/testbindings\.cpp'],
        ['exclude', 'bridge/testbindings\.mm'],
        ['exclude', 'bridge/testqtbindings\.cpp'],
        ['exclude', 'editing/[^/]+/'],
        ['include', 'editing/mac/'],
        ['exclude', 'history/[^/]+/'],
        ['include', 'history/cf/'],
        ['include', 'history/mac/'],
        ['exclude', 'loader/[^/]+/'],
        ['include', 'loader/appcache/'],
        ['include', 'loader/archive/'],
        ['include', 'loader/cache/'],
        ['include', 'loader/cf/'],
        ['include', 'loader/icon/'],
        ['include', 'loader/mac/'],
        ['exclude', 'page/[^/]+/'],
        ['include', 'page/animation/'],
        ['include', 'page/mac/'],
        ['exclude', 'platform/[^/]+/'],
        ['include', 'platform/animation/'],
        ['include', 'platform/audio/'],
        ['exclude', 'platform/audio/[^/]+/'],
        ['include', 'platform/audio/mac/'],
        ['include', 'platform/audio/fftw/'], # FIXME: Is this correct? mkl is the other choice.
        ['include', 'platform/audio/resources/'],
        ['include', 'platform/cf/'],
        ['include', 'platform/cocoa/'],
        ['include', 'platform/graphics/'],
        ['exclude', 'platform/graphics/[^/]+/'],
        ['include', 'platform/graphics/ca/'],
        ['include', 'platform/graphics/cg/'],
        # FIXME: This file appears to be misplaced.
        ['exclude', 'platform/graphics/cg/FontPlatformData\.h'],
        ['include', 'platform/graphics/cocoa/'],
        ['include', 'platform/graphics/filters/'],
        ['include', 'platform/graphics/gpu/'],
        ['include', 'platform/graphics/mac/'],
        ['include', 'platform/graphics/opengl/'],
        ['include', 'platform/graphics/transforms/'],
        ['include', 'platform/mac/'],
        ['include', 'platform/mock/'],
        ['include', 'platform/network/'],
        ['exclude', 'platform/network/[^/]+/'],
        ['include', 'platform/network/cf'],
        ['include', 'platform/network/mac'],
        ['include', 'platform/posix/'],
        ['include', 'platform/sql/'],
        ['exclude', 'platform/sql/chromium'],
        ['include', 'platform/text/'],
        ['exclude', 'platform/text/[^/]+/'],
        ['include', 'platform/text/cf'],
        ['include', 'platform/text/mac'],
        ['include', 'platform/text/transcoder'],
        ['exclude', 'DerivedSources\.cpp$'],
        # FIXME: Consider using one or more AllInOne files.
        ['exclude', '(Chromium|Win|Qt)\.cpp$'],
        ['exclude', 'AllInOne\.cpp$'],
        ['exclude', 'WebCore\.gyp/mac/Empty\.cpp$']
      ],
      'mac_framework_headers': [
        '<@(webcore_publicheader_files)',
      ],
      'mac_framework_private_headers': [
        '<@(webcore_privateheader_files)',
      ],
      'xcode_config_file': '../Configurations/WebCore.xcconfig',
      # FIXME: A number of these actions aren't supposed to run if "${ACTION}" = "installhdrs"
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
        {
          'postbuild_name': 'Check For Inappropriate Files in Framework',
          'action': [
            'sh', '<(DEPTH)/gyp/run-if-exists.sh', '<(DEPTH)/../Tools/Scripts/check-for-inappropriate-files-in-framework'
          ],
        },
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
        'action_name': 'Generate Derived Sources',
        'inputs': [],
        'outputs': [],
        'action': [
          'sh', 'generate-derived-sources.sh',
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
