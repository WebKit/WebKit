# Copyright (C) 2017 Igalia S.L.
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

import json
import logging
import os

from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.common.test_expectations import TestExpectations
from webkitpy.webdriver_tests.webdriver_driver import create_driver
from webkitpy.webdriver_tests.webdriver_test_runner_selenium import WebDriverTestRunnerSelenium
from webkitpy.webdriver_tests.webdriver_test_runner_w3c import WebDriverTestRunnerW3C

_log = logging.getLogger(__name__)


class WebDriverTestRunner(object):

    RUNNER_CLASSES = (WebDriverTestRunnerSelenium, WebDriverTestRunnerW3C)

    def __init__(self, port):
        self._port = port
        _log.info('Using port %s' % self._port.name())
        _log.info('Test configuration: %s' % self._port.test_configuration())
        _log.info('Using display server %s' % (self._port._display_server))

        self._display_driver = self._port._driver_class()(self._port, worker_number=0, pixel_tests=False, no_timeout=True)
        if not self._display_driver.check_driver(self._port):
            raise RuntimeError("Failed to check driver %s" % self._display_driver.__class__.__name__)

        driver = create_driver(self._port)
        _log.info('Using driver at %s' % (driver.binary_path()))
        _log.info('Browser: %s' % (driver.browser_name()))

        _log.info('Parsing expectations')
        self._tests_dir = WebKitFinder(self._port.host.filesystem).path_from_webkit_base('WebDriverTests')
        expectations_file = os.path.join(self._tests_dir, 'TestExpectations.json')
        build_type = 'Debug' if self._port.get_option('debug') else 'Release'
        self._expectations = TestExpectations(self._port.name(), expectations_file, build_type)
        for test in self._expectations._expectations.keys():
            if not os.path.isfile(os.path.join(self._tests_dir, test)):
                _log.warning('Test %s does not exist' % test)

        env = self._display_driver._setup_environ_for_test()
        self._runners = [runner_cls(self._port, driver, env, self._expectations) for runner_cls in self.RUNNER_CLASSES]

    def teardown(self):
        self._display_driver.stop()

    def run(self, tests=[]):
        runner_tests = [runner.collect_tests(tests) for runner in self._runners]
        collected_count = sum([len(tests) for tests in runner_tests])
        if not collected_count:
            _log.info('No tests found')
            return 0

        _log.info('Collected %d test files' % collected_count)
        for i in range(len(self._runners)):
            if runner_tests[i]:
                self._runners[i].run(runner_tests[i])

    def process_results(self):
        results = {}
        expected_count = 0
        passed_count = 0
        failures_count = 0
        timeout_count = 0
        test_results = []
        for runner in self._runners:
            test_results.extend(runner.results())
        for result in test_results:
            if result.status == 'OK':
                for subtest, status, _, _ in result.subtest_results:
                    if status in ['PASS', 'SKIP', 'XFAIL']:
                        expected_count += 1
                    elif status in ['FAIL', 'ERROR']:
                        results.setdefault('FAIL', []).append(os.path.join(os.path.dirname(result.test), subtest))
                        failures_count += 1
                    elif status == 'TIMEOUT':
                        results.setdefault('TIMEOUT', []).append(os.path.join(os.path.dirname(result.test), subtest))
                        timeout_count += 1
                    elif status in ['XPASS', 'XPASS_TIMEOUT']:
                        results.setdefault(status, []).append(os.path.join(os.path.dirname(result.test), subtest))
                        passed_count += 1
            else:
                # FIXME: handle other results.
                pass

        _log.info('')
        retval = 0

        if not results:
            _log.info('All tests run as expected')
            return retval

        _log.info('%d tests ran as expected, %d didn\'t\n' % (expected_count, failures_count + timeout_count + passed_count))

        def report(status, actual, expected=None):
            retval = 0
            if status not in results:
                return retval

            tests = results[status]
            tests_count = len(tests)
            if expected is None:
                _log.info('Unexpected %s (%d)' % (actual, tests_count))
                retval += tests_count
            else:
                _log.info('Expected to %s, but %s (%d)' % (expected, actual, tests_count))
            for test in tests:
                _log.info('  %s' % test)
            _log.info('')

            return retval

        report('XPASS', 'passed', 'fail')
        report('XPASS_TIMEOUT', 'passed', 'timeout')
        retval += report('FAIL', 'failures')
        retval += report('TIMEOUT', 'timeouts')

        return retval

    def dump_results_to_json_file(self, output_path):
        json_results = {}
        json_results['results'] = []
        test_results = []
        for runner in self._runners:
            test_results.extend(runner.results())
        for result in test_results:
            results = {}
            results['test'] = result.test
            results['status'] = result.status
            results['message'] = result.message
            results['subtests'] = []
            for name, status, message, _ in result.subtest_results:
                subtest = {}
                subtest['name'] = name.split('::', 1)[1]
                subtest['status'] = status
                subtest['message'] = message
                results['subtests'].append(subtest)
            json_results['results'].append(results)

        directory = os.path.dirname(output_path)
        if directory and not os.path.exists(directory):
            os.makedirs(directory)

        with open(output_path, 'wb') as fp:
            json.dump(json_results, fp)
