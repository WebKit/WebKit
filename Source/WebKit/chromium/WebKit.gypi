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
    'variables': {
        # List of DevTools source files, ordered by dependencies. It is used both
        # for copying them to resource dir, and for generating 'devtools.html' file.
        'devtools_js_files': [
            'src/js/DevTools.js',
            'src/js/DevToolsExtensionAPI.js',
            'src/js/Tests.js',
        ],
        'devtools_css_files': [
            'src/js/devTools.css',
        ],
        'devtools_extension_api_files': [
            '../../WebCore/inspector/front-end/ExtensionAPI.js',
            'src/js/DevToolsExtensionAPI.js'
        ],
        'devtools_image_files': [
            'src/js/Images/segmentChromium.png',
            'src/js/Images/segmentHoverChromium.png',
            'src/js/Images/segmentHoverEndChromium.png',
            'src/js/Images/segmentSelectedChromium.png',
            'src/js/Images/segmentSelectedEndChromium.png',
            'src/js/Images/statusbarBackgroundChromium.png',
            'src/js/Images/statusbarBottomBackgroundChromium.png',
            'src/js/Images/statusbarButtonsChromium.png',
            'src/js/Images/statusbarMenuButtonChromium.png',
            'src/js/Images/statusbarMenuButtonSelectedChromium.png',
        ],
        'webkit_unittest_files': [
            'tests/ArenaTestHelpers.h',
            'tests/AssociatedURLLoaderTest.cpp',
            'tests/Canvas2DLayerChromiumTest.cpp',
            'tests/CCDamageTrackerTest.cpp',
            'tests/CCDelayBasedTimeSourceTest.cpp',
            'tests/CCFrameRateControllerTest.cpp',
            'tests/CCLayerImplTest.cpp',
            'tests/CCLayerIteratorTest.cpp',
            'tests/CCLayerQuadTest.cpp',
            'tests/CCLayerSorterTest.cpp',
            'tests/CCLayerTreeHostCommonTest.cpp',
            'tests/CCLayerTreeHostImplTest.cpp',
            'tests/CCLayerTreeHostTest.cpp',
            'tests/CCLayerTreeTestCommon.h',
            'tests/CCQuadCullerTest.cpp',
            'tests/CCRenderSurfaceTest.cpp',
            'tests/CCSchedulerStateMachineTest.cpp',
            'tests/CCSchedulerTestCommon.h',
            'tests/CCSchedulerTest.cpp',
            'tests/CCTiledLayerImplTest.cpp',
            'tests/CCThreadTaskTest.cpp',
            'tests/CCTimerTest.cpp',
            'tests/CompositorFakeGraphicsContext3D.h',
            'tests/CompositorFakeWebGraphicsContext3D.h',
            'tests/FakeGraphicsContext3DTest.cpp',
            'tests/FakeWebGraphicsContext3D.h',
            'tests/FloatQuadTest.cpp',
            'tests/FrameTestHelpers.cpp',
            'tests/FrameTestHelpers.h',
            'tests/IDBBindingUtilitiesTest.cpp',
            'tests/IDBKeyPathTest.cpp',
            'tests/IDBLevelDBCodingTest.cpp',
            'tests/InnerGestureRecognizerTest.cpp',
            'tests/KeyboardTest.cpp',
            'tests/KURLTest.cpp',
            'tests/LayerChromiumTest.cpp',
            'tests/LayerTextureUpdaterTest.cpp',
            'tests/PODArenaTest.cpp',
            'tests/PODIntervalTreeTest.cpp',
            'tests/PODRedBlackTreeTest.cpp',
            'tests/RenderTableCellTest.cpp',
            'tests/TextureManagerTest.cpp',
            'tests/TiledLayerChromiumTest.cpp',
            'tests/TilingDataTest.cpp',
            'tests/TreeSynchronizerTest.cpp',
            'tests/TreeTestHelpers.cpp',
            'tests/TreeTestHelpers.h',
            'tests/WebCompositorInputHandlerImplTest.cpp',
            'tests/WebExternalTextureLayerImplTest.cpp',
            'tests/WebFrameTest.cpp',
            'tests/WebGLLayerChromiumTest.cpp',
            'tests/WebLayerTest.cpp',
            'tests/WebPageNewSerializerTest.cpp',
            'tests/WebPageSerializerTest.cpp',
            'tests/WebURLRequestTest.cpp',
            'tests/WebViewTest.cpp',
        ],

        'conditions': [
            ['use_skia == 1', {
                'webkit_unittest_files': [
                    'tests/DragImageTest.cpp',
                    'tests/ImageLayerChromiumTest.cpp',
                    'tests/PlatformContextSkiaTest.cpp',
                ],
            }],
            ['OS=="win"', {
                'webkit_unittest_files': [
                    # FIXME: Port PopupMenuTest to Linux and Mac.
                    'tests/PopupMenuTest.cpp',
                    'tests/TransparencyWinTest.cpp',
                    'tests/UniscribeHelperTest.cpp',
                    'tests/WebPageNewSerializerTest.cpp',
                    'tests/WebPageSerializerTest.cpp',
                ],
            }],
            ['OS!="mac"', {
                'webkit_unittest_files': [
                    # Mac uses ScrollAnimatorMac instead of ScrollAnimatorNone.
                    'tests/ScrollAnimatorNoneTest.cpp',
                ],
            }],
            ['toolkit_uses_gtk == 1', {
                'webkit_unittest_files': [
                    'tests/WebInputEventFactoryTestGtk.cpp',
                ],
            }],
        ],
    },
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
