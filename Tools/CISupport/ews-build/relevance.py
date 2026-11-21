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

# Which changed files make a queue worth running. Each factory picks one of the rules
# at the bottom of this file through its changeRelevance attribute; factories without
# one run on every change.

import re


class Relevance(object):
    MAX_LINE_SIZE = 250

    def __init__(self, include=None, exclude=None):
        if not include and not exclude:
            raise ValueError('A relevance rule needs include or exclude paths')
        self.include = list(include or [])
        self.exclude = list(exclude or [])

    def _is_relevant_line(self, line):
        # EWS lists changed files as '+++ path' (added or modified) or '--- path' (removed or renamed).
        if line.startswith(b'--- ') and self.exclude:
            # A rename only lists the new path, so we can't know the old path was excluded.
            return True
        if line.startswith((b'+++ ', b'--- ')):
            line = line[4:]
        path = line[:self.MAX_LINE_SIZE]
        if self.include and not any(regex.search(path) for regex in self.include):
            return False
        return not any(regex.search(path) for regex in self.exclude)

    def is_relevant(self, changed_files):
        if isinstance(changed_files, str):
            changed_files = changed_files.encode('utf-8', errors='replace')
        lines = changed_files.splitlines()
        if not lines:
            # Without a list of files we can't tell, so run.
            return True
        return any(self._is_relevant_line(line) for line in lines)


# Changes that only touch these paths have no build inputs.
NO_BUILD_INPUTS = [
    re.compile(rb'^Websites/', re.IGNORECASE),
    re.compile(rb'^docs/', re.IGNORECASE),
    re.compile(rb'^metadata/', re.IGNORECASE),
    re.compile(rb'^Tools/CISupport/(ews-build|build-webkit-org|ews-app)/', re.IGNORECASE),
    re.compile(rb'^(?!Source/).*\.md$', re.IGNORECASE),
]

BUILD = Relevance(exclude=NO_BUILD_INPUTS)

BINDINGS_TESTS = Relevance(include=[
    re.compile(rb'Source/WebCore', re.IGNORECASE),
    re.compile(rb'Tools', re.IGNORECASE),
])

SERVICES = Relevance(include=[
    re.compile(rb'Tools/CISupport/build-webkit-org', re.IGNORECASE),
    re.compile(rb'Tools/CISupport/ews-build', re.IGNORECASE),
    re.compile(rb'Tools/CISupport/Shared', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/libraries/resultsdbpy', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/libraries/webkitcorepy', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/libraries/webkitscmpy', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/webkitpy/autoinstalled', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/webkitpy/__init__.py', re.IGNORECASE),
])

JSC = Relevance(include=[
    re.compile(rb'.*javascriptcore.*', re.IGNORECASE),
    re.compile(rb'JSTests/', re.IGNORECASE),
    re.compile(rb'LayoutTests/js.*', re.IGNORECASE),  # This catches both js/ and jsc-layout-tests.yaml
    re.compile(rb'Source/WTF/', re.IGNORECASE),
    re.compile(rb'Source/bmalloc/', re.IGNORECASE),
    re.compile(rb'Source/cmake/', re.IGNORECASE),
    re.compile(rb'.*Makefile.*', re.IGNORECASE),
    re.compile(rb'Tools/.*jsc.*', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/build-webkit', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/webkitdirs.pm', re.IGNORECASE),
])

WK1_TESTS = Relevance(include=[
    re.compile(rb'Source/WebKitLegacy', re.IGNORECASE),
    re.compile(rb'Source/WebCore', re.IGNORECASE),
    re.compile(rb'Source/WebInspectorUI', re.IGNORECASE),
    re.compile(rb'Source/WebDriver', re.IGNORECASE),
    re.compile(rb'Source/WTF', re.IGNORECASE),
    re.compile(rb'Source/bmalloc', re.IGNORECASE),
    re.compile(rb'Source/JavaScriptCore', re.IGNORECASE),
    re.compile(rb'Source/ThirdParty', re.IGNORECASE),
    re.compile(rb'LayoutTests', re.IGNORECASE),
    re.compile(rb'Tools', re.IGNORECASE),
])

WEBKITPY_TESTS = Relevance(include=[
    re.compile(rb'Tools/Scripts/webkitpy', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/libraries', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/commit-log-editor', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/test262', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/webkit-sysprof', re.IGNORECASE),
    re.compile(rb'Source/WebKit/Scripts', re.IGNORECASE),
    re.compile(rb'metadata/contributors.json', re.IGNORECASE),
])

SAFER_CPP = Relevance(include=[
    re.compile(rb'Source/WebCore', re.IGNORECASE),
    re.compile(rb'Source/WebDriver', re.IGNORECASE),
    re.compile(rb'Source/WebGPU', re.IGNORECASE),
    re.compile(rb'Source/WebInspectorUI', re.IGNORECASE),
    re.compile(rb'Source/WebKit', re.IGNORECASE),
    re.compile(rb'Source/WebKitLegacy', re.IGNORECASE),
    re.compile(rb'Source/WTF', re.IGNORECASE),
    re.compile(rb'Source/JavaScriptCore', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/build-and-analyze', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/generate-dirty-files', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/compare-static-analysis-results', re.IGNORECASE),
    re.compile(rb'Tools/CISupport/Shared/download-and-install-build-tools', re.IGNORECASE),
    re.compile(rb'Tools/CISupport/safer-cpp-swift-version', re.IGNORECASE),
])

WEBDRIVER_TESTS = Relevance(include=[
    re.compile(rb'Source/WebDriver/', re.IGNORECASE),
    re.compile(rb'Tools/minidriver/', re.IGNORECASE),
    re.compile(rb'Source/WebKit/UIProcess/Automation/', re.IGNORECASE),
    re.compile(rb'Source/WebKit/WebProcess/Automation/', re.IGNORECASE),
    re.compile(rb'Source/WebKit/Shared/Automation/', re.IGNORECASE),
    re.compile(rb'Source/WebCore/automation/', re.IGNORECASE),
    re.compile(rb'Source/WebCore/Modules/webdriver/', re.IGNORECASE),
    re.compile(rb'Tools/MiniBrowser/mac/', re.IGNORECASE),
    re.compile(rb'Tools/MiniBrowser/MiniBrowser.xcodeproj', re.IGNORECASE),
    re.compile(rb'WebDriverTests/', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/webkitpy/webdriver_tests/', re.IGNORECASE),
    re.compile(rb'Tools/Scripts/run-webdriver-tests', re.IGNORECASE),
])
