#
# Copyright (C) 2009 Google Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

{
  'includes': [
    # FIXME: Sense whether upstream or downstream build, and
    # include the right features.gypi
    '../../WebKit/chromium/features.gypi',
    '../WebCore.gypi',
  ],
  'variables': {
    # FIXEME: Sense whether upstream or downstream build, and
    # point to the right src dir
    'chromium_src_dir': '../../../..',

    # If set to 1, doesn't compile debug symbols into webcore reducing the
    # size of the binary and increasing the speed of gdb.  gcc only.
    'remove_webcore_debug_symbols%': 0,
  
    'webcore_include_dirs': [
      '../',
      '../accessibility',
      '../accessibility/chromium',
      '../bindings/v8',
      '../bindings/v8/custom',
      '../bridge',
      '../css',
      '../dom',
      '../dom/default',
      '../editing',
      '../history',
      '../html',
      '../html/canvas',
      '../inspector',
      '../loader',
      '../loader/appcache',
      '../loader/archive',
      '../loader/icon',
      '../notifications',
      '../page',
      '../page/animation',
      '../page/chromium',
      '../platform',
      '../platform/animation',
      '../platform/chromium',
      '../platform/graphics',
      '../platform/graphics/chromium',
      '../platform/graphics/opentype',
      '../platform/graphics/skia',
      '../platform/graphics/transforms',
      '../platform/image-decoders',
      '../platform/image-decoders/bmp',
      '../platform/image-decoders/gif',
      '../platform/image-decoders/ico',
      '../platform/image-decoders/jpeg',
      '../platform/image-decoders/png',
      '../platform/image-decoders/skia',
      '../platform/image-decoders/xbm',
      '../platform/image-encoders/skia',
      '../platform/mock',
      '../platform/network',
      '../platform/network/chromium',
      '../platform/sql',
      '../platform/text',
      '../plugins',
      '../rendering',
      '../rendering/style',
      '../storage',
      '../svg',
      '../svg/animation',
      '../svg/graphics',
      '../workers',
      '../xml',
    ],
    'conditions': [
      ['OS=="mac"', {
        'webcore_include_dirs+': [
          # platform/graphics/cg and mac needs to come before
          # platform/graphics/chromium so that the Mac build picks up the
          # version of ImageBufferData.h in the cg directory and
          # FontPlatformData.h in the mac directory.  The + prepends this
          # directory to the list.
          # FIXME: This shouldn't need to be prepended.
          # FIXME: Eliminate dependency on platform/graphics/mac and
          # related directories.
          # platform/graphics/cg may need to stick around, though.
          '../platform/graphics/cg',
          '../platform/graphics/mac',
        ],
        'webcore_include_dirs': [
          # FIXME: Eliminate dependency on platform/mac and related
          # directories.
          '../loader/archive/cf',
          '../platform/mac',
          '../platform/text/mac',
        ],
      }],
      ['OS=="win"', {
        'webcore_include_dirs': [
          '../page/win',
          '../platform/graphics/win',
          '../platform/text/win',
          '../platform/win',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'webcore',
      'type': '<(library)',
      'msvs_guid': '1C16337B-ACF3-4D03-AA90-851C5B5EADA6',
      'dependencies': [
        '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:pcre',
        '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:wtf',
        '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(chromium_src_dir)/skia/skia.gyp:skia',
        '<(chromium_src_dir)/third_party/libjpeg/libjpeg.gyp:libjpeg',
        '<(chromium_src_dir)/third_party/libpng/libpng.gyp:libpng',
        '<(chromium_src_dir)/third_party/libxml/libxml.gyp:libxml',
        '<(chromium_src_dir)/third_party/libxslt/libxslt.gyp:libxslt',
        '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
        '<(chromium_src_dir)/third_party/sqlite/sqlite.gyp:sqlite',
      ],
      'defines': [
        'WEBCORE_NAVIGATOR_VENDOR="Google Inc."', 
      ],
      'conditions': [
        ['OS=="linux"', {
          'defines': [
            # Mozilla on Linux effectively uses uname -sm, but when running
            # 32-bit x86 code on an x86_64 processor, it uses
            # "Linux i686 (x86_64)".  Matching that would require making a
            # run-time determination.
            'WEBCORE_NAVIGATOR_PLATFORM="Linux i686"',
          ],
        }],
        ['OS=="mac"', {
          'defines': [
            # Match Safari and Mozilla on Mac x86.
            'WEBCORE_NAVIGATOR_PLATFORM="MacIntel"',

            # Chromium's version of WebCore includes the following Objective-C
            # classes. The system-provided WebCore framework may also provide
            # these classes. Because of the nature of Objective-C binding
            # (dynamically at runtime), it's possible for the Chromium-provided
            # versions to interfere with the system-provided versions.  This may
            # happen when a system framework attempts to use WebCore.framework,
            # such as when converting an HTML-flavored string to an
            # NSAttributedString.  The solution is to force Objective-C class
            # names that would conflict to use alternate names.

            # FIXME: This list will hopefully shrink but may also grow.
            # Periodically run:
            # nm libwebcore.a | grep -E '[atsATS] ([+-]\[|\.objc_class_name)'
            # and make sure that everything listed there has the alternate
            # ChromiumWebCoreObjC name, and that nothing extraneous is listed
            # here. If all Objective-C can be eliminated from Chromium's WebCore
            # library, these defines should be removed entirely.
            'ScrollbarPrefsObserver=ChromiumWebCoreObjCScrollbarPrefsObserver',
            'WebCoreRenderThemeNotificationObserver=ChromiumWebCoreObjCWebCoreRenderThemeNotificationObserver',
            'WebFontCache=ChromiumWebCoreObjCWebFontCache',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            # Match Safari and Mozilla on Windows.
            'WEBCORE_NAVIGATOR_PLATFORM="Win32"',
          ],
          'dependencies': [
            # Needed on windows for some actions and rules
            '<(chromium_src_dir)/build/win/system.gyp:cygwin'
          ],
        }],
      ], 
      'actions': [
        # Actions to build derived sources.
        {
          'action_name': 'CSSPropertyNames',
          'inputs': [
            '../css/makeprop.pl',
            '../css/CSSPropertyNames.in',
            '../css/SVGCSSPropertyNames.in',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/CSSPropertyNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSPropertyNames.h',
          ],
          'action': ['python', '<(chromium_src_dir)/webkit/build/action_csspropertynames.py', '<@(_outputs)', '--', '<@(_inputs)'],
        },
        {
          'action_name': 'CSSValueKeywords',
          'inputs': [
            '../css/makevalues.pl',
            '../css/CSSValueKeywords.in',
            '../css/SVGCSSValueKeywords.in',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/CSSValueKeywords.c',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSValueKeywords.h',
          ],
          'action': ['python', '<(chromium_src_dir)/webkit/build/action_cssvaluekeywords.py', '<@(_outputs)', '--', '<@(_inputs)'],
        },
        {
          'action_name': 'HTMLNames',
          'inputs': [
            '../dom/make_names.pl',
            '../html/HTMLTagNames.in',
            '../html/HTMLAttributeNames.in',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/HTMLNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLNames.h',
            '<(INTERMEDIATE_DIR)/HTMLElementFactory.cpp',
            # Pass --wrapperFactory to make_names to get these (JSC build?)
            #'<(INTERMEDIATE_DIR)/JSHTMLElementWrapperFactory.cpp',
            #'<(INTERMEDIATE_DIR)/JSHTMLElementWrapperFactory.h',
          ],
          'action': ['python', '<(chromium_src_dir)/webkit/build/action_makenames.py', '<@(_outputs)', '--', '<@(_inputs)', '--', '--factory', '--extraDefines', '<(feature_defines)'],
          'process_outputs_as_sources': 1,
        },
        {
          'action_name': 'SVGNames',
          'inputs': [
            '../dom/make_names.pl',
            '../svg/svgtags.in',
            '../svg/svgattrs.in',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/SVGNames.cpp',
            '<(INTERMEDIATE_DIR)/SVGNames.h',
            '<(INTERMEDIATE_DIR)/SVGElementFactory.cpp',
            '<(INTERMEDIATE_DIR)/SVGElementFactory.h',
            # Pass --wrapperFactory to make_names to get these (JSC build?)
            #'<(INTERMEDIATE_DIR)/JSSVGElementWrapperFactory.cpp',
            #'<(INTERMEDIATE_DIR)/JSSVGElementWrapperFactory.h',
          ],
          'action': ['python', '<(chromium_src_dir)/webkit/build/action_makenames.py', '<@(_outputs)', '--', '<@(_inputs)', '--', '--factory', '--extraDefines', '<(feature_defines)'],
          'process_outputs_as_sources': 1,
        },
        {
          'action_name': 'UserAgentStyleSheets',
          'inputs': [
            '../css/make-css-file-arrays.pl',
            '../css/html.css',
            '../css/quirks.css',
            '../css/view-source.css',
            '../css/themeChromiumLinux.css',
            '../css/themeWin.css',
            '../css/themeWinQuirks.css',
            '../css/svg.css',
            '../css/mediaControls.css',
            '../css/mediaControlsChromium.css',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/UserAgentStyleSheets.h',
            '<(INTERMEDIATE_DIR)/UserAgentStyleSheetsData.cpp',
          ],
          'action': ['python', '<(chromium_src_dir)/webkit/build/action_useragentstylesheets.py', '<@(_outputs)', '--', '<@(_inputs)'],
          'process_outputs_as_sources': 1,
        },
        {
          'action_name': 'XLinkNames',
          'inputs': [
            '../dom/make_names.pl',
            '../svg/xlinkattrs.in',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/XLinkNames.cpp',
            '<(INTERMEDIATE_DIR)/XLinkNames.h',
          ],
          'action': ['python', '<(chromium_src_dir)/webkit/build/action_makenames.py', '<@(_outputs)', '--', '<@(_inputs)', '--', '--extraDefines', '<(feature_defines)'],
          'process_outputs_as_sources': 1,
        },
        {
          'action_name': 'XMLNames',
          'inputs': [
            '../dom/make_names.pl',
            '../xml/xmlattrs.in',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/XMLNames.cpp',
            '<(INTERMEDIATE_DIR)/XMLNames.h',
          ],
          'action': ['python', '<(chromium_src_dir)/webkit/build/action_makenames.py', '<@(_outputs)', '--', '<@(_inputs)', '--', '--extraDefines', '<(feature_defines)'],
          'process_outputs_as_sources': 1,
        },
        {
          'action_name': 'tokenizer',
          'inputs': [
            '../css/maketokenizer',
            '../css/tokenizer.flex',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/tokenizer.cpp',
          ],
          'action': ['python', '<(chromium_src_dir)/webkit/build/action_maketokenizer.py', '<@(_outputs)', '--', '<@(_inputs)'],
        },
      ],
      'rules': [
        # Rules to build derived sources.
        {
          'rule_name': 'bison',
          'extension': 'y',
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).cpp',
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).h'
          ],
          'action': ['python', '<(chromium_src_dir)/webkit/build/rule_bison.py', '<(RULE_INPUT_PATH)', '<(INTERMEDIATE_DIR)'],
          'process_outputs_as_sources': 1,
        },
        {
          'rule_name': 'gperf',
          'extension': 'gperf',
          # gperf output is only ever #included by other source files.  As
          # such, process_outputs_as_sources is off.  Some gperf output is
          # #included as *.c and some as *.cpp.  Since there's no way to tell
          # which one will be needed in a rule definition, declare both as
          # outputs.  The harness script will generate one file and copy it to
          # the other.
          #
          # This rule places outputs in SHARED_INTERMEDIATE_DIR because glue
          # needs access to HTMLEntityNames.c.
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/<(RULE_INPUT_ROOT).c',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/<(RULE_INPUT_ROOT).cpp',
          ],
          'action': ['python', '<(chromium_src_dir)/webkit/build/rule_gperf.py', '<(RULE_INPUT_PATH)', '<(SHARED_INTERMEDIATE_DIR)/webkit'],
          'process_outputs_as_sources': 0,
        },
        # Rule to build generated JavaScript (V8) bindings from .idl source.
        {
          'rule_name': 'binding',
          'extension': 'idl',
          'msvs_external_rule': 1,
          'inputs': [
            '../bindings/scripts/generate-bindings.pl',
            '../bindings/scripts/CodeGenerator.pm',
            '../bindings/scripts/CodeGeneratorV8.pm',
            '../bindings/scripts/IDLParser.pm',
            '../bindings/scripts/IDLStructure.pm',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/bindings/V8<(RULE_INPUT_ROOT).cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8<(RULE_INPUT_ROOT).h',
          ],
          'variables': {
            'generator_include_dirs': [
              '--include', '../css',
              '--include', '../dom',
              '--include', '../html',
              '--include', '../notifications',
              '--include', '../page',
              '--include', '../plugins',
              '--include', '../svg',
              '--include', '../workers',
              '--include', '../xml',
            ],
          },
          'action': ['python', '<(chromium_src_dir)/webkit/build/rule_binding.py', '<(RULE_INPUT_PATH)', '<(INTERMEDIATE_DIR)/bindings', '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings', '--', '<@(_inputs)', '--', '--defines', '<(feature_defines) LANGUAGE_JAVASCRIPT V8_BINDING', '--generator', 'V8', '<@(generator_include_dirs)'],
          # They are included by DerivedSourcesAllInOne.cpp instead.
          'process_outputs_as_sources': 0,
          'message': 'Generating binding from <(RULE_INPUT_PATH)',
        },
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings',
        '<@(webcore_include_dirs)',
      ],
      'sources': [
        # bison rule
        '../css/CSSGrammar.y',
        '../xml/XPathGrammar.y',

        # gperf rule
        '../html/DocTypeStrings.gperf',
        '../html/HTMLEntityNames.gperf',
        '../platform/ColorData.gperf',

        # This file includes all the .cpp files generated from the .idl files
        # in webcore_files.
        '../bindings/v8/DerivedSourcesAllInOne.cpp',

        '<@(webcore_files)',

        '<(chromium_src_dir)/webkit/extensions/v8/gc_extension.cc',
        '<(chromium_src_dir)/webkit/extensions/v8/gc_extension.h',
        '<(chromium_src_dir)/webkit/extensions/v8/gears_extension.cc',
        '<(chromium_src_dir)/webkit/extensions/v8/gears_extension.h',
        '<(chromium_src_dir)/webkit/extensions/v8/interval_extension.cc',
        '<(chromium_src_dir)/webkit/extensions/v8/interval_extension.h',
        '<(chromium_src_dir)/webkit/extensions/v8/playback_extension.cc',
        '<(chromium_src_dir)/webkit/extensions/v8/playback_extension.h',
        '<(chromium_src_dir)/webkit/extensions/v8/profiler_extension.cc',
        '<(chromium_src_dir)/webkit/extensions/v8/profiler_extension.h',
        '<(chromium_src_dir)/webkit/extensions/v8/benchmarking_extension.cc',
        '<(chromium_src_dir)/webkit/extensions/v8/benchmarking_extension.h',

        # For WebCoreSystemInterface, Mac-only.
        '../../WebKit/mac/WebCoreSupport/WebSystemInterface.m',
      ],
      'sources/': [
        # Exclude JSC custom bindings.
        ['exclude', 'bindings/js'],

        # SVG_FILTERS only.
        ['exclude', 'svg/SVG(FE|Filter)[^/]*\\.idl$'],

        # Fortunately, many things can be excluded by using broad patterns.

        # Exclude things that don't apply to the Chromium platform on the basis
        # of their enclosing directories and tags at the ends of their
        # filenames.
        ['exclude', '(android|cairo|cf|cg|curl|gtk|haiku|linux|mac|opentype|posix|qt|soup|symbian|win|wx)/'],
        ['exclude', '(?<!Chromium)(SVGAllInOne|Android|Cairo|CF|CG|Curl|Gtk|Linux|Mac|OpenType|POSIX|Posix|Qt|Safari|Soup|Symbian|Win|Wx)\\.(cpp|mm?)$'],

        # JSC-only.
        ['exclude', 'inspector/JavaScript[^/]*\\.cpp$'],

        # ENABLE_OFFLINE_WEB_APPLICATIONS, exclude most of webcore's impl
        ['exclude', 'loader/appcache/'],
        ['include', 'loader/appcache/ApplicationCacheHost\.h$'],
        ['include', 'loader/appcache/DOMApplicationCache\.(h|cpp|idl)$'],

        # SVG_FILTERS only.
        ['exclude', '(platform|svg)/graphics/filters/'],
        ['exclude', 'svg/Filter[^/]*\\.cpp$'],
        ['exclude', 'svg/SVG(FE|Filter)[^/]*\\.cpp$'],

        # Exclude some DB-related files.
        ['exclude', 'platform/sql/SQLiteFileSystem.cpp'],
      ],
      'sources!': [
        # Custom bindings in bindings/v8/custom exist for these.
        '../dom/EventListener.idl',
        '../dom/EventTarget.idl',
        '../html/VoidCallback.idl',

        # JSC-only.
        '../inspector/JavaScriptCallFrame.idl',

        # ENABLE_GEOLOCATION only.
        '../page/Geolocation.idl',
        '../page/Geoposition.idl',
        '../page/PositionCallback.idl',
        '../page/PositionError.idl',
        '../page/PositionErrorCallback.idl',

        # Bindings with custom Objective-C implementations.
        '../page/AbstractView.idl',

        # FIXME: I don't know why all of these are excluded.
        # Extra SVG bindings to exclude.
        '../svg/ElementTimeControl.idl',
        '../svg/SVGAnimatedPathData.idl',
        '../svg/SVGComponentTransferFunctionElement.idl',
        '../svg/SVGExternalResourcesRequired.idl',
        '../svg/SVGFitToViewBox.idl',
        '../svg/SVGHKernElement.idl',
        '../svg/SVGLangSpace.idl',
        '../svg/SVGLocatable.idl',
        '../svg/SVGStylable.idl',
        '../svg/SVGTests.idl',
        '../svg/SVGTransformable.idl',
        '../svg/SVGViewSpec.idl',
        '../svg/SVGZoomAndPan.idl',

        # FIXME: I don't know why these are excluded, either.
        # Someone (me?) should figure it out and add appropriate comments.
        '../css/CSSUnknownRule.idl',

        # A few things can't be excluded by patterns.  List them individually.

        # Don't build StorageNamespace.  We have our own implementation.
        '../storage/StorageNamespace.cpp',

        # Use history/BackForwardListChromium.cpp instead.
        '../history/BackForwardList.cpp',

        # Use loader/icon/IconDatabaseNone.cpp instead.
        '../loader/icon/IconDatabase.cpp',

        # Use platform/KURLGoogle.cpp instead.
        '../platform/KURL.cpp',

        # Use platform/MIMETypeRegistryChromium.cpp instead.
        '../platform/MIMETypeRegistry.cpp',

        # Theme.cpp is used only if we're using USE_NEW_THEME. We are not for
        # Windows and Linux. We manually include Theme.cpp for the Mac below.
        '../platform/Theme.cpp',

        # Exclude some, but not all, of plugins.
        '../plugins/PluginDatabase.cpp',
        '../plugins/PluginInfoStore.cpp',
        '../plugins/PluginMainThreadScheduler.cpp',
        '../plugins/PluginPackage.cpp',
        '../plugins/PluginStream.cpp',
        '../plugins/PluginView.cpp',
        '../plugins/npapi.cpp',

        # Use LinkHashChromium.cpp instead
        '../platform/LinkHash.cpp',

        # Don't build these.
        # FIXME: I don't know exactly why these are excluded.  It would
        # be nice to provide more explicit comments.  Some of these do actually
        # compile.
        '../dom/StaticStringList.cpp',
        '../loader/icon/IconFetcher.cpp',
        '../loader/UserStyleSheetLoader.cpp',
        '../platform/graphics/GraphicsLayer.cpp',
        '../platform/graphics/RenderLayerBacking.cpp',
        '../platform/graphics/RenderLayerCompositor.cpp',

        # We use a multi-process version from the WebKit API.
        '../dom/default/PlatformMessagePortChannel.cpp',
        '../dom/default/PlatformMessagePortChannel.h',

      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings',
          '<@(webcore_include_dirs)',
        ],
        'mac_framework_dirs': [
          '$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework/Frameworks',
        ],
      },
      'export_dependent_settings': [
        '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:wtf',
        '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(chromium_src_dir)/skia/skia.gyp:skia',
        '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
      ],
      'link_settings': {
        'mac_bundle_resources': [
          '../Resources/aliasCursor.png',
          '../Resources/cellCursor.png',
          '../Resources/contextMenuCursor.png',
          '../Resources/copyCursor.png',
          '../Resources/crossHairCursor.png',
          '../Resources/eastResizeCursor.png',
          '../Resources/eastWestResizeCursor.png',
          '../Resources/helpCursor.png',
          '../Resources/linkCursor.png',
          '../Resources/missingImage.png',
          '../Resources/moveCursor.png',
          '../Resources/noDropCursor.png',
          '../Resources/noneCursor.png',
          '../Resources/northEastResizeCursor.png',
          '../Resources/northEastSouthWestResizeCursor.png',
          '../Resources/northResizeCursor.png',
          '../Resources/northSouthResizeCursor.png',
          '../Resources/northWestResizeCursor.png',
          '../Resources/northWestSouthEastResizeCursor.png',
          '../Resources/notAllowedCursor.png',
          '../Resources/progressCursor.png',
          '../Resources/southEastResizeCursor.png',
          '../Resources/southResizeCursor.png',
          '../Resources/southWestResizeCursor.png',
          '../Resources/verticalTextCursor.png',
          '../Resources/waitCursor.png',
          '../Resources/westResizeCursor.png',
          '../Resources/zoomInCursor.png',
          '../Resources/zoomOutCursor.png',
        ],
      },
      'hard_dependency': 1,
      'mac_framework_dirs': [
        '$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework/Frameworks',
      ],
      'msvs_disabled_warnings': [
        4138, 4244, 4291, 4305, 4344, 4355, 4521, 4099,
      ],
      'scons_line_length' : 1,
      'xcode_settings': {
        # Some Mac-specific parts of WebKit won't compile without having this
        # prefix header injected.
        # FIXME: make this a first-class setting.
        'GCC_PREFIX_HEADER': '../WebCorePrefix.h',
      },
      'conditions': [
        ['javascript_engine=="v8"', {
          'dependencies': [
            '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
          ],
          'export_dependent_settings': [
            '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
          ],
        }],
        ['OS=="linux" or OS=="freebsd"', {
          'dependencies': [
            '<(chromium_src_dir)/build/linux/system.gyp:fontconfig',
            '<(chromium_src_dir)/build/linux/system.gyp:gtk',
          ],
          'sources': [
            '../platform/graphics/chromium/VDMXParser.cpp',
            '../platform/graphics/chromium/HarfbuzzSkia.cpp',
          ],
          'sources/': [
            # Cherry-pick files excluded by the broader regular expressions above.
            ['include', 'platform/chromium/KeyCodeConversionGtk\\.cpp$'],
            ['include', 'platform/graphics/chromium/FontCacheLinux\\.cpp$'],
            ['include', 'platform/graphics/chromium/FontLinux\\.cpp$'],
            ['include', 'platform/graphics/chromium/FontPlatformDataLinux\\.cpp$'],
            ['include', 'platform/graphics/chromium/GlyphPageTreeNodeLinux\\.cpp$'],
            ['include', 'platform/graphics/chromium/SimpleFontDataLinux\\.cpp$'],
          ],
          'cflags': [
            # WebCore does not work with strict aliasing enabled.
            # https://bugs.webkit.org/show_bug.cgi?id=25864
            '-fno-strict-aliasing',
          ],
        }],
        ['OS=="mac"', {
          'actions': [
            {
              # Allow framework-style #include of
              # <WebCore/WebCoreSystemInterface.h>.
              'action_name': 'WebCoreSystemInterface.h',
              'inputs': [
                '../platform/mac/WebCoreSystemInterface.h',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/WebCore/WebCoreSystemInterface.h',
              ],
              'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
            },
          ],
          'include_dirs': [
            '../../WebKitLibraries',
          ],
          'sources/': [
            # Additional files from the WebCore Mac build that are presently
            # used in the WebCore Chromium Mac build too.

            # The Mac build is PLATFORM_CF but does not use CFNetwork.
            ['include', 'CF\\.cpp$'],
            ['exclude', 'network/cf/'],

            # The Mac build is PLATFORM_CG too.  platform/graphics/cg is the
            # only place that CG files we want to build are located, and not
            # all of them even have a CG suffix, so just add them by a
            # regexp matching their directory.
            ['include', 'platform/graphics/cg/[^/]*(?<!Win)?\\.(cpp|mm?)$'],

            # Use native Mac font code from WebCore.
            ['include', 'platform/(graphics/)?mac/[^/]*Font[^/]*\\.(cpp|mm?)$'],

            # Cherry-pick some files that can't be included by broader regexps.
            # Some of these are used instead of Chromium platform files, see
            # the specific exclusions in the "sources!" list below.
            ['include', 'loader/archive/cf/LegacyWebArchive\\.cpp$'],
            ['include', 'platform/graphics/mac/ColorMac\\.mm$'],
            ['include', 'platform/graphics/mac/FloatPointMac\\.mm$'],
            ['include', 'platform/graphics/mac/FloatRectMac\\.mm$'],
            ['include', 'platform/graphics/mac/FloatSizeMac\\.mm$'],
            ['include', 'platform/graphics/mac/GlyphPageTreeNodeMac\\.cpp$'],
            ['include', 'platform/graphics/mac/GraphicsContextMac\\.mm$'],
            ['include', 'platform/graphics/mac/IntRectMac\\.mm$'],
            ['include', 'platform/mac/BlockExceptions\\.mm$'],
            ['include', 'platform/mac/LocalCurrentGraphicsContext\\.mm$'],
            ['include', 'platform/mac/PurgeableBufferMac\\.cpp$'],
            ['include', 'platform/mac/ScrollbarThemeMac\\.mm$'],
            ['include', 'platform/mac/WebCoreSystemInterface\\.mm$'],
            ['include', 'platform/mac/WebCoreTextRenderer\\.mm$'],
            ['include', 'platform/text/mac/ShapeArabic\\.c$'],
            ['include', 'platform/text/mac/String(Impl)?Mac\\.mm$'],
            # Use USE_NEW_THEME on Mac.
            ['include', 'platform/Theme\\.cpp$'],

            ['include', 'WebKit/mac/WebCoreSupport/WebSystemInterface\\.m$'],
          ],
          'sources!': [
            # The Mac currently uses FontCustomPlatformData.cpp from
            # platform/graphics/mac, included by regex above, instead.
            '../platform/graphics/chromium/FontCustomPlatformData.cpp',

            # The Mac currently uses ScrollbarThemeMac.mm, included by regex
            # above, instead of ScrollbarThemeChromium.cpp.
            '../platform/chromium/ScrollbarThemeChromium.cpp',

            # The Mac uses ImageSourceCG.cpp from platform/graphics/cg, included
            # by regex above, instead.
            '../platform/graphics/ImageSource.cpp',

            # These Skia files aren't currently built on the Mac, which uses
            # CoreGraphics directly for this portion of graphics handling.
            '../platform/graphics/skia/FloatPointSkia.cpp',
            '../platform/graphics/skia/FloatRectSkia.cpp',
            '../platform/graphics/skia/GradientSkia.cpp',
            '../platform/graphics/skia/GraphicsContextSkia.cpp',
            '../platform/graphics/skia/ImageBufferSkia.cpp',
            '../platform/graphics/skia/ImageSkia.cpp',
            '../platform/graphics/skia/ImageSourceSkia.cpp',
            '../platform/graphics/skia/IntPointSkia.cpp',
            '../platform/graphics/skia/IntRectSkia.cpp',
            '../platform/graphics/skia/PathSkia.cpp',
            '../platform/graphics/skia/PatternSkia.cpp',
            '../platform/graphics/skia/TransformationMatrixSkia.cpp',

            # RenderThemeChromiumSkia is not used on mac since RenderThemeChromiumMac
            # does not reference the Skia code that is used by Windows and Linux.
            '../rendering/RenderThemeChromiumSkia.cpp',

            # Skia image-decoders are also not used on mac.  CoreGraphics
            # is used directly instead.
            '../platform/image-decoders/ImageDecoder.h',
            '../platform/image-decoders/bmp/BMPImageDecoder.cpp',
            '../platform/image-decoders/bmp/BMPImageDecoder.h',
            '../platform/image-decoders/bmp/BMPImageReader.cpp',
            '../platform/image-decoders/bmp/BMPImageReader.h',
            '../platform/image-decoders/gif/GIFImageDecoder.cpp',
            '../platform/image-decoders/gif/GIFImageDecoder.h',
            '../platform/image-decoders/gif/GIFImageReader.cpp',
            '../platform/image-decoders/gif/GIFImageReader.h',
            '../platform/image-decoders/ico/ICOImageDecoder.cpp',
            '../platform/image-decoders/ico/ICOImageDecoder.h',
            '../platform/image-decoders/jpeg/JPEGImageDecoder.cpp',
            '../platform/image-decoders/jpeg/JPEGImageDecoder.h',
            '../platform/image-decoders/png/PNGImageDecoder.cpp',
            '../platform/image-decoders/png/PNGImageDecoder.h',
            '../platform/image-decoders/skia/ImageDecoderSkia.cpp',
            '../platform/image-decoders/xbm/XBMImageDecoder.cpp',
            '../platform/image-decoders/xbm/XBMImageDecoder.h',
          ],
          'link_settings': {
            'libraries': [
              '../../WebKitLibraries/libWebKitSystemInterfaceLeopard.a',
            ],
          },
          'direct_dependent_settings': {
            'include_dirs': [
              '../../WebKitLibraries',
              '../../WebKit/mac/WebCoreSupport',
            ],
          },
        }],
        ['OS=="win"', {
          'dependencies': ['<(chromium_src_dir)/build/win/system.gyp:cygwin'],
          'sources/': [
            ['exclude', 'Posix\\.cpp$'],
            ['include', '/opentype/'],
            ['include', '/TransparencyWin\\.cpp$'],
            ['include', '/SkiaFontWin\\.cpp$'],
          ],
          'defines': [
            '__PRETTY_FUNCTION__=__FUNCTION__',
          ],
          # This is needed because Event.h in this directory is blocked
          # by a system header on windows.
          'include_dirs++': ['../dom'],
          'direct_dependent_settings': {
            'include_dirs+++': ['../dom'],
          },
        }],
        ['OS!="linux" and OS!="freebsd"', {'sources/': [['exclude', '(Gtk|Linux)\\.cpp$']]}],
        ['OS!="mac"', {'sources/': [['exclude', 'Mac\\.(cpp|mm?)$']]}],
        ['OS!="win"', {
          'sources/': [
            ['exclude', 'Win\\.cpp$'],
            ['exclude', '/(Windows|Uniscribe)[^/]*\\.cpp$']
          ],
        }],
        ['OS!="win" and remove_webcore_debug_symbols==1', {
          'configurations': {
            'Debug': {
              'cflags!': ['-g'],
            }
          },
        }],
      ],
    },
  ], # targets
}
