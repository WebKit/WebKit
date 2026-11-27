# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import re
import unittest

from . import relevance
from .relevance import Relevance


def changes(*files):
    return '\n'.join(files).encode()


class RelevanceTestCase(unittest.TestCase):
    rule = None

    def assertRelevant(self, *files):
        self.assertTrue(self.rule.is_relevant(changes(*files)), files)

    def assertIrrelevant(self, *files):
        self.assertFalse(self.rule.is_relevant(changes(*files)), files)


class TestRelevance(unittest.TestCase):
    def test_requires_paths(self):
        with self.assertRaises(ValueError):
            Relevance()

    def test_accepts_str_and_bytes(self):
        rule = Relevance(include=[re.compile(rb'^Source/')])
        self.assertTrue(rule.is_relevant('+++ Source/WebCore/dom/Node.cpp'))
        self.assertTrue(rule.is_relevant(b'+++ Source/WebCore/dom/Node.cpp'))

    def test_empty_change_is_relevant(self):
        self.assertTrue(Relevance(include=[re.compile(rb'^Source/')]).is_relevant(b''))
        self.assertTrue(Relevance(exclude=[re.compile(rb'^docs/')]).is_relevant(b''))

    def test_include_matches_removed_files_by_path(self):
        rule = Relevance(include=[re.compile(rb'^Source/')])
        self.assertTrue(rule.is_relevant(b'--- Source/WebCore/dom/Node.cpp'))
        self.assertFalse(rule.is_relevant(b'--- docs/old.md'))

    def test_exclude_always_runs_removed_files(self):
        # GitHub lists removals and renames as '--- path', and a rename only keeps the new path.
        rule = Relevance(exclude=[re.compile(rb'^docs/')])
        self.assertTrue(rule.is_relevant(b'--- docs/old.md'))
        self.assertTrue(rule.is_relevant(b'+++ docs/other.md\n--- docs/moved.md'))

    def test_include_and_exclude(self):
        rule = Relevance(include=[re.compile(rb'^Source/')], exclude=[re.compile(rb'\.md$')])
        self.assertTrue(rule.is_relevant(b'+++ Source/WebCore/dom/Node.cpp'))
        self.assertFalse(rule.is_relevant(b'+++ Source/WebCore/README.md'))
        self.assertFalse(rule.is_relevant(b'+++ Tools/Scripts/foo.py'))


class TestBuildRelevance(RelevanceTestCase):
    rule = relevance.BUILD

    def test_changes_without_build_inputs_are_skipped(self):
        for files in [
            ('+++ Websites/webkit.org/_posts/2026-10-19-foo.md',),
            ('+++ docs/porting-webkit.md', '+++ docs/images/diagram.png'),
            ('+++ metadata/contributors.json',),
            ('+++ Tools/CISupport/ews-build/steps.py', '+++ Tools/CISupport/build-webkit-org/loadConfig.py', '+++ Tools/CISupport/ews-app/ews/views/status.py'),
            ('+++ README.md', '+++ JSTests/README.md', '+++ LayoutTests/fast/README.md'),
            ('+++ Websites/webkit.org/foo.html', '+++ docs/bar.md', '+++ metadata/contributors.json'),
        ]:
            self.assertIrrelevant(*files)

    def test_changes_with_build_inputs_need_a_build(self):
        for files in [
            ('+++ Source/WebCore/dom/Node.cpp',),
            ('+++ docs/a.md', '+++ Source/WebCore/dom/Node.cpp'),
            ('+++ Tools/CISupport/kill-old-processes',),
            ('+++ Tools/CISupport/built-product-archive',),
            ('+++ Tools/CISupport/Shared/steps.py',),
            ('+++ Source/WebKit/WPEPlatform/docs/overview.md',),
            ('+++ Tools/Scripts/docs/foo.py',),
            ('+++ SomeNewTopLevelDir/foo.txt',),
            ('+++ LayoutTests/fast/dom/foo.html',),
        ]:
            self.assertRelevant(*files)

    def test_removed_or_renamed_files_need_a_build(self):
        self.assertRelevant('--- docs/old.md')
        self.assertRelevant('+++ docs/other.md', '--- docs/moved.md')


class TestJSCRelevance(RelevanceTestCase):
    rule = relevance.JSC

    def test_relevant(self):
        for file_name in [
            'JSTests/stress/foo.js', 'Source/JavaScriptCore/runtime/JSObject.cpp', 'Source/WTF/wtf/Vector.h',
            'Source/bmalloc/bmalloc/Heap.cpp', 'Makefile', 'Source/Makefile.shared', 'Tools/Scripts/build-webkit',
            'Tools/Scripts/build-jsc', 'Tools/Scripts/run-jsc-stress-tests', 'Tools/Scripts/run-javascriptcore-tests',
            'Tools/Scripts/webkitdirs.pm', 'Source/cmake/OptionsJSCOnly.cmake', 'LayoutTests/js/dom/foo.html',
        ]:
            self.assertRelevant(f'+++ {file_name}')

    def test_irrelevant(self):
        self.assertIrrelevant('+++ Source/WebCore/dom/Node.cpp', '+++ LayoutTests/fast/dom/foo.html')


class TestWK1TestsRelevance(RelevanceTestCase):
    rule = relevance.WK1_TESTS

    def test_relevant(self):
        for file_name in [
            'Source/WebKitLegacy/mac/WebView/WebView.mm', 'Source/WebCore/dom/Node.cpp', 'Source/WebInspectorUI/foo.js',
            'Source/WebDriver/Session.cpp', 'Source/WTF/wtf/Vector.h', 'Source/bmalloc/bmalloc/Heap.cpp',
            'Source/JavaScriptCore/runtime/JSObject.cpp', 'Source/ThirdParty/ANGLE/foo.cpp', 'LayoutTests/fast/dom/foo.html',
            'Tools/Scripts/run-webkit-tests',
        ]:
            self.assertRelevant(f'+++ {file_name}')

    def test_irrelevant(self):
        self.assertIrrelevant('+++ Source/WebKit/UIProcess/WebPageProxy.cpp', '+++ JSTests/stress/foo.js')


class TestWebKitPyTestsRelevance(RelevanceTestCase):
    rule = relevance.WEBKITPY_TESTS

    def test_relevant(self):
        for file_name in [
            'Tools/Scripts/webkitpy/common/foo.py', 'Tools/Scripts/libraries/webkitscmpy/foo.py',
            'Tools/Scripts/commit-log-editor', 'Source/WebKit/Scripts/generate-message-receiver.py', 'metadata/contributors.json',
        ]:
            self.assertRelevant(f'+++ {file_name}')

    def test_irrelevant(self):
        self.assertIrrelevant('+++ Source/WebCore/dom/Node.cpp', '+++ Tools/CISupport/ews-build/steps.py')


class TestServicesRelevance(RelevanceTestCase):
    rule = relevance.SERVICES

    def test_relevant(self):
        for file_name in [
            'Tools/CISupport/build-webkit-org/steps.py', 'Tools/CISupport/ews-build/steps.py', 'Tools/CISupport/Shared/steps.py',
            'Tools/Scripts/libraries/resultsdbpy/foo.py', 'Tools/Scripts/libraries/webkitcorepy/foo.py',
            'Tools/Scripts/libraries/webkitscmpy/foo.py', 'Tools/Scripts/webkitpy/autoinstalled/twisted.py',
        ]:
            self.assertRelevant(f'+++ {file_name}')

    def test_irrelevant(self):
        self.assertIrrelevant('+++ Source/WebCore/dom/Node.cpp', '+++ Tools/Scripts/webkitpy/common/foo.py')


class TestSaferCPPRelevance(RelevanceTestCase):
    rule = relevance.SAFER_CPP

    def test_relevant(self):
        for file_name in [
            'Source/WebKit/UIProcess/WebPageProxy.cpp', 'Tools/Scripts/build-and-analyze',
            'Tools/CISupport/Shared/download-and-install-build-tools', 'Tools/CISupport/safer-cpp-swift-version',
        ]:
            self.assertRelevant(f'+++ {file_name}')

    def test_irrelevant(self):
        self.assertIrrelevant('+++ LayoutTests/fast/dom/foo.html', '+++ Tools/Scripts/run-webkit-tests')


class TestBindingsTestsRelevance(RelevanceTestCase):
    rule = relevance.BINDINGS_TESTS

    def test_relevant(self):
        self.assertRelevant('+++ Source/WebCore/bindings/scripts/CodeGeneratorJS.pm')
        self.assertRelevant('+++ Tools/Scripts/run-bindings-tests')
        self.assertRelevant('+++ Source/WebKit/WebProcess/Extensions/Bindings/Scripts/CodeGeneratorExtensions.pm')

    def test_irrelevant(self):
        self.assertIrrelevant('+++ Source/WebKit/UIProcess/WebPageProxy.cpp', '+++ LayoutTests/fast/dom/foo.html')


class TestWebDriverTestsRelevance(RelevanceTestCase):
    rule = relevance.WEBDRIVER_TESTS

    def test_relevant(self):
        for file_name in [
            'Source/WebDriver/Session.cpp',
            'Tools/minidriver/minidriver.xcodeproj/project.pbxproj',
            'Source/WebKit/UIProcess/Automation/WebAutomationSession.cpp',
            'Source/WebKit/WebProcess/Automation/WebAutomationSessionProxy.cpp',
            'Source/WebKit/Shared/Automation/AutomationProtocolObjects.h',
            'Source/WebCore/automation/AutomationInstrumentation.cpp',
            'Source/WebCore/Modules/webdriver/NavigatorWebDriver.cpp',
            'Tools/MiniBrowser/mac/AutomationConnection.m',
            'Tools/MiniBrowser/MiniBrowser.xcodeproj/project.pbxproj',
            'WebDriverTests/TestExpectations.json',
            'Tools/Scripts/webkitpy/webdriver_tests/webdriver_test_runner.py',
            'Tools/Scripts/run-webdriver-tests',
        ]:
            self.assertRelevant('+++ Tools/ChangeLog', f'+++ {file_name}')

    def test_irrelevant(self):
        for file_name in [
            'Source/WebKit/UIProcess/WebPageProxy.cpp',
            'Source/WebCore/dom/Document.cpp',
            'Tools/MiniBrowser/gtk/main.c',
            'Tools/Scripts/run-webkit-tests',
            'LayoutTests/imported/w3c/web-platform-tests/webdriver/tests/support/helpers.py',
        ]:
            self.assertIrrelevant(f'+++ {file_name}')


class TestPortRelevance(unittest.TestCase):
    def ports(self, path):
        return relevance.ports_for_path(path.encode())

    def assertPorts(self, ports, *paths):
        for path in paths:
            self.assertEqual(self.ports(path), frozenset(ports), path)

    def test_unknown_port(self):
        with self.assertRaises(ValueError):
            relevance.PortRelevance('android')

    def test_apple_directories(self):
        self.assertPorts(
            {relevance.APPLE},
            'Source/WebCore/platform/mac/ScrollbarThemeMac.mm',
            'Source/WebKit/UIProcess/ios/WKContentView.mm',
            'Source/WebKit/UIProcess/API/Cocoa/WKWebView.mm',
            'Source/WebKitLegacy/mac/WebView/WebView.mm',
            'Tools/TestWebKitAPI/Tests/WebKitCocoa/cocoa/Foo.mm',
        )

    def test_non_apple_directories(self):
        self.assertPorts(
            relevance.NON_APPLE_PORTS,
            'Source/WebKit/UIProcess/API/glib/WebKitWebView.cpp',
            'Source/WebKit/UIProcess/API/gtk/WebKitWebViewBase.cpp',
            'Source/WebKit/WPEPlatform/wpe/WPEDisplay.cpp',
            'Source/WebCore/platform/graphics/gstreamer/MediaPlayerPrivateGStreamer.cpp',
            'Source/WebCore/platform/network/soup/ResourceHandleSoup.cpp',
            'Source/WebCore/platform/graphics/skia/GraphicsContextSkia.cpp',
            'Source/WebCore/platform/graphics/texmap/TextureMapper.cpp',
            'Source/WebCore/platform/network/curl/CurlContext.cpp',
            'Source/WebCore/platform/libwpe/PlatformKeyboardEventLibWPE.cpp',
            'Source/WebCore/platform/adwaita/ThemeAdwaita.cpp',
            'Source/JavaScriptCore/API/glib/JSCContext.cpp',
            'Tools/wpe/backends/HeadlessViewBackend.cpp',
            'Tools/MiniBrowser/gtk/main.c',
        )

    def test_windows_and_playstation_directories(self):
        self.assertPorts({relevance.WIN}, 'Source/WebKit/UIProcess/win/WebView.cpp', 'Source/WTF/wtf/win/RunLoopWin.cpp')
        self.assertPorts({relevance.PLAYSTATION}, 'Source/WebKit/UIProcess/playstation/WebPageProxyPlayStation.cpp', 'Tools/wpe/backends/playstation/ViewBackend.cpp')

    def test_nested_directories(self):
        self.assertPorts({relevance.WIN}, 'Source/WebCore/platform/graphics/skia/win/FontSkiaWin.cpp')
        self.assertPorts(relevance.ALL_PORTS, 'Source/WebCore/platform/glib/cocoa/Unexpected.cpp')

    def test_shared_paths_affect_every_port(self):
        self.assertPorts(
            relevance.ALL_PORTS,
            'Source/WebCore/dom/Node.cpp',
            'Source/WTF/wtf/posix/FileSystemPOSIX.cpp',
            'Source/WebKit/Platform/unix/EnvironmentUtilities.cpp',
            'Source/WebCore/platform/generic/KeyedDecoderGeneric.cpp',
            'Source/cmake/OptionsGTK.cmake',
            'LayoutTests/fast/dom/mac/foo.html',
            'JSTests/stress/gtk/foo.js',
            'Source/WebCore/platform/gtk',
            'mac/README',
        )

    def test_paths_copied_as_a_whole_by_apple(self):
        self.assertPorts(relevance.ALL_PORTS, 'Tools/TestWebKitAPI/Resources/glib/new-resource.html')
        self.assertPorts(relevance.NON_APPLE_PORTS, 'Tools/TestWebKitAPI/Tests/WTF/glib/GRefPtr.cpp')

    def test_third_party_only_uses_library_directory(self):
        self.assertPorts(relevance.ALL_PORTS, 'Source/WebCore/PAL/ThirdParty/dav1d/config/win/config.h')
        self.assertPorts(relevance.NON_APPLE_PORTS, 'Source/ThirdParty/skia/src/core/SkCanvas.cpp', 'Source/ThirdParty/skia/src/ports/mac/SkFontMgr_mac.cpp')
        self.assertPorts(
            relevance.ALL_PORTS,
            'Source/ThirdParty/libwebrtc/Source/third_party/libvpx/source/config/linux/arm64-highbd/vpx_config.h',
            'Source/ThirdParty/ANGLE/src/libANGLE/renderer/gl/glx/DisplayGLX.cpp',
            'Source/ThirdParty/ANGLE/src/common/apple/foo.h',
            'Source/ThirdParty/libwebrtc/Source/webrtc/modules/audio_device/mac/audio_device_mac.cc',
        )

    def test_layout_tests_platform(self):
        self.assertPorts(
            {relevance.APPLE},
            'LayoutTests/platform/mac/TestExpectations',
            'LayoutTests/platform/mac-sequoia-wk2/fast/foo-expected.txt',
            'LayoutTests/platform/ios-simulator/TestExpectations',
            'LayoutTests/platform/ipad/TestExpectations',
            'LayoutTests/platform/visionos-26/TestExpectations',
        )
        self.assertPorts(
            relevance.NON_APPLE_PORTS,
            'LayoutTests/platform/glib/TestExpectations',
            'LayoutTests/platform/gtk/fast/foo-expected.txt',
            'LayoutTests/platform/gtk3/TestExpectations',
            'LayoutTests/platform/wpe-legacy-api/TestExpectations',
        )
        self.assertPorts({relevance.WIN}, 'LayoutTests/platform/win/TestExpectations', 'LayoutTests/platform/win-wk2/TestExpectations')
        self.assertPorts(
            relevance.ALL_PORTS,
            'LayoutTests/platform/wk2/TestExpectations',
            'LayoutTests/platform/machine/TestExpectations',
            'LayoutTests/TestExpectations',
        )

    def test_is_relevant(self):
        gtk = relevance.PortRelevance(relevance.GTK)
        apple = relevance.PortRelevance(relevance.APPLE)
        glib_change = changes('+++ Source/WebKit/UIProcess/API/glib/WebKitWebView.cpp')
        self.assertTrue(gtk.is_relevant(glib_change))
        self.assertFalse(apple.is_relevant(glib_change))
        self.assertTrue(apple.is_relevant(glib_change + b'\n+++ Source/WebKit/UIProcess/ios/WKContentView.mm'))
        self.assertTrue(apple.is_relevant('+++ Source/WebKit/UIProcess/API/glib/WebKitWebView.cpp\n+++ Source/WebCore/dom/Node.cpp'))
        self.assertTrue(apple.is_relevant(b''))

    def test_possibly_truncated_file_list_is_relevant(self):
        rule = relevance.PortRelevance(relevance.GTK)
        baselines = [f'+++ LayoutTests/platform/mac/fast/test{i}-expected.txt' for i in range(relevance.RelevanceRule.POSSIBLY_TRUNCATED_SIZE)]
        self.assertFalse(rule.is_relevant('\n'.join(baselines[:-1])))
        self.assertTrue(rule.is_relevant('\n'.join(baselines)))
        self.assertTrue(relevance.WEBKITPY_TESTS.is_relevant('\n'.join(baselines)))

    def test_removed_files_affect_every_port(self):
        self.assertTrue(relevance.PortRelevance(relevance.APPLE).is_relevant(b'--- Source/WebKit/UIProcess/gtk/Removed.cpp'))

    def test_port_for_platform(self):
        for platform, port in [
            ('mac-sequoia', relevance.APPLE), ('mac-golden-gate', relevance.APPLE), ('ios-26', relevance.APPLE),
            ('ios-simulator-27', relevance.APPLE), ('watchos-simulator-27', relevance.APPLE), ('tvos-27', relevance.APPLE),
            ('visionos-27', relevance.APPLE), ('gtk', relevance.GTK), ('wpe', relevance.WPE), ('win', relevance.WIN),
            ('playstation', relevance.PLAYSTATION), ('*', None), ('jsc-only', None), (None, None),
        ]:
            self.assertEqual(relevance.port_for_platform(platform), port, platform)

    def test_narrowed_to_platform(self):
        self.assertIsNone(relevance.narrowed_to_platform(None, 'gtk'))
        self.assertIs(relevance.narrowed_to_platform(relevance.BUILD, '*'), relevance.BUILD)
        rule = relevance.narrowed_to_platform(relevance.BUILD, 'mac-sequoia')
        self.assertFalse(rule.is_relevant(b'+++ Source/WebCore/platform/graphics/gstreamer/GStreamerCommon.cpp'))
        self.assertFalse(rule.is_relevant(b'+++ docs/foo.md'))
        self.assertTrue(rule.is_relevant(b'+++ Source/WebCore/platform/mac/ScrollbarThemeMac.mm'))
