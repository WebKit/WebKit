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
        '<(DEPTH)/WebCore',
        '<(DEPTH)/WebCore/icu',
        '<(DEPTH)/WebCore/ForwardingHeaders',
        '<(PRODUCT_DIR)/usr/local/include',
        '/usr/include/libxml2',
        '<(PRODUCT_DIR)/DerivedSources/WebCore',
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
        # FIXME: Figure out how to build these mm files.
        ['exclude', 'DerivedSources/.*\\.mm$'],

        # FIXME: These need to be built, but they're hard.
        ['exclude', 'platform/mac/DragDataMac.mm$'],
        ['exclude', 'platform/mac/HTMLConverter.mm$'],
        ['exclude', 'platform/mac/PasteboardMac.mm$'],
        ['exclude', 'platform/mac/WebCoreObjCExtras.mm$'],

        ['exclude', 'bindings/[^/]+/'],
        ['include', 'bindings/generic/'],
        ['include', 'bindings/js/'],
        # FIXME: Build bindings/objc/
        ['exclude', 'bindings/js/ScriptControllerMac\\.mm$'],
        ['include', 'bindings/objc/[^/]+\\.h$'],

        # FIXME: This could should move to Source/ThirdParty.
        ['exclude', 'thirdparty/'],

        # FIXME: Figure out how to store these patterns in a variable.
        ['exclude', '(android|brew|cairo|chromium|curl|efl|freetype|fftw|gstreamer|gtk|haiku|linux|mkl|openvg|pango|posix|qt|skia|soup|symbian|texmap|iphone|v8|win|wince|wx)/'],
        ['exclude', '(Android|Brew|Cairo|CF|Curl|Chromium|Efl|Haiku|Gtk|JSC|Linux|OpenType|POSIX|Posix|Qt|Safari|Soup|Symbian|V8|Win|WinCE|Wx)\\.(cpp|mm?)$'],
        ['exclude', 'Chromium[^/]*\\.(cpp|mm?)$'],

        ['exclude', 'platform/image-decoders/'],
        ['exclude', 'platform/image-encoders/'],

        ['exclude', 'bridge/testbindings\\.cpp$'], # Remove from GYPI?
        ['exclude', 'bridge/testbindings\\.mm$'], # Remove from GYPI?
        ['exclude', 'bridge/testqtbindings\\.cpp$'], # Remove from GYPI?
        ['exclude', 'platform/KillRingNone\\.cpp$'],
        ['exclude', 'platform/graphics/cg/FontPlatformData\\.h$'],
        ['exclude', 'platform/graphics/gpu/LoopBlinnPathProcessor\\.(cpp|h)$'],
        # FIXME: Consider excluding GL as a suffix.
        ['exclude', 'platform/graphics/opengl/TextureMapperGL\\.cpp$'],
        ['exclude', 'platform/graphics/opentype/OpenTypeUtilities\\.(cpp|h)$'],
        ['exclude', 'platform/graphics/ImageSource\\.cpp$'],
        ['exclude', 'platform/text/LocalizedNumberICU\\.cpp$'],
        ['exclude', 'platform/text/LocalizedNumberNone\\.cpp$'],
        ['exclude', 'platform/text/TextEncodingDetectorNone\\.cpp$'],
        ['exclude', 'platform/text/Hyphenation\\.cpp$'],
        ['exclude', 'plugins/PluginDataNone\\.cpp$'],
        ['exclude', 'plugins/PluginDatabase\\.cpp$'],
        ['exclude', 'plugins/PluginPackageNone\\.cpp$'],
        ['exclude', 'plugins/PluginStream\\.cpp$'],
        ['exclude', 'plugins/PluginView\\.cpp$'],
        ['exclude', 'plugins/mac/PluginViewMac\\.mm$'],
        ['exclude', 'plugins/npapi\\.cpp$'],

        # FIXME: Check whether we need to build these derived source files.
        ['exclude', 'JSAbstractView\\.(cpp|h)'],
        ['exclude', 'JSElementTimeControl\\.(cpp|h)'],
        ['exclude', 'JSMathMLElementWrapperFactory\\.(cpp|h)'],
        ['exclude', 'JSSVGExternalResourcesRequired\\.(cpp|h)'],
        ['exclude', 'JSSVGFilterPrimitiveStandardAttributes\\.(cpp|h)'],
        ['exclude', 'JSSVGFitToViewBox\\.(cpp|h)'],
        ['exclude', 'JSSVGLangSpace\\.(cpp|h)'],
        ['exclude', 'JSSVGLocatable\\.(cpp|h)'],
        ['exclude', 'JSSVGStylable\\.(cpp|h)'],
        ['exclude', 'JSSVGTests\\.(cpp|h)'],
        ['exclude', 'JSSVGTransformable\\.(cpp|h)'],
        ['exclude', 'JSSVGURIReference\\.(cpp|h)'],
        ['exclude', 'JSSVGZoomAndPan\\.(cpp|h)'],
        ['exclude', 'tokenizer\\.cpp'],

        ['exclude', 'AllInOne\\.cpp$'],
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
            'ALWAYS_SEARCH_USER_PATHS': 'NO',
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
