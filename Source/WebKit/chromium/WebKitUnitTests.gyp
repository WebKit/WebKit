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
    'includes': [
        'features.gypi',
        'WebKit.gypi',
    ],
    'variables': {
        'conditions': [
            # Location of the chromium src directory and target type is different
            # if webkit is built inside chromium or as standalone project.
            ['inside_chromium_build==0', {
                # Webkit is being built outside of the full chromium project.
                # e.g. via build-webkit --chromium
                'chromium_src_dir': '../../WebKit/chromium',
            },{
                # WebKit is checked out in src/chromium/third_party/WebKit
                'chromium_src_dir': '../../../../..',
            }],
        ],
    },
    'targets': [
        {
            'target_name': 'webkit_unit_tests',
            'type': 'executable',
            'variables': { 'enable_wexit_time_destructors': 1, },
            'msvs_guid': '7CEFE800-8403-418A-AD6A-2D52C6FC3EAD',
            'dependencies': [
                'WebKit.gyp:webkit',
                '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
                '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
                '<(chromium_src_dir)/testing/gtest.gyp:gtest',
                '<(chromium_src_dir)/testing/gmock.gyp:gmock',
                '<(chromium_src_dir)/base/base.gyp:base',
                '<(chromium_src_dir)/base/base.gyp:base_i18n',
                '<(chromium_src_dir)/base/base.gyp:test_support_base',
                '<(chromium_src_dir)/third_party/zlib/zlib.gyp:zlib',
                '<(chromium_src_dir)/webkit/support/webkit_support.gyp:webkit_support',
            ],
            'sources': [
                'tests/RunAllTests.cpp',
            ],
            'include_dirs': [
                'public',
                'src',
                # WebKit unit tests are allowed to include WebKit and WebCore header files, which may include headers in the
                # Platform API as <public/WebFoo.h>. Thus we need to have the Platform API include path, but we can't depend
                # directly on Platform.gyp:webkit_platform since platform cannot link as a separate library. Instead, we just
                # add the include path directly.
                '../../Platform/chromium',
            ],
            'conditions': [
                ['inside_chromium_build==1 and component=="shared_library"', {
                    'defines': [
                        'WEBKIT_DLL_UNITTEST',
                    ],
                }, {
                    'dependencies': [
                        '../../WebCore/WebCore.gyp/WebCore.gyp:webcore',
                    ],
                    'defines': [
                        'WEBKIT_IMPLEMENTATION=1',
                    ],
                    'sources': [
                        '<@(webkit_unittest_files)',
                    ],
                    'conditions': [
                        ['toolkit_uses_gtk == 1', {
                            'include_dirs': [
                                'public/gtk',
                            ],
                            'variables': {
                            # FIXME: Enable warnings on other platforms.
                            'chromium_code': 1,
                            },
                        }],
                    ],
                }],
                ['inside_chromium_build==1 and OS=="win" and component!="shared_library"', {
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
                ['OS=="android" and gtest_target_type == "shared_library"', {
                    'type': 'shared_library',
                    'dependencies': [
                        '<(chromium_src_dir)/testing/android/native_test.gyp:native_test_native_code',
                        '<(chromium_src_dir)/tools/android/forwarder2/forwarder.gyp:forwarder2',
                    ],
                }],
                ['OS=="mac"', {
                    'include_dirs': [
                        'public/mac',
                    ],
                }],
            ],
        }                
    ], # targets
    'conditions': [
        ['os_posix==1 and OS!="mac" and gcc_version>=46', {
            'target_defaults': {
                # Disable warnings about c++0x compatibility, as some names (such
                # as nullptr) conflict with upcoming c++0x types.
                'cflags_cc': ['-Wno-c++0x-compat'],
            },
        }],
        ['OS=="android" and gtest_target_type == "shared_library"', {
            # Wrap libwebkit_unit_tests.so into an android apk for execution.
            'targets': [{
                'target_name': 'webkit_unit_tests_apk',
                'type': 'none',
                'dependencies': [
                    '<(chromium_src_dir)/base/base.gyp:base',
                    '<(chromium_src_dir)/net/net.gyp:net',
                    'webkit_unit_tests',
                ],
                'variables': {
                    'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)webkit_unit_tests<(SHARED_LIB_SUFFIX)',
                    'input_jars_paths': [
                        '<(PRODUCT_DIR)/lib.java/chromium_base.jar',
                        '<(PRODUCT_DIR)/lib.java/chromium_net.jar',
                    ],
                    'conditions': [
                        ['inside_chromium_build==1', {
                            'ant_build_to_chromium_src': '<(ant_build_out)/../../',
                        }, {
                            'ant_build_to_chromium_src': '<(ant_build_out)/../../Source/WebKit/chromium',
                        }],
                    ],
                },
                # Part of the following was copied from <(chromium_src_dir)/build/apk_test.gpyi.
                # Not including it because gyp include doesn't support variable in path or under
                # conditions. And we also have some different requirements.
                'actions': [{
                    'action_name': 'apk_webkit_unit_tests',
                    'message': 'Building webkit_unit_tests test apk.',
                    'inputs': [
                        '<(chromium_src_dir)/testing/android/AndroidManifest.xml',
                        '<(chromium_src_dir)/testing/android/generate_native_test.py',
                        '<(input_shlib_path)',
                        '>@(input_jars_paths)',
                    ],
                    'outputs': [
                        '<(PRODUCT_DIR)/webkit_unit_tests_apk/webkit_unit_tests-debug.apk',
                    ],
                    'action': [
                        '<(chromium_src_dir)/testing/android/generate_native_test.py',
                        '--native_library',
                        '<(input_shlib_path)',
                        '--jars',
                        '">@(input_jars_paths)"',
                        '--output',
                        '<(PRODUCT_DIR)/webkit_unit_tests_apk',
                        '--strip-binary=<(android_strip)',
                        '--ant-args',
                        '-quiet',
                        '--ant-args',
                        '-DANDROID_SDK=<(android_sdk)',
                        '--ant-args',
                        '-DANDROID_SDK_ROOT=<(android_sdk_root)',
                        '--ant-args',
                        '-DANDROID_SDK_TOOLS=<(android_sdk_tools)',
                        '--ant-args',
                        '-DANDROID_SDK_VERSION=<(android_sdk_version)',
                        '--ant-args',
                        '-DANDROID_TOOLCHAIN=<(android_toolchain)',
                        '--ant-args',
                        '-DANDROID_GDBSERVER=<(android_gdbserver)',
                        '--ant-args',
                        '-DPRODUCT_DIR=<(ant_build_out)',
                        '--ant-args',
                        '-DCHROMIUM_SRC=<(ant_build_to_chromium_src)',
                        '--app_abi',
                        '<(android_app_abi)',
                    ],
                }],
            }],
        }],
        ['clang==1', {
            'target_defaults': {
                'cflags': ['-Wunused-parameter'],
                'xcode_settings': {
                    'WARNING_CFLAGS': ['-Wunused-parameter'],
                },
            },
        }],
    ],
}
