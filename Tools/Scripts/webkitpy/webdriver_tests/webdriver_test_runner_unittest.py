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

from webkitcorepy import OutputCapture

from webkitpy.common.host_mock import MockHost
from webkitpy.results.upload import Upload
from webkitpy.tool.mocktool import MockOptions
from webkitpy.webdriver_tests.webdriver_test_result import WebDriverTestResult
from webkitpy.webdriver_tests.webdriver_test_runner import WebDriverTestRunner

STATUS_TEST = 'imported/w3c/webdriver/tests/bidi/session/status/status.py'
NAVIGATE_TEST = 'imported/w3c/webdriver/tests/classic/navigate_to/navigate.py'
TABS_TEST = 'imported/w3c/webdriver/tests/bidi/browsing_context/close/close.py'


def make_result(test, status, subtests=(), message=None):
    result = WebDriverTestResult(test, status, message)
    for name, subtest_status, *texts in subtests:
        subtest_message, backtrace = texts or (None, None)
        result.add_subtest_results('%s::%s' % (os.path.basename(test), name), subtest_status, subtest_message, backtrace)
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

    def test_log_for_upload(self):
        self.assertEqual(WebDriverTestRunner.log_for_upload('setup error', 'KeyError: session'), 'setup error\nKeyError: session')
        self.assertEqual(WebDriverTestRunner.log_for_upload(None, 'assert 1 == 2'), 'assert 1 == 2')
        self.assertIsNone(WebDriverTestRunner.log_for_upload(None, None))
        self.assertIsNone(WebDriverTestRunner.log_for_upload('', ''))

    def test_log_for_upload_masks_uuids_and_object_addresses(self):
        self.assertEqual(
            WebDriverTestRunner.log_for_upload(
                'driver = <WebDriver (session="8642bd7a-ff65-455d-a28d-9f168eb82457")>\npages = <Pages object at 0x7faf7d8ecec0>',
                'E       +  where False = is_fullscreen(<Session 9394c9b3-4359-492e-9edc-038775436744>)\nE       assert 0x10 == 16\n'
                'AssertionError: Unable to close window page-BDBB4326-266B-4CDC-9512-EA4A4ABAD6CD',
            ),
            'driver = <WebDriver (session="...")>\npages = <Pages object at 0x...>\n'
            'E       +  where False = is_fullscreen(<Session ...>)\nE       assert 0x10 == 16\n'
            'AssertionError: Unable to close window page-...',
        )

    def test_upload_log_for_unexpected_failures(self):
        results = WebDriverTestRunner.results_for_upload([make_result(STATUS_TEST, 'OK', [
            ('test_fail', 'FAIL', None, 'assert 1 == 2'),
            ('test_error', 'ERROR', 'setup error', 'KeyError: session'),
            ('test_timeout', 'TIMEOUT', None, 'Failed: Timeout >30.0s'),
        ])])
        self.assertEqual(results, {
            STATUS_TEST + '/test_fail': {'actual': 'FAIL', 'log': 'assert 1 == 2'},
            STATUS_TEST + '/test_error': {'actual': 'ERROR', 'log': 'setup error\nKeyError: session'},
            STATUS_TEST + '/test_timeout': {'actual': 'TIMEOUT', 'log': 'Failed: Timeout >30.0s'},
        })

    def test_no_upload_log_for_expected_results(self):
        results = WebDriverTestRunner.results_for_upload([make_result(STATUS_TEST, 'OK', [
            ('test_pass', 'PASS', None, 'passed'),
            ('test_expected_failure', 'XFAIL', None, 'assert 1 == 2'),
            ('test_unexpected_pass', 'XPASS', None, 'passed'),
            ('test_unexpected_pass_of_timeout', 'XPASS_TIMEOUT', None, 'passed'),
        ])])
        self.assertEqual(results, {
            STATUS_TEST + '/test_pass': {},
            STATUS_TEST + '/test_expected_failure': {'expected': 'FAIL', 'actual': 'FAIL'},
            STATUS_TEST + '/test_unexpected_pass': {'expected': 'FAIL'},
            STATUS_TEST + '/test_unexpected_pass_of_timeout': {'expected': 'TIMEOUT'},
        })

    def test_upload_log_for_harness_failures(self):
        results = WebDriverTestRunner.results_for_upload([
            make_result(NAVIGATE_TEST, 'ERROR', message='KeyError outside of any subtest'),
            make_result(TABS_TEST, 'CRASH'),
        ])
        self.assertEqual(results, {
            NAVIGATE_TEST: {'actual': 'ERROR', 'log': 'KeyError outside of any subtest'},
            TABS_TEST: {'actual': 'CRASH'},
        })

    def test_upload_log_is_truncated(self):
        backtrace = '\n'.join(f'frame {index}: ' + 'x' * 100 for index in range(200))
        results = WebDriverTestRunner.results_for_upload([make_result(STATUS_TEST, 'OK', [('test_fail', 'FAIL', None, backtrace)])])
        log = results[STATUS_TEST + '/test_fail']['log']
        self.assertEqual(log, Upload.truncate_log(backtrace))
        self.assertLess(len(log), len(backtrace))
        self.assertLessEqual(len(log.encode('utf-8')), Upload.MAXIMUM_LOG_LENGTH)

    def test_upload_log_for_every_failure(self):
        backtrace = 'x' * Upload.MAXIMUM_LOG_LENGTH
        results = WebDriverTestRunner.results_for_upload([make_result(STATUS_TEST, 'OK', [(f'test_fail_{index}', 'FAIL', None, backtrace) for index in range(40)])])
        self.assertEqual([result['log'] for result in results.values()], [backtrace] * 40)

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
        with OutputCapture() as captured:
            succeeded, _ = self.upload(self.runner([]), upload_options(report_urls=['https://results.webkit.org', 'https://example.com']), succeeds=False)
        self.assertFalse(succeeded)
        self.assertIn('Failed to upload test results to https://results.webkit.org\n', captured.root.log.getvalue())
        self.assertIn('Failed to upload test results to https://example.com\n', captured.root.log.getvalue())
