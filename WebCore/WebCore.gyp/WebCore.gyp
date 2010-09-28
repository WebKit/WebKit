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
  # Location of the chromium src directory.
  'conditions': [
    ['inside_chromium_build==0', {
      # Webkit is being built outside of the full chromium project.
      'variables': {'chromium_src_dir': '../../WebKit/chromium'},
    },{
      # WebKit is checked out in src/chromium/third_party/WebKit
      'variables': {'chromium_src_dir': '../../../..'},
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
          'type': 'static_library',
          'variables': {
            'adjusted_library_path':
                '<(PRODUCT_DIR)/libWebKitSystemInterfaceLeopardPrivateExtern.a',
          },
          'sources': [
            # An empty source file is needed to convince Xcode to produce
            # output for this target.  The resulting library won't actually
            # contain anything.  The library at adjusted_library_path will,
            # and that library is pushed to dependents of this target below.
            'mac/Empty.cpp',
          ],
          'actions': [
            {
              'action_name': 'Adjust Visibility',
              'inputs': [
                'mac/adjust_visibility.sh',
                '../../WebKitLibraries/libWebKitSystemInterfaceLeopard.a',
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
            ],
          },  # link_settings
        },  # target webkit_system_interface
      ],  # targets
    }],  # condition OS == "mac"
    ['OS!="win" and remove_webcore_debug_symbols==1', {
      # Remove -g from all targets defined here.
      'target_defaults': {
        'cflags!': ['-g'],
      },
    }],
  ],  # conditions

  'variables': {
    # If set to 1, doesn't compile debug symbols into webcore reducing the
    # size of the binary and increasing the speed of gdb.  gcc only.
    'remove_webcore_debug_symbols%': 0,

    # If set to 0, doesn't build SVG support, reducing the size of the
    # binary and increasing the speed of gdb.
    'enable_svg%': 1,

    'webcore_include_dirs': [
      '../',
      '../accessibility',
      '../accessibility/chromium',
      '../bindings',
      '../bindings/generic',
      '../bindings/v8',
      '../bindings/v8/custom',
      '../bindings/v8/specialization',
      '../css',
      '../dom',
      '../dom/default',
      '../editing',
      '../fileapi',
      '../history',
      '../html',
      '../html/canvas',
      '../html/parser',
      '../inspector',
      '../loader',
      '../loader/appcache',
      '../loader/archive',
      '../loader/icon',
      '../mathml',
      '../notifications',
      '../page',
      '../page/animation',
      '../page/chromium',
      '../platform',
      '../platform/animation',
      '../platform/chromium',
      '../platform/graphics',
      '../platform/graphics/chromium',
      '../platform/graphics/filters',
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
      '../platform/image-decoders/xbm',
      '../platform/image-encoders/skia',
      '../platform/mock',
      '../platform/network',
      '../platform/network/chromium',
      '../platform/sql',
      '../platform/text',
      '../platform/text/transcoder',
      '../plugins',
      '../plugins/chromium',
      '../rendering',
      '../rendering/style',
      '../rendering/svg',
      '../storage',
      '../storage/chromium',
      '../svg',
      '../svg/animation',
      '../svg/graphics',
      '../svg/graphics/filters',
      '../websockets',
      '../workers',
      '../xml',
    ],

    'bindings_idl_files': [
      '<@(webcore_bindings_idl_files)',
    ],

    'bindings_idl_files!': [
      # Custom bindings in bindings/v8/custom exist for these.
      '../dom/EventListener.idl',
      '../dom/EventTarget.idl',
      '../html/VoidCallback.idl',

      # Bindings with custom Objective-C implementations.
      '../page/AbstractView.idl',

      # FIXME: I don't know why all of these are excluded.
      # Extra SVG bindings to exclude.
      '../svg/ElementTimeControl.idl',
      '../svg/SVGAnimatedPathData.idl',
      '../svg/SVGExternalResourcesRequired.idl',
      '../svg/SVGFilterPrimitiveStandardAttributes.idl',
      '../svg/SVGFitToViewBox.idl',

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
    ],

    'conditions': [
      # TODO(maruel): Move it in its own project or generate it anyway?
      ['enable_svg!=0', {
        'bindings_idl_files': [
          '<@(webcore_svg_bindings_idl_files)',
        ],
      }],
      ['OS=="mac"', {
        'webcore_include_dirs+': [
          # platform/graphics/cg and cocoa need to come before
          # platform/graphics/chromium so that the Mac build picks up the
          # version of ImageBufferData.h in the cg directory and
          # FontPlatformData.h in the cocoa directory.  The + prepends this
          # directory to the list.
          # FIXME: This shouldn't need to be prepended.
          '../platform/graphics/cocoa',
          '../platform/graphics/cg',
        ],
        'webcore_include_dirs': [
          # FIXME: Eliminate dependency on platform/mac and related
          # directories.
          # FIXME: Eliminate dependency on platform/graphics/mac and
          # related directories.
          # platform/graphics/cg may need to stick around, though.
          '../loader/archive/cf',
          '../platform/graphics/mac',
          '../platform/mac',
          '../platform/text/mac',
        ],
        # enable -Wall and -Werror, just for Mac and Linux builds for now
        # FIXME: Also enable this for Windows after verifying no warnings
        'chromium_code': 1,
      }],
# FIXME: disabled for now due to failures on different gcc versions
#     ['OS=="linux"', {
#        'chromium_code': 1,
#      }],
      ['OS=="win"', {
        'webcore_include_dirs': [
          '../page/win',
          '../platform/graphics/win',
          '../platform/text/win',
          '../platform/win',
        ],
      }],
      ['OS=="win" and buildtype=="Official"', {
        # On windows official release builds, we try to preserve symbol space.
        'derived_sources_aggregate_files': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSourcesAll.cpp',
        ],
      },{
        'derived_sources_aggregate_files': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources1.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources2.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources3.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources4.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources5.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources6.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources7.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8DerivedSources8.cpp',
        ],
      }]
    ],
  },
  'targets': [
    {
      'target_name': 'inspector_protocol_sources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generateInspectorProtocolSources',
          # The second input item will be used as item name in vcproj.
          # It is not possible to put Inspector.idl there because
          # all idl files are marking as excluded by gyp generator.
          'inputs': [
            '../bindings/scripts/generate-bindings.pl',
            '../inspector/CodeGeneratorInspector.pm',
            '../bindings/scripts/CodeGenerator.pm',
            '../bindings/scripts/IDLParser.pm',
            '../bindings/scripts/IDLStructure.pm',
            '../inspector/Inspector.idl',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorBackendDispatcher.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorBackendStub.js',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/InspectorBackendDispatcher.h',
            '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorFrontend.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/InspectorFrontend.h',
          ],
          'variables': {
            'generator_include_dirs': [
            ],
          },
          'action': [
            'python',
            'scripts/rule_binding.py',
            '../inspector/Inspector.idl',
            '<(SHARED_INTERMEDIATE_DIR)/webcore',
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
            '--',
            '<@(_inputs)',
            '--',
            '--defines', '<(feature_defines) LANGUAGE_JAVASCRIPT',
            '--generator', 'Inspector',
            '<@(generator_include_dirs)'
          ],
          'message': 'Generating Inspector protocol sources from Inspector.idl',
        },
      ]
    },
    {
      'target_name': 'webcore_bindings_sources',
      'type': 'none',
      'hard_dependency': 1,
      'sources': [
        # bison rule
        '../css/CSSGrammar.y',
        '../xml/XPathGrammar.y',

        # gperf rule
        '../html/DocTypeStrings.gperf',
        '../platform/ColorData.gperf',

        # idl rules
        '<@(bindings_idl_files)',
      ],
      'actions': [
        # Actions to build derived sources.
        {
          'action_name': 'HTMLEntityTable',
          'inputs': [
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
            '<@(_inputs)'
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
            '<@(_inputs)'
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
            '../css/mathml.css',
            '../css/mediaControls.css',
            '../css/mediaControlsChromium.css',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/UserAgentStyleSheets.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/UserAgentStyleSheetsData.cpp',
          ],
          'action': [
            'python',
            'scripts/action_useragentstylesheets.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)'
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
          'action_name': 'tokenizer',
          'inputs': [
            '../css/maketokenizer',
            '../css/tokenizer.flex',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/tokenizer.cpp',
          ],
          'action': [
            'python',
            'scripts/action_maketokenizer.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)'
          ],
        },
        {
          'action_name': 'derived_sources_all_in_one',
          'variables': {
            # Write sources into a file, so that the action command line won't
            # exceed OS limites.
            'idls_list_temp_file': '<|(idls_list_temp_file.tmp <@(bindings_idl_files))',
          },
          'inputs': [
            'scripts/action_derivedsourcesallinone.py',
            '<(idls_list_temp_file)',
            '<!@(cat <(idls_list_temp_file))',
          ],
          'outputs': [
            '<@(derived_sources_aggregate_files)',
          ],
          'action': [
            'python',
            'scripts/action_derivedsourcesallinone.py',
            '<(idls_list_temp_file)',
            '--',
            '<@(derived_sources_aggregate_files)',
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
            '<(SHARED_INTERMEDIATE_DIR)/webkit'
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
          'action': [
            'perl',
            '../make-hash-tools.pl',
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
            '<(RULE_INPUT_PATH)',
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
            '../bindings/scripts/IDLStructure.pm',
          ],
          'outputs': [
            # FIXME:  The .cpp file should be in webkit/bindings once
            # we coax GYP into supporting it (see 'action' below).
            '<(SHARED_INTERMEDIATE_DIR)/webcore/bindings/V8<(RULE_INPUT_ROOT).cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings/V8<(RULE_INPUT_ROOT).h',
          ],
          'variables': {
            'generator_include_dirs': [
              '--include', '../css',
              '--include', '../dom',
              '--include', '../fileapi',
              '--include', '../html',
              '--include', '../notifications',
              '--include', '../page',
              '--include', '../plugins',
              '--include', '../storage',
              '--include', '../svg',
              '--include', '../websockets',
              '--include', '../workers',
              '--include', '../xml',
            ],
          },
          # FIXME:  Note that we put the .cpp files in webcore/bindings
          # but the .h files in webkit/bindings.  This is to work around
          # the unfortunate fact that GYP strips duplicate arguments
          # from lists.  When we have a better GYP way to suppress that
          # behavior, change the output location.
          'action': [
            'python',
            'scripts/rule_binding.py',
            '<(RULE_INPUT_PATH)',
            '<(SHARED_INTERMEDIATE_DIR)/webcore/bindings',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/bindings',
            '--',
            '<@(_inputs)',
            '--',
            '--defines', '<(feature_defines) LANGUAGE_JAVASCRIPT V8_BINDING',
            '--generator', 'V8',
            '<@(generator_include_dirs)'
          ],
          'message': 'Generating binding from <(RULE_INPUT_PATH)',
        },
      ],
    },
    {
      'target_name': 'webcore_bindings',
      'type': '<(library)',
      'hard_dependency': 1,
      'dependencies': [
        'webcore_bindings_sources',
        'inspector_protocol_sources',
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
        '<(SHARED_INTERMEDIATE_DIR)/webkit/DocTypeStrings.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/ColorData.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSPropertyNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSValueKeywords.cpp',

        # Additional .cpp files from webcore_bindings_sources actions.
        '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLElementFactory.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/UserAgentStyleSheetsData.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/V8HTMLElementWrapperFactory.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/XLinkNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLNSNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/SVGNames.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/MathMLElementFactory.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/MathMLNames.cpp',

        # Generated from HTMLEntityNames.in
        '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLEntityTable.cpp',

        # Additional .cpp files from the webcore_bindings_sources rules.
        '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSGrammar.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webkit/XPathGrammar.cpp',

        # Additional .cpp files from the webcore_inspector_sources list.
        '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorFrontend.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/webcore/InspectorBackendDispatcher.cpp',
      ],
      'conditions': [
        ['javascript_engine=="v8"', {
          'dependencies': [
            '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
          ],
          'conditions': [
            ['inside_chromium_build==1 and OS=="win" and component=="shared_library"', {
              'defines': [
                'USING_V8_SHARED',
              ],
            }],
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
            '../../WebKitLibraries',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '<(chromium_src_dir)/build/win/system.gyp:cygwin'
          ],
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
        }],
      ],
    },
    {
      # We'll soon split libwebcore in multiple smaller libraries.
      # webcore_prerequisites will be the 'base' target of every sub-target.
      'target_name': 'webcore_prerequisites',
      'type': 'none',
      'dependencies': [
        'webcore_bindings',
        '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:pcre',
        '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:wtf',
        '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(chromium_src_dir)/skia/skia.gyp:skia',
        '<(chromium_src_dir)/third_party/libjpeg/libjpeg.gyp:libjpeg',
        '<(chromium_src_dir)/third_party/libpng/libpng.gyp:libpng',
        '<(chromium_src_dir)/third_party/libxml/libxml.gyp:libxml',
        '<(chromium_src_dir)/third_party/libxslt/libxslt.gyp:libxslt',
        '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
        '<(chromium_src_dir)/third_party/ots/ots.gyp:ots',
        '<(chromium_src_dir)/third_party/sqlite/sqlite.gyp:sqlite',
        '<(chromium_src_dir)/third_party/angle/src/build_angle.gyp:translator_common',
      ],
      'export_dependent_settings': [
        'webcore_bindings',
        '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:pcre',
        '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:wtf',
        '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(chromium_src_dir)/skia/skia.gyp:skia',
        '<(chromium_src_dir)/third_party/libjpeg/libjpeg.gyp:libjpeg',
        '<(chromium_src_dir)/third_party/libpng/libpng.gyp:libpng',
        '<(chromium_src_dir)/third_party/libxml/libxml.gyp:libxml',
        '<(chromium_src_dir)/third_party/libxslt/libxslt.gyp:libxslt',
        '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
        '<(chromium_src_dir)/third_party/ots/ots.gyp:ots',
        '<(chromium_src_dir)/third_party/sqlite/sqlite.gyp:sqlite',
        '<(chromium_src_dir)/third_party/angle/src/build_angle.gyp:translator_common',
      ],
      # This is needed for mac because of webkit_system_interface. It'd be nice
      # if this hard dependency could be split off the rest.
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'defines': [
          'WEBCORE_NAVIGATOR_VENDOR="Google Inc."',
        ],
        'include_dirs': [
          '<(INTERMEDIATE_DIR)',
          '<@(webcore_include_dirs)',
          '<(chromium_src_dir)/gpu',
          '<(chromium_src_dir)/third_party/angle/include/GLSLANG',
        ],
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
      },
      'conditions': [
        ['javascript_engine=="v8"', {
          'dependencies': [
            '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
          ],
          'export_dependent_settings': [
            '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
          ],
          'conditions': [
            ['inside_chromium_build==1 and OS=="win" and component=="shared_library"', {
              'defines': [
                'USING_V8_SHARED',
              ],
            }],
          ],
        }],
        ['use_accelerated_compositing==1', {
          'dependencies': [
            '<(chromium_src_dir)/gpu/gpu.gyp:gles2_c_lib',
          ],
          'export_dependent_settings': [
            '<(chromium_src_dir)/gpu/gpu.gyp:gles2_c_lib',
          ],
        }],
        ['OS=="linux" or OS=="freebsd"', {
          'dependencies': [
            '<(chromium_src_dir)/build/linux/system.gyp:fontconfig',
            '<(chromium_src_dir)/build/linux/system.gyp:gtk',
          ],
          'export_dependent_settings': [
            '<(chromium_src_dir)/build/linux/system.gyp:fontconfig',
            '<(chromium_src_dir)/build/linux/system.gyp:gtk',
          ],
          'cflags': [
            # WebCore does not work with strict aliasing enabled.
            # https://bugs.webkit.org/show_bug.cgi?id=25864
            '-fno-strict-aliasing',
          ],
        }],
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
          'dependencies': [
            'webkit_system_interface',
          ],
          'export_dependent_settings': [
            'webkit_system_interface',
          ],
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
          'include_dirs': [
            '../../WebKitLibraries',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '<(chromium_src_dir)/build/win/system.gyp:cygwin'
          ],
          'export_dependent_settings': [
            '<(chromium_src_dir)/build/win/system.gyp:cygwin'
          ],
          'defines': [
            # Match Safari and Mozilla on Windows.
            'WEBCORE_NAVIGATOR_PLATFORM="Win32"',
            '__PRETTY_FUNCTION__=__FUNCTION__',
          ],
          # This is needed because Event.h in this directory is blocked
          # by a system header on windows.
          'include_dirs++': ['../dom'],
        }],
      ],
    },
    {
      'target_name': 'webcore_svg',
      'type': '<(library)',
      'dependencies': [
        'webcore_prerequisites',
      ],
      'sources': [
        '<@(webcore_files)',
      ],
      'sources/': [
        # Start by excluding everything then include svg files only. Note that
        # css/SVG* and bindings/v8/custom/V8SVG* are still built in
        # webcore_remaining.
        ['exclude', '.*'],
        ['include', 'svg/'],
        ['include', 'css/svg/'],
        ['include', 'rendering/style/SVG'],
        ['include', 'rendering/RenderSVG'],
        ['include', 'rendering/SVG'],
        ['exclude', 'svg/SVGAllInOne\\.cpp$'],
      ],
    },
    {
      'target_name': 'webcore_platform',
      'type': '<(library)',
      'dependencies': [
        'webcore_prerequisites',
      ],
      # This is needed for mac because of webkit_system_interface. It'd be nice
      # if this hard dependency could be split off the rest.
      'hard_dependency': 1,
      'sources': [
        '<@(webcore_files)',

        # For WebCoreSystemInterface, Mac-only.
        '../../WebKit/mac/WebCoreSupport/WebSystemInterface.mm',
      ],
      'sources/': [
        # Start by excluding everything then include platform files only.
        ['exclude', '.*'],
        ['include', 'platform/'],

        # Exclude things that don't apply to the Chromium platform on the basis
        # of their enclosing directories and tags at the ends of their
        # filenames.
        ['exclude', '(android|cairo|cf|cg|curl|gtk|haiku|linux|mac|opentype|posix|qt|soup|svg|symbian|win|wx)/'],
        ['exclude', '(?<!Chromium)(Android|Cairo|CF|CG|Curl|Gtk|Linux|Mac|OpenType|POSIX|Posix|Qt|Safari|Soup|Symbian|Win|Wx)\\.(cpp|mm?)$'],

        # A few things can't be excluded by patterns.  List them individually.

        ['include', 'platform/graphics/opentype/OpenTypeSanitizer\\.cpp$'],

        # Exclude some DB-related files.
        ['exclude', 'platform/sql/SQLiteFileSystem\\.cpp$'],

        # Use platform/KURLGoogle.cpp instead.
        ['exclude', 'platform/KURL\\.cpp$'],

        # Use platform/MIMETypeRegistryChromium.cpp instead.
        ['exclude', 'platform/MIMETypeRegistry\\.cpp$'],

        # Theme.cpp is used only if we're using USE_NEW_THEME. We are not for
        # Windows and Linux. We manually include Theme.cpp for the Mac below.
        ['exclude', 'platform/Theme\\.cpp$'],

        # Use LinkHashChromium.cpp instead
        ['exclude', 'platform/LinkHash\\.cpp$'],
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd"', {
          'sources/': [
            # Cherry-pick files excluded by the broader regular expressions above.
            ['include', 'platform/chromium/KeyCodeConversionGtk\\.cpp$'],
            ['include', 'platform/graphics/chromium/FontCacheLinux\\.cpp$'],
            ['include', 'platform/graphics/chromium/FontLinux\\.cpp$'],
            ['include', 'platform/graphics/chromium/FontPlatformDataLinux\\.cpp$'],
            ['include', 'platform/graphics/chromium/GlyphPageTreeNodeLinux\\.cpp$'],
            ['include', 'platform/graphics/chromium/SimpleFontDataLinux\\.cpp$'],
          ],
        }],
        ['OS=="mac"', {
          # Necessary for Mac .mm stuff.
          'include_dirs': [
            '../../WebKitLibraries',
          ],
          'dependencies': [
            'webkit_system_interface',
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
            ['include', 'platform/graphics/mac/ComplexText[^/]*\\.(cpp|h)$'],

            # Cherry-pick some files that can't be included by broader regexps.
            # Some of these are used instead of Chromium platform files, see
            # the specific exclusions in the "sources!" list below.
            ['include', 'rendering/RenderThemeMac\\.mm$'],
            ['include', 'loader/archive/cf/LegacyWebArchive\\.cpp$'],
            ['include', 'platform/graphics/mac/ColorMac\\.mm$'],
            ['include', 'platform/graphics/mac/FloatPointMac\\.mm$'],
            ['include', 'platform/graphics/mac/FloatRectMac\\.mm$'],
            ['include', 'platform/graphics/mac/FloatSizeMac\\.mm$'],
            ['include', 'platform/graphics/mac/GlyphPageTreeNodeMac\\.cpp$'],
            ['include', 'platform/graphics/mac/GraphicsContextMac\\.mm$'],
            ['include', 'platform/graphics/mac/IntRectMac\\.mm$'],
            ['include', 'platform/mac/BlockExceptions\\.mm$'],
            ['include', 'platform/mac/KillRingMac\\.mm$'],
            ['include', 'platform/mac/LocalCurrentGraphicsContext\\.mm$'],
            ['include', 'platform/mac/PurgeableBufferMac\\.cpp$'],
            ['include', 'platform/mac/WebCoreSystemInterface\\.mm$'],
            ['include', 'platform/mac/WebCoreTextRenderer\\.mm$'],
            ['include', 'platform/text/mac/ShapeArabic\\.c$'],
            ['include', 'platform/text/mac/String(Impl)?Mac\\.mm$'],
            # Use USE_NEW_THEME on Mac.
            ['include', 'platform/Theme\\.cpp$'],

            ['include', 'WebKit/mac/WebCoreSupport/WebSystemInterface\\.mm$'],

            # Chromium Mac does not use skia.
            ['exclude', 'platform/graphics/skia/[^/]*Skia\\.(cpp|h)$'],

            # The Mac uses platform/mac/KillRingMac.mm instead of the dummy
            # implementation.
            ['exclude', 'platform/KillRingNone\\.cpp$'],

            # The Mac currently uses FontCustomPlatformData.cpp from
            # platform/graphics/mac, included by regex above, instead.
            ['exclude', 'platform/graphics/skia/FontCustomPlatformData\\.cpp$'],

            # The Mac currently uses ScrollbarThemeChromiumMac.mm, which is not
            # related to ScrollbarThemeChromium.cpp.
            ['exclude', 'platform/chromium/ScrollbarThemeChromium\\.cpp$'],

            # The Mac currently uses ImageChromiumMac.mm from
            # platform/graphics/chromium, included by regex above, instead.
            ['exclude', 'platform/graphics/chromium/ImageChromium\\.cpp$'],

            # The Mac uses ImageSourceCG.cpp from platform/graphics/cg, included
            # by regex above, instead.
            ['exclude', 'platform/graphics/ImageSource\\.cpp$'],

            # Skia image-decoders are also not used on mac.  CoreGraphics
            # is used directly instead.
            ['exclude', 'platform/image-decoders/ImageDecoder\\.h$'],
            ['exclude', 'platform/image-decoders/bmp/BMPImageDecoder\\.(cpp|h)$'],
            ['exclude', 'platform/image-decoders/bmp/BMPImageReader\\.(cpp|h)$'],
            ['exclude', 'platform/image-decoders/gif/GIFImageDecoder\\.(cpp|h)$'],
            ['exclude', 'platform/image-decoders/gif/GIFImageReader\\.(cpp|h)$'],
            ['exclude', 'platform/image-decoders/ico/ICOImageDecoder\\.(cpp|h)$'],
            ['exclude', 'platform/image-decoders/jpeg/JPEGImageDecoder\\.(cpp|h)$'],
            ['exclude', 'platform/image-decoders/png/PNGImageDecoder\\.(cpp|h)$'],
            ['exclude', 'platform/image-decoders/skia/ImageDecoderSkia\\.cpp$'],
            ['exclude', 'platform/image-decoders/xbm/XBMImageDecoder\\.(cpp|h)$'],

            # Again, Skia is not used on Mac.
            ['exclude', 'platform/chromium/DragImageChromiumSkia\\.cpp$'],
          ],
        }],
        ['OS!="linux" and OS!="freebsd"', {
          'sources/': [
            ['exclude', '(Gtk|Linux)\\.cpp$'],
            ['exclude', 'Harfbuzz[^/]+\\.(cpp|h)$'],
            ['exclude', 'VDMX[^/]+\\.(cpp|h)$'],
          ],
        }],
        ['OS!="mac"', {
          'sources/': [['exclude', 'Mac\\.(cpp|mm?)$']]
        }],
        ['OS!="win"', {
          'sources/': [
            ['exclude', 'Win\\.cpp$'],
            ['exclude', '/(Windows|Uniscribe)[^/]*\\.cpp$']
          ],
        }],
        ['OS=="win"', {
          'sources/': [
            ['exclude', 'Posix\\.cpp$'],
          ],
        }],
      ],
    },
    {
      'target_name': 'webcore_remaining',
      'type': '<(library)',
      'dependencies': [
        'webcore_prerequisites',
      ],
      # This is needed for mac because of webkit_system_interface. It'd be nice
      # if this hard dependency could be split off the rest.
      'hard_dependency': 1,
      'sources': [
        '<@(webcore_files)',
      ],
      'sources/': [
        # Exclude JSC custom bindings.
        ['exclude', 'bindings/js'],

        # Fortunately, many things can be excluded by using broad patterns.

        # Exclude things that don't apply to the Chromium platform on the basis
        # of their enclosing directories and tags at the ends of their
        # filenames.
        ['exclude', '(android|cairo|cf|cg|curl|gtk|haiku|linux|mac|opentype|platform|posix|qt|soup|svg|symbian|win|wx)/'],
        ['exclude', '(?<!Chromium)(Android|Cairo|CF|CG|Curl|Gtk|Linux|Mac|OpenType|POSIX|Posix|Qt|Safari|Soup|Symbian|Win|Wx)\\.(cpp|mm?)$'],

        # Exclude most of SVG except css and javascript bindings.
        ['exclude', 'rendering/style/SVG[^/]+.(cpp|h)$'],
        ['exclude', 'rendering/RenderSVG[^/]+.(cpp|h)$'],
        ['exclude', 'rendering/SVG[^/]+.(cpp|h)$'],

        # JSC-only.
        ['exclude', 'inspector/JavaScript[^/]*\\.cpp$'],

        # ENABLE_OFFLINE_WEB_APPLICATIONS, exclude most of webcore's impl
        ['exclude', 'loader/appcache/'],
        ['include', 'loader/appcache/ApplicationCacheHost\.h$'],
        ['include', 'loader/appcache/DOMApplicationCache\.(h|cpp)$'],

        # Exclude some DB-related files.
        ['exclude', 'storage/DatabaseTracker\\.cpp$'],
        ['exclude', 'storage/DatabaseTrackerClient\\.h$'],
        ['exclude', 'storage/OriginQuotaManager\\.(cpp|h)$'],
        ['exclude', 'storage/OriginUsageRecord\\.(cpp|h)$'],
        ['exclude', 'storage/SQLTransactionClient\\.cpp$'],

        # Don't build StorageNamespace.  We have our own implementation.
        ['exclude', 'storage/StorageNamespace\\.cpp$'],

        # Don't build StorageEventDispatcher.  We have our own implementation.
        ['exclude', 'storage/StorageEventDispatcher\\.cpp$'],

        # Don't build IDBFactoryBackendInterface.  We have our own implementation.
        ['exclude', 'storage/IDBFactoryBackendInterface\\.cpp$'],

        # Don't build IDBKeyPathBackendImpl.  We have our own implementation.
        ['exclude', 'storage/IDBKeyPathBackendImpl\\.cpp$'],

        # Use history/BackForwardListChromium.cpp instead.
        ['exclude', 'history/BackForwardListImpl\\.cpp$'],

        # Use loader/icon/IconDatabaseNone.cpp instead.
        ['exclude', 'loader/icon/IconDatabase\\.cpp$'],

        # Exclude some, but not all, of plugins.
        ['exclude', 'plugins/PluginDatabase\\.cpp$'],
        ['exclude', 'plugins/PluginMainThreadScheduler\\.cpp$'],
        ['exclude', 'plugins/PluginPackage\\.cpp$'],
        ['exclude', 'plugins/PluginStream\\.cpp$'],
        ['exclude', 'plugins/PluginView\\.cpp$'],
        ['exclude', 'plugins/npapi\\.cpp$'],

        # FIXME: I don't know exactly why these are excluded.  It would
        # be nice to provide more explicit comments.  Some of these do actually
        # compile.
        ['exclude', 'dom/StaticStringList\\.cpp$'],
        ['exclude', 'loader/icon/IconFetcher\\.cpp$'],
        ['exclude', 'loader/UserStyleSheetLoader\\.cpp$'],

        # We use a multi-process version from the WebKit API.
        ['exclude', 'dom/default/PlatformMessagePortChannel\\.(cpp|h)$'],
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
      'conditions': [
        ['OS=="win"', {
          'sources/': [
            ['exclude', 'Posix\\.cpp$'],
            ['include', '/opentype/'],
            ['include', '/ScrollAnimatorWin\\.cpp$'],
            ['include', '/ScrollAnimatorWin\\.h$'],
            ['include', '/SkiaFontWin\\.cpp$'],
            ['include', '/TransparencyWin\\.cpp$'],
          ],
        }],
        ['OS=="mac"', {
          'sources/': [
            # RenderThemeChromiumSkia is not used on mac since RenderThemeChromiumMac
            # does not reference the Skia code that is used by Windows and Linux.
            ['exclude', 'rendering/RenderThemeChromiumSkia\\.cpp$'],
          ],
        }],
        ['OS!="linux" and OS!="freebsd"', {
          'sources/': [
            ['exclude', '(Gtk|Linux)\\.cpp$'],
          ],
        }],
        ['OS!="mac"', {
          'sources/': [['exclude', 'Mac\\.(cpp|mm?)$']]
        }],
        ['OS!="win"', {
          'sources/': [
            ['exclude', 'Win\\.cpp$'],
            ['exclude', '/(Windows|Uniscribe)[^/]*\\.cpp$']
          ],
        }],
      ],
    },
    {
      'target_name': 'webcore',
      'type': 'none',
      'dependencies': [
        'webcore_platform',
        'webcore_remaining',
        # Exported.
        'webcore_bindings',
        '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:wtf',
        '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(chromium_src_dir)/skia/skia.gyp:skia',
        '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
      ],
      'export_dependent_settings': [
        'webcore_bindings',
        '../../JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:wtf',
        '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(chromium_src_dir)/skia/skia.gyp:skia',
        '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<@(webcore_include_dirs)',
        ],
        'mac_framework_dirs': [
          '$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework/Frameworks',
        ],
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
        ['OS=="mac"', {
          'direct_dependent_settings': {
            'include_dirs': [
              '../../WebKitLibraries',
              '../../WebKit/mac/WebCoreSupport',
            ],
          },
        }],
        ['OS=="win"', {
          'direct_dependent_settings': {
            'include_dirs+++': ['../dom'],
          },
        }],
        ['enable_svg!=0', {
          'dependencies': [
            'webcore_svg',
          ],
        }],
      ],
    },
  ],  # targets
}
