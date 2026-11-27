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
# one run on every change. Queues for a single port are further narrowed to changes that
# can affect that port, through narrowed_to_platform().

import re


class RelevanceRule(object):
    MAX_LINE_SIZE = 250

    # The changed files come from GitHub's pull request API, which events.py reads 100 files at a
    # time for at most 29 pages. A list this long may be missing files, so it is always relevant.
    POSSIBLY_TRUNCATED_SIZE = 2900

    def _is_relevant_line(self, line):
        raise NotImplementedError

    def is_relevant(self, changed_files):
        # EWS lists changed files as '+++ path' (added or modified) or '--- path' (removed or renamed).
        if isinstance(changed_files, str):
            changed_files = changed_files.encode('utf-8', errors='replace')
        lines = changed_files.splitlines()
        if not lines or len(lines) >= self.POSSIBLY_TRUNCATED_SIZE:
            # Without the full list of files we can't tell, so run.
            return True
        return any(self._is_relevant_line(line) for line in lines)


class Relevance(RelevanceRule):
    def __init__(self, include=None, exclude=None):
        if not include and not exclude:
            raise ValueError('A relevance rule needs include or exclude paths')
        self.include = list(include or [])
        self.exclude = list(exclude or [])

    def _is_relevant_line(self, line):
        if line.startswith(b'--- ') and self.exclude:
            # A rename only lists the new path, so we can't know the old path was excluded.
            return True
        if line.startswith((b'+++ ', b'--- ')):
            line = line[4:]
        path = line[:self.MAX_LINE_SIZE]
        if self.include and not any(regex.search(path) for regex in self.include):
            return False
        return not any(regex.search(path) for regex in self.exclude)


# Ports, as far as relevance goes. Apple platforms share too much code to be told apart by path.
APPLE = 'apple'
GTK = 'gtk'
WPE = 'wpe'
WIN = 'win'
PLAYSTATION = 'playstation'

ALL_PORTS = frozenset({APPLE, GTK, WPE, WIN, PLAYSTATION})
NON_APPLE_PORTS = ALL_PORTS - {APPLE}

# Directory names only say which ports build a file under these roots. Elsewhere, like in
# LayoutTests/ or JSTests/, a directory called mac/ or gtk/ may hold tests or data for every port.
SOURCE_ROOTS = frozenset({b'Source', b'Tools'})

# Paths that affect every port even though they contain directories listed below.
# Tools/TestWebKitAPI/Resources is copied as a whole folder by the Apple Xcode build, which only
# leaves out the files it explicitly lists. A new file in Resources/glib/ is therefore copied on Apple too.
ALL_PORTS_PATHS = (
    b'Tools/TestWebKitAPI/Resources/',
)

# Ports that may build or test files in a directory with this name. A file nested in several of
# these directories affects the ports they have in common.
#
# Apple directories never build on other ports, and non-Apple directories never build on Apple ports,
# since each side's code depends on libraries the other side doesn't have. Library directories like
# skia/, libwpe/ or adwaita/ map to every non-Apple port, rather than to the ports using them today,
# because non-Apple ports adopt each other's libraries from time to time. For the same reason, gtk/
# and wpe/ map to every non-Apple port too, so changes to one of them still run the other's queues.
#
# posix/, unix/ and generic/ aren't listed: they hold portable code that Apple, Windows or PlayStation
# also build.
DIRECTORY_PORTS = {
    **{name: frozenset({APPLE}) for name in (b'mac', b'ios', b'cocoa', b'Cocoa')},
    **{name: NON_APPLE_PORTS for name in (
        b'gtk', b'wpe', b'WPEPlatform', b'glib', b'gstreamer', b'soup', b'atspi', b'linux', b'wayland', b'gbm',
        b'drm', b'manette', b'x11', b'skia', b'texmap', b'coordinated', b'CoordinatedGraphics', b'nicosia', b'cairo',
        b'adwaita', b'curl', b'libwpe',
    )},
    b'win': frozenset({WIN}),
    b'playstation': frozenset({PLAYSTATION}),
}

# LayoutTests/platform/<name> is only read by the ports whose baseline search path includes it.
# Anything not listed here, like wk2, is read by several port families.
LAYOUT_TESTS_PLATFORM_PORTS = [
    (re.compile(rb'^(mac|ios|ipad|iphone|visionos|watchos|tvos)(-|$)'), frozenset({APPLE})),
    (re.compile(rb'^(gtk\d*|wpe|glib)(-|$)'), NON_APPLE_PORTS),
    (re.compile(rb'^win(-|$)'), frozenset({WIN})),
]

# The port of each EWS queue, from the queue's platform in config-template.yaml.
PLATFORM_PORTS = [
    (re.compile(r'^(mac|ios|watchos|tvos|visionos)(-|$)'), APPLE),
    (re.compile(r'^gtk$'), GTK),
    (re.compile(r'^wpe$'), WPE),
    (re.compile(r'^win$'), WIN),
    (re.compile(r'^playstation$'), PLAYSTATION),
]


def ports_for_path(path):
    parts = path.split(b'/')
    if len(parts) > 3 and parts[0] == b'LayoutTests' and parts[1] == b'platform':
        for regex, ports in LAYOUT_TESTS_PLATFORM_PORTS:
            if regex.match(parts[2]):
                return ports
        return ALL_PORTS
    if parts[0] not in SOURCE_ROOTS or path.startswith(ALL_PORTS_PATHS):
        return ALL_PORTS
    directories = parts[:-1]
    if b'ThirdParty' in directories:
        # Third-party code follows its upstream's layout, where platform names don't mean the same.
        # For example, Apple builds of libwebrtc use libvpx's config/linux/ headers. Only the library
        # directory itself counts.
        directories = directories[:directories.index(b'ThirdParty') + 2]
    ports = ALL_PORTS
    for directory in directories:
        ports = ports & DIRECTORY_PORTS.get(directory, ALL_PORTS)
    # Directories of different ports nested in each other, like cocoa/ inside glib/, are unexpected.
    return ports or ALL_PORTS


class PortRelevance(RelevanceRule):
    # Relevant if some changed file can affect the given port.
    def __init__(self, port):
        if port not in ALL_PORTS:
            raise ValueError(f'Unknown port {port}')
        self.port = port

    def _is_relevant_line(self, line):
        if line.startswith(b'--- '):
            # A rename only lists the new path, so we can't know which ports the old path affected.
            return True
        if line.startswith(b'+++ '):
            line = line[4:]
        return self.port in ports_for_path(line[:self.MAX_LINE_SIZE])


class AllOf(object):
    def __init__(self, *rules):
        self.rules = rules

    def is_relevant(self, changed_files):
        return all(rule.is_relevant(changed_files) for rule in self.rules)


def port_for_platform(platform):
    for regex, port in PLATFORM_PORTS:
        if regex.match(platform or ''):
            return port
    return None


def narrowed_to_platform(rule, platform):
    # A queue's rule, restricted to changes that can affect the port of its platform. Queues that
    # run on any platform ('*'), or on one without a port above, keep their rule as is.
    port = port_for_platform(platform)
    if not rule or not port:
        return rule
    return AllOf(rule, PortRelevance(port))


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
    re.compile(rb'Source/WebKit/WebProcess/Extensions/Bindings', re.IGNORECASE),
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
