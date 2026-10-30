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

import http.client
import logging
import os
import json
import sys
import time

from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.webdriver_tests.test_process import TestProcess

from mozlog import structuredlog

w3c_tools_dir = WebKitFinder(FileSystem()).path_from_webkit_base('WebDriverTests', 'imported', 'w3c', 'tools')


def _ensure_directory_in_path(directory):
    if not directory in sys.path:
        sys.path.insert(0, directory)
_ensure_directory_in_path(os.path.join(w3c_tools_dir, 'webdriver'))
_ensure_directory_in_path(os.path.join(w3c_tools_dir, 'wptrunner'))

from wptrunner.executors.base import WdspecExecutor, WdspecProtocol  # noqa
from wptrunner.browsers.base import WebDriverBrowser  # noqa

pytest_runner = None


def do_delayed_imports():
    global pytest_runner
    import webkitpy.webdriver_tests.pytest_runner as pytest_runner


_log = logging.getLogger(__name__)


class MessageLogger(object):

    def __init__(self, message_func):
        self.name = 'WebKit WebDriver WPT logger'
        self.send_message = message_func

    def _log_data(self, action, **kwargs):
        self.send_message('log', action, kwargs)

    def process_output(self, process, data, command):
        self._log_data('process_output', process=process, data=data, command=command)


class TestRunner(object):

    def __init__(self):
        self.logger = MessageLogger(self.send_message)
        structuredlog.set_default_logger(self.logger)

    def send_message(self, command, *args):
        if command == 'log':
            self._log(*args)

    def _log(self, level, details):
        if level == 'process_output':
            self._process_output(details['process'], details['command'], details['data'])
            return

        if not 'message' in details:
            return
        message = details['message']
        if level == 'info':
            _log.info(message)
        elif level == 'debug':
            _log.debug(message)
        elif level == 'error':
            _log.error(message)
        elif level == 'criticial':
            _log.critical(message)
        elif level == 'warning':
            _log.warning(message)

    def _process_output(self, pid, command, data):
        _log.debug('(%s:%d): %s' % (os.path.basename(command).split()[0], pid, data))


def _log_func(level_name):
    def log(self, message):
        self._log_data(level_name.lower(), message=message)
    log.__name__ = str(level_name).lower()
    return log

# Create all the methods on StructuredLog for debug levels.
for level_name in structuredlog.log_levels:
    setattr(MessageLogger, level_name.lower(), _log_func(level_name))


class WebKitDriverBrowser(WebDriverBrowser):
    test_env = None

    def __init__(self, logger, binary=None, webdriver_binary=None, webdriver_args=None, host="127.0.0.1", port=None, base_path="/", env=None, supports_pac=True, **kwargs):
        WebDriverBrowser.__init__(self, logger, binary, webdriver_binary, webdriver_args, host, port, base_path, self.test_env, supports_pac, manager_number=0)

    def make_command(self):
        return [self.webdriver_binary, f"--port={self.port}"] + self.webdriver_args


class WebDriverW3CExecutor(WdspecExecutor):
    WARM_UP_TIMEOUT_SECONDS = 120

    def __init__(self, driver, server, env, timeout, expectations, target_platform):
        WebKitDriverBrowser.test_env = env
        WebKitDriverBrowser.test_env.update(driver.browser_env())
        WebKitDriverBrowser.test_env['WEBKIT_TLS_CAFILE_PEM'] = server.cacert_pem_file()
        server_config = {'browser_host': server.host(),
                         'domains': {'': {'': server.host()},
                                     'alt':{ '': '127.0.0.1'}},
                         'ports': {'http': [server.http_port()],
                                   'https': [server.https_port()]},
                         'doc_root': server.document_root()}
        self.runner = TestRunner()
        browser = WebKitDriverBrowser(self.runner.logger, webdriver_binary=driver.binary_path())
        WdspecExecutor.__init__(self, self.runner.logger, browser, server_config, driver.binary_path(), None, capabilities=driver.capabilities(), target_platform=target_platform)

        self._timeout = timeout
        self._expectations = expectations
        self._test_process = None

    def setup(self):
        super(WebDriverW3CExecutor, self).setup(self.runner)
        self.browser.start(None)
        # When a subtest times out, the browser may be stuck too, for example showing a dialog.
        self._test_process = TestProcess(self._run_file, self._timeout, restart=self._restart_driver)
        self._test_process.start()

    def warm_up(self):
        # The first launch of a newly built browser can take much longer than later ones, for example while
        # the system checks the new executables. Start and end a session before running any tests, so that
        # the first test doesn't time out.
        start_time = time.monotonic()
        connection = http.client.HTTPConnection(self.browser.host, self.browser.port, timeout=self.WARM_UP_TIMEOUT_SECONDS)
        try:
            connection.request('POST', '/session', json.dumps({'capabilities': {'alwaysMatch': self.capabilities}}), {'Content-Type': 'application/json'})
            value = json.loads(connection.getresponse().read())['value']
            if 'sessionId' not in value:
                _log.warning('Failed to start a session before running tests: %s' % value)
                return
            connection.request('DELETE', '/session/%s' % value['sessionId'])
            connection.getresponse().read()
        except (OSError, http.client.HTTPException, ValueError, KeyError, TypeError) as error:
            _log.warning('Failed to start a session before running tests: %s' % error)
            return
        finally:
            connection.close()
        _log.info('Started and ended a session in %.1f s before running tests' % (time.monotonic() - start_time))

    def teardown(self):
        if self._test_process is not None:
            self._test_process.stop()
            self._test_process = None
        super(WebDriverW3CExecutor, self).teardown()
        self.browser.stop(force=True)

    def _restart_driver(self):
        # Stopping the driver also stops the browser it launched.
        self.browser.stop(force=True)
        self.browser.start(None)

    def _run_file(self, test, deselected, report):
        # Runs in the test process.
        if pytest_runner is None:
            do_delayed_imports()

        with pytest_runner.TemporaryDirectory() as cache_directory:
            config_path = os.path.join(cache_directory, 'wd_config.json')
            env = {'WDSPEC_CONFIG_FILE': config_path}

            config = {'host': self.browser.host,
                      'port': self.browser.port,
                      'capabilities': self.capabilities,
                      'webdriver': {'binary': self.browser.webdriver_binary},
                      'wptserve': self.server_config,
                      'timeout_multiplier': 10,
                      'target_platform': self.target_platform,
                      }
            with open(config_path, 'w') as f:
                json.dump(config, f)

            env.update(WebKitDriverBrowser.test_env)
            args = ['--strict-markers', '-p', 'no:mozlog']
            harness_result, _ = pytest_runner.run(test, args, self._timeout, env, self._expectations, report=report, deselected=deselected)
            return harness_result

    def run(self, test):
        return self._test_process.run(test)
