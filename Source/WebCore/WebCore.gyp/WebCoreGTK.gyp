#
# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2013 Igalia S.L.
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
    '../WebCore.gypi',
  ],

  'target_defaults' : {
      'cflags' : [ '<@(global_cflags)', ],
      'defines': [ '<@(global_defines)' ],
  },

  'variables': {
    'WebCore': '..',
    'Platform': '../../Platform',
    'Source': '../..',
    'Dependencies': '<(Source)/WebKit/gtk/gyp/Dependencies.gyp',
    'webcoregtk_include_dirs': [
      '<(WebCore)/accessibility/atk',
      '<(WebCore)/bindings/js',
      '<(WebCore)/bridge/jsc',
      '<(WebCore)/bridge/c',
      '<(WebCore)/loader/gtk',
      '<(WebCore)/page/gtk',
      '<(WebCore)/platform/animation',
      '<(WebCore)/platform/audio/gstreamer',
      '<(WebCore)/platform/cairo',
      '<(WebCore)/platform/geoclue',
      '<(WebCore)/platform/graphics/cairo',
      '<(WebCore)/platform/graphics/egl',
      '<(WebCore)/platform/graphics/freetype',
      '<(WebCore)/platform/graphics/glx',
      '<(WebCore)/platform/graphics/gstreamer',
      '<(WebCore)/platform/graphics/gtk',
      '<(WebCore)/platform/graphics/harfbuzz',
      '<(WebCore)/platform/graphics/harfbuzz',
      '<(WebCore)/platform/graphics/harfbuzz/ng',
      '<(WebCore)/platform/graphics/harfbuzz/ng',
      '<(WebCore)/platform/graphics/opengl',
      '<(WebCore)/platform/gtk',
      '<(WebCore)/platform/linux',
      '<(WebCore)/platform/network/gtk',
      '<(WebCore)/platform/network/soup',
      '<(WebCore)/platform/text/enchant',
      '<(WebCore)/plugins/win',
      '<(Platform)/gtk',
      '<@(webcore_include_dirs)',
      '<(SHARED_INTERMEDIATE_DIR)/WebCore',
      '<(SHARED_INTERMEDIATE_DIR)/WebCore/bindings',
    ],

    'bindings_idl_files': [
      '<@(webcore_bindings_idl_files)',
    ],

    'webcoregtk_test_support_idl_files': [
      '<(WebCore)/testing/Internals.idl',
      '<(WebCore)/testing/InternalSettings.idl',
      '<(WebCore)/testing/MallocStatistics.idl',
      '<(WebCore)/testing/TypeConversions.idl',
      '<(SHARED_INTERMEDIATE_DIR)/WebCore/InternalSettingsGenerated.idl',
    ],

    'excluded_directories_pattern': '(android|chromium|ca|cf|cg|curl|efl|linux|mac|openvg|opentype|posix|qt|skia|iphone|win|wince|wx)/',
    'excluded_files_suffixes': '(Android|CF|CG|Curl|Linux|Mac|OpenType|POSIX|Posix|Qt|Safari|Skia|Win|WinCE|Wx|V8)\\.(cpp|mm?)$',
    'excluded_files_patterns': '.*(Chromium|Android).*',
  },


  'targets': [
    {
      'target_name': 'WebCoreDependencies',
      'type': 'none',
      'dependencies': [
        '<(Dependencies):cairo',
        '<(Dependencies):freetype',
        '<(Dependencies):gail',
        '<(Dependencies):glib',
        '<(Dependencies):gstreamer',
        '<(Dependencies):gtk',
        '<(Dependencies):icu',
        '<(Dependencies):libsoup',
        '<(Dependencies):libsecret',
        '<(Source)/JavaScriptCore/JavaScriptCore.gyp/JavaScriptCoreGTK.gyp:libjavascriptcoregtk',
        '<(Source)/WTF/WTF.gyp/WTFGTK.gyp:wtf',
        '<(Source)/ThirdParty/ANGLE/ANGLE.gyp/ANGLE.gyp:angle',
        'WebCoreBindingsSources',
      ],
      'export_dependent_settings': [
        '<(Dependencies):cairo',
        '<(Dependencies):freetype',
        '<(Dependencies):gail',
        '<(Dependencies):glib',
        '<(Dependencies):gstreamer',
        '<(Dependencies):gtk',
        '<(Dependencies):icu',
        '<(Dependencies):libsoup',
        '<(Dependencies):libsecret',
        '<(Source)/JavaScriptCore/JavaScriptCore.gyp/JavaScriptCoreGTK.gyp:libjavascriptcoregtk',
        '<(Source)/WTF/WTF.gyp/WTFGTK.gyp:wtf',
        '<(Source)/ThirdParty/ANGLE/ANGLE.gyp/ANGLE.gyp:angle',
      ],
    },
    {
      'target_name': 'InspectorProtocolSources',
      'type': 'none',
      'dependencies': [
        'InspectorProtocolVersion'
      ],
      'actions': [
        {
          'action_name': 'GenerateInspectorProtocolSources',
          'inputs': [
            # First input. It stands for python script in action below.
            '<(WebCore)/inspector/CodeGeneratorInspector.py',
            # Other inputs. They go as arguments to the python script.
            '<(WebCore)/inspector/Inspector.json',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/InspectorBackendDispatcher.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/InspectorBackendDispatcher.h',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/InspectorFrontend.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/InspectorFrontend.h',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/InspectorTypeBuilder.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/InspectorTypeBuilder.h',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/InspectorBackendCommands.js',
          ],
          'variables': {
            'generator_include_dirs': [
            ],
          },
          'action': [
            '<(PYTHON)',
            '<@(_inputs)',
            '--output_h_dir', '<(SHARED_INTERMEDIATE_DIR)/WebCore',

            # The relative path here is to work around a quirk of gyp that removes duplicate command-line arguments.
            # See http://crbug.com/146682.
            '--output_cpp_dir', '<(SHARED_INTERMEDIATE_DIR)/WebCore/../WebCore',
          ],
          'message': 'Generating Inspector protocol sources from Inspector.json',
        },
      ]
    },
    {
      'target_name': 'InspectorProtocolVersion',
      'type': 'none',
      'actions': [
         {
          'action_name': 'GenerateInspectorProtocolVersion',
          'inputs': [
            '<(WebCore)/inspector/generate-inspector-protocol-version',
            '<(WebCore)/inspector/Inspector.json',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/InspectorProtocolVersion.h',
          ],
          'variables': {
            'generator_include_dirs': [
            ],
          },
          'action': [
            '<(PYTHON)',
            '<(WebCore)/inspector/generate-inspector-protocol-version',
            '-o',
            '<@(_outputs)',
            '<(WebCore)/inspector/Inspector.json',
          ],
          'message': 'Validate inspector protocol for backwards compatibility and generate version file',
        }
      ]
    },
    {
      'target_name': 'InjectedScriptSource',
      'type': 'none',
      'variables': {
        'input_file_path': '<(WebCore)/inspector/InjectedScriptSource.js',
        'output_file_path': '<(SHARED_INTERMEDIATE_DIR)/WebCore/InjectedScriptSource.h',
        'character_array_name': 'InjectedScriptSource_js',
      },
      'includes': [ 'ConvertFileToHeaderWithCharacterArray.gypi' ],
    },
    {
      'target_name': 'InjectedScriptCanvasModuleSource',
      'type': 'none',
      'variables': {
        'input_file_path': '<(WebCore)/inspector/InjectedScriptCanvasModuleSource.js',
        'output_file_path': '<(SHARED_INTERMEDIATE_DIR)/WebCore/InjectedScriptCanvasModuleSource.h',
        'character_array_name': 'InjectedScriptCanvasModuleSource_js',
      },
      'includes': [ 'ConvertFileToHeaderWithCharacterArray.gypi' ],
    },
    {
      'target_name': 'InspectorOverlayPage',
      'type': 'none',
      'variables': {
        'input_file_path': '<(WebCore)/inspector/InspectorOverlayPage.html',
        'output_file_path': '<(SHARED_INTERMEDIATE_DIR)/WebCore/InspectorOverlayPage.h',
        'character_array_name': 'InspectorOverlayPage_html',
      },
      'includes': [ 'ConvertFileToHeaderWithCharacterArray.gypi' ],
    },
    {
      'target_name': 'SupplementalDependencies',
      'type': 'none',
      'actions': [
        {
          'action_name': 'GenerateSupplementalDependency',
          'variables': {
            # Write sources into a file, so that the action command line won't
            # exceed OS limits.
            'idl_files_list': '<|(idl_files_list.tmp <@(bindings_idl_files))',
            'conditions': [
              ['enable_svg!=0', {
               'idl_files_list': '<|(idl_files_list.tmp <@(bindings_idl_files) <@(webcore_svg_bindings_idl_files))',
              }],
            ],
          },
          'inputs': [
            '<(WebCore)/bindings/scripts/preprocess-idls.pl',
            '<(idl_files_list)',
            '<!@(cat <(idl_files_list))',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/supplemental_dependency.tmp',
          ],
          'action': [
            '<(PERL)',
            '-w',
            '-I<(WebCore)/bindings/scripts',
            '<(WebCore)/bindings/scripts/preprocess-idls.pl',
            '--defines',
            '<(feature_defines) LANGUAGE_JAVASCRIPT',
            '--idlFilesList',
            '<(idl_files_list)',
            '--supplementalDependencyFile',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/supplemental_dependency.tmp',
          ],
          'message': 'Resolving [Supplemental=XXX] dependencies in all IDL files',
        }
      ]
    },
    {
      'target_name': 'Settings',
      'type': 'none',
      'actions': [
        {
          'action_name': 'GenerateSettings',
          'inputs': [
            '<(WebCore)/page/make_settings.pl',
            '<(WebCore)/page/Settings.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/SettingsMacros.h',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/InternalSettingsGenerated.idl',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/InternalSettingsGenerated.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/InternalSettingsGenerated.h',
          ],
          'action': [
            '<(PYTHON)',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
          ],
        },
      ]
    },
    {
      'target_name': 'HTMLNames',
      'type': 'none',
      'variables': {
        'make_names_inputs': [
          '<(WebCore)/html/HTMLTagNames.in',
          '<(WebCore)/html/HTMLAttributeNames.in',
        ],
        'make_names_outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/HTMLNames.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/HTMLNames.h',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/HTMLElementFactory.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/bindings/JSHTMLElementWrapperFactory.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/bindings/JSHTMLElementWrapperFactory.h',
        ],
        'make_names_extra_arguments': [ '--factory', '--wrapperFactory' ],
      },
      'includes': [ 'MakeNames.gypi' ],
    },
    {
      'target_name': 'WebKitFontFamilyNames',
      'type': 'none',
      'variables': {
        'make_names_inputs': [ '<(WebCore)/css/WebKitFontFamilyNames.in', ],
        'make_names_outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/WebKitFontFamilyNames.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/WebKitFontFamilyNames.h',
        ],
        'make_names_extra_arguments': [ '--fonts' ],
      },
      'includes': [ 'MakeNames.gypi' ],
    },
    {
      'target_name': 'SVGNames',
      'type': 'none',
      'variables': {
        'make_names_inputs': [
          '<(WebCore)/svg/svgtags.in',
          '<(WebCore)/svg/svgattrs.in',
        ],
        'make_names_outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/SVGNames.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/SVGNames.h',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/SVGElementFactory.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/SVGElementFactory.h',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/JSSVGElementWrapperFactory.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/JSSVGElementWrapperFactory.h',
        ],
        'make_names_extra_arguments': [ '--factory', '--wrapperFactory' ],
      },
      'includes': [ 'MakeNames.gypi' ],
    },
    {
      'target_name': 'MathMLNames',
      'type': 'none',
      'variables': {
        'make_names_inputs': [
          '<(WebCore)/mathml/mathtags.in',
          '<(WebCore)/mathml/mathattrs.in',
        ],
        'make_names_outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/MathMLNames.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/MathMLNames.h',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/MathMLElementFactory.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/MathMLElementFactory.h',
        ],
        'make_names_extra_arguments': [ '--factory', ],
      },
      'includes': [ 'MakeNames.gypi' ],
    },
    {
      'target_name': 'XLinkNames',
      'type': 'none',
      'variables': {
        'make_names_inputs': [ '<(WebCore)/svg/xlinkattrs.in', ],
        'make_names_outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/XLinkNames.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/XLinkNames.h',
        ],
        'make_names_extra_arguments': [ '--factory', ],
      },
      'includes': [ 'MakeNames.gypi' ],
    },
    {
      'target_name': 'XMLNSNames',
      'type': 'none',
      'variables': {
        'make_names_inputs': [ '<(WebCore)/xml/xmlnsattrs.in', ],
        'make_names_outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/XMLNSNames.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/XMLNSNames.h',
        ],
        'make_names_extra_arguments': [ ],
      },
      'includes': [ 'MakeNames.gypi' ],
    },
    {
      'target_name': 'XMLNames',
      'type': 'none',
      'variables': {
        'make_names_inputs': [ '<(WebCore)/xml/xmlattrs.in', ],
        'make_names_outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/XMLNames.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/WebCore/XMLNames.h',
        ],
        'make_names_extra_arguments': [ ],
      },
      'includes': [ 'MakeNames.gypi' ],
    },
    {
      'target_name': 'XMLViewerCSS',
      'type': 'none',
      'variables': {
        'input_file_path': '<(WebCore)/xml/XMLViewer.css',
        'output_file_path': '<(SHARED_INTERMEDIATE_DIR)/WebCore/XMLViewerCSS.h',
        'character_array_name': 'XMLViewer_css',
      },
      'includes': [ 'ConvertFileToHeaderWithCharacterArray.gypi' ],
    },
    {
      'target_name': 'XMLViewerJS',
      'type': 'none',
      'variables': {
        'input_file_path': '<(WebCore)/xml/XMLViewer.js',
        'output_file_path': '<(SHARED_INTERMEDIATE_DIR)/WebCore/XMLViewerJS.h',
        'character_array_name': 'XMLViewer_js',
      },
      'includes': [ 'ConvertFileToHeaderWithCharacterArray.gypi' ],
    },
    {
      'target_name': 'WebCoreBindingsSources',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'HTMLNames',
        'SVGNames',
        'MathMLNames',
        'XLinkNames',
        'XMLNSNames',
        'XMLNames',
        'XMLViewerCSS',
        'XMLViewerJS',
        'WebKitFontFamilyNames',
        'SupplementalDependencies',
        'Settings',
      ],
      'sources': [
        # bison rule
        '<(SHARED_INTERMEDIATE_DIR)/WebCore/CSSGrammar.y',
        '<(WebCore)/xml/XPathGrammar.y',

        # gperf rule
        '<(WebCore)/platform/ColorData.gperf',

        # idl rules
        '<@(bindings_idl_files)',
        '<@(webcoregtk_test_support_idl_files)',
      ],
      'sources!': [
        # Custom bindings in bindings/js exist for these.
        '<(WebCore)/dom/EventListener.idl',

        # Bindings with custom Objective-C implementations.
        '<(WebCore)/page/AbstractView.idl',

        # These bindings are excluded, as they're only used through inheritance
        # and don't define constants that would need a constructor.
        '<(WebCore)/svg/ElementTimeControl.idl',
        '<(WebCore)/svg/SVGExternalResourcesRequired.idl',
        '<(WebCore)/svg/SVGFilterPrimitiveStandardAttributes.idl',
        '<(WebCore)/svg/SVGFitToViewBox.idl',
        '<(WebCore)/svg/SVGLangSpace.idl',
        '<(WebCore)/svg/SVGLocatable.idl',
        '<(WebCore)/svg/SVGTests.idl',
        '<(WebCore)/svg/SVGTransformable.idl',

        '<(WebCore)/css/CSSUnknownRule.idl',
      ],

      'conditions': [
        ['enable_svg!=0', {
          'sources': [ '<@(webcore_svg_bindings_idl_files)', ],
        }],
      ],
      'actions': [
        {
          'action_name': 'GenerateHTMLEntityTable',
          'inputs': [
            '<(WebCore)/html/parser/create-html-entity-table',
            '<(WebCore)/html/parser/HTMLEntityNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/HTMLEntityTable.cpp'
          ],
          'action': [
            '<(PYTHON)',
            '<(WebCore)/html/parser/create-html-entity-table',
            '-o',
            '<@(_outputs)',
            '<(WebCore)/html/parser/HTMLEntityNames.in',
          ],
        },
        {
          'action_name': 'GenerateCSSPropertyNames',
          'inputs': [
            '<(WebCore)/css/makeprop.pl',
            '<(WebCore)/css/CSSPropertyNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/CSSPropertyNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/CSSPropertyNames.h',
          ],
          'action': [
            '<(PYTHON)',
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
                '<(WebCore)/css/SVGCSSPropertyNames.in',
              ],
            }],
          ],
        },
        {
          'action_name': 'GenerateCSSValueKeywords',
          'inputs': [
            '<(WebCore)/css/makevalues.pl',
            '<(WebCore)/css/CSSValueKeywords.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/CSSValueKeywords.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/CSSValueKeywords.h',
          ],
          'action': [
            '<(PYTHON)',
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
                '<(WebCore)/css/SVGCSSValueKeywords.in',
              ],
            }],
          ],
        },
        {
          'action_name': 'GenerateEventFactory',
          'inputs': [
            '<(WebCore)/dom/make_event_factory.pl',
            '<(WebCore)/dom/EventNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/EventFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/EventHeaders.h',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/EventInterfaces.h',
          ],
          'action': [
            '<(PYTHON)',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
          ],
        },
        {
          'action_name': 'GenreateEventTargetFactory',
          'inputs': [
            '<(WebCore)/dom/make_event_factory.pl',
            '<(WebCore)/dom/EventTargetFactory.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/EventTargetHeaders.h',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/EventTargetInterfaces.h',
          ],
          'action': [
            '<(PYTHON)',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
          ],
        },
        {
          'action_name': 'GenerateExceptionCodeDescription',
          'inputs': [
            '<(WebCore)/dom/make_dom_exceptions.pl',
            '<(WebCore)/dom/DOMExceptions.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/ExceptionCodeDescription.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/ExceptionCodeDescription.h',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/ExceptionHeaders.h',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/ExceptionInterfaces.h',
          ],
          'action': [
            '<(PYTHON)',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
          ],
        },
        {
          'action_name': 'GeneateUserAgentStyleSheets',
          'variables': {
            'scripts': [
              '<(WebCore)/css/make-css-file-arrays.pl',
              '<(WebCore)/bindings/scripts/preprocessor.pm',
            ],
            'stylesheets': [
              '<(WebCore)/css/html.css',
              '<(WebCore)/css/mathml.css',
              '<(WebCore)/css/quirks.css',
              '<(WebCore)/css/view-source.css',
              '<(WebCore)/css/svg.css',
              '<(WebCore)/css/mediaControls.css',
              '<(WebCore)/css/mediaControlsGtk.css',
              '<(WebCore)/css/fullscreen.css',
              '<(WebCore)/css/plugIns.css',
              # Skip fullscreenQuickTime.
            ],
          },
          'inputs': [
            '<@(scripts)',
            '<@(stylesheets)'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/UserAgentStyleSheets.h',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/UserAgentStyleSheetsData.cpp',
          ],
          'action': [
            '<(PYTHON)',
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
          'action_name': 'PreprocessGrammar',
          'inputs': [
            '<(WebCore)/css/CSSGrammar.y.in',
            '<(WebCore)/css/CSSGrammar.y.includes',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/CSSGrammar.y',
          ],
          'action': [
            '<(PERL)',
            '-I',
            '<(WebCore)/bindings/scripts',
            '<(WebCore)/css/makegrammar.pl',
            '--outputDir',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore',
            '--extraDefines',
            '<(feature_defines)',
            '--symbolsPrefix',
            'cssyy',
            '<@(_inputs)',
          ],
        },
      ],
      'rules': [
        # Rules to build derived sources.
        {
          'rule_name': 'Bison',
          'extension': 'y',
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/<(RULE_INPUT_ROOT).cpp',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/<(RULE_INPUT_ROOT).h'
          ],
          'action': [
            '<(PYTHON)',
            'scripts/rule_bison.py',
            '<(RULE_INPUT_PATH)',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore',
            '<(BISON)',
          ],
        },
        {
          'rule_name': 'GPerf',
          'extension': 'gperf',
          #
          # gperf outputs are generated by WebCore/make-hash-tools.pl
          #
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/<(RULE_INPUT_ROOT).cpp',
          ],
          'inputs': [
            '<(WebCore)/make-hash-tools.pl',
          ],
          'action': [
            '<(PERL)',
            '<(WebCore)/make-hash-tools.pl',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore',
            '<(RULE_INPUT_PATH)',
          ],
        },
        # Rule to build generated JavaScript (JSC) bindings from .idl source.
        {
          'rule_name': 'DOMBindings',
          'extension': 'idl',
          'inputs': [
            '<(WebCore)/bindings/scripts/generate-bindings.pl',
            '<(WebCore)/bindings/scripts/CodeGenerator.pm',
            '<(WebCore)/bindings/scripts/CodeGeneratorJS.pm',
            '<(WebCore)/bindings/scripts/IDLParser.pm',
            '<(WebCore)/bindings/scripts/IDLAttributes.txt',
            '<(WebCore)/bindings/scripts/preprocessor.pm',
            '<!@pymod_do_main(supplemental_idl_files <@(bindings_idl_files))',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/bindings/JS<(RULE_INPUT_ROOT).cpp',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/bindings/JS<(RULE_INPUT_ROOT).h',
          ],
          'variables': {
            'generator_include_dirs': [
              '--include', '<(WebCore)/Modules/filesystem',
              '--include', '<(WebCore)/Modules/indexeddb',
              '--include', '<(WebCore)/Modules/mediasource',
              '--include', '<(WebCore)/Modules/mediastream',
              '--include', '<(WebCore)/Modules/navigatorcontentutils',
              '--include', '<(WebCore)/Modules/notifications',
              '--include', '<(WebCore)/Modules/webaudio',
              '--include', '<(WebCore)/Modules/webdatabase',
              '--include', '<(WebCore)/css',
              '--include', '<(WebCore)/dom',
              '--include', '<(WebCore)/fileapi',
              '--include', '<(WebCore)/html',
              '--include', '<(WebCore)/page',
              '--include', '<(WebCore)/plugins',
              '--include', '<(WebCore)/storage',
              '--include', '<(WebCore)/svg',
              '--include', '<(WebCore)/testing',
              '--include', '<(WebCore)/workers',
              '--include', '<(WebCore)/xml',
              '--include', '<(SHARED_INTERMEDIATE_DIR)/WebCore',
            ],
          },
          'action': [
            '<(PERL)',
            '-w',
            '-I<(WebCore)/bindings/scripts',
            '<(WebCore)/bindings/scripts/generate-bindings.pl',
            '--outputDir',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/bindings',
            '--idlAttributesFile',
            '<(WebCore)/bindings/scripts/IDLAttributes.txt',
            '--defines',
            '<(feature_defines) LANGUAGE_JAVASCRIPT',
            '--generator',
            'JS',
            '<@(generator_include_dirs)',
            '--supplementalDependencyFile',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/supplemental_dependency.tmp',
            '--additionalIdlFiles',
            '<(webcoregtk_test_support_idl_files)',
            '<(RULE_INPUT_PATH)',
          ],
          'message': 'Generating binding from <(RULE_INPUT_PATH)',
        },
      ],
    },
    {
      'target_name': 'WebCoreBindings',
      'type': 'static_library',
      'hard_dependency': 1,
      'dependencies': [
        'WebCoreDependencies',
        'InjectedScriptSource',
        'InjectedScriptCanvasModuleSource',
        'InspectorOverlayPage',
        'InspectorProtocolSources',
      ],
      'include_dirs': [ '<@(webcoregtk_include_dirs)', ],
      'sources': [
        '<@(webcore_derived_source_files)',
      ],
      'conditions': [
        # TODO(maruel): Move it in its own project or generate it anyway?
        ['enable_svg!=0', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/SVGElementFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/WebCore/JSSVGElementWrapperFactory.cpp',
            '<@(webcore_svg_derived_source_files)',
         ],
        }],
      ],
    },
    {
      'target_name': 'WebCoreDOM',
      'type': 'static_library',
      'dependencies': [ 'WebCoreDependencies', ],
      'include_dirs': [ '<@(webcoregtk_include_dirs)', ],
      'sources': [ '<@(webcore_dom_files)', ],
      'sources/': [
        ['exclude', '<(excluded_directories_pattern)'],
        ['exclude', '<(excluded_files_suffixes)'],
        ['exclude', '<(excluded_files_patterns)'],
        ['exclude', 'AllInOne\\.cpp$'],
      ],
    },
    {
      'target_name': 'WebCoreHTML',
      'type': 'static_library',
      'dependencies': [ 'WebCoreDependencies', ],
      'include_dirs': [ '<@(webcoregtk_include_dirs)', ],
      'sources': [ '<@(webcore_html_files)', ],
      'sources/': [
        ['exclude', 'AllInOne\\.cpp$'],
        ['exclude', 'Android\\.cpp$'],
      ],
    },
    {
      'target_name': 'WebCorePlatform',
      'type': 'static_library',
      'dependencies': [ 'WebCoreDependencies', ],
      'include_dirs': [ '<@(webcoregtk_include_dirs)', ],
      'sources': [
        '<@(webcore_platform_files)',
        '../Platform/gtk/GtkVersioning.h',
       ],
      'sources/': [
        ['exclude', '.*'],
        ['include', 'platform/'],

        ['exclude', '<(excluded_directories_pattern)'],
        ['exclude', '<(excluded_files_suffixes)'],
        ['exclude', '<(excluded_files_patterns)'],
        ['exclude', 'platform/Theme\\.cpp$'],
        ['exclude', 'platform/image-encoders/.*ImageEncoder\\.(cpp|h)$'],
        ['exclude', 'platform/text/LocaleToScriptMappingICU\\.cpp$'],
        ['exclude', 'platform/text/TextEncodingDetectorNone\\.cpp$'],
        ['exclude', 'platform/graphics/FontPlatformData\\.cpp$'],
        ['exclude', 'platform/graphics/harfbuzz/FontHarfBuzz\\.cpp$'],
        ['exclude', 'platform/graphics/harfbuzz/HarfBuzzFaceCoreText\\.cpp$'],
        ['exclude', 'platform/graphics/harfbuzz/FontPlatformDataHarfBuzz\\.cpp$'],
        ['exclude', 'platform/network/NetworkStorageSessionStub\\.cpp$'],
      ],
    },
    {
      'target_name': 'WebCorePlatformGeometry',
      'type': 'static_library',
      'dependencies': [ 'WebCoreDependencies', ],
      'include_dirs': [ '<@(webcoregtk_include_dirs)', ],
      'sources': [ '<@(webcore_platform_geometry_files)', ],
    },
    {
      'target_name': 'WebCore',
      'type': 'none',
      'dependencies': [
        'WebCoreDOM',
        'WebCoreHTML',
        'WebCoreBindings',
        'WebCorePlatform',
        'WebCorePlatformGeometry',
      ],
    },
  ],
}
