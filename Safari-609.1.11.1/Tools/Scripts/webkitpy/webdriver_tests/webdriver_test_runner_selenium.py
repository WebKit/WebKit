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

import logging
import os
import sys

from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.webdriver_tests.webdriver_selenium_executor import WebDriverSeleniumExecutor
from webkitpy.webdriver_tests.webdriver_test_result import WebDriverTestResult

_log = logging.getLogger(__name__)


class WebDriverTestRunnerSelenium(object):

    def __init__(self, port, driver, env, expectations):
        self._port = port
        self._driver = driver
        self._env = env
        self._expectations = expectations
        self._results = []
        self._tests_dir = WebKitFinder(self._port.host.filesystem).path_from_webkit_base('WebDriverTests')

    def collect_tests(self, tests):
        if self._driver.selenium_name() is None:
            return []

        skipped = [os.path.join(self._tests_dir, test) for test in self._expectations.skipped_tests()]
        relative_tests_dir = os.path.join('imported', 'selenium', 'py', 'test')
        executor = WebDriverSeleniumExecutor(self._driver, self._env)
        # Collected tests are relative to test directory.
        base_dir = os.path.join(self._tests_dir, os.path.dirname(relative_tests_dir))
        collected_tests = {}
        for test, subtests in executor.collect(os.path.join(self._tests_dir, relative_tests_dir)).iteritems():
            collected_tests[os.path.join(base_dir, test)] = subtests
        selenium_tests = []
        if not tests:
            tests = [relative_tests_dir]
        for test in tests:
            subtest = None
            if '::' in test:
                test, subtest = test.split('::')

            if not test.startswith(relative_tests_dir):
                continue

            test_path = os.path.join(self._tests_dir, test)
            if os.path.isdir(test_path):
                selenium_tests.extend([test for test in collected_tests if test.startswith(test_path) and test not in skipped])
            elif test_path in collected_tests and test_path not in skipped:
                if subtest is not None:
                    if subtest in collected_tests[test_path]:
                        selenium_tests.append(test_path + '::' + subtest)
                else:
                    selenium_tests.append(test_path)
        return selenium_tests

    def run(self, tests=[]):
        if self._driver.selenium_name() is None:
            return

        executor = WebDriverSeleniumExecutor(self._driver, self._env)
        timeout = self._port.get_option('timeout')
        for test in tests:
            test_name = os.path.relpath(test.split('::')[0], self._tests_dir)
            harness_result, test_results = executor.run(test, timeout, self._expectations)
            result = WebDriverTestResult(test_name, *harness_result)
            if harness_result[0] == 'OK':
                for subtest, status, message, backtrace in test_results:
                    result.add_subtest_results(os.path.basename(subtest), status, message, backtrace)
            else:
                # FIXME: handle other results.
                pass
            self._results.append(result)

    def results(self):
        return self._results
