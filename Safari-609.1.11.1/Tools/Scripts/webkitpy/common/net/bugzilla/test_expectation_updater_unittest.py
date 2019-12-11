#!/usr/bin/env python

# Copyright (C) 2017 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.net.bugzilla.test_expectation_updater import TestExpectationUpdater
from webkitpy.common.net.bugzilla.attachment import Attachment

FAKE_FILES = {
    '/tests/csswg/css-fake-1/empty_dir/README.txt': '',
    '/mock-checkout/LayoutTests/w3c/css-fake-1/README.txt': '',
}


class MockAttachment(Attachment):
    def __init__(self, attachment_dictionary, contents):
        Attachment.__init__(self, attachment_dictionary, self)
        self._contents = contents

    def contents(self):
        return self._contents


class MockBugzilla():
    def __init__(self, include_wk2=True, include_win_future=False):
        self._include_wk2 = include_wk2
        self._include_win_future = include_win_future

    def fetch_bug(self, id):
        return self

    def attachments(self):
        attachments = []
        if self._include_wk2:
            attachments.append(MockAttachment({"id": 1, "name": "Archive of layout-test-results from ews103 for mac-elcapitan-wk2"}, "mac-wk2"))
        attachments.append(MockAttachment({"id": 2, "name": "Archive of layout-test-results from ews103 for mac-elcapitan"}, "mac-wk1a"))
        attachments.append(MockAttachment({"id": 3, "name": "Archive of layout-test-results from ews104 for mac-elcapitan"}, "mac-wk1b"))
        attachments.append(MockAttachment({"id": 4, "name": "Archive of layout-test-results from ews122 for ios-simulator-wk2"}, "ios-sim"))
        if self._include_win_future:
            attachments.append(MockAttachment({"id": 5, "name": "Archive of layout-test-results from ews124 for win-future"}, "win-future"))
        return attachments


class MockZip():
    def __init__(self):
        self.content = None
        mac_wk2_files = {
            "full_results.json": 'ADD_RESULTS({"tests":{"http":{"tests":{"media":{"hls":{"video-controls-live-stream.html":{"report":"FLAKY","expected":"PASS","actual":"TEXT PASS"},"video-duration-accessibility.html":{"report":"FLAKY","expected":"PASS","actual":"TEXT PASS"}}}}},"imported":{"w3c":{"web-platform-tests":{"html":{"browsers":{"windows":{"browsing-context.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}},"fetch":{"api":{"redirect":{"redirect-count.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"redirect-location.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"redirect-count-worker.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"redirect-count-cross-origin.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"redirect-location-worker.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}}}}},"media":{"track":{"track-in-band-style.html":{"report":"FLAKY","expected":"PASS","actual":"TEXT PASS"}}}},"skipped":3348,"num_regressions":6,"other_crashes":{},"interrupted":false,"num_missing":0,"layout_tests_dir":"/Volumes/Data/EWS/WebKit/LayoutTests","version":4,"num_passes":44738,"pixel_tests_enabled":false,"date":"07:18PM on April 08, 2017","has_pretty_patch":true,"fixable":3357,"num_flaky":3,"uses_expectations_file":true});',
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-cross-origin-actual.txt": "a",
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-worker-actual.txt": "b",
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-actual.txt": "c",
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-worker-actual.txt": "d",
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-actual.txt": "e",
            "imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-actual.txt": "f"}
        mac_wk1a_files = {"full_results.json": 'ADD_RESULTS({"tests":{"http":{"tests":{"loading":{"resourceLoadStatistics":{"non-prevalent-resource-without-user-interaction.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}}}},"imported":{"w3c":{"web-platform-tests":{"IndexedDB":{"abort-in-initial-upgradeneeded.html":{"report":"FLAKY","expected":"PASS","actual":"TEXT PASS"}},"html":{"browsers":{"windows":{"browsing-context.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}},"fetch":{"api":{"redirect":{"redirect-count.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"redirect-location.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"redirect-count-worker.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"redirect-count-cross-origin.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"redirect-location-worker.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}}},"IndexedDB-private-browsing":{"idbfactory_open9.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}},"blink":{"storage":{"indexeddb":{"blob-delete-objectstore-db.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}}}}},"skipped":3537,"num_regressions":6,"other_crashes":{},"interrupted":false,"num_missing":0,"layout_tests_dir":"/Volumes/Data/EWS/WebKit/LayoutTests","version":4,"num_passes":44561,"pixel_tests_enabled":false,"date":"07:18PM on April 08, 2017","has_pretty_patch":true,"fixable":3547,"num_flaky":4,"uses_expectations_file":true});',
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-cross-origin-actual.txt": "a",
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-worker-actual.txt": "b",
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-actual.txt": "c",
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-worker-actual.txt": "d",
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-actual.txt": "e",
            "imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-actual.txt": "f-wk1a"}
        mac_wk1b_files = {"full_results.json": 'ADD_RESULTS({"tests":{"http":{"tests":{"loading":{"resourceLoadStatistics":{"non-prevalent-resource-without-user-interaction.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}}}},"imported":{"w3c":{"web-platform-tests":{"IndexedDB":{"abort-in-initial-upgradeneeded.html":{"report":"FLAKY","expected":"PASS","actual":"TEXT PASS"}},"html":{"browsers":{"windows":{"browsing-context.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}},"fetch":{"api":{"redirect":{"redirect-count.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"redirect-location.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"redirect-count-worker.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"redirect-count-cross-origin.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"redirect-location-worker.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}}},"IndexedDB-private-browsing":{"idbfactory_open9.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}},"blink":{"storage":{"indexeddb":{"blob-delete-objectstore-db.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}}}}},"skipped":3537,"num_regressions":6,"other_crashes":{},"interrupted":false,"num_missing":0,"layout_tests_dir":"/Volumes/Data/EWS/WebKit/LayoutTests","version":4,"num_passes":44561,"pixel_tests_enabled":false,"date":"07:18PM on April 08, 2017","has_pretty_patch":true,"fixable":3547,"num_flaky":4,"uses_expectations_file":true});',
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-cross-origin-actual.txt": "a",
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-worker-actual.txt": "b",
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-actual.txt": "c",
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-worker-actual.txt": "d",
            "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-actual.txt": "e",
            "imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-actual.txt": "f-wk1b"}
        ios_sim_files = {"full_results.json": 'ADD_RESULTS({"tests":{"imported":{"w3c":{"web-platform-tests":{"url":{"interfaces.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}},"html":{"browsers":{"windows":{"browsing-context.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}},"the-window-object":{"apis-for-creating-and-navigating-browsing-contexts-by-name":{"open-features-tokenization-001.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}}},"dom":{"events":{"EventTarget-dispatchEvent.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}}}},"animations":{"trigger-container-scroll-empty.html":{"report":"FLAKY","expected":"PASS","actual":"TEXT PASS"}}},"skipped":9881,"num_regressions":4,"other_crashes":{},"interrupted":false,"num_missing":0,"layout_tests_dir":"/Volumes/Data/EWS/WebKit/LayoutTests","version":4,"num_passes":38225,"pixel_tests_enabled":false,"date":"07:33PM on April 08, 2017","has_pretty_patch":true,"fixable":48110,"num_flaky":1,"uses_expectations_file":true});',
            "imported/w3c/web-platform-tests/dom/events/EventTarget-dispatchEvent-actual.txt": "g",
            "imported/w3c/web-platform-tests/html/browsers/the-window-object/apis-for-creating-and-navigating-browsing-contexts-by-name/open-features-tokenization-001-actual.txt": "h",
            "imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-actual.txt": "i",
            "imported/w3c/web-platform-tests/url/interfaces-actual.txt": "j"}
        win_future_files = {"full_results.json": 'ADD_RESULTS({"tests":{"imported":{"w3c":{"web-platform-tests":{"url":{"interfaces.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}},"html":{"browsers":{"windows":{"browsing-context.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}},"the-window-object":{"apis-for-creating-and-navigating-browsing-contexts-by-name":{"open-features-tokenization-001.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}}},"dom":{"events":{"EventTarget-dispatchEvent.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}}}},"animations":{"trigger-container-scroll-empty.html":{"report":"FLAKY","expected":"PASS","actual":"TEXT PASS"}}},"skipped":9881,"num_regressions":4,"other_crashes":{},"interrupted":false,"num_missing":0,"layout_tests_dir":"/Volumes/Data/EWS/WebKit/LayoutTests","version":4,"num_passes":38225,"pixel_tests_enabled":false,"date":"07:33PM on April 08, 2017","has_pretty_patch":true,"fixable":48110,"num_flaky":1,"uses_expectations_file":true});',
            "imported/w3c/web-platform-tests/dom/events/EventTarget-dispatchEvent-actual.txt": "g",
            "imported/w3c/web-platform-tests/html/browsers/the-window-object/apis-for-creating-and-navigating-browsing-contexts-by-name/open-features-tokenization-001-actual.txt": "h",
            "imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-actual.txt": "i",
            "imported/w3c/web-platform-tests/url/interfaces-actual.txt": "j"}
        self.files = {"mac-wk2": mac_wk2_files, "mac-wk1a": mac_wk1a_files, "mac-wk1b": mac_wk1b_files, "ios-sim": ios_sim_files, "win-future": win_future_files}

    def unzip(self, content):
        self.content = content
        return self

    def read(self, filename):
        return self.files[self.content][filename]


class TestExpectationUpdaterTest(unittest.TestCase):
    def _exists(self, host, filename):
        return host.filesystem.exists("/mock-checkout/LayoutTests/" + filename)

    def _is_matching(self, host, filename, content):
        return host.filesystem.read_text_file("/mock-checkout/LayoutTests/" + filename) == content

    def test_update_test_expectations(self):
        host = MockHost()
        host.executive = MockExecutive2(exception=OSError())
        host.filesystem = MockFileSystem(files={
            '/mock-checkout/LayoutTests/platform/mac-wk1/imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-expected.txt': 'e-wk1',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/dom/events/EventTarget-dispatchEvent-expected.txt': "g",
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/html/browsers/the-window-object/apis-for-creating-and-navigating-browsing-contexts-by-name/open-features-tokenization-001-expected.txt': "h",
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-expected.txt': "i",
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/url/interfaces-expected.txt': "j-mac-wk2"})

        mock_zip = MockZip()
        updater = TestExpectationUpdater(host, "123456", True, False, MockBugzilla(), lambda content: mock_zip.unzip(content))
        updater.do_update()
        # mac-wk2 expectation
        self.assertTrue(self._is_matching(host, "imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-cross-origin-expected.txt", "a"))
        # no need to add mac-wk1 specific expectation
        self.assertFalse(self._exists(host, "platform/mac-wk1/imported/w3c/web-platform-tests/fetch/api/redirect/redirect-count-cross-origin-expected.txt"))
        # mac-wk1/ios-simulator-wk2 specific expectation
        self.assertTrue(self._is_matching(host, "platform/mac-wk1/imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-expected.txt", "f-wk1b"))
        self.assertTrue(self._is_matching(host, "platform/ios-wk2/imported/w3c/web-platform-tests/url/interfaces-expected.txt", "j"))
        # removal of mac-wk1 expectation since no longer different
        self.assertFalse(self._exists(host, "platform/mac-wk1/imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-expected.txt"))

    def test_update_win_future_test_expectations(self):
        host = MockHost()
        host.executive = MockExecutive2(exception=OSError())
        host.filesystem = MockFileSystem(files={
            '/mock-checkout/LayoutTests/platform/mac-wk1/imported/w3c/web-platform-tests/fetch/api/redirect/redirect-location-expected.txt': 'e-wk1',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/dom/events/EventTarget-dispatchEvent-expected.txt': "g",
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/html/browsers/the-window-object/apis-for-creating-and-navigating-browsing-contexts-by-name/open-features-tokenization-001-expected.txt': "h",
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/html/browsers/windows/browsing-context-expected.txt': "i",
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/url/interfaces-expected.txt': "j-mac-wk2"})

        mock_zip = MockZip()
        updater = TestExpectationUpdater(host, "123456", True, False, MockBugzilla(include_wk2=False, include_win_future=True), lambda content: mock_zip.unzip(content))
        updater.do_update()
        self.assertTrue(self._is_matching(host, "platform/win/imported/w3c/web-platform-tests/url/interfaces-expected.txt", "j"))
