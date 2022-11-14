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
import json
import sys

from multiprocessing import Process, Queue
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.webkit_finder import WebKitFinder

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
        WebDriverBrowser.__init__(self, logger, binary, webdriver_binary, webdriver_args, host, port, base_path, self.test_env, supports_pac)

    def make_command(self):
        return [self.webdriver_binary, f"--port={self.port}"] + self.webdriver_args


class WebDriverW3CExecutor(WdspecExecutor):
    def __init__(self, driver, server, env, timeout, expectations):
        WebKitDriverBrowser.test_env = env
        WebKitDriverBrowser.test_env.update(driver.browser_env())
        server_config = {'browser_host': server.host(),
                         'domains': {'': {'': server.host()},
                                     'alt':{ '': '127.0.0.1'}},
                         'ports': {'http': [server.http_port()],
                                   'https': [server.https_port()]},
                         'doc_root': server.document_root()}
        self.runner = TestRunner()
        browser = WebKitDriverBrowser(self.runner.logger, webdriver_binary=driver.binary_path())
        WdspecExecutor.__init__(self, self.runner.logger, browser, server_config, driver.binary_path(), None, capabilities=driver.capabilities())

        self._timeout = timeout
        self._expectations = expectations
        self._test_queue = Queue()
        self._result_queue = Queue()

    def setup(self):
        super(WebDriverW3CExecutor, self).setup(self.runner)
        self.browser.start(None)
        args = (self._test_queue,
                self._result_queue,
                self.browser.host,
                self.browser.port,
                self.capabilities,
                self.browser.webdriver_binary,
                self.server_config,
                self._timeout,
                self._expectations)
        self._process = Process(target=WebDriverW3CExecutor._runner, args=args)
        self._process.start()

    def teardown(self):
        self.protocol.teardown()
        self.browser.stop(force=True)
        self._test_queue.put('TEARDOWN')
        self._process = None

    @staticmethod
    def _runner(test_queue, result_queue, host, port, capabilities, webdriver_binary, server_config, timeout, expectations):
        if pytest_runner is None:
            do_delayed_imports()

        while True:
            test = test_queue.get()
            if test == 'TEARDOWN':
                break

            with pytest_runner.TemporaryDirectory() as cache_directory:
                config_path = os.path.join(cache_directory, 'wd_config.json')
                env = {'WDSPEC_CONFIG_FILE': config_path}

                config = {'host': host,
                          'port': port,
                          'capabilities': capabilities,
                          'webdriver': {'binary': webdriver_binary},
                          'wptserve': server_config
                          }
                with open(config_path, 'w') as f:
                    json.dump(config, f)

                env.update(WebKitDriverBrowser.test_env)
                args = ['--strict-markers', '-p', 'no:mozlog']
                result_queue.put(pytest_runner.run(test, args, timeout, env, expectations))

    def run(self, test):
        self._test_queue.put(test)
        return self._result_queue.get()
