#
# Copyright (C) 2010 Google Inc. All rights reserved.
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
        '../../../WebKit/chromium/features.gypi',
    ],
    'variables': {
        'webkit_top': '../../..',
        'webkit_api_dir': '<(webkit_top)/WebKit/chromium',
        'conditions': [
            # Location of the chromium src directory and target type is different
            # if webkit is built inside chromium or as standalone project.
            ['inside_chromium_build==0', {
                # DumpRenderTree is being built outside of the full chromium project.
                # e.g. via build-dumprendertree --chromium
                'chromium_src_dir': '<(webkit_api_dir)',
            },{
                # WebKit is checked out in src/chromium/third_party/WebKit
                'chromium_src_dir': '<(webkit_top)/../..',
            }],
        ],
    },
    'targets': [
        {
            'target_name': 'DumpRenderTree',
            'type': 'executable',
            'mac_bundle': 1,
            'dependencies': [
                '<(webkit_api_dir)/WebKit.gyp:webkit',
                '<(webkit_top)/JavaScriptCore/JavaScriptCore.gyp/JavaScriptCore.gyp:wtf_config',
                '<(chromium_src_dir)/third_party/npapi/npapi.gyp:npapi',
                '<(chromium_src_dir)/skia/skia.gyp:skia',
                '<(chromium_src_dir)/webkit/webkit.gyp:webkit_support',
            ],
            'include_dirs': [
                '.',
                '<(webkit_api_dir)',
                '<(webkit_top)/JavaScriptCore',
                '<(webkit_top)/WebKit/mac/WebCoreSupport', # For WebSystemInterface.h
                '<(chromium_src_dir)',
            ],
            'defines': [
                # Technically not a unit test but require functions available only to
                # unit tests.
                'UNIT_TEST',
            ],
            'sources': [
                '../chromium/AccessibilityController.cpp',
                '../chromium/AccessibilityController.h',
                '../chromium/AccessibilityUIElement.cpp',
                '../chromium/AccessibilityUIElement.h',
                '../chromium/CppBoundClass.cpp',
                '../chromium/CppBoundClass.h',
                '../chromium/CppVariant.cpp',
                '../chromium/CppVariant.h',
                '../chromium/DumpRenderTree.cpp',
                '../chromium/EventSender.cpp',
                '../chromium/EventSender.h',
                '../chromium/LayoutTestController.cpp',
                '../chromium/LayoutTestController.h',
                '../chromium/PlainTextController.cpp',
                '../chromium/PlainTextController.h',
                '../chromium/TestNavigationController.cpp',
                '../chromium/TestNavigationController.h',
                '../chromium/TestShell.cpp',
                '../chromium/TestShell.h',
                '../chromium/TextInputController.cpp',
                '../chromium/TextInputController.h',
                '../chromium/WebViewHost.cpp',
                '../chromium/WebViewHost.h',
            ],
            'conditions': [
                ['OS=="win"', {
                    'sources': [
                        '../chromium/TestShellWin.cpp',
                    ],
                }],
                ['OS=="mac"', {
                    'sources': [
                        '../chromium/TestShellMac.mm',
                    ],
                }],
            ],
            'mac_bundle_resources': [
                '../qt/fonts/AHEM____.TTF',
                '../fonts/WebKitWeightWatcher100.ttf',
                '../fonts/WebKitWeightWatcher200.ttf',
                '../fonts/WebKitWeightWatcher300.ttf',
                '../fonts/WebKitWeightWatcher400.ttf',
                '../fonts/WebKitWeightWatcher500.ttf',
                '../fonts/WebKitWeightWatcher600.ttf',
                '../fonts/WebKitWeightWatcher700.ttf',
                '../fonts/WebKitWeightWatcher800.ttf',
                '../fonts/WebKitWeightWatcher900.ttf',
            ],
        },
    ], # targets
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
