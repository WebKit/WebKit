#
# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
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
    'variables': {
        'ahem_path': '../../DumpRenderTree/qt/fonts/AHEM____.TTF',
        'tools_dir': '../..',
        'source_dir': '../../../Source',
        'conditions': [
            # Location of the chromium src directory and target type is different
            # if webkit is built inside chromium or as standalone project.
            ['inside_chromium_build==0', {
                # Webkit is being built outside of the full chromium project.
                # e.g. via build-webkit --chromium
                'chromium_src_dir': '<(source_dir)/WebKit/chromium',
            },{
                # WebKit is checked out in src/chromium/third_party/WebKit
                'chromium_src_dir': '<(tools_dir)/../../..',
            }],
        ],
    },
    'includes': [
        '../DumpRenderTree.gypi',
        '../../../Source/WebKit/chromium/features.gypi',
    ],
    'targets': [
        {
            'target_name': 'ImageDiff',
            'type': 'executable',
            'dependencies': [
                '<(chromium_src_dir)/webkit/support/webkit_support.gyp:webkit_support_gfx',
                '<(source_dir)/WTF/WTF.gyp/WTF.gyp:wtf',
            ],
            'include_dirs': [
                '<(source_dir)/JavaScriptCore',
                '<(DEPTH)',
            ],
            'sources': [
                '<(tools_dir)/DumpRenderTree/chromium/ImageDiff.cpp',
            ],
            'conditions': [
                ['OS=="android"', {
                    # The Chromium Android port will compare images on host rather
                    # than target (a device or emulator) for performance reasons.
                    'toolsets': ['host'],
                }],
            ],
        },
        {
            'target_name': 'DumpRenderTree',
            'type': 'executable',
            'mac_bundle': 1,
            'dependencies': [
                '<(source_dir)/WebKit/chromium/WebKit.gyp:inspector_resources',
                '<(source_dir)/WebKit/chromium/WebKit.gyp:webkit',
                '<(source_dir)/WTF/WTF.gyp/WTF.gyp:wtf',
                '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
                '<(chromium_src_dir)/third_party/icu/icu.gyp:icuuc',
                '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
                '<(chromium_src_dir)/base/base.gyp:test_support_base',
                '<(chromium_src_dir)/webkit/support/webkit_support.gyp:blob',
                '<(chromium_src_dir)/webkit/support/webkit_support.gyp:webkit_support',
                '<(chromium_src_dir)/webkit/support/webkit_support.gyp:webkit_user_agent',
            ],
            'include_dirs': [
                '<(chromium_src_dir)',
                '<(source_dir)/WebKit/chromium/public',
                '<(source_dir)/JavaScriptCore',
                '<(DEPTH)',
            ],
            'defines': [
                # Technically not a unit test but require functions available only to
                # unit tests.
                'UNIT_TEST',
            ],
            'sources': [
                '<@(drt_files)',
            ],
            'conditions': [
                ['OS=="win"', {
                    'dependencies': [
                        'LayoutTestHelper',
                        '<(chromium_src_dir)/third_party/angle/src/build_angle.gyp:libEGL',
                        '<(chromium_src_dir)/third_party/angle/src/build_angle.gyp:libGLESv2',
                    ],

                    'resource_include_dirs': ['<(SHARED_INTERMEDIATE_DIR)/webkit'],
                    'sources': [
                        '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.rc',
                    ],
                    'conditions': [
                        ['inside_chromium_build==1', {
                            'configurations': {
                                'Debug_Base': {
                                    'msvs_settings': {
                                        'VCLinkerTool': {
                                            'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                                        },
                                    },
                                },
                            },
                        }],
                    ],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)',
                        'files': ['<(ahem_path)'],
                    }],
                },{ # OS!="win"
                    'sources/': [
                        ['exclude', 'Win\\.cpp$'],
                    ],
                    'actions': [
                        {
                            'action_name': 'repack_locale',
                            'variables': {
                                'repack_path': '<(chromium_src_dir)/tools/grit/grit/format/repack.py',
                                'pak_inputs': [
                                    '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                                    '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.pak',
                                    '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
                                    '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.pak',
                                    '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
                            ]},
                            'inputs': [
                                '<(repack_path)',
                                '<@(pak_inputs)',
                            ],
                            'outputs': [
                                '<(INTERMEDIATE_DIR)/repack/DumpRenderTree.pak',
                            ],
                            'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
                            'process_outputs_as_mac_bundle_resources': 1,
                        },
                    ], # actions
                }],
                ['OS=="mac"', {
                    'dependencies': [
                        '<(source_dir)/WebKit/chromium/WebKit.gyp:copy_mesa',
                        'LayoutTestHelper',
                    ],
                    'mac_bundle_resources': [
                        '<(ahem_path)',
                        '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher100.ttf',
                        '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher200.ttf',
                        '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher300.ttf',
                        '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher400.ttf',
                        '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher500.ttf',
                        '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher600.ttf',
                        '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher700.ttf',
                        '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher800.ttf',
                        '<(tools_dir)/DumpRenderTree/fonts/WebKitWeightWatcher900.ttf',
                        '<(SHARED_INTERMEDIATE_DIR)/webkit/textAreaResizeCorner.png',
                    ],
                },{ # OS!="mac"
                    'sources/': [
                        # .mm is already excluded by common.gypi
                        ['exclude', 'Mac\\.cpp$'],
                    ],
                }],
                ['use_x11 == 1', {
                    'dependencies': [
                        '<(chromium_src_dir)/build/linux/system.gyp:fontconfig',
                    ],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)',
                        'files': [
                            '<(ahem_path)',
                            '<(tools_dir)/DumpRenderTree/chromium/fonts.conf',
                            '<(INTERMEDIATE_DIR)/repack/DumpRenderTree.pak',
                        ]
                    }],
                    'variables': {
                        # FIXME: Enable warnings on other platforms.
                        'chromium_code': 1,
                    },
                    'conditions': [
                        ['linux_use_tcmalloc == 1', {
                            'dependencies': [
                                '<(chromium_src_dir)/base/allocator/allocator.gyp:allocator',
                            ],
                        }],
                    ],
                },{ # use_x11 != 1
                    'sources/': [
                        ['exclude', 'Linux\\.cpp$']
                    ]
                }],
                ['toolkit_uses_gtk == 1', {
                    'defines': [
                        'WTF_USE_GTK=1',
                    ],
                    'dependencies': [
                        '<(chromium_src_dir)/build/linux/system.gyp:gtk',
                    ],
                    'include_dirs': [
                        '<(source_dir)/WebKit/chromium/public/gtk',
                    ],
                },{ # toolkit_uses_gtk != 1
                    'sources/': [
                        ['exclude', 'Gtk\\.cpp$']
                    ]
                }],
                ['OS=="android"', {
                    'dependencies': [
                        'ImageDiff#host',
                    ],
                    'sources/': [
                        ['include', 'chromium/TestShellLinux\\.cpp$'],
                    ],
                },{ # OS!="android"
                    'sources/': [
                        ['exclude', '(Android)\\.cpp$']
                    ],
                    'dependencies': [
                        'ImageDiff',
                        'copy_TestNetscapePlugIn',
                        '<(chromium_src_dir)/third_party/mesa/mesa.gyp:osmesa',
                    ],
                }],
                ['inside_chromium_build==1 and component=="shared_library"', {
                    'sources': [
                        '<(source_dir)/WebKit/chromium/src/ChromiumCurrentTime.cpp',
                        '<(source_dir)/WebKit/chromium/src/ChromiumThreading.cpp',
                    ],
                    'include_dirs': [
                        '<(source_dir)/WebKit/chromium/public',
                    ],
                    'dependencies': [
                        '<(source_dir)/WTF/WTF.gyp/WTF.gyp:wtf',
                    ],
                }],
                ['inside_chromium_build==0', {
                    'dependencies': [
                        '<(chromium_src_dir)/webkit/support/setup_third_party.gyp:third_party_headers',
                    ]
                }],
                ['inside_chromium_build==0 or component!="shared_library"', {
                    'dependencies': [
                        '<(source_dir)/WebCore/WebCore.gyp/WebCore.gyp:webcore_test_support',
                    ],
                    'include_dirs': [
                        # WARNING: Do not view this particular case as a precedent for
                        # including WebCore headers in DumpRenderTree project.
                        '<(source_dir)/WebCore/testing/v8', # for WebCoreTestSupport.h, needed to link in window.internals code.
                    ],
                    'sources': [
                        '<(source_dir)/WebKit/chromium/src/WebTestingSupport.cpp',
                        '<(source_dir)/WebKit/chromium/public/WebTestingSupport.h',
                    ],
                }],
            ],
        },
        {
            'target_name': 'TestNetscapePlugIn',
            'type': 'loadable_module',
            'sources': [ '<@(test_plugin_files)' ],
            'dependencies': [
                '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
            ],
            'include_dirs': [
                '<(chromium_src_dir)',
                '<(tools_dir)/DumpRenderTree/TestNetscapePlugIn',
                '<(tools_dir)/DumpRenderTree/chromium/TestNetscapePlugIn/ForwardingHeaders',
            ],
            'conditions': [
                ['OS=="mac"', {
                    'mac_bundle': 1,
                    'product_extension': 'plugin',
                    'link_settings': {
                        'libraries': [
                            '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
                            '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
                            '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
                        ]
                    },
                    'xcode_settings': {
                        'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
                        'INFOPLIST_FILE': '<(tools_dir)/DumpRenderTree/TestNetscapePlugIn/mac/Info.plist',
                    },
                }],
                ['os_posix == 1 and OS != "mac"', {
                    'cflags': [
                        '-fvisibility=default',
                    ],
                }],
                ['OS=="win"', {
                    'defines': [
                        # This seems like a hack, but this is what Safari Win does.
                        'snprintf=_snprintf',
                    ],
                    'sources': [
                        '<(tools_dir)/DumpRenderTree/TestNetscapePlugIn/win/TestNetscapePlugin.def',
                        '<(tools_dir)/DumpRenderTree/TestNetscapePlugIn/win/TestNetscapePlugin.rc',
                    ],
                    # The .rc file requires that the name of the dll is npTestNetscapePlugIn.dll.
                    'product_name': 'npTestNetscapePlugIn',
                }],
            ],
        },
        {
            'target_name': 'copy_TestNetscapePlugIn',
            'type': 'none',
            'dependencies': [
                'TestNetscapePlugIn',
            ],
            'conditions': [
                ['OS=="win"', {
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/plugins',
                        'files': ['<(PRODUCT_DIR)/npTestNetscapePlugIn.dll'],
                    }],
                }],
                ['OS=="mac"', {
                    'dependencies': ['TestNetscapePlugIn'],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/plugins/',
                        'files': ['<(PRODUCT_DIR)/TestNetscapePlugIn.plugin/'],
                    }],
                }],
                ['os_posix == 1 and OS != "mac"', {
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/plugins',
                        'files': ['<(PRODUCT_DIR)/libTestNetscapePlugIn.so'],
                    }],
                }],
            ],
        },
    ], # targets
    'conditions': [
        ['OS=="win"', {
            'targets': [{
                'target_name': 'LayoutTestHelper',
                'type': 'executable',
                'sources': ['<(tools_dir)/DumpRenderTree/chromium/LayoutTestHelperWin.cpp'],
            }],
        }],
        ['OS=="mac"', {
            'targets': [{
                'target_name': 'LayoutTestHelper',
                'type': 'executable',
                'sources': ['<(tools_dir)/DumpRenderTree/chromium/LayoutTestHelper.mm'],
                'link_settings': {
                    'libraries': [
                        '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
                    ],
                },
            }],
        }],
        ['os_posix==1 and OS!="mac" and OS!="android" and gcc_version==46', {
            'target_defaults': {
                # Disable warnings about c++0x compatibility, as some names (such
                # as nullptr) conflict with upcoming c++0x types.
                'cflags_cc': ['-Wno-c++0x-compat'],
            },
        }],
    ], # conditions
}
