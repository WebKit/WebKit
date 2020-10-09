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

import errno
import json
import os
import shutil
import sys
import tempfile

from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.webkit_finder import WebKitFinder
import pytest
from _pytest.main import EXIT_INTERNALERROR


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


def get_item_name(item, ignore_param):
    if ignore_param is None:
        return item.name

    single_param = '[%s]' % ignore_param
    if item.name.endswith(single_param):
        return item.name[:-len(single_param)]

    param = '[%s-' % ignore_param
    if param in item.name:
        return item.name.replace('%s-' % ignore_param, '')

    return item.name


class CollectRecorder(object):

    def __init__(self, ignore_param):
        self._ignore_param = ignore_param
        self.tests = {}

    def pytest_collectreport(self, report):
        if report.nodeid:
            self.tests.setdefault(report.nodeid, [])
            for subtest in report.result:
                self.tests[report.nodeid].append(get_item_name(subtest, self._ignore_param))


class HarnessResultRecorder(object):

    def __init__(self):
        self.outcome = ('OK', None)

    def pytest_collectreport(self, report):
        if report.outcome == 'failed':
            self.outcome = ('ERROR', None)
        elif report.outcome == 'skipped':
            self.outcome = ('SKIP', None)


class SubtestResultRecorder(object):

    def __init__(self):
        self.results = []

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
        return hasattr(report.longrepr, 'reprcrash') and report.longrepr.reprcrash.message.startswith('Failed: Timeout >')

    def record_pass(self, report):
        if hasattr(report, 'wasxfail'):
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
        if hasattr(report, 'wasxfail'):
            if self._was_timeout(report) and report.wasxfail != 'Timeout':
                self.record(report.nodeid, 'TIMEOUT', stack=report.longrepr)
            else:
                self.record(report.nodeid, 'XFAIL')
        else:
            self.record(report.nodeid, 'SKIP')

    def record(self, test, status, message=None, stack=None):
        if stack is not None:
            stack = str(stack)
        new_result = (test, status, message, stack)
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
            item_name = get_item_name(item, self._ignore_param)
            if self._expectations.is_slow(test, item_name):
                item.add_marker(pytest.mark.timeout(self._timeout * 5))
            expected = self._expectations.get_expectation(test, item_name)[0]
            if expected == 'FAIL':
                item.add_marker(pytest.mark.xfail)
            elif expected == 'TIMEOUT':
                item.add_marker(pytest.mark.xfail(reason="Timeout"))
            elif expected == 'SKIP':
                item.add_marker(pytest.mark.skip)


def collect(directory, args, ignore_param=None):
    collect_recorder = CollectRecorder(ignore_param)
    stdout = sys.stdout
    with open(os.devnull, 'wb') as devnull:
        sys.stdout = devnull
        with TemporaryDirectory() as cache_directory:
            cmd = ['--collect-only',
                   '--basetemp=%s' % cache_directory]
            cmd.extend(args)
            cmd.append(directory)
            pytest.main(cmd, plugins=[collect_recorder])
    sys.stdout = stdout
    return collect_recorder.tests


def run(path, args, timeout, env, expectations, ignore_param=None):
    harness_recorder = HarnessResultRecorder()
    subtests_recorder = SubtestResultRecorder()
    expectations_marker = TestExpectationsMarker(expectations, timeout, ignore_param)
    _environ = dict(os.environ)
    os.environ.clear()
    os.environ.update(env)

    with TemporaryDirectory() as cache_directory:
        try:
            cmd = ['-vv',
                   '--capture=no',
                   '--basetemp=%s' % cache_directory,
                   '--showlocals',
                   '--timeout=%s' % timeout,
                   '-p', 'no:cacheprovider',
                   '-p', 'pytest_timeout']
            cmd.extend(args)
            cmd.append(path)
            result = pytest.main(cmd, plugins=[harness_recorder, subtests_recorder, expectations_marker])

            if result == EXIT_INTERNALERROR:
                harness_recorder.outcome = ('ERROR', None)
        except Exception as e:
            harness_recorder.outcome = ('ERROR', str(e))

    os.environ.clear()
    os.environ.update(_environ)

    return harness_recorder.outcome, subtests_recorder.results
