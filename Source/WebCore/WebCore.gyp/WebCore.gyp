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
    '../../WebKit/chromium/WinPrecompile.gypi',
    # FIXME: Sense whether upstream or downstream build, and
    # include the right features.gypi
    '../../WebKit/chromium/features.gypi',
    '../WebCore.gypi',
  ],

  'variables': {
    # If set to 1, doesn't compile debug symbols into webcore reducing the
    # size of the binary and increasing the speed of gdb.  gcc only.
    'remove_webcore_debug_symbols%': 0,

    # If set to 0, doesn't build SVG support, reducing the size of the
    # binary and increasing the speed of gdb.
    'enable_svg%': 1,

    'enable_wexit_time_destructors': 1,

    'webcore_include_dirs': [
      '../',
      '../..',
      '../Modules/battery',
      '../Modules/filesystem',
      '../Modules/filesystem/chromium',
      '../Modules/gamepad',
      '../Modules/geolocation',
      '../Modules/intents',
      '../Modules/indexeddb',
      '../Modules/mediasource',
      '../Modules/mediastream',
      '../Modules/navigatorcontentutils',
      '../Modules/notifications',
      '../Modules/quota',
      '../Modules/speech',
      '../Modules/webaudio',
      '../Modules/webdatabase',
      '../Modules/webdatabase/chromium',
      '../Modules/websockets',
      '../accessibility',
      '../accessibility/chromium',
      '../bindings',
      '../bindings/generic',
      '../bindings/v8',
      '../bindings/v8/custom',
      '../bridge',
      '../bridge/jni',
      '../bridge/jni/v8',
      '../css',
      '../dom',
      '../dom/default',
      '../editing',
      '../fileapi',
      '../history',
      '../html',
      '../html/canvas',
      '../html/parser',
      '../html/shadow',
      '../html/track',
      '../inspector',
      '../loader',
      '../loader/appcache',
      '../loader/archive',
      '../loader/archive/cf',
      '../loader/archive/mhtml',
      '../loader/cache',
      '../loader/icon',
      '../mathml',
      '../page',
      '../page/animation',
      '../page/chromium',
      '../page/scrolling',
      '../page/scrolling/chromium',
      '../platform',
      '../platform/animation',
      '../platform/audio',
      '../platform/audio/chromium',
      '../platform/chromium',
      '../platform/chromium/support',
      '../platform/graphics',
      '../platform/graphics/chromium',
      '../platform/graphics/chromium/cc',
      '../platform/graphics/cpu/arm',
      '../platform/graphics/cpu/arm/filters',
      '../platform/graphics/filters',
      '../platform/graphics/filters/skia',
      '../platform/graphics/gpu',
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
      '../platform/image-decoders/webp',
      '../platform/image-encoders/skia',
      '../platform/leveldb',
      '../platform/mediastream',
      '../platform/mediastream/chromium',
      '../platform/mock',
      '../platform/network',
      '../platform/network/chromium',
      '../platform/sql',
      '../platform/text',
      '../platform/text/transcoder',
      '../plugins',
      '../plugins/chromium',
      '../rendering',
      '../rendering/mathml',
      '../rendering/style',
      '../rendering/svg',
      '../storage',
      '../svg',
      '../svg/animation',
      '../svg/graphics',
      '../svg/graphics/filters',
      '../svg/properties',
      '../../ThirdParty/glu',
      '../workers',
      '../xml',
      '../xml/parser',
    ],

    'bindings_idl_files': [
      '<@(webcore_bindings_idl_files)',
    ],

    'bindings_idl_files!': [
      # Custom bindings in bindings/v8/custom exist for these.
      '../dom/EventListener.idl',

      # Bindings with custom Objective-C implementations.
      '../page/AbstractView.idl',

      # These bindings are excluded, as they're only used through inheritance and don't define constants that would need a constructor.
      '../svg/ElementTimeControl.idl',
      '../svg/SVGExternalResourcesRequired.idl',
      '../svg/SVGFilterPrimitiveStandardAttributes.idl',
      '../svg/SVGFitToViewBox.idl',

      '../svg/SVGLangSpace.idl',
      '../svg/SVGLocatable.idl',
      '../svg/SVGStylable.idl',
      '../svg/SVGTests.idl',
      '../svg/SVGTransformable.idl',

      # FIXME: I don't know why these are excluded, either.
      # Someone (me?) should figure it out and add appropriate comments.
      '../css/CSSUnknownRule.idl',
    ],

    'conditions': [
      # Location of the chromium src directory.
      ['inside_chromium_build==0', {
        # webkit is being built outside of the full chromium project.
        'chromium_src_dir': '../../WebKit/chromium',
        'libjpeg_gyp_path': '../../WebKit/chromium/third_party/libjpeg_turbo/libjpeg.gyp',
      },{
        # webkit is checked out in src/chromium/third_party/webkit
        'chromium_src_dir': '../../../../..',
      }],
      # TODO(maruel): Move it in its own project or generate it anyway?
      ['enable_svg!=0', {
        'bindings_idl_files': [
          '<@(webcore_svg_bindings_idl_files)',
        ],
      }],
      ['OS=="mac"', {
        'webcore_include_dirs': [
          # FIXME: Eliminate dependency on platform/mac and related
          # directories.
          # FIXME: Eliminate dependency on platform/graphics/mac and
          # related directories.
          # platform/graphics/cg may need to stick around, though.
          '../platform/audio/mac',
          '../platform/cocoa',
          '../platform/graphics/cg',
          '../platform/graphics/cocoa',
          '../platform/graphics/mac',
          '../platform/mac',
          '../platform/text/mac',
          '../platform/graphics/harfbuzz',
          '../platform/graphics/harfbuzz/ng',
        ],
      }],
      ['OS=="win"', {
        'webcore_include_dirs': [
          '../page/win',
          '../platform/audio/win',
          '../platform/graphics/win',
          '../platform/text/win',
          '../platform/win',
        ],
        # Using native perl rather than cygwin perl cuts execution time of idl
        # preprocessing rules by a bit more than 50%.
        'perl_exe': '<(DEPTH)/third_party/perl/perl/bin/perl.exe',
        'gperf_exe': '<(DEPTH)/third_party/gperf/bin/gperf.exe',
        'bison_exe': '<(DEPTH)/third_party/bison/bin/bison.exe',
        # Using cl instead of cygwin gcc cuts the processing time from
        # 1m58s to 0m52s.
        'preprocessor': '--preprocessor "cl.exe -nologo -EP -TP"',
      },{
        # enable -Wall and -Werror, just for Mac and Linux builds for now
        # FIXME: Also enable this for Windows after verifying no warnings
        'chromium_code': 1,
        'perl_exe': 'perl',
        'gperf_exe': 'gperf',
        'bison_exe': 'bison',
        # Without one specified, the scripts default to 'gcc'.
        'preprocessor': '',
      }],
      ['use_x11==1 or OS=="android"', {
        'webcore_include_dirs': [
          '../platform/graphics/harfbuzz',
        ],
      }],
      ['use_x11==1', {
        'webcore_include_dirs': [
          '../platform/graphics/harfbuzz/ng',
        ],
      }],
      ['OS=="win" and buildtype=="Official"', {
        # On windows official release builds, we try to preserve symbol space.
        'derived_sources_aggregate_files': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSourcesAll.cpp',
        ],
      },{
        'derived_sources_aggregate_files': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources01.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources02.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources03.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources04.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources05.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources06.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources07.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources08.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources09.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources10.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources11.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources12.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources13.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources14.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources15.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources16.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources17.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources18.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources19.cpp',
        ],
      }],
    ],
  },  # variables

  'target_defaults': {
    'variables': {
      'optimize': 'max',
    },
  },

  'conditions': [
    ['OS!="win" and remove_webcore_debug_symbols==1', {
      # Remove -g from all targets defined here.
      'target_defaults': {
        'cflags!': ['-g'],
      },
    }],
    ['os_posix==1 and OS!="mac" and gcc_version>=46', {
      'target_defaults': {
        # Disable warnings about c++0x compatibility, as some names (such as nullptr) conflict
        # with upcoming c++0x types.
        'cflags_cc': ['-Wno-c++0x-compat'],
      },
    }],
    ['OS=="linux" and target_arch=="arm"', {
      # Due to a bug in gcc arm, we get warnings about uninitialized timesNewRoman.unstatic.3258
      # and colorTransparent.unstatic.4879.
      'target_defaults': {
        'cflags': ['-Wno-uninitialized'],
      },
    }],
    ['OS == "mac"', {
      'targets': [
        {
          # On the Mac, libWebKitSystemInterface*.a is used to help WebCore
          # interface with the system.  This library is supplied as a static
          # library in binary format.  At present, it contains many global
          # symbols not marked private_extern.  It should be considered an
          # implementation detail of WebCore, and does not need these symbols
          # to be exposed so widely.
          #
          # This target contains an action that cracks open the existing
          # static library and rebuilds it with these global symbols
          # transformed to private_extern.
          'target_name': 'webkit_system_interface',
          'type': 'none',
          'variables': {
            'adjusted_library_path':
                '<(PRODUCT_DIR)/libWebKitSystemInterfaceLeopardPrivateExtern.a',
          },
          'actions': [
            {
              'action_name': 'Adjust Visibility',
              'inputs': [
                'mac/adjust_visibility.sh',
                '<(chromium_src_dir)/third_party/apple_webkit/libWebKitSystemInterfaceLeopard.a',
              ],
              'outputs': [
                '<(adjusted_library_path)',
              ],
              'action': [
                '<@(_inputs)',
                '<@(_outputs)',
                '<(INTERMEDIATE_DIR)/adjust_visibility',  # work directory
              ],
            },
          ],  # actions
          'link_settings': {
            'libraries': [
              '<(adjusted_library_path)',

              # libWebKitSystemInterfaceLeopard.a references _kCIFormatRGBA8.
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
            ],
          },  # link_settings
        },  # target webkit_system_interface
      ],  # targets
    }],  # condition OS == "mac"
    ['clang==1', {
      'target_defaults': {
        'cflags': ['-Wglobal-constructors', '-Wunused-parameter'],
        'xcode_settings': {
          'WARNING_CFLAGS': ['-Wglobal-constructors', '-Wunused-parameter'],
        },
      },
    }],
  ],  # conditions

  'targets': [
    {
      'target_name': 'inspector_protocol_sources',
      'type': 'none',
      'dependencies': [
        'generate_inspector_protocol_version'
      ],
      'actions': [
        {
          'action_name': 'generateInspectorProtocolSources',
          'inputs': [
            # First input. It stands for python script in action below.
            '../inspector/CodeGeneratorInspector.py',
            # Other inputs. They go as arguments to the python script.
            '../inspector/Inspector.json',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorBackendDispatcher.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/InspectorBackendDispatcher.h',
            '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorFrontend.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/InspectorFrontend.h',
            '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorTypeBuilder.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/InspectorTypeBuilder.h',
            '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorBackendCommands.js',
          ],
          'variables': {
            'generator_include_dirs': [
            ],
          },
          'action': [
            'python',
            '<@(_inputs)',
            '--output_h_dir', '<(SHARED_INTERMEDIATE_DIR)/webkit',
            '--output_cpp_dir', '<(SHARED_INTERMEDIATE_DIR)/webcore',
          ],
          'message': 'Generating Inspector protocol sources from Inspector.json',
        },
      ]
    },
    {
      'target_name': 'generate_inspector_protocol_version',
      'type': 'none',
      'actions': [
         {
          'action_name': 'generateInspectorProtocolVersion',
          'inputs': [
            '../inspector/generate-inspector-protocol-version',
            '../inspector/Inspector.json',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/InspectorProtocolVersion.h',
          ],
          'variables': {
            'generator_include_dirs': [
            ],
          },
          'action': [
            'python',
            '../inspector/generate-inspector-protocol-version',
            '-o',
            '<@(_outputs)',
            '<@(_inputs)'
          ],
          'message': 'Validate inspector protocol for backwards compatibility and generate version file',
        }
      ]
    },
    {
      'target_name': 'inspector_overlay_page',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generateInspectorOverlayPage',
          'inputs': [
            '../inspector/InspectorOverlayPage.html',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/InspectorOverlayPage.h',
          ],
          'msvs_cygwin_shell': 0,
          'action': [
            '<(perl_exe)',
            '../inspector/xxd.pl',
            'InspectorOverlayPage_html',
            '<@(_inputs)',
            '<@(_outputs)'
          ],
          'message': 'Generating InspectorOverlayPage.h from InspectorOverlayPage.html',
        },
      ]
    },
    {
      'target_name': 'injected_canvas_script_source',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generateInjectedScriptCanvasModuleSource',
          'inputs': [
            '../inspector/InjectedScriptCanvasModuleSource.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/InjectedScriptCanvasModuleSource.h',
          ],
          'msvs_cygwin_shell': 0,
          'action': [
            '<(perl_exe)',
            '../inspector/xxd.pl',
            'InjectedScriptCanvasModuleSource_js',
            '<@(_inputs)',
            '<@(_outputs)'
          ],
          'message': 'Generating InjectedScriptCanvasModuleSource.h from InjectedScriptCanvasModuleSource.js',
        },
      ]
    },
    {
      'target_name': 'injected_script_source',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generateInjectedScriptSource',
          'inputs': [
            '../inspector/InjectedScriptSource.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/InjectedScriptSource.h',
          ],
          'msvs_cygwin_shell': 0,
          'action': [
            '<(perl_exe)',
            '../inspector/xxd.pl',
            'InjectedScriptSource_js',
            '<@(_inputs)',
            '<@(_outputs)'
          ],
          'message': 'Generating InjectedScriptSource.h from InjectedScriptSource.js',
        },
      ]
    },
    {
      'target_name': 'debugger_script_source',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generateDebuggerScriptSource',
          'inputs': [
            '../bindings/v8/DebuggerScript.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/DebuggerScriptSource.h',
          ],
          'msvs_cygwin_shell': 0,
          'action': [
            '<(perl_exe)',
            '../inspector/xxd.pl',
            'DebuggerScriptSource_js',
            '<@(_inputs)',
            '<@(_outputs)'
          ],
          'message': 'Generating DebuggerScriptSource.h from DebuggerScript.js',
        },
      ]
    },
    {
      'target_name': 'generate_supplemental_dependency',
      'type': 'none',
      'actions': [
         {
          'action_name': 'generateSupplementalDependency',
          'variables': {
            # Write sources into a file, so that the action command line won't
            # exceed OS limits.
            'idl_files_list': '<|(idl_files_list.tmp <@(bindings_idl_files))',
          },
          'inputs': [
            '../bindings/scripts/preprocess-idls.pl',
            '../bindings/scripts/IDLParser.pm',
            '../bindings/scripts/IDLAttributes.txt',
            '<(idl_files_list)',
            '<!@(cat <(idl_files_list))',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
          ],
          'msvs_cygwin_shell': 0,
          'action': [
            '<(perl_exe)',
            '-w',
            '-I../bindings/scripts',
            '../bindings/scripts/preprocess-idls.pl',
            '--defines',
            '<(feature_defines) LANGUAGE_JAVASCRIPT V8_BINDING',
            '--idlFilesList',
            '<(idl_files_list)',
            '--supplementalDependencyFile',
            '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
            '--idlAttributesFile',
            '../bindings/scripts/IDLAttributes.txt',
            '<@(preprocessor)',
          ],
          'message': 'Resolving [Supplemental=XXX] dependencies in all IDL files',
        }
      ]
    },
    {
      'target_name': 'webcore_bindings_sources',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'generate_supplemental_dependency',
      ],
      'sources': [
        # bison rule
        '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSGrammar.y',
        '../xml/XPathGrammar.y',

        # gperf rule
        '../platform/ColorData.gperf',

        # idl rules
        '<@(bindings_idl_files)',
        '<@(webcore_test_support_idl_files)',
      ],
      'actions': [
        # Actions to build derived sources.
        {
          'action_name': 'generateV8ArrayBufferViewCustomScript',
          'inputs': [
            '../bindings/v8/custom/V8ArrayBufferViewCustomScript.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/V8ArrayBufferViewCustomScript.h',
          ],
          'msvs_cygwin_shell': 0,
          'action': [
            '<(perl_exe)',
            '../inspector/xxd.pl',
            'V8ArrayBufferViewCustomScript_js',
            '<@(_inputs)',
            '<@(_outputs)'
          ],
          'message': 'Generating V8ArrayBufferViewCustomScript.h from V8ArrayBufferViewCustomScript.js',
        },
        {
          'action_name': 'generateXMLViewerCSS',
          'inputs': [
            '../xml/XMLViewer.css',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLViewerCSS.h',
          ],
          'msvs_cygwin_shell': 0,
          'action': [
            '<(perl_exe)',
            '../inspector/xxd.pl',
            'XMLViewer_css',
            '<@(_inputs)',
            '<@(_outputs)'
          ],
        },
        {
          'action_name': 'generateXMLViewerJS',
          'inputs': [
            '../xml/XMLViewer.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLViewerJS.h',
          ],
          'msvs_cygwin_shell': 0,
          'action': [
            '<(perl_exe)',
            '../inspector/xxd.pl',
            'XMLViewer_js',
            '<@(_inputs)',
            '<@(_outputs)'
          ],
        },
        {
          'action_name': 'HTMLEntityTable',
          'inputs': [
            '../html/parser/create-html-entity-table',
            '../html/parser/HTMLEntityNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLEntityTable.cpp'
          ],
          'action': [
            'python',
            '../html/parser/create-html-entity-table',
            '-o',
            '<@(_outputs)',
            '<@(_inputs)'
          ],
        },
        {
          'action_name': 'CSSPropertyNames',
          'inputs': [
            '../css/makeprop.pl',
            '../css/CSSPropertyNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSPropertyNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSPropertyNames.h',
          ],
          'action': [
            'python',
            'scripts/action_csspropertynames.py',
            '<@(_outputs)',
            '--',
            '--defines', '<(feature_defines)',
            '--',
            '<@(_inputs)',
          ],
          'conditions': [
            # TODO(maruel): Move it in its own project or generate it anyway?
            ['enable_svg!=0', {
              'inputs': [
                '../css/SVGCSSPropertyNames.in',
              ],
            }],
          ],
        },
        {
          'action_name': 'CSSValueKeywords',
          'inputs': [
            '../css/makevalues.pl',
            '../css/CSSValueKeywords.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSValueKeywords.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSValueKeywords.h',
          ],
          'action': [
            'python',
            'scripts/action_cssvaluekeywords.py',
            '<@(_outputs)',
            '--',
            '--defines', '<(feature_defines)',
            '--',
            '<@(_inputs)',
          ],
          'conditions': [
            # TODO(maruel): Move it in its own project or generate it anyway?
            ['enable_svg!=0', {
              'inputs': [
                '../css/SVGCSSValueKeywords.in',
              ],
            }],
          ],
        },
        {
          'action_name': 'HTMLNames',
          'inputs': [
            '../dom/make_names.pl',
            '../html/HTMLTagNames.in',
            '../html/HTMLAttributeNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLNames.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLElementFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/V8HTMLElementWrapperFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/V8HTMLElementWrapperFactory.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
            '--',
            '--factory',
            '--wrapperFactoryV8',
            '--extraDefines', '<(feature_defines)'
          ],
        },
        {
          'action_name': 'WebKitFontFamilyNames',
          'inputs': [
            '../dom/make_names.pl',
            '../css/WebKitFontFamilyNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/WebKitFontFamilyNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/WebKitFontFamilyNames.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
            '--',
            '--fonts',
          ],
        },
        {
          'action_name': 'SVGNames',
          'inputs': [
            '../dom/make_names.pl',
            '../svg/svgtags.in',
            '../svg/svgattrs.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/SVGNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/SVGNames.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/SVGElementFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/SVGElementFactory.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/V8SVGElementWrapperFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/V8SVGElementWrapperFactory.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
            '--',
            '--factory',
            '--wrapperFactoryV8',
            '--extraDefines', '<(feature_defines)'
          ],
        },
        {
          'action_name': 'EventFactory',
          'inputs': [
            '../dom/make_event_factory.pl',
            '../dom/EventNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/EventFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/EventHeaders.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/EventInterfaces.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
          ],
        },
        {
          'action_name': 'EventTargetFactory',
          'inputs': [
            '../dom/make_event_factory.pl',
            '../dom/EventTargetFactory.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/EventTargetHeaders.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/EventTargetInterfaces.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
          ],
        },
        {
          'action_name': 'ExceptionCodeDescription',
          'inputs': [
            '../dom/make_dom_exceptions.pl',
            '../dom/DOMExceptions.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/ExceptionCodeDescription.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/ExceptionCodeDescription.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/ExceptionHeaders.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/ExceptionInterfaces.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
          ],
        },
        {
          'action_name': 'MathMLNames',
          'inputs': [
            '../dom/make_names.pl',
            '../mathml/mathtags.in',
            '../mathml/mathattrs.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/MathMLNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/MathMLNames.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/MathMLElementFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/MathMLElementFactory.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
            '--',
            '--factory',
            '--extraDefines', '<(feature_defines)'
          ],
        },
        {
          'action_name': 'Settings',
          'inputs': [
            '../page/make_settings.pl',
            '../page/Settings.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/SettingsMacros.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
          ],
        },
        {
          'action_name': 'UserAgentStyleSheets',
          'variables': {
            'scripts': [
              '../css/make-css-file-arrays.pl',
              '../bindings/scripts/preprocessor.pm',
            ],
            # The .css files are in the same order as ../DerivedSources.make.
            'stylesheets': [
              '../css/html.css',
              '../css/quirks.css',
              '../css/view-source.css',
              '../css/themeChromium.css', # Chromium only.
              '../css/themeChromiumAndroid.css', # Chromium only.
              '../css/themeChromiumLinux.css', # Chromium only.
              '../css/themeChromiumSkia.css',  # Chromium only.
              '../css/themeWin.css',
              '../css/themeWinQuirks.css',
              '../css/svg.css',
              '../css/mathml.css',
              '../css/mediaControls.css',
              '../css/mediaControlsChromium.css',
              '../css/mediaControlsChromiumAndroid.css',
              '../css/fullscreen.css',
              # Skip fullscreenQuickTime.
            ],
          },
          'inputs': [
            '<@(scripts)',
            '<@(stylesheets)'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/UserAgentStyleSheets.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/UserAgentStyleSheetsData.cpp',
          ],
          'action': [
            'python',
            'scripts/action_useragentstylesheets.py',
            '<@(_outputs)',
            '<@(stylesheets)',
            '--',
            '<@(scripts)',
            '--',
            '--defines', '<(feature_defines)',
          ],
        },
        {
          'action_name': 'PickerCommon',
          'inputs': [
            '../Resources/pagepopups/pickerCommon.css',
            '../Resources/pagepopups/pickerCommon.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/PickerCommon.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/PickerCommon.cpp',
          ],
          'action': [
            'python',
            '../make-file-arrays.py',
            '--condition=ENABLE(CALENDAR_PICKER) OR ENABLE(INPUT_TYPE_COLOR)',
            '--out-h=<(SHARED_INTERMEDIATE_DIR)/webkit/PickerCommon.h',
            '--out-cpp=<(SHARED_INTERMEDIATE_DIR)/webkit/PickerCommon.cpp',
            '<@(_inputs)',
          ],
        },
        {
          'action_name': 'CalendarPicker',
          'inputs': [
            '../Resources/pagepopups/calendarPicker.css',
            '../Resources/pagepopups/calendarPicker.js',
            '../Resources/pagepopups/suggestionPicker.css',
            '../Resources/pagepopups/suggestionPicker.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CalendarPicker.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CalendarPicker.cpp',
          ],
          'action': [
            'python',
            '../make-file-arrays.py',
            '--condition=ENABLE(CALENDAR_PICKER)',
            '--out-h=<(SHARED_INTERMEDIATE_DIR)/webkit/CalendarPicker.h',
            '--out-cpp=<(SHARED_INTERMEDIATE_DIR)/webkit/CalendarPicker.cpp',
            '<@(_inputs)',
          ],
        },
        {
          'action_name': 'CalendarPickerMac',
          'inputs': [
            '../Resources/pagepopups/calendarPickerMac.css',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CalendarPickerMac.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CalendarPickerMac.cpp',
          ],
          'action': [
            'python',
            '../make-file-arrays.py',
            '--condition=ENABLE(CALENDAR_PICKER)',
            '--out-h=<(SHARED_INTERMEDIATE_DIR)/webkit/CalendarPickerMac.h',
            '--out-cpp=<(SHARED_INTERMEDIATE_DIR)/webkit/CalendarPickerMac.cpp',
            '<@(_inputs)',
          ],
        },
        {
          'action_name': 'ColorSuggestionPicker',
          'inputs': [
            '../Resources/pagepopups/colorSuggestionPicker.css',
            '../Resources/pagepopups/colorSuggestionPicker.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/ColorSuggestionPicker.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/ColorSuggestionPicker.cpp',
          ],
          'action': [
            'python',
            '../make-file-arrays.py',
            '--condition=ENABLE(INPUT_TYPE_COLOR)',
            '--out-h=<(SHARED_INTERMEDIATE_DIR)/webkit/ColorSuggestionPicker.h',
            '--out-cpp=<(SHARED_INTERMEDIATE_DIR)/webkit/ColorSuggestionPicker.cpp',
            '<@(_inputs)',
          ],
        },
        {
          'action_name': 'XLinkNames',
          'inputs': [
            '../dom/make_names.pl',
            '../svg/xlinkattrs.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XLinkNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XLinkNames.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
            '--',
            '--extraDefines', '<(feature_defines)'
          ],
        },
        {
          'action_name': 'XMLNSNames',
          'inputs': [
            '../dom/make_names.pl',
            '../xml/xmlnsattrs.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLNSNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLNSNames.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
            '--',
            '--extraDefines', '<(feature_defines)'
          ],
        },
        {
          'action_name': 'XMLNames',
          'inputs': [
            '../dom/make_names.pl',
            '../xml/xmlattrs.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLNames.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
            '--',
            '--extraDefines', '<(feature_defines)'
          ],
        },
        {
          'action_name': 'derived_sources_all_in_one',
          'inputs': [
            'scripts/action_derivedsourcesallinone.py',
            '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
          ],
          'outputs': [
            '<@(derived_sources_aggregate_files)',
          ],
          'action': [
            'python',
            'scripts/action_derivedsourcesallinone.py',
            '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
            '--',
            '<@(derived_sources_aggregate_files)',
          ],
        },
        {
          'action_name': 'preprocess_grammar',
          'inputs': [
            '../css/CSSGrammar.y.in',
            '../css/CSSGrammar.y.includes',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSGrammar.y',
          ],
          'action': [
            '<(perl_exe)',
            '-I',
            '../bindings/scripts',
            '../css/makegrammar.pl',
            '--outputDir',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/',
            '--extraDefines',
            '<(feature_defines)',
            '--preprocessOnly',
            '<@(preprocessor)',
            '<@(_inputs)',
          ],
        },
      ],
      'rules': [
        # Rules to build derived sources.
        {
          'rule_name': 'bison',
          'extension': 'y',
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/<(RULE_INPUT_ROOT).cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/<(RULE_INPUT_ROOT).h'
          ],
          'action': [
            'python',
            'scripts/rule_bison.py',
            '<(RULE_INPUT_PATH)',
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
            '<(bison_exe)',
          ],
        },
        {
          'rule_name': 'gperf',
          'extension': 'gperf',
          #
          # gperf outputs are generated by WebCore/make-hash-tools.pl
          #
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/<(RULE_INPUT_ROOT).cpp',
          ],
          'inputs': [
            '../make-hash-tools.pl',
          ],
          'msvs_cygwin_shell': 0,
          'action': [
            '<(perl_exe)',
            '../make-hash-tools.pl',
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
            '<(RULE_INPUT_PATH)',
            '<(gperf_exe)',
          ],
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
            '../bindings/scripts/preprocessor.pm',
            '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
            '<@(webcore_test_support_idl_files)',
          ],
          'outputs': [
            # FIXME:  The .cpp file should be in webkit/bindings once
            # we coax GYP into supporting it (see 'action' below).
            '<(SHARED_INTERMEDIATE_DIR)/webcore/bindings/V8<(RULE_INPUT_ROOT).cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8<(RULE_INPUT_ROOT).h',
          ],
          'variables': {
            'generator_include_dirs': [
              '--include', '../Modules/filesystem',
              '--include', '../Modules/indexeddb',
              '--include', '../Modules/intents',
              '--include', '../Modules/mediasource',
              '--include', '../Modules/mediastream',
              '--include', '../Modules/navigatorcontentutils',
              '--include', '../Modules/notifications',
              '--include', '../Modules/webaudio',
              '--include', '../Modules/webdatabase',
              '--include', '../css',
              '--include', '../dom',
              '--include', '../fileapi',
              '--include', '../html',
              '--include', '../page',
              '--include', '../plugins',
              '--include', '../storage',
              '--include', '../svg',
              '--include', '../testing',
              '--include', '../workers',
              '--include', '../xml',
            ],
          },
          'msvs_cygwin_shell': 0,
          # FIXME:  Note that we put the .cpp files in webcore/bindings
          # but the .h files in webkit/bindings.  This is to work around
          # the unfortunate fact that GYP strips duplicate arguments
          # from lists.  When we have a better GYP way to suppress that
          # behavior, change the output location.
          'action': [
            '<(perl_exe)',
            '-w',
            '-I../bindings/scripts',
            '../bindings/scripts/generate-bindings.pl',
            '--outputHeadersDir',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings',
            '--outputDir',
            '<(SHARED_INTERMEDIATE_DIR)/webcore/bindings',
            '--defines',
            '<(feature_defines) LANGUAGE_JAVASCRIPT V8_BINDING',
            '--generator',
            'V8',
            '<@(generator_include_dirs)',
            '--supplementalDependencyFile',
            '<(SHARED_INTERMEDIATE_DIR)/supplemental_dependency.tmp',
            '--additionalIdlFiles',
            '<(webcore_test_support_idl_files)',
            '<(RULE_INPUT_PATH)',
            '<@(preprocessor)',
          ],
          'message': 'Generating binding from <(RULE_INPUT_PATH)',
        },
      ],
    },
    {
      'target_name': 'webcore_bindings',
      'type': 'static_library',
      'hard_dependency': 1,
      'dependencies': [
        'webcore_bindings_sources',
        'inspector_overlay_page',
        'inspector_protocol_sources',
        'injected_canvas_script_source',
        'injected_script_source',
        'debugger_script_source',
        '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:yarr',
        '../../WTF/WTF.gyp/WTF.gyp:wtf',
        '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(chromium_src_dir)/skia/skia.gyp:skia',
        '<(chromium_src_dir)/third_party/iccjpeg/iccjpeg.gyp:iccjpeg',
        '<(chromium_src_dir)/third_party/libpng/libpng.gyp:libpng',
        '<(chromium_src_dir)/third_party/libxml/libxml.gyp:libxml',
        '<(chromium_src_dir)/third_party/libxslt/libxslt.gyp:libxslt',
        '<(chromium_src_dir)/third_party/libwebp/libwebp.gyp:libwebp',
        '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
        '<(chromium_src_dir)/third_party/qcms/qcms.gyp:qcms',
        '<(chromium_src_dir)/third_party/sqlite/sqlite.gyp:sqlite',
        '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
        '<(libjpeg_gyp_path):libjpeg',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        # FIXME:  Remove <(SHARED_INTERMEDIATE_DIR)/webcore when we
        # can entice gyp into letting us put both the .cpp and .h
        # files in the same output directory.
        '<(SHARED_INTERMEDIATE_DIR)/webcore',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings',
        '<@(webcore_include_dirs)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings',
        ],
      },
      'sources': [
        # These files include all the .cpp files generated from the .idl files
        # in webcore_files.
        '<@(derived_sources_aggregate_files)',

        # Additional .cpp files for HashTools.h
        '<(SHARED_INTERMEDIATE_DIR)/webkit/ColorData.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSPropertyNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSValueKeywords.cpp',

        # Additional .cpp files from webcore_bindings_sources actions.
        '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLElementFactory.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/CalendarPicker.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/ColorSuggestionPicker.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/EventFactory.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/EventHeaders.h',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/EventInterfaces.h',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/EventTargetHeaders.h',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/EventTargetInterfaces.h',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/ExceptionCodeDescription.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/PickerCommon.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/UserAgentStyleSheetsData.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/V8HTMLElementWrapperFactory.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/XLinkNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLNSNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/SVGNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/MathMLElementFactory.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/MathMLNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/WebKitFontFamilyNames.cpp',

        # Generated from HTMLEntityNames.in
        '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLEntityTable.cpp',

        # Additional .cpp files from the webcore_bindings_sources rules.
        '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSGrammar.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/XPathGrammar.cpp',

        # Additional .cpp files from the webcore_inspector_sources list.
        '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorFrontend.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorBackendDispatcher.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorTypeBuilder.cpp',
      ],
      'conditions': [
        ['inside_chromium_build==1 and OS=="win" and component=="shared_library"', {
          'defines': [
            'USING_V8_SHARED',
          ],
        }],
        # TODO(maruel): Move it in its own project or generate it anyway?
        ['enable_svg!=0', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/SVGElementFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/V8SVGElementWrapperFactory.cpp',
         ],
        }],
        ['OS=="mac"', {
          'include_dirs': [
            '<(chromium_src_dir)/third_party/apple_webkit',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CalendarPickerMac.cpp',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            'WEBCORE_NAVIGATOR_PLATFORM="Win32"',
            '__PRETTY_FUNCTION__=__FUNCTION__',
          ],
          # This is needed because Event.h in this directory is blocked
          # by a system header on windows.
          'include_dirs++': ['../dom'],
          'direct_dependent_settings': {
            'include_dirs+++': ['../dom'],
          },
          # In generated bindings code: 'switch contains default but no case'.
          'msvs_disabled_warnings': [ 4065 ],
        }],
        ['OS in ("linux", "android") and "WTF_USE_WEBAUDIO_IPP=1" in feature_defines', {
          'cflags': [
            '<!@(pkg-config --cflags-only-I ipp)',
          ],
        }],
      ],
    },
    {
      # We'll soon split libwebcore in multiple smaller libraries.
      # webcore_prerequisites will be the 'base' target of every sub-target.
      'target_name': 'webcore_prerequisites',
      'type': 'none',
      'dependencies': [
        'debugger_script_source',
        'injected_canvas_script_source',
        'injected_script_source',
        'inspector_overlay_page',
        'inspector_protocol_sources',
        'webcore_bindings_sources',
        '../../ThirdParty/glu/glu.gyp:libtess',
        '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:yarr',
        '../../WTF/WTF.gyp/WTF.gyp:wtf',
        '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(chromium_src_dir)/skia/skia.gyp:skia',
        '<(chromium_src_dir)/third_party/iccjpeg/iccjpeg.gyp:iccjpeg',
        '<(chromium_src_dir)/third_party/libwebp/libwebp.gyp:libwebp',
        '<(chromium_src_dir)/third_party/libpng/libpng.gyp:libpng',
        '<(chromium_src_dir)/third_party/libxml/libxml.gyp:libxml',
        '<(chromium_src_dir)/third_party/libxslt/libxslt.gyp:libxslt',
        '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
        '<(chromium_src_dir)/third_party/ots/ots.gyp:ots',
        '<(chromium_src_dir)/third_party/qcms/qcms.gyp:qcms',
        '<(chromium_src_dir)/third_party/sqlite/sqlite.gyp:sqlite',
        '<(chromium_src_dir)/third_party/angle/src/build_angle.gyp:translator_glsl',
        '<(chromium_src_dir)/third_party/zlib/zlib.gyp:zlib',
        '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
        '<(libjpeg_gyp_path):libjpeg',
      ],
      'export_dependent_settings': [
        '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:yarr',
        '../../WTF/WTF.gyp/WTF.gyp:wtf',
        '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(chromium_src_dir)/skia/skia.gyp:skia',
        '<(chromium_src_dir)/third_party/iccjpeg/iccjpeg.gyp:iccjpeg',
        '<(chromium_src_dir)/third_party/libwebp/libwebp.gyp:libwebp',
        '<(chromium_src_dir)/third_party/libpng/libpng.gyp:libpng',
        '<(chromium_src_dir)/third_party/libxml/libxml.gyp:libxml',
        '<(chromium_src_dir)/third_party/libxslt/libxslt.gyp:libxslt',
        '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
        '<(chromium_src_dir)/third_party/ots/ots.gyp:ots',
        '<(chromium_src_dir)/third_party/qcms/qcms.gyp:qcms',
        '<(chromium_src_dir)/third_party/sqlite/sqlite.gyp:sqlite',
        '<(chromium_src_dir)/third_party/angle/src/build_angle.gyp:translator_glsl',
        '<(chromium_src_dir)/third_party/zlib/zlib.gyp:zlib',
        '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
        '<(libjpeg_gyp_path):libjpeg',
      ],
      # This is needed for mac because of webkit_system_interface. It'd be nice
      # if this hard dependency could be split off the rest.
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'defines': [
          'WEBCORE_NAVIGATOR_VENDOR="Google Inc."',
          'WEBKIT_IMPLEMENTATION=1',
        ],
        'include_dirs': [
          '../../Platform/chromium',
          '<(INTERMEDIATE_DIR)',
          '<@(webcore_include_dirs)',
          '<(chromium_src_dir)/gpu',
          '<(chromium_src_dir)/third_party/angle/include/GLSLANG',
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings',
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
      },
      'conditions': [
        ['inside_chromium_build==1 and OS=="win" and component=="shared_library"', {
          'direct_dependent_settings': {
            'defines': [
               'USING_V8_SHARED',
            ],
          },
        }],
        ['use_accelerated_compositing==1', {
          'dependencies': [
            '<(chromium_src_dir)/gpu/gpu.gyp:gles2_c_lib',
          ],
          'export_dependent_settings': [
            '<(chromium_src_dir)/gpu/gpu.gyp:gles2_c_lib',
          ],
        }],
        ['use_x11 == 1', {
          'dependencies': [
            '<(chromium_src_dir)/build/linux/system.gyp:fontconfig',
          ],
          'export_dependent_settings': [
            '<(chromium_src_dir)/build/linux/system.gyp:fontconfig',
          ],
          'direct_dependent_settings': {
            'cflags': [
              # WebCore does not work with strict aliasing enabled.
              # https://bugs.webkit.org/show_bug.cgi?id=25864
              '-fno-strict-aliasing',
            ],
          },
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '<(chromium_src_dir)/build/linux/system.gyp:gtk',
          ],
          'export_dependent_settings': [
            '<(chromium_src_dir)/build/linux/system.gyp:gtk',
          ],
        }],
        ['OS=="android"', {
          'sources/': [
            ['exclude', 'accessibility/'],
          ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            'webkit_system_interface',
          ],
          'export_dependent_settings': [
            'webkit_system_interface',
          ],
          'direct_dependent_settings': {
            'defines': [
              # Match Safari and Mozilla on Mac x86.
              'WEBCORE_NAVIGATOR_PLATFORM="MacIntel"',

              # Chromium's version of WebCore includes the following Objective-C
              # classes. The system-provided WebCore framework may also provide
              # these classes. Because of the nature of Objective-C binding
              # (dynamically at runtime), it's possible for the
              # Chromium-provided versions to interfere with the system-provided
              # versions.  This may happen when a system framework attempts to
              # use WebCore.framework, such as when converting an HTML-flavored
              # string to an NSAttributedString.  The solution is to force
              # Objective-C class names that would conflict to use alternate
              # names.
              #
              # This list will hopefully shrink but may also grow.  Its
              # performance is monitored by the "Check Objective-C Rename"
              # postbuild step, and any suspicious-looking symbols not handled
              # here or whitelisted in that step will cause a build failure.
              #
              # If this is unhandled, the console will receive log messages
              # such as:
              # com.google.Chrome[] objc[]: Class ScrollbarPrefsObserver is implemented in both .../Google Chrome.app/Contents/Versions/.../Google Chrome Helper.app/Contents/MacOS/../../../Google Chrome Framework.framework/Google Chrome Framework and /System/Library/Frameworks/WebKit.framework/Versions/A/Frameworks/WebCore.framework/Versions/A/WebCore. One of the two will be used. Which one is undefined.
              'WebCascadeList=ChromiumWebCoreObjCWebCascadeList',
              'WebCoreFlippedView=ChromiumWebCoreObjCWebCoreFlippedView',
              'WebCoreTextFieldCell=ChromiumWebCoreObjCWebCoreTextFieldCell',
              'WebScrollbarPrefsObserver=ChromiumWebCoreObjCWebScrollbarPrefsObserver',
              'WebCoreRenderThemeNotificationObserver=ChromiumWebCoreObjCWebCoreRenderThemeNotificationObserver',
              'WebFontCache=ChromiumWebCoreObjCWebFontCache',
              'WebScrollAnimationHelperDelegate=ChromiumWebCoreObjCWebScrollAnimationHelperDelegate',
              'WebScrollbarPainterControllerDelegate=ChromiumWebCoreObjCWebScrollbarPainterControllerDelegate',
              'WebScrollbarPainterDelegate=ChromiumWebCoreObjCWebScrollbarPainterDelegate',
              'WebScrollbarPartAnimation=ChromiumWebCoreObjCWebScrollbarPartAnimation',
            ],
            'include_dirs': [
              '<(chromium_src_dir)/third_party/apple_webkit',
            ],
            'postbuilds': [
              {
                # This step ensures that any Objective-C names that aren't
                # redefined to be "safe" above will cause a build failure.
                'postbuild_name': 'Check Objective-C Rename',
                'variables': {
                  'class_whitelist_regex':
                      'ChromiumWebCoreObjC|TCMVisibleView|RTCMFlippedView',
                  'category_whitelist_regex':
                      'TCMInterposing|ScrollAnimatorChromiumMacExt',
                },
                'action': [
                  'mac/check_objc_rename.sh',
                  '<(class_whitelist_regex)',
                  '<(category_whitelist_regex)',
                ],
              },
            ],
          },
        }],
        ['OS=="win"', {
          'direct_dependent_settings': {
            'defines': [
              # Match Safari and Mozilla on Windows.
              'WEBCORE_NAVIGATOR_PLATFORM="Win32"',
              '__PRETTY_FUNCTION__=__FUNCTION__',
            ],
            # This is needed because Event.h in this directory is blocked
            # by a system header on windows.
            'include_dirs++': ['../dom'],
          },
        }],
        ['OS in ("linux", "android") and "WTF_USE_WEBAUDIO_IPP=1" in feature_defines', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags-only-I ipp)',
            ],
          },
        }],
        ['"WTF_USE_WEBAUDIO_FFMPEG=1" in feature_defines', {
          # This directory needs to be on the include path for multiple sub-targets of webcore.
          'direct_dependent_settings': {
            'include_dirs': [
              '<(chromium_src_dir)/third_party/ffmpeg',
            ],
          },
          'dependencies': [
            '<(chromium_src_dir)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
        }],
        # Windows shared builder needs extra help for linkage
        ['OS=="win" and "WTF_USE_WEBAUDIO_FFMPEG=1" in feature_defines', {
          'export_dependent_settings': [
            '<(chromium_src_dir)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
        }],
        ['"WTF_USE_LEVELDB=1" in feature_defines', {
          'dependencies': [
            '<(chromium_src_dir)/third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
          ],
          'export_dependent_settings': [
            '<(chromium_src_dir)/third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
          ],
        }],
      ],
    },
    {
      'target_name': 'webcore_dom',
      'type': 'static_library',
      'dependencies': [
        'webcore_prerequisites',
      ],
      'sources': [
        '<@(webcore_dom_files)',
      ],
      'sources!': [
        '../dom/default/PlatformMessagePortChannel.cpp',
        '../dom/default/PlatformMessagePortChannel.h',
      ],
      'sources/': [
        # FIXME: Figure out how to store these patterns in a variable.
        ['exclude', '(cairo|ca|cf|cg|curl|efl|freetype|gstreamer|gtk|linux|mac|opengl|openvg|opentype|pango|posix|qt|soup|svg|texmap|iphone|win|wince|wx)/'],
        ['exclude', '(?<!Chromium)(Cairo|CF|CG|Curl|Gtk|JSC|Linux|Mac|OpenType|POSIX|Posix|Qt|Safari|Soup|Win|WinCE|Wx)\\.(cpp|mm?)$'],

        ['exclude', 'AllInOne\\.cpp$'],
      ],
    },
    {
      'target_name': 'webcore_html',
      'type': 'static_library',
      'dependencies': [
        'webcore_prerequisites',
      ],
      'sources': [
        '<@(webcore_html_files)',
      ],
      'sources/': [
        ['exclude', 'AllInOne\\.cpp$'],
      ],
      'conditions': [
        ['OS!="android"', {
          'sources/': [
            ['exclude', 'Android\\.cpp$'],
          ],
        }],
      ],
    },
    {
      'target_name': 'webcore_svg',
      'type': 'static_library',
      'dependencies': [
        'webcore_prerequisites',
      ],
      'sources': [
        '<@(webcore_svg_files)',
      ],
      'sources/': [
        ['exclude', 'AllInOne\\.cpp$'],
      ],
    },
    {
      'target_name': 'webcore_platform',
      'type': 'static_library',
      'dependencies': [
        'webcore_prerequisites',
      ],
      # This is needed for mac because of webkit_system_interface. It'd be nice
      # if this hard dependency could be split off the rest.
      'hard_dependency': 1,
      'sources': [
        '<@(webcore_platform_files)',

        # For WebCoreSystemInterface, Mac-only.
        '../../WebKit/mac/WebCoreSupport/WebSystemInterface.mm',
      ],
      'sources/': [
        ['exclude', '.*'],
        ['include', 'platform/'],

        # FIXME: Figure out how to store these patterns in a variable.
        ['exclude', '(cairo|ca|cf|cg|curl|efl|freetype|gstreamer|gtk|harfbuzz|linux|mac|opengl|openvg|opentype|pango|posix|qt|soup|svg|texmap|iphone|win|wince|wx)/'],
        ['exclude', '(?<!Chromium)(Cairo|CF|CG|Curl|Gtk|JSC|Linux|Mac|OpenType|POSIX|Posix|Qt|Safari|Soup|Win|WinCE|Wx)\\.(cpp|mm?)$'],

        ['exclude', 'platform/LinkHash\\.cpp$'],
        ['exclude', 'platform/MIMETypeRegistry\\.cpp$'],
        ['exclude', 'platform/Theme\\.cpp$'],
        # *NEON.cpp files need special compile options.
        # They are moved to the webcore_arm_neon target.
        ['exclude', 'platform/graphics/cpu/arm/filters/.*NEON\\.(cpp|h)'],
        ['exclude', 'platform/image-encoders/JPEGImageEncoder\\.(cpp|h)$'],
        ['exclude', 'platform/image-encoders/PNGImageEncoder\\.(cpp|h)$'],
        ['exclude', 'platform/network/ResourceHandle\\.cpp$'],
        ['exclude', 'platform/sql/SQLiteFileSystem\\.cpp$'],
        ['exclude', 'platform/text/LocaleToScriptMappingICU\\.cpp$'],
        ['exclude', 'platform/text/TextEncodingDetectorNone\\.cpp$'],
      ],
      'conditions': [
        ['inside_chromium_build==1', {
            'conditions': [
                ['component=="shared_library"', {
                    'defines': [
                        'WEBKIT_DLL',
                    ],
                }],
            ],
        }],
        ['use_default_render_theme==1', {
          'sources/': [
            ['exclude', 'platform/chromium/PlatformThemeChromiumWin.h'],
            ['exclude', 'platform/chromium/PlatformThemeChromiumWin.cpp'],
            ['exclude', 'platform/chromium/ScrollbarThemeChromiumWin.cpp'],
            ['exclude', 'platform/chromium/ScrollbarThemeChromiumWin.h'],
          ],
        }, { # use_default_render_theme==0
          'sources/': [
            ['exclude', 'platform/chromium/PlatformThemeChromiumDefault.cpp'],
            ['exclude', 'platform/chromium/PlatformThemeChromiumDefault.h'],
            ['exclude', 'platform/chromium/ScrollbarThemeChromiumDefault.cpp'],
            ['exclude', 'platform/chromium/ScrollbarThemeChromiumDefault.h'],
          ],
        }],
        ['use_x11 == 1', {
          'sources/': [
            # Cherry-pick files excluded by the broader regular expressions above.
            ['include', 'platform/graphics/harfbuzz/FontHarfBuzz\\.cpp$'],
            ['include', 'platform/graphics/harfbuzz/FontPlatformDataHarfBuzz\\.cpp$'],
            ['include', 'platform/graphics/harfbuzz/HarfBuzzShaperBase\\.(cpp|h)$'],
            ['include', 'platform/graphics/harfbuzz/ng/HarfBuzzNGFace\\.(cpp|h)$'],
            ['include', 'platform/graphics/harfbuzz/ng/HarfBuzzNGFaceSkia\\.cpp$'],
            ['include', 'platform/graphics/harfbuzz/ng/HarfBuzzShaper\\.(cpp|h)$'],
            ['include', 'platform/graphics/opentype/OpenTypeTypes\\.h$'],
            ['include', 'platform/graphics/opentype/OpenTypeVerticalData\\.(cpp|h)$'],
            ['include', 'platform/graphics/skia/SimpleFontDataSkia\\.cpp$'],
          ],
        }, { # use_x11==0
          'sources/': [
            ['exclude', 'Linux\\.cpp$'],
            ['exclude', 'Harfbuzz[^/]+\\.(cpp|h)$'],
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'sources/': [
            # Cherry-pick files excluded by the broader regular expressions above.
            ['include', 'platform/chromium/KeyCodeConversionGtk\\.cpp$'],
          ],
        }, { # toolkit_uses_gtk==0
          'sources/': [
            ['exclude', 'Gtk\\.cpp$'],
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '<(chromium_src_dir)/third_party/harfbuzz-ng/harfbuzz.gyp:harfbuzz-ng',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            '<(chromium_src_dir)/third_party/harfbuzz/harfbuzz.gyp:harfbuzz',
          ],
        }],
        ['OS=="mac"', {
          # Necessary for Mac .mm stuff.
          'include_dirs': [
            '<(chromium_src_dir)/third_party/apple_webkit',
          ],
          'dependencies': [
            'webkit_system_interface',
            '<(chromium_src_dir)/third_party/harfbuzz-ng/harfbuzz.gyp:harfbuzz-ng',
          ],
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
          'sources': [
            '../editing/SmartReplaceCF.cpp',
            '../../WebKit/mac/WebCoreSupport/WebSystemInterface.mm',
          ],
          'sources/': [
            # Additional files from the WebCore Mac build that are presently
            # used in the WebCore Chromium Mac build too.

            # The Mac build is USE(CF) but does not use CFNetwork.
            ['include', 'CF\\.cpp$'],
            ['exclude', 'network/cf/'],

            # Use native Mac font code from WebCore.
            ['include', 'platform/(graphics/)?mac/[^/]*Font[^/]*\\.(cpp|mm?)$'],
            ['include', 'platform/graphics/mac/ComplexText[^/]*\\.(cpp|h)$'],

            # We can use this for the fast Accelerate.framework FFT.
            ['include', 'platform/audio/mac/FFTFrameMac\\.cpp$'],

            # Cherry-pick some files that can't be included by broader regexps.
            # Some of these are used instead of Chromium platform files, see
            # the specific exclusions in the "exclude" list below.
            ['include', 'platform/graphics/mac/ColorMac\\.mm$'],
            ['include', 'platform/graphics/mac/ComplexTextControllerCoreText\\.mm$'],
            ['include', 'platform/graphics/mac/FloatPointMac\\.mm$'],
            ['include', 'platform/graphics/mac/FloatRectMac\\.mm$'],
            ['include', 'platform/graphics/mac/FloatSizeMac\\.mm$'],
            ['include', 'platform/graphics/mac/GlyphPageTreeNodeMac\\.cpp$'],
            ['include', 'platform/graphics/mac/IntPointMac\\.mm$'],
            ['include', 'platform/graphics/mac/IntRectMac\\.mm$'],
            ['include', 'platform/mac/BlockExceptions\\.mm$'],
            ['include', 'platform/mac/KillRingMac\\.mm$'],
            ['include', 'platform/mac/LocalCurrentGraphicsContext\\.mm$'],
            ['include', 'platform/mac/NSScrollerImpDetails\\.mm$'],
            ['include', 'platform/mac/PurgeableBufferMac\\.cpp$'],
            ['include', 'platform/mac/ScrollbarThemeMac\\.mm$'],
            ['include', 'platform/mac/ScrollAnimatorMac\\.mm$'],
            ['include', 'platform/mac/ScrollElasticityController\\.mm$'],
            ['include', 'platform/mac/ThemeMac\\.h$'],
            ['include', 'platform/mac/ThemeMac\\.mm$'],
            ['include', 'platform/mac/WebCoreSystemInterface\\.mm$'],
            ['include', 'platform/mac/WebCoreTextRenderer\\.mm$'],
            ['include', 'platform/text/mac/ShapeArabic\\.c$'],
            ['include', 'platform/text/mac/String(Impl)?Mac\\.mm$'],
            # Use USE_NEW_THEME on Mac.
            ['include', 'platform/Theme\\.cpp$'],

            ['include', 'WebKit/mac/WebCoreSupport/WebSystemInterface\\.mm$'],

            # We use LocaleMac.mm instead of LocaleICU.cpp in order to
            # apply system locales.
            ['exclude', 'platform/text/LocaleICU\\.cpp$'],
            ['exclude', 'platform/text/LocaleICU\\.h$'],
            ['include', 'platform/text/mac/LocaleMac\\.mm$'],

            # The Mac uses platform/mac/KillRingMac.mm instead of the dummy
            # implementation.
            ['exclude', 'platform/KillRingNone\\.cpp$'],

            # The Mac currently uses FontCustomPlatformData.cpp from
            # platform/graphics/mac, included by regex above, instead.
            ['exclude', 'platform/graphics/skia/FontCustomPlatformData\\.cpp$'],

            # The Mac currently uses ScrollbarThemeChromiumMac.mm, which is not
            # related to ScrollbarThemeChromium.cpp.
            ['exclude', 'platform/chromium/ScrollbarThemeChromium\\.cpp$'],

            # The Mac does not use ImageSourceCG.cpp from platform/graphics/cg
            # even though it is included by regex above.
            ['exclude', 'platform/graphics/cg/ImageSourceCG\\.cpp$'],
            ['exclude', 'platform/graphics/cg/PDFDocumentImage\\.cpp$'],

            # Mac uses only ScrollAnimatorMac.
            ['exclude', 'platform/ScrollAnimatorNone\\.cpp$'],
            ['exclude', 'platform/ScrollAnimatorNone\\.h$'],

            ['include', 'platform/graphics/cg/FloatPointCG\\.cpp$'],
            ['include', 'platform/graphics/cg/FloatRectCG\\.cpp$'],
            ['include', 'platform/graphics/cg/FloatSizeCG\\.cpp$'],
            ['include', 'platform/graphics/cg/IntPointCG\\.cpp$'],
            ['include', 'platform/graphics/cg/IntRectCG\\.cpp$'],
            ['include', 'platform/graphics/cg/IntSizeCG\\.cpp$'],
            ['exclude', 'platform/graphics/mac/FontMac\\.mm$'],
            ['exclude', 'platform/graphics/skia/FontCacheSkia\\.cpp$'],
            ['exclude', 'platform/graphics/skia/GlyphPageTreeNodeSkia\\.cpp$'],
            ['exclude', 'platform/graphics/skia/SimpleFontDataSkia\\.cpp$'],

            # Mac uses Harfbuzz-ng.
            ['include', 'platform/graphics/harfbuzz/HarfBuzzShaperBase\\.(cpp|h)$'],
            ['include', 'platform/graphics/harfbuzz/ng/HarfBuzzNGFaceCoreText\\.cpp$'],
            ['include', 'platform/graphics/harfbuzz/ng/HarfBuzzNGFace\\.(cpp|h)$'],
            ['include', 'platform/graphics/harfbuzz/ng/HarfBuzzShaper\\.(cpp|h)$'],            
          ],
        },{ # OS!="mac"
          'sources/': [
            ['exclude', 'Mac\\.(cpp|mm?)$'],

            # Linux uses FontLinux; Windows uses FontWin. Additionally, FontSkia
            # is excluded by a rule above if WebKit uses CG instead of Skia.
            ['exclude', 'platform/graphics/skia/FontSkia\\.cpp$'],

            # FIXME: We will eventually compile this too, but for now it's
            # only used on mac.
            ['exclude', 'platform/graphics/FontPlatformData\\.cpp$'],
          ],
        }],
        ['use_x11 == 0 and OS != "mac"', {
          'sources/': [
            ['exclude', 'VDMX[^/]+\\.(cpp|h)$'],
          ],
        }],
        ['OS=="win"', {
          'sources/': [
            ['exclude', 'Posix\\.cpp$'],

            ['include', '/opentype/'],
            ['include', '/SkiaFontWin\\.cpp$'],
            ['include', '/TransparencyWin\\.cpp$'],

            # The Chromium Win currently uses GlyphPageTreeNodeChromiumWin.cpp from
            # platform/graphics/chromium, included by regex above, instead.
            ['exclude', 'platform/graphics/skia/FontCacheSkia\\.cpp$'],
            ['exclude', 'platform/graphics/skia/GlyphPageTreeNodeSkia\\.cpp$'],
            ['exclude', 'platform/graphics/skia/SimpleFontDataSkia\\.cpp$'],

            # SystemInfo.cpp is useful and we don't want to copy it.
            ['include', 'platform/win/SystemInfo\\.cpp$'],

            ['exclude', 'platform/text/LocaleICU\\.cpp$'],
            ['exclude', 'platform/text/LocaleICU\\.h$'],
            ['include', 'platform/text/win/LocaleWin\.cpp$'],
            ['include', 'platform/text/win/LocaleWin\.h$'],
          ],
        },{ # OS!="win"
          'sources/': [
            ['exclude', 'Win\\.cpp$'],
            ['exclude', '/(Windows|Uniscribe)[^/]*\\.cpp$'],
            ['include', 'platform/graphics/opentype/OpenTypeSanitizer\\.cpp$'],
          ],
        }],
        ['OS=="android"', {
          'sources/': [
            ['include', 'platform/chromium/ClipboardChromiumLinux\\.cpp$'],
            ['include', 'platform/chromium/FileSystemChromiumLinux\\.cpp$'],
            ['include', 'platform/graphics/chromium/GlyphPageTreeNodeLinux\\.cpp$'],
            ['exclude', 'platform/graphics/chromium/IconChromium\\.cpp$'],
            ['include', 'platform/graphics/chromium/VDMXParser\\.cpp$'],
            ['include', 'platform/graphics/harfbuzz/ComplexTextControllerHarfBuzz\\.cpp$'],
            ['include', 'platform/graphics/harfbuzz/FontHarfBuzz\\.cpp$'],
            ['include', 'platform/graphics/harfbuzz/FontPlatformDataHarfBuzz\\.cpp$'],
            ['include', 'platform/graphics/harfbuzz/HarfBuzzSkia\\.cpp$'],
            ['include', 'platform/graphics/harfbuzz/HarfBuzzShaperBase\\.cpp$'],
            ['include', 'platform/graphics/opentype/OpenTypeTypes\\.h$'],
            ['include', 'platform/graphics/opentype/OpenTypeVerticalData\\.(cpp|h)$'],
            ['exclude', 'platform/graphics/skia/FontCacheSkia\\.cpp$'],
            ['include', 'platform/graphics/skia/SimpleFontDataSkia\\.cpp$'],
          ],
        }, { # OS!="android"
          'sources/': [
            ['exclude', 'Android\\.cpp$'],
          ],
        }],
      ],
    },
    {
      'target_name': 'webcore_platform_geometry',
      'type': 'static_library',
      'dependencies': [
        'webcore_prerequisites',
      ],
      'sources': [
        '<@(webcore_platform_geometry_files)',
      ],
    },
    # The *NEON.cpp files fail to compile when -mthumb is passed. Force
    # them to build in ARM mode.
    # See https://bugs.webkit.org/show_bug.cgi?id=62916.
    {
      'target_name': 'webcore_arm_neon',
      'conditions': [
        ['target_arch=="arm"', {
          'type': 'static_library',
          'dependencies': [
            'webcore_prerequisites',
          ],
          'hard_dependency': 1,
          'sources': [
            '<@(webcore_files)',
          ],
          'sources/': [
            ['exclude', '.*'],
            ['include', 'platform/graphics/cpu/arm/filters/.*NEON\\.(cpp|h)'],
          ],
          'cflags': ['-marm'],
          'conditions': [
            ['OS=="android"', {
              'cflags!': ['-mthumb'],
            }],
          ],
        },{  # target_arch!="arm"
          'type': 'none',
        }],
      ],
    },
    {
      'target_name': 'webcore_rendering',
      'type': 'static_library',
      'dependencies': [
        'webcore_prerequisites',
      ],
      'sources': [
        '<@(webcore_files)',
      ],
      'sources/': [
        ['exclude', '.*'],
        ['include', 'rendering/'],

        # FIXME: Figure out how to store these patterns in a variable.
        ['exclude', '(cairo|ca|cf|cg|curl|efl|freetype|gstreamer|gtk|linux|mac|opengl|openvg|opentype|pango|posix|qt|soup|svg|texmap|iphone|win|wince|wx)/'],
        ['exclude', '(?<!Chromium)(Cairo|CF|CG|Curl|Gtk|JSC|Linux|Mac|OpenType|POSIX|Posix|Qt|Safari|Soup|Win|WinCE|Wx)\\.(cpp|mm?)$'],
        # Previous rule excludes things like ChromiumFooWin, include those.
        ['include', 'rendering/.*Chromium.*\\.(cpp|mm?)$'],
        ['exclude', 'AllInOne\\.cpp$'],
      ],
      'conditions': [
        ['use_default_render_theme==0', {
          'sources/': [
            ['exclude', 'rendering/RenderThemeChromiumDefault.*'],
          ],
        }],
        ['use_default_render_theme==1', {
          'sources/': [
            ['exclude', 'RenderThemeChromiumWin.*'],
          ],
        }],
        ['OS=="win"', {
          'sources/': [
            ['exclude', 'Posix\\.cpp$'],
          ],
        },{ # OS!="win"
          'sources/': [
            ['exclude', 'Win\\.cpp$'],
          ],
        }],
        ['OS=="mac"', {
          'sources/': [
            # RenderThemeChromiumSkia is not used on mac since RenderThemeChromiumMac
            # does not reference the Skia code that is used by Windows, Linux and Android.
            ['exclude', 'rendering/RenderThemeChromiumSkia\\.cpp$'],
            # RenderThemeChromiumFontProvider is used by RenderThemeChromiumSkia.
            ['exclude', 'rendering/RenderThemeChromiumFontProvider\\.cpp'],
            ['exclude', 'rendering/RenderThemeChromiumFontProvider\\.h'],
          ],
        },{ # OS!="mac"
          'sources/': [['exclude', 'Mac\\.(cpp|mm?)$']]
        }],
        ['os_posix == 1 and OS != "mac" and gcc_version == 42', {
          # Due to a bug in gcc 4.2.1 (the current version on hardy), we get
          # warnings about uninitialized this.
          'cflags': ['-Wno-uninitialized'],
        }],
        ['OS == "android" and target_arch == "ia32" and gcc_version == 46', {
          # Due to a bug in gcc 4.6 in android NDK, we get warnings about uninitialized variable.
          'cflags': ['-Wno-uninitialized'],
        }],
        ['use_x11 == 0', {
          'sources/': [
            ['exclude', 'Linux\\.cpp$'],
          ],
        }],
        ['toolkit_uses_gtk == 0', {
          'sources/': [
            ['exclude', 'Gtk\\.cpp$'],
          ],
        }],
        ['OS=="android"', {
          'sources/': [
            ['include', 'rendering/RenderThemeChromiumFontProviderLinux\\.cpp$'],
            ['include', 'rendering/RenderThemeChromiumDefault\\.cpp$'],
          ],
        },{ # OS!="android"
          'sources/': [
            ['exclude', 'Android\\.cpp$'],
          ],
        }],
      ],
    },
    {
      'target_name': 'webcore_remaining',
      'type': 'static_library',
      'dependencies': [
        '<(chromium_src_dir)/third_party/v8-i18n/build/all.gyp:v8-i18n',
        'webcore_prerequisites',
      ],
      # This is needed for mac because of webkit_system_interface. It'd be nice
      # if this hard dependency could be split off the rest.
      'hard_dependency': 1,
      'sources': [
        '<@(webcore_files)',
      ],
      'sources/': [
        ['exclude', 'platform/'],
        ['exclude', 'rendering/'],

        # Exclude most of bindings, except of the V8-related parts.
        ['exclude', 'bindings/[^/]+/'],
        ['include', 'bindings/generic/'],
        ['include', 'bindings/v8/'],

        # Exclude most of bridge, except for the V8-related parts.
        ['exclude', 'bridge/'],
        ['include', 'bridge/jni/'],
        ['exclude', 'bridge/jni/[^/]+_jsobject\\.mm$'],
        ['exclude', 'bridge/jni/[^/]+_objc\\.mm$'],
        ['exclude', 'bridge/jni/jsc/'],

        # FIXME: Figure out how to store these patterns in a variable.
        ['exclude', '(atk|cairo|ca|cf|cg|curl|efl|freetype|gstreamer|gtk|linux|mac|opengl|openvg|opentype|pango|posix|qt|soup|svg|texmap|iphone|win|wince|wx)/'],
        ['exclude', '(?<!Chromium)(Cairo|CF|CG|Curl|Gtk|JSC|Linux|Mac|OpenType|POSIX|Posix|Qt|Safari|Soup|Win|WinCE|Wx)\\.(cpp|mm?)$'],

        ['exclude', 'AllInOne\\.cpp$'],

        ['exclude', 'Modules/filesystem/LocalFileSystem\\.cpp$'],
        ['exclude', 'Modules/indexeddb/IDBFactoryBackendInterface\\.cpp$'],
        ['exclude', 'Modules/webdatabase/DatabaseManagerClient\\.h$'],
        ['exclude', 'Modules/webdatabase/DatabaseTracker\\.cpp$'],
        ['exclude', 'Modules/webdatabase/OriginQuotaManager\\.(cpp|h)$'],
        ['exclude', 'Modules/webdatabase/OriginUsageRecord\\.(cpp|h)$'],
        ['exclude', 'Modules/webdatabase/SQLTransactionClient\\.cpp$'],
        ['exclude', 'inspector/InspectorFrontendClientLocal\\.cpp$'],
        ['exclude', 'inspector/JavaScript[^/]*\\.cpp$'],
        ['exclude', 'loader/UserStyleSheetLoader\\.cpp$'],
        ['exclude', 'loader/appcache/'],
        ['exclude', 'loader/archive/cf/LegacyWebArchiveMac\\.mm$'],
        ['exclude', 'loader/archive/cf/LegacyWebArchive\\.cpp$'],
        ['exclude', 'loader/icon/IconDatabase\\.cpp$'],
        ['exclude', 'plugins/PluginDataNone\\.cpp$'],
        ['exclude', 'plugins/PluginDatabase\\.cpp$'],
        ['exclude', 'plugins/PluginMainThreadScheduler\\.cpp$'],
        ['exclude', 'plugins/PluginPackageNone\\.cpp$'],
        ['exclude', 'plugins/PluginPackage\\.cpp$'],
        ['exclude', 'plugins/PluginStream\\.cpp$'],
        ['exclude', 'plugins/PluginView\\.cpp$'],
        ['exclude', 'plugins/npapi\\.cpp$'],
        ['exclude', 'storage/StorageAreaImpl\\.(cpp|h)$'],
        ['exclude', 'storage/StorageAreaSync\\.(cpp|h)$'],
        ['exclude', 'storage/StorageEventDispatcher\\.(cpp|h)$'],
        ['exclude', 'storage/StorageMap\\.(cpp|h)$'],
        ['exclude', 'storage/StorageNamespace\\.cpp$'],
        ['exclude', 'storage/StorageNamespaceImpl\\.(cpp|h)$'],
        ['exclude', 'storage/StorageSyncManager\\.(cpp|h)$'],
        ['exclude', 'storage/StorageTask\\.(cpp|h)$'],
        ['exclude', 'storage/StorageThread\\.(cpp|h)$'],
        ['exclude', 'storage/StorageTracker\\.(cpp|h)$'],
        ['exclude', 'storage/StorageTrackerClient\\.h$'],
        ['exclude', 'workers/DefaultSharedWorkerRepository\\.(cpp|h)$'],

        ['include', 'loader/appcache/ApplicationCacheHost\.h$'],
        ['include', 'loader/appcache/DOMApplicationCache\.(cpp|h)$'],
      ],
      'conditions': [
        # Shard this taret into ten parts to work around linker limitations.
        # on link time code generation builds.
        ['OS=="win" and buildtype=="Official"', {
          'msvs_shard': 10,
        }],
        ['os_posix == 1 and OS != "mac" and gcc_version == 42', {
          # Due to a bug in gcc 4.2.1 (the current version on hardy), we get
          # warnings about uninitialized this.
          'cflags': ['-Wno-uninitialized'],
        }],
        ['use_x11 == 0', {
          'sources/': [
            ['exclude', 'Linux\\.cpp$'],
          ],
        }],
        ['toolkit_uses_gtk == 0', {
          'sources/': [
            ['exclude', 'Gtk\\.cpp$'],
          ],
        }],
        ['OS=="android"', {
          'cflags': [
            # WebCore does not work with strict aliasing enabled.
            # https://bugs.webkit.org/show_bug.cgi?id=25864
            '-fno-strict-aliasing',
          ],
        }, { # OS!="android"
          'sources/': [['exclude', 'Android\\.cpp$']]
        }],
        ['OS!="mac"', {
          'sources/': [['exclude', 'Mac\\.(cpp|mm?)$']]
        }],
      ],
    },
    {
      'target_name': 'webcore',
      'type': 'none',
      'dependencies': [
        'webcore_dom',
        'webcore_html',
        'webcore_platform',
        'webcore_platform_geometry',
        'webcore_remaining',
        'webcore_rendering',
        # Exported.
        'webcore_bindings',
        '../../WTF/WTF.gyp/WTF.gyp:wtf',
        '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(chromium_src_dir)/skia/skia.gyp:skia',
        '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
        '<(chromium_src_dir)/third_party/qcms/qcms.gyp:qcms',
        '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
      ],
      'export_dependent_settings': [
        'webcore_bindings',
        '../../WTF/WTF.gyp/WTF.gyp:wtf',
        '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(chromium_src_dir)/skia/skia.gyp:skia',
        '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
        '<(chromium_src_dir)/third_party/qcms/qcms.gyp:qcms',
        '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<@(webcore_include_dirs)',
        ],
      },
      'conditions': [
        ['target_arch=="arm"', {
          'dependencies': [
            'webcore_arm_neon',
          ],
        }],
        ['OS=="mac"', {
          'direct_dependent_settings': {
            'include_dirs': [
              '<(chromium_src_dir)/third_party/apple_webkit',
              '../../WebKit/mac/WebCoreSupport',
            ],
          },
        }],
        ['OS=="win"', {
          'direct_dependent_settings': {
            'include_dirs+++': ['../dom'],
          },
        }],
        ['OS=="linux" and "WTF_USE_WEBAUDIO_IPP=1" in feature_defines', {
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L ipp)',
            ],
            'libraries': [
              '-lipps -lippcore',
            ],
          },
        }],
        # Use IPP static libraries for x86 Android.
        ['OS=="android" and "WTF_USE_WEBAUDIO_IPP=1" in feature_defines', {
          'link_settings': {
            'libraries': [
               '<!@(pkg-config --libs ipp|sed s/-L//)/libipps_l.a',
               '<!@(pkg-config --libs ipp|sed s/-L//)/libippcore_l.a',
            ]
          },
        }],
        ['enable_svg!=0', {
          'dependencies': [
            'webcore_svg',
          ],
        }],
      ],
    },
    {
      'target_name': 'webcore_test_support',
      'type': 'static_library',
      'dependencies': [
        'webcore',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/webcore',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings',
        '<@(webcore_include_dirs)',
        '../testing',
        '../testing/v8',
        '../../Platform/chromium',
      ],
      'sources': [
        '<@(webcore_test_support_files)',
        '<(SHARED_INTERMEDIATE_DIR)/webcore/bindings/V8MallocStatistics.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8MallocStatistics.h',
        '<(SHARED_INTERMEDIATE_DIR)/webcore/bindings/V8Internals.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8Internals.h',
        '<(SHARED_INTERMEDIATE_DIR)/webcore/bindings/V8InternalSettings.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8InternalSettings.h',
      ],
      'sources/': [
        ['exclude', 'testing/js'],
      ],
    },
  ],  # targets
}
