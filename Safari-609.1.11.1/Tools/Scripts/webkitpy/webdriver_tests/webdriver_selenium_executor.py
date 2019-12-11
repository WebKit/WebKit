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

pytest_runner = None


def do_delayed_imports():
    global pytest_runner
    import webkitpy.webdriver_tests.pytest_runner as pytest_runner


_log = logging.getLogger(__name__)


class WebDriverSeleniumExecutor(object):

    def __init__(self, driver, env):
        self._driver_name = driver.selenium_name()
        self._env = env
        self._env.update(driver.browser_env())

        self._args = ['--driver=%s' % self._driver_name, '--driver-binary=%s' % driver.binary_path().encode()]
        browser_path = driver.browser_path().encode()
        if browser_path:
            self._args.extend(['--browser-binary=%s' % browser_path])
        browser_args = driver.browser_args()
        if browser_args:
            self._args.extend(['--browser-args=%s' % ' '.join(browser_args)])

        if pytest_runner is None:
            do_delayed_imports()

    def collect(self, directory):
        return pytest_runner.collect(directory, self._args, self._driver_name)

    def run(self, test, timeout, expectations):
        return pytest_runner.run(test, self._args, timeout, self._env, expectations, self._driver_name)
