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
from webkitpy.webdriver_tests.webdriver_w3c_executor import WebDriverW3CExecutor
from webkitpy.webdriver_tests.webdriver_test_result import WebDriverTestResult
from webkitpy.webdriver_tests.webdriver_w3c_web_server import WebDriverW3CWebServer

_log = logging.getLogger(__name__)


class WebDriverTestRunnerW3C(object):

    def __init__(self, port, driver, env, expectations):
        self._port = port
        self._driver = driver
        self._env = env
        self._expectations = expectations
        self._results = []
        self._tests_dir = WebKitFinder(self._port.host.filesystem).path_from_webkit_base('WebDriverTests')

        self._server = WebDriverW3CWebServer(self._port)

    def collect_tests(self, tests):
        skipped = [os.path.join(self._tests_dir, test) for test in self._expectations.skipped_tests()]
        relative_tests_dir = os.path.join('imported', 'w3c', 'webdriver', 'tests')
        w3c_tests = []
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
                w3c_tests.extend(self._scan_directory(test_path, skipped))
            elif self._is_test(test_path) and test_path not in skipped:
                if subtest is not None:
                    w3c_tests.append(test_path + '::' + subtest)
                else:
                    w3c_tests.append(test_path)
        return w3c_tests

    def _is_test(self, test):
        if not os.path.isfile(test):
            return False
        if os.path.splitext(test)[1] != '.py':
            return False
        if os.path.basename(test) in ['conftest.py', '__init__.py']:
            return False
        if os.path.basename(os.path.dirname(test)) == 'support':
            return False
        return True

    def _scan_directory(self, directory, skipped):
        tests = []
        for path in self._port.host.filesystem.files_under(directory):
            if self._is_test(path) and path not in skipped:
                tests.append(path)
        return tests

    def _subtest_name(self, subtest):
        path, test = subtest.split('::', 1)
        return os.path.basename(path) + '::' + test

    def run(self, tests=[]):
        self._server.start()

        executor = WebDriverW3CExecutor(self._driver, self._server, self._env, self._port.get_option('timeout'), self._expectations)
        executor.setup()
        need_restart = False
        try:
            for test in tests:
                test_name = os.path.relpath(test.split('::')[0], self._tests_dir)
                harness_result, test_results = executor.run(test)
                result = WebDriverTestResult(test_name, *harness_result)
                if harness_result[0] == 'OK':
                    for subtest, status, message, backtrace in test_results:
                        result.add_subtest_results(self._subtest_name(subtest), status, message, backtrace)
                        need_restart = need_restart or status in ('FAIL', 'ERROR', 'XFAIL', 'TIMEOUT')
                else:
                    need_restart = True
                self._results.append(result)

                if need_restart:
                    executor.teardown()
                    executor.setup()
        finally:
            executor.teardown()
            self._server.stop()

    def results(self):
        return self._results
