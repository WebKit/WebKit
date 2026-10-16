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

import asyncio
import contextlib
import errno
import os
import selectors
import shutil
import signal
import sys
import tempfile

from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.webkit_finder import WebKitFinder
import pytest
import pytest_timeout
import pytest_asyncio
from _pytest.config import ExitCode


class TemporaryDirectory(object):

    def __enter__(self):
        self.path = tempfile.mkdtemp(prefix="pytest-")
        return self.path

    def __exit__(self, *args):
        try:
            shutil.rmtree(self.path)
        except OSError as e:
            # no such file or directory
            if e.errno != errno.ENOENT:
                raise


def remove_ignored_param(item_name, ignore_param):
    if ignore_param is None:
        return item_name

    single_param = '[%s]' % ignore_param
    if item_name.endswith(single_param):
        return item_name[:-len(single_param)]

    param = '[%s-' % ignore_param
    if param in item_name:
        return item_name.replace('%s-' % ignore_param, '')

    return item_name


class CollectRecorder(object):

    def __init__(self, ignore_param):
        self._ignore_param = ignore_param
        self.tests = {}

    def pytest_collectreport(self, report):
        if report.nodeid and report.result:
            self.tests.setdefault(report.nodeid, [])
            for subtest in report.result:
                self.tests[report.nodeid].append(remove_ignored_param(subtest.name, self._ignore_param))


class HarnessResultRecorder(object):

    def __init__(self):
        self.outcome = ('OK', None)

    def pytest_collectreport(self, report):
        if report.outcome == 'failed':
            self.outcome = ('ERROR', None)
        elif report.outcome == 'skipped':
            self.outcome = ('SKIP', None)


class SubtestResultRecorder(object):

    def __init__(self, ignore_param):
        self.results = []
        self._ignore_param = ignore_param

    def pytest_runtest_logreport(self, report):
        if report.passed and report.when == 'call':
            self.record_pass(report)
        elif report.failed:
            if report.when != 'call':
                self.record_error(report)
            else:
                self.record_fail(report)
        elif report.skipped:
            self.record_skip(report)

    def _was_timeout(self, report):
        return hasattr(report.longrepr, 'reprcrash') and report.longrepr.reprcrash.message.startswith('Failed: Timeout')

    def record_pass(self, report):
        expected = dict(report.user_properties).get("expectations", [])
        # Avoid expected flaky passes from being reported as XPASS, distracting from
        # true XPASS tests.
        if hasattr(report, 'wasxfail') and 'PASS' not in expected:
            if report.wasxfail == 'Timeout':
                self.record(report.nodeid, 'XPASS_TIMEOUT')
            else:
                self.record(report.nodeid, 'XPASS')
        else:
            self.record(report.nodeid, 'PASS')

    def record_fail(self, report):
        if self._was_timeout(report):
            self.record(report.nodeid, 'TIMEOUT', stack=report.longrepr)
        else:
            self.record(report.nodeid, 'FAIL', stack=report.longrepr)

    def record_error(self, report):
        # error in setup/teardown
        if report.when != 'call':
            message = '%s error' % report.when
        self.record(report.nodeid, 'ERROR', message, report.longrepr)

    def record_skip(self, report):
        expected = dict(report.user_properties).get("expectations", [])
        if hasattr(report, 'wasxfail'):
            # Avoid expected flaky fail/timeouts from being reported as unexpected TIMEOUT,
            # distracting from true TIMEOUT regressions.
            if self._was_timeout(report) and report.wasxfail != 'Timeout' and 'TIMEOUT' not in expected:
                self.record(report.nodeid, 'TIMEOUT', stack=report.longrepr)
            else:
                self.record(report.nodeid, 'XFAIL')
        else:
            self.record(report.nodeid, 'SKIP')

    def record(self, test, status, message=None, stack=None):
        if stack is not None:
            stack = str(stack)
        test_name = remove_ignored_param(test, self._ignore_param)
        new_result = (test_name, status, message, stack)
        self.results.append(new_result)


class TestExpectationsMarker(object):

    def __init__(self, expectations, timeout, ignore_param):
        self._expectations = expectations
        self._timeout = timeout
        self._ignore_param = ignore_param
        self._base_dir = WebKitFinder(FileSystem()).path_from_webkit_base('WebDriverTests')

    def pytest_collection_modifyitems(self, session, config, items):
        for item in items:
            test = os.path.relpath(str(item.fspath), self._base_dir)
            item_name = remove_ignored_param(item.name, self._ignore_param)
            if self._expectations.is_slow(test, item_name):
                item.add_marker(pytest.mark.timeout(self._timeout * 5))
            expected = self._expectations.get_expectation(test, item_name)
            if 'FAIL' in expected:
                # Adding a "flaky" reason would overwrite the actual reason the test failed,
                # making it harder to identify the actual reason
                item.add_marker(pytest.mark.xfail)
            elif 'TIMEOUT' in expected:
                item.add_marker(pytest.mark.xfail(reason="Timeout"))
            elif 'SKIP' in expected:
                item.add_marker(pytest.mark.skip)

            item.user_properties.append(("expectations", expected))


class TimeoutSignalHandler(object):
    """Keep pytest-timeout's SIGALRM from firing inside asyncio's scheduler.

    pytest-timeout raises from the signal handler wherever the alarm lands. Inside the
    event loop's own scheduling code (e.g. while Future.set_result() is queueing the
    wake-up of the awaiting task) that leaves the future done but the task never resumed,
    and the loop runs forever. Defer the alarm until the loop is idle in the selector, from
    where the exception propagates cleanly out of run_until_complete().
    """

    RETRY_INTERVAL = 0.005

    @pytest.hookimpl(wrapper=True)
    def pytest_timeout_set_timer(self, item, settings):
        result = yield
        if settings.method != 'signal':
            return result
        handler = signal.getsignal(signal.SIGALRM)
        if callable(handler):
            def deferring_handler(signum, frame):
                __tracebackhide__ = True
                if self._should_defer_timeout(frame):
                    signal.setitimer(signal.ITIMER_REAL, self.RETRY_INTERVAL)
                else:
                    handler(signum, frame)
            signal.signal(signal.SIGALRM, deferring_handler)
        return result

    @staticmethod
    def _should_defer_timeout(frame):
        # Raising timeout exceptions while the loop is running outside selector wait might
        # corrupt internal state updates, leaving tasks hanging.
        try:
            asyncio.get_running_loop()
        except RuntimeError:
            return False
        code = frame.f_code
        return not (code.co_filename == selectors.__file__ and code.co_name == 'select')


def collect(directory, args, ignore_param=None):
    collect_recorder = CollectRecorder(ignore_param)
    with open(os.devnull, 'w') as f:
        with contextlib.redirect_stdout(f):
            with TemporaryDirectory() as cache_directory:
                cmd = ['--collect-only',
                       '--basetemp', cache_directory]
                cmd.extend(args)
                cmd.append(directory)
                pytest.main(cmd, plugins=[collect_recorder])
    return collect_recorder.tests


def run(path, args, timeout, env, expectations, ignore_param=None, extra_plugins=None):
    harness_recorder = HarnessResultRecorder()
    subtests_recorder = SubtestResultRecorder(ignore_param)
    expectations_marker = TestExpectationsMarker(expectations, timeout, ignore_param)
    _environ = dict(os.environ)
    os.environ.clear()
    os.environ.update(env)

    with TemporaryDirectory() as cache_directory:
        try:
            cmd = ['-vv',
                   '--capture', 'no',
                   '--basetemp', cache_directory,
                   '--showlocals',
                   '--timeout', str(timeout),
                   '-p', 'no:cacheprovider']
            cmd.extend(args)
            cmd.append(path)
            result = pytest.main(cmd, plugins=[harness_recorder, subtests_recorder, expectations_marker, TimeoutSignalHandler(), *(extra_plugins or [])])

            if result == ExitCode.INTERNAL_ERROR:
                harness_recorder.outcome = ('ERROR', None)
            elif result == ExitCode.USAGE_ERROR:
                harness_recorder.outcome = ('ERROR', f'Error invoking pytest.main: Arguments: {cmd}')
        except Exception as e:
            harness_recorder.outcome = ('ERROR', str(e))

    os.environ.clear()
    os.environ.update(_environ)

    return harness_recorder.outcome, subtests_recorder.results
