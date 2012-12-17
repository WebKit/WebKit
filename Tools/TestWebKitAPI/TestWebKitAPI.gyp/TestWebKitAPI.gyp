#
# Copyright (C) 2012 Google Inc. All rights reserved.
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
        '../TestWebKitAPI.gypi',
        '../../../Source/WebKit/chromium/features.gypi',
    ],
    'targets': [
        {
            'target_name': 'TestWebKitAPI',
            'type': 'executable',
            'dependencies': [
                '<(source_dir)/WebCore/WebCore.gyp/WebCore.gyp:webcore',
                '<(source_dir)/WebKit/chromium/WebKit.gyp:webkit',
                '<(source_dir)/WTF/WTF.gyp/WTF.gyp:wtf',
                '<(chromium_src_dir)/build/temp_gyp/googleurl.gyp:googleurl',
                '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',
                '<(chromium_src_dir)/base/base.gyp:test_support_base',
                '<(chromium_src_dir)/testing/gtest.gyp:gtest',
                '<(chromium_src_dir)/testing/gmock.gyp:gmock',
                '<(chromium_src_dir)/webkit/support/webkit_support.gyp:webkit_support',
            ],
            'include_dirs': [
                '<(tools_dir)/TestWebKitAPI',
                # Needed by tests/RunAllTests.cpp, as well as ChromiumCurrentTime.cpp and
                # ChromiumThreading.cpp in chromium shared library configuration.
                '<(source_dir)/WebKit/chromium/public',
                '<(tools_dir)/TestWebKitAPI/ForwardingHeaders',
            ],
            'sources': [
                # Reuse the same testing driver of Chromium's webkit_unit_tests.
                '<(source_dir)/WebKit/chromium/tests/RunAllTests.cpp',
                '<@(TestWebKitAPI_files)',
            ],
            'conditions': [
                ['inside_chromium_build==1 and component=="shared_library"', {
                    'sources': [
                        # To satisfy linking of WTF::currentTime() etc. in shared library configuration,
                        # as the symbols are not exported from the DLLs.
                        '<(source_dir)/WebKit/chromium/src/ChromiumCurrentTime.cpp',
                        '<(source_dir)/WebKit/chromium/src/ChromiumThreading.cpp',
                    ],
                }],
                ['OS=="android" and gtest_target_type == "shared_library"', {
                    'type': 'shared_library',
                    'dependencies': [
                        '<(chromium_src_dir)/testing/android/native_test.gyp:native_test_native_code',
                    ],
                }],
            ],
        },
    ], # targets
    'conditions': [
        ['OS=="android" and gtest_target_type == "shared_library"', {
            # Wrap libTestWebKitAPI.so into an android apk for execution.
            'targets': [{
                'target_name': 'TestWebKitAPI_apk',
                'type': 'none',
                'dependencies': [
                    '<(chromium_src_dir)/base/base.gyp:base',
                    'TestWebKitAPI',
                ],
                'variables': {
                    'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)TestWebKitAPI<(SHARED_LIB_SUFFIX)',
                    'input_jars_paths': [
                        '<(PRODUCT_DIR)/lib.java/chromium_base.jar',
                    ],
                    'conditions': [
                        ['inside_chromium_build==1', {
                            'ant_build_to_chromium_src': '<(ant_build_out)/../../',
                        }, {
                            'ant_build_to_chromium_src': '<(chromium_src_dir)',
                        }],
                    ],
                },
                # Part of the following was copied from <(chromium_src_dir)/build/apk_test.gpyi.
                # Not including it because gyp include doesn't support variable in path or under
                # conditions. And we also have some different requirements.
                'actions': [{
                    'action_name': 'apk_TestWebKitAPI',
                    'message': 'Building TestWebKitAPI test apk.',
                    'inputs': [
                        '<(chromium_src_dir)/testing/android/AndroidManifest.xml',
                        '<(chromium_src_dir)/testing/android/generate_native_test.py',
                        '<(input_shlib_path)',
                        '<@(input_jars_paths)',
                    ],
                    'outputs': [
                        '<(PRODUCT_DIR)/TestWebKitAPI_apk/TestWebKitAPI-debug.apk',
                    ],
                    'action': [
                        '<(chromium_src_dir)/testing/android/generate_native_test.py',
                        '--native_library',
                        '<(input_shlib_path)',
                        '--jars',
                        '"<@(input_jars_paths)"',
                        '--output',
                        '<(PRODUCT_DIR)/TestWebKitAPI_apk',
                        '--strip-binary=<(android_strip)',
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
    ],
}
