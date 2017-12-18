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
import webkitpy.thirdparty.autoinstalled.mozlog
import webkitpy.thirdparty.autoinstalled.pytest
import webkitpy.thirdparty.autoinstalled.pytest_timeout
import pytest

# Since W3C tests also use pytest, we use pytest and some other tools for selenium too.
w3c_tools_dir = WebKitFinder(FileSystem()).path_from_webkit_base('WebDriverTests', 'imported', 'w3c', 'tools')


def _ensure_directory_in_path(directory):
    if not directory in sys.path:
        sys.path.insert(0, directory)
_ensure_directory_in_path(os.path.join(w3c_tools_dir, 'wptrunner'))

from wptrunner.executors.pytestrunner.runner import HarnessResultRecorder, SubtestResultRecorder, TemporaryDirectory

_log = logging.getLogger(__name__)


class CollectRecorder(object):

    def __init__(self):
        self.tests = []

    def pytest_collectreport(self, report):
        if report.nodeid:
            self.tests.append(report.nodeid)


class WebDriverSeleniumExecutor(object):

    def __init__(self, driver, display_driver):
        self._env = {}
        self._env['WD_DRIVER_PATH'] = driver.binary_path()
        browser_path = driver.browser_path()
        if browser_path:
            self._env['WD_BROWSER_PATH'] = browser_path
        browser_args = driver.browser_args()
        if browser_args:
            self._env['WD_BROWSER_ARGS'] = ' '.join(browser_args)
        self._env.update(display_driver._setup_environ_for_test())
        self._env.update(driver.browser_env())

        self._name = driver.selenium_name()

    def collect(self, directory):
        collect_recorder = CollectRecorder()
        stdout = sys.stdout
        with open(os.devnull, 'wb') as devnull:
            sys.stdout = devnull
            with TemporaryDirectory() as cache_directory:
                pytest.main(['--driver=%s' % self._name,
                             '--collect-only',
                             '--basetemp', cache_directory,
                             directory], plugins=[collect_recorder])
        sys.stdout = stdout
        return collect_recorder.tests

    def run(self, test, timeout=0):
        harness_recorder = HarnessResultRecorder()
        subtests_recorder = SubtestResultRecorder()
        _environ = dict(os.environ)
        os.environ.clear()
        os.environ.update(self._env)

        with TemporaryDirectory() as cache_directory:
            try:
                pytest.main(['--driver=%s' % self._name,
                             '--verbose',
                             '--capture=no',
                             '--basetemp', cache_directory,
                             '--showlocals',
                             '--timeout=%s' % timeout,
                             '-p', 'no:cacheprovider',
                             '-p', 'pytest_timeout',
                             test], plugins=[harness_recorder, subtests_recorder])
            except Exception as e:
                harness_recorder.outcome = ("ERROR", str(e))

        os.environ.clear()
        os.environ.update(_environ)

        return harness_recorder.outcome, subtests_recorder.results
