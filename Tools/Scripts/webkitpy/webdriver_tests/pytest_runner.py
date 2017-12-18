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

import webkitpy.thirdparty.autoinstalled.pytest
import webkitpy.thirdparty.autoinstalled.pytest_timeout
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


class CollectRecorder(object):

    def __init__(self):
        self.tests = []

    def pytest_collectreport(self, report):
        if report.nodeid:
            self.tests.append(report.nodeid)


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


def collect(directory, args):
    collect_recorder = CollectRecorder()
    stdout = sys.stdout
    with open(os.devnull, 'wb') as devnull:
        sys.stdout = devnull
        with TemporaryDirectory() as cache_directory:
            pytest.main(args + ['--collect-only',
                                '--basetemp', cache_directory,
                                directory],
                        plugins=[collect_recorder])
    sys.stdout = stdout
    return collect_recorder.tests


def run(path, args, timeout, env={}):
    harness_recorder = HarnessResultRecorder()
    subtests_recorder = SubtestResultRecorder()
    _environ = dict(os.environ)
    os.environ.clear()
    os.environ.update(env)

    with TemporaryDirectory() as cache_directory:
        try:
            result = pytest.main(args + ['--verbose',
                                         '--capture=no',
                                         '--basetemp', cache_directory,
                                         '--showlocals',
                                         '--timeout=%s' % timeout,
                                         '-p', 'no:cacheprovider',
                                         '-p', 'pytest_timeout',
                                         path],
                                 plugins=[harness_recorder, subtests_recorder])
            if result == EXIT_INTERNALERROR:
                harness_recorder.outcome = ('ERROR', None)
        except Exception as e:
            harness_recorder.outcome = ('ERROR', str(e))

    os.environ.clear()
    os.environ.update(_environ)

    return harness_recorder.outcome, subtests_recorder.results
