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
