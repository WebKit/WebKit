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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import unittest
from unittest import mock

from webkitpy.common.host_mock import MockHost
from webkitpy.results.upload import Upload
from webkitpy.tool.mocktool import MockOptions
from webkitpy.webdriver_tests.webdriver_test_result import WebDriverTestResult
from webkitpy.webdriver_tests.webdriver_test_runner import WebDriverTestRunner

STATUS_TEST = 'imported/w3c/webdriver/tests/bidi/session/status/status.py'
NAVIGATE_TEST = 'imported/w3c/webdriver/tests/classic/navigate_to/navigate.py'
TABS_TEST = 'imported/w3c/webdriver/tests/bidi/browsing_context/close/close.py'


def make_result(test, status, subtests=()):
    result = WebDriverTestResult(test, status)
    for name, subtest_status in subtests:
        result.add_subtest_results('%s::%s' % (os.path.basename(test), name), subtest_status, None, None)
    return result


class FakeRunner(object):

    def __init__(self, results):
        self._results = results

    def results(self):
        return self._results


def upload_options(**options):
    return MockOptions(**dict(dict(report_urls=['https://results.webkit.org'], suite=None), **options))


class WebDriverTestRunnerUploadTest(unittest.TestCase):

    def runner(self, results):
        runner = WebDriverTestRunner.__new__(WebDriverTestRunner)
        runner._port = MockHost().port_factory.get('mac', MockOptions())
        runner._runners = [FakeRunner(results)]
        runner._start_time = 100
        runner._end_time = 200
        return runner

    def upload(self, runner, options, succeeds=True):
        with mock.patch.object(runner._port, 'commits_for_upload', return_value=[Upload.create_commit('webkit', '1@main')]), \
                mock.patch.object(Upload, 'upload', autospec=True, return_value=succeeds) as upload:
            result = runner.upload_results(options)
        return result, [call[0][0] for call in upload.call_args_list]

    def test_results_for_upload(self):
        results = WebDriverTestRunner.results_for_upload([
            make_result(STATUS_TEST, 'OK', [
                ('test_pass', 'PASS'),
                ('test_fail', 'FAIL'),
                ('test_error', 'ERROR'),
                ('test_timeout', 'TIMEOUT'),
                ('test_expected_failure', 'XFAIL'),
                ('test_unexpected_pass', 'XPASS'),
                ('test_unexpected_pass_of_timeout', 'XPASS_TIMEOUT'),
                ('test_skip', 'SKIP'),
            ]),
            make_result(NAVIGATE_TEST, 'ERROR'),
            make_result(TABS_TEST, 'CRASH'),
        ])
        self.assertEqual(results, {
            STATUS_TEST + '/test_pass': {},
            STATUS_TEST + '/test_fail': {'actual': 'FAIL'},
            STATUS_TEST + '/test_error': {'actual': 'ERROR'},
            STATUS_TEST + '/test_timeout': {'actual': 'TIMEOUT'},
            STATUS_TEST + '/test_expected_failure': {'expected': 'FAIL', 'actual': 'FAIL'},
            STATUS_TEST + '/test_unexpected_pass': {'expected': 'FAIL'},
            STATUS_TEST + '/test_unexpected_pass_of_timeout': {'expected': 'TIMEOUT'},
            NAVIGATE_TEST: {'actual': 'ERROR'},
            TABS_TEST: {'actual': 'CRASH'},
        })

    def test_upload(self):
        succeeded, uploads = self.upload(self.runner([make_result(STATUS_TEST, 'OK', [('test_fail', 'FAIL'), ('test_skip', 'SKIP')])]), upload_options())
        self.assertTrue(succeeded)
        self.assertEqual(len(uploads), 1)
        self.assertEqual(uploads[0].suite, 'webdriver-tests')
        self.assertEqual(uploads[0].run_stats, {'start_time': 100, 'end_time': 200, 'tests_skipped': 1})
        self.assertEqual(uploads[0].results, {STATUS_TEST + '/test_fail': {'actual': 'FAIL'}})
        self.assertEqual(uploads[0].commits, [{'repository_id': 'webkit', 'id': '1@main'}])

    def test_upload_with_suite(self):
        _, uploads = self.upload(self.runner([]), upload_options(suite='webdriver-tests-custom'))
        self.assertEqual(uploads[0].suite, 'webdriver-tests-custom')

    def test_failed_upload(self):
        succeeded, _ = self.upload(self.runner([]), upload_options(report_urls=['https://results.webkit.org', 'https://example.com']), succeeds=False)
        self.assertFalse(succeeded)
