# Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import inspect
import json
import logging
import operator
import os
import shutil
import sys
import tempfile
import time
from unittest import skip as skipTest
from unittest.mock import call, create_autospec, patch

from buildbot.process import remotetransfer
from buildbot.process.results import SUCCESS, FAILURE, WARNINGS, SKIPPED, RETRY
from buildbot.test.fake.fakebuild import FakeBuild
from buildbot.test.reactor import TestReactorMixin
from buildbot.test.steps import Expect, ExpectShell
from buildbot.test.steps import TestBuildStepMixin as BuildStepMixin
from buildbot.util import identifiers as buildbot_identifiers
from twisted.internet import defer, error, reactor
from twisted.python import failure, log
from twisted.trial import unittest

from Shared.steps import *

from . import send_email
from .layout_test_failures import LayoutTestFailures
from .steps import *

# Workaround for https://github.com/buildbot/buildbot/issues/4669
FakeBuild.addStepsAfterCurrentStep = lambda FakeBuild, step_factories: None
FakeBuild._builderid = 1

# Prevent unit-tests from talking to live bugzilla and github servers
BugzillaMixin.fetch_data_from_url_with_authentication_bugzilla = lambda x, y: None
GitHubMixin.fetch_data_from_url_with_authentication_github = lambda x, y: None

# Prevent unit tests from having huge amounts of logging from alembic.
logging.getLogger('alembic.runtime.migration').setLevel(logging.WARNING)

SCAN_BUILD_OUTPUT_DIR = 'scan-build-output'
LLVM_DIR = 'llvm-project'


def expectedFailure(f):
    """A unittest.expectedFailure-like decorator for twisted.trial.unittest"""
    f.todo = 'expectedFailure'
    return f


def mock_step(step, logs='', results=SUCCESS, stopped=False, properties=None):
    mock = create_autospec(step, spec_set=True)
    mock.configure_mock(
        logs=logs,
        results=results,
        stopped=stopped,
    )
    return mock


def mock_load_contributors(*args, **kwargs):
    return {
        'reviewer@apple.com': {'name': 'WebKit Reviewer', 'status': 'reviewer', 'email': 'reviewer@apple.com'},
        'webkit-reviewer': {'name': 'WebKit Reviewer', 'status': 'reviewer', 'email': 'reviewer@apple.com'},
        'WebKit Reviewer': {'status': 'reviewer'},
        'committer@webkit.org': {'name': 'WebKit Committer', 'status': 'committer', 'email': 'committer@webkit.org'},
        'webkit-commit-queue': {'name': 'WebKit Committer', 'status': 'committer', 'email': 'committer@webkit.org'},
        'WebKit Committer': {'status': 'committer'},
        'Myles C. Maxfield': {'status': 'reviewer'},
        'Abrar Protyasha': {'status': 'reviewer'},
    }, []


class ExpectMasterShellCommand(object):
    def __init__(self, command, workdir=None, env=None, usePTY=0):
        self.args = command
        self.usePTY = usePTY
        self.rc = None
        self.path = None
        self.logs = []

        if env is not None:
            self.env = env
        else:
            self.env = os.environ
        if workdir:
            self.path = os.path.join(os.getcwd(), workdir)

    def exit(self, rc):
        self.rc = rc

    @classmethod
    def log(self, name, value):
        return ('log', name, value)

    def __add__(self, other):
        if isinstance(other, int):
            self.rc = other
        elif isinstance(other, tuple) and other[0] == 'log':
            self.logs.append((other[1], other[2]))
        return self

    def __repr__(self):
        return f'ExpectMasterShellCommand({repr(self.args)})'


class BuildStepMixinAdditions(BuildStepMixin, TestReactorMixin):
    def setup_test_build_step(self):
        self.patch(reactor, 'spawnProcess', lambda *args, **kwargs: self._checkSpawnProcess(*args, **kwargs))
        self.patch(send_email, 'send_email', self._send_email)
        self.patch(send_email, 'get_email_ids', lambda c: ['test@webkit.org'])
        self.patch(BugzillaMixin, 'get_bugzilla_api_key', lambda f: 'TEST-API-KEY')
        self._emails_list = []
        self._expected_local_commands = []
        self.setup_test_reactor()

        self._temp_directory = tempfile.mkdtemp()
        os.chdir(self._temp_directory)
        self._expected_uploaded_files = []

        super().setup_test_build_step()

    def tear_down_test_build_step(self):
        shutil.rmtree(self._temp_directory)

    def fakeBuildFinished(self, text, results):
        self.build.text = text
        self.build.results = results

    def setup_step(self, step, *args, **kwargs):
        self.previous_steps = kwargs.get('previous_steps') or []
        if self.previous_steps:
            del kwargs['previous_steps']

        super().setup_step(step, *args, **kwargs)
        self.build.terminate = False
        self.build.stopped = False
        self.build.executedSteps = self.executedSteps
        self.build.buildFinished = self.fakeBuildFinished
        self._expected_added_urls = []
        self._expected_sources = None

    @property
    def executedSteps(self):
        return [step for step in self.previous_steps if not step.stopped]

    def setProperty(self, name, value, source='Unknown'):
        self.build.setProperty(name, value, source)

    def getProperty(self, name):
        return self.build.getProperty(name)

    def expectAddedURLs(self, added_urls):
        self._expected_added_urls = added_urls

    def expectUploadedFile(self, path):
        self._expected_uploaded_files.append(path)

    def expectLocalCommands(self, *expected_commands):
        assert False, "totally borked"
        self._expected_local_commands.extend(expected_commands)

    def expectRemoteCommands(self, *expected_commands):
        self.expect_commands(*expected_commands)

    def expectSources(self, expected_sources):
        self._expected_sources = expected_sources

    def _checkSpawnProcess(self, processProtocol, executable, args, env, path, usePTY, **kwargs):
        got = (executable, args, env, path, usePTY)
        if not self._expected_local_commands:
            self.fail(f'got local command {got} when no further commands were expected')
        local_command = self._expected_local_commands.pop(0)
        try:
            self.assertEqual(got, (local_command.args[0], local_command.args, local_command.env, local_command.path, local_command.usePTY))
        except AssertionError:
            log.err()
            raise
        for name, value in local_command.logs:
            if name == 'stdout':
                processProtocol.outReceived(value)
            elif name == 'stderr':
                processProtocol.errReceived(value)
        if local_command.rc != 0:
            value = error.ProcessTerminated(exitCode=local_command.rc)
        else:
            value = error.ProcessDone(None)
        processProtocol.processEnded(failure.Failure(value))

    def _added_files(self):
        results = []
        for dirpath, dirnames, filenames in os.walk(self._temp_directory):
            relative_root_path = os.path.relpath(dirpath, start=self._temp_directory)
            if relative_root_path == '.':
                relative_root_path = ''
            for name in filenames:
                results.append(os.path.join(relative_root_path, name))
        return results

    def _send_email(self, to_emails, subject, text, reference=''):
        if not to_emails:
            self._emails_list.append('Error: skipping email since no recipient is specified')
            return False
        if not subject or not text:
            self._emails_list.append('Error: skipping email since no subject or text is specified')
            return False
        self._emails_list.append(f'Subject: {subject}\nTo: {to_emails}\nReference: {reference}\nBody:\n\n{text}')
        return True

    def run_step(self):
        def check(result):
            self.assertEqual(self._expected_local_commands, [], 'assert all expected local commands were run')
            self.expectAddedURLs(self._expected_added_urls)
            self.assertEqual(self._added_files(), self._expected_uploaded_files)
            if self._expected_sources is not None:
                # Convert to dictionaries because assertEqual() only knows how to diff Python built-in types.
                actual_sources = sorted([source.asDict() for source in self.build.sources], key=operator.itemgetter('codebase'))
                expected_sources = sorted([source.asDict() for source in self._expected_sources], key=operator.itemgetter('codebase'))
                self.assertEqual(actual_sources, expected_sources)
        deferred_result = super().run_step()
        deferred_result.addCallback(check)
        return deferred_result


def uploadFileWithContentsOfString(string, timestamp=None):
    def behavior(command):
        writer = command.args['writer']
        writer.remote_write(string + '\n')
        writer.remote_close()
        if timestamp:
            writer.remote_utime(timestamp)
    return behavior


class TestGitHub(unittest.TestCase):
    def test_pr_url(self):
        self.assertEqual(
            GitHub.pr_url(1234),
            'https://github.com/WebKit/WebKit/pull/1234',
        )

    def test_pr_url_with_repository(self):
        self.assertEqual(
            GitHub.pr_url(1234, 'https://github.com/WebKit/WebKit'),
            'https://github.com/WebKit/WebKit/pull/1234',
        )

    def test_pr_url_with_invalid_repository(self):
        self.assertEqual(
            GitHub.pr_url(1234, 'https://github.example.com/WebKit/WebKit'),
            '',
        )

    def test_commit_url(self):
        self.assertEqual(
            GitHub.commit_url('936e3f7cab4a826519121a75bf4481fe56e727e2'),
            'https://github.com/WebKit/WebKit/commit/936e3f7cab4a826519121a75bf4481fe56e727e2',
        )

    def test_commit_url_with_repository(self):
        self.assertEqual(
            GitHub.commit_url('936e3f7cab4a826519121a75bf4481fe56e727e2', 'https://github.com/WebKit/WebKit'),
            'https://github.com/WebKit/WebKit/commit/936e3f7cab4a826519121a75bf4481fe56e727e2',
        )

    def test_commit_url_with_invalid_repository(self):
        self.assertEqual(
            GitHub.commit_url('936e3f7cab4a826519121a75bf4481fe56e727e2', 'https://github.example.com/WebKit/WebKit'),
            '',
        )


class TestGitHubMixin(unittest.TestCase):
    class Response(object):
        @staticmethod
        def fromText(data, url=None, headers=None):
            assert isinstance(data, str)
            return TestGitHubMixin.Response(text=data, url=url, headers=headers)

        @staticmethod
        def fromJson(data, url=None, headers=None, status_code=None):
            assert isinstance(data, list) or isinstance(data, dict)

            headers = headers or {}
            if 'Content-Type' not in headers:
                headers['Content-Type'] = 'text/json'

            return TestGitHubMixin.Response(text=json.dumps(data), url=url, headers=headers, status_code=status_code)

        def __init__(self, status_code=None, text=None, content=None, url=None, headers=None):
            if status_code is not None:
                self.status_code = status_code
            elif text is not None:
                self.status_code = 200
            else:
                self.status_code = 204  # No content

            if text and content:
                raise ValueError("Cannot define both 'text' and 'content'")
            elif text:
                self.content = text.encode('utf-8')
            else:
                self.content = content or b''

            self.url = url
            self.headers = headers or {}

            if 'Content-Type' not in self.headers:
                self.headers['Content-Type'] = 'text'
            if 'Content-Length' not in self.headers:
                self.headers['Content-Length'] = len(self.content) if self.content else 0

        @property
        def text(self):
            return self.content.decode('utf-8')

        def json(self):
            return json.loads(self.text)

    @defer.inlineCallbacks
    def test_no_reviewers(self):
        logs = dict(stdio=[])
        mixin = GitHubMixin()
        mixin.fetch_data_from_url_with_authentication_github = lambda url: defer.succeed(self.Response.fromJson([]))
        mixin._addToLog = lambda logName, message, logs=logs: logs[logName].append(message)
        reviewers = yield mixin.get_reviewers(1234)
        self.assertEqual(reviewers, [])
        self.assertEqual(logs, dict(stdio=[]))

    @defer.inlineCallbacks
    def test_single_review(self):
        logs = dict(stdio=[])
        mixin = GitHubMixin()
        mixin.fetch_data_from_url_with_authentication_github = lambda url: defer.succeed(self.Response.fromJson([
            dict(id=1, state='APPROVED', user=dict(login='webkit-reviewer')),
        ], url=url))
        mixin._addToLog = lambda logName, message, logs=logs: logs[logName].append(message)
        reviewers = yield mixin.get_reviewers(1234)
        self.assertEqual(reviewers, ['webkit-reviewer'])
        self.assertEqual(logs, dict(stdio=[]))

    @defer.inlineCallbacks
    def test_multipe_reviews(self):
        logs = dict(stdio=[])
        mixin = GitHubMixin()
        mixin.fetch_data_from_url_with_authentication_github = lambda url: defer.succeed(self.Response.fromJson([
            dict(id=1, state='APPROVED', user=dict(login='webkit-reviewer')),
            dict(id=2, state='COMMENTED', user=dict(login='webkit-committer')),
            dict(id=3, state='APPROVED', user=dict(login='webkit-committer')),
        ], url=url))
        mixin._addToLog = lambda logName, message, logs=logs: logs[logName].append(message)
        reviewers = yield mixin.get_reviewers(1234)
        self.assertEqual(reviewers, ['webkit-committer', 'webkit-reviewer'])
        self.assertEqual(logs, dict(stdio=[]))

    @defer.inlineCallbacks
    def test_retracted_review(self):
        logs = dict(stdio=[])
        mixin = GitHubMixin()
        mixin.fetch_data_from_url_with_authentication_github = lambda url: defer.succeed(self.Response.fromJson([
            dict(id=1, state='APPROVED', user=dict(login='webkit-reviewer')),
            dict(id=2, state='CHANGES_REQUESTED', user=dict(login='webkit-reviewer')),
        ], url=url))
        mixin._addToLog = lambda logName, message, logs=logs: logs[logName].append(message)
        reviewers = yield mixin.get_reviewers(1234)
        self.assertEqual(reviewers, [])
        self.assertEqual(logs, dict(stdio=[]))

    @defer.inlineCallbacks
    def test_pagination(self):
        logs = dict(stdio=[])
        mixin = GitHubMixin()
        mixin.fetch_data_from_url_with_authentication_github = lambda url: defer.succeed(self.Response.fromJson([
            dict(id=101, state='APPROVED', user=dict(login='webkit-committer')),
        ], url=url)) if 'page=2' in url else defer.succeed(self.Response.fromJson([
            dict(id=1, state='APPROVED', user=dict(login='webkit-reviewer')),
        ] + [
            dict(id=i, state='COMMENTED', user=dict(login='webkit-reviewer')) for i in range(1, 100)
        ], url=url))
        mixin._addToLog = lambda logName, message, logs=logs: logs[logName].append(message)
        reviewers = yield mixin.get_reviewers(1234)
        self.assertEqual(reviewers, ['webkit-committer', 'webkit-reviewer'])
        self.assertEqual(logs, dict(stdio=[]))

    @defer.inlineCallbacks
    def test_reviewers_invalid_response(self):
        logs = dict(stdio=[])
        mixin = GitHubMixin()
        mixin.fetch_data_from_url_with_authentication_github = lambda url: defer.succeed(self.Response.fromJson({}, url=url))
        mixin._addToLog = lambda logName, message, logs=logs: logs[logName].append(message)
        reviewers = yield mixin.get_reviewers(1234)
        self.assertEqual(reviewers, [])
        self.assertEqual(logs, dict(stdio=[]))

    @defer.inlineCallbacks
    def test_reviewers_error(self):
        logs = dict(stdio=[])
        mixin = GitHubMixin()
        mixin.fetch_data_from_url_with_authentication_github = lambda url: defer.succeed(None)
        mixin._addToLog = lambda logName, message, logs=logs: logs[logName].append(message)
        reviewers = yield mixin.get_reviewers(1234)
        self.assertEqual(reviewers, [])
        self.assertEqual(logs, dict(stdio=[]))


class TestStepNameShouldBeValidIdentifier(BuildStepMixinAdditions, unittest.TestCase):
    def test_step_names_are_valid(self):
        from . import steps
        build_step_classes = inspect.getmembers(steps, inspect.isclass)
        for build_step in build_step_classes:
            if 'name' in vars(build_step[1]):
                name = build_step[1].name
                self.assertFalse(' ' in name, f'step name "{name}" contain space.')
                self.assertTrue(buildbot_identifiers.ident_re.match(name), f'step name "{name}" is not a valid buildbot identifier.')


class TestCheckStyle(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success_internal(self):
        self.setup_step(CheckStyle())
        self.setProperty('try-codebase', 'internal')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/check-webkit-style'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='check-webkit-style')
        return self.run_step()

    def test_failure_unknown_try_codebase(self):
        self.setup_step(CheckStyle())
        self.setProperty('try-codebase', 'foo')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/check-webkit-style'],
                        )
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='check-webkit-style (failure)')
        return self.run_step()

    def test_failures_with_style_issues(self):
        self.setup_step(CheckStyle())
        self.setProperty('try-codebase', 'internal')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/check-webkit-style'],
                        )
            .log('stdio', stdout='''ERROR: Source/WebCore/layout/FloatingContext.cpp:36:  Code inside a namespace should not be indented.  [whitespace/indent] [4]
ERROR: Source/WebCore/layout/FormattingContext.h:94:  Weird number of spaces at line-start.  Are you using a 4-space indent?  [whitespace/indent] [3]
ERROR: Source/WebCore/layout/LayoutContext.cpp:52:  Place brace on its own line for function definitions.  [whitespace/braces] [4]
ERROR: Source/WebCore/layout/LayoutContext.cpp:55:  Extra space before last semicolon. If this should be an empty statement, use { } instead.  [whitespace/semicolon] [5]
ERROR: Source/WebCore/layout/LayoutContext.cpp:60:  Tab found; better to use spaces  [whitespace/tab] [1]
ERROR: Source/WebCore/layout/Verification.cpp:88:  Missing space before ( in while(  [whitespace/parens] [5]
Total errors found: 8 in 48 files''')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='8 style errors')
        return self.run_step()

    def test_failures_no_style_issues(self):
        self.setup_step(CheckStyle())
        self.setProperty('try-codebase', 'internal')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/check-webkit-style'],
                        )
            .log('stdio', stdout='Total errors found: 0 in 6 files')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='check-webkit-style')
        return self.run_step()

    def test_failures_no_changes(self):
        self.setup_step(CheckStyle())
        self.setProperty('try-codebase', 'internal')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/check-webkit-style'],
                        )
            .log('stdio', stdout='Total errors found: 0 in 0 files')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='check-webkit-style (failure)')
        return self.run_step()


class TestRunBindingsTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'bindings_test_results.json'
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RunBindingsTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/run-bindings-tests', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed bindings tests')
        return self.run_step()

    def test_failure(self):
        self.setup_step(RunBindingsTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/run-bindings-tests', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        )
            .log('stdio', stdout='FAIL: (JS) JSTestInterface.cpp')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='bindings-tests (failure)')
        return self.run_step()


class TestRunWebKitPerlTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(RunWebKitPerlTests())

    def test_success(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['perl', 'Tools/Scripts/test-webkitperl'],
                        timeout=120,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed webkitperl tests')
        return self.run_step()

    def test_failure(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['perl', 'Tools/Scripts/test-webkitperl'],
                        timeout=120,
                        )
            .log('stdio', stdout='''Failed tests:  1-3, 5-7, 9, 11-13
Files=40, Tests=630,  4 wallclock secs ( 0.16 usr  0.09 sys +  2.78 cusr  0.64 csys =  3.67 CPU)
Result: FAIL
Failed 1/40 test programs. 10/630 subtests failed.''')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed webkitperl tests')
        return self.run_step()


class TestRunWebKitPyTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'webkitpy_test_results.json'
        self.json_with_failure = '''{"failures": [{"name": "webkitpy.port.wpe_unittest.WPEPortTest.test_diff_image"}]}\n'''
        self.json_with_errros = '''{"failures": [],
"errors": [{"name": "webkitpy.style.checkers.cpp_unittest.WebKitStyleTest.test_os_version_checks"}, {"name": "webkitpy.port.win_unittest.WinPortTest.test_diff_image__missing_actual"}]}\n'''
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RunWebKitPyTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed webkitpy tests')
        return self.run_step()

    def test_unexpected_failure(self):
        self.setup_step(RunWebKitPyTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        )
            .log('stdio', stdout='''Ran 1744 tests in 5.913s
FAILED (failures=1, errors=0)''')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='webkitpy-tests (failure)')
        return self.run_step()

    def test_failure(self):
        self.setup_step(RunWebKitPyTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        )
            .log('json', stdout=self.json_with_failure)
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Found 1 webkitpy test failure: webkitpy.port.wpe_unittest.WPEPortTest.test_diff_image')
        return self.run_step()

    def test_errors(self):
        self.setup_step(RunWebKitPyTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        )
            .log('json', stdout=self.json_with_errros)
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Found 2 webkitpy test failures: webkitpy.style.checkers.cpp_unittest.WebKitStyleTest.test_os_version_checks, webkitpy.port.win_unittest.WinPortTest.test_diff_image__missing_actual')
        return self.run_step()

    def test_lot_of_failures(self):
        self.setup_step(RunWebKitPyTests())
        json_with_failures = json.dumps({'failures': [{f'name': f'test{i}'} for i in range(1, 31)]})

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        )
            .log('json', stdout=json_with_failures)
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Found 30 webkitpy test failures: test1, test2, test3, test4, test5, test6, test7, test8, test9, test10 ...')
        return self.run_step()


class TestRunBuildbotCheckConfigForEWS(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RunBuildbotCheckConfigForEWS())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport/ews-build',
                        timeout=120,
                        log_environ=False,
                        command=['python3', '../buildbot-cmd', 'checkconfig'],
                        env={'LC_CTYPE': 'en_US.UTF-8'}
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed buildbot checkconfig')
        return self.run_step()

    def test_failure(self):
        self.setup_step(RunBuildbotCheckConfigForEWS())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport/ews-build',
                        timeout=120,
                        log_environ=False,
                        command=['python3', '../buildbot-cmd', 'checkconfig'],
                        env={'LC_CTYPE': 'en_US.UTF-8'}
                        )
            .log('stdio', stdout='Configuration Errors:  builder(s) iOS-14-Debug-Build-EWS have no schedulers to drive them')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed buildbot checkconfig')
        return self.run_step()


class TestRunBuildbotCheckConfigForBuildWebKit(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RunBuildbotCheckConfigForBuildWebKit())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport/build-webkit-org',
                        timeout=120,
                        log_environ=False,
                        command=['python3', '../buildbot-cmd', 'checkconfig'],
                        env={'LC_CTYPE': 'en_US.UTF-8'}
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed buildbot checkconfig')
        return self.run_step()

    def test_failure(self):
        self.setup_step(RunBuildbotCheckConfigForBuildWebKit())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport/build-webkit-org',
                        timeout=120,
                        log_environ=False,
                        command=['python3', '../buildbot-cmd', 'checkconfig'],
                        env={'LC_CTYPE': 'en_US.UTF-8'}
                        )
            .log('stdio', stdout='Configuration Errors:  builder(s) Apple-iOS-14-Release-Build have no schedulers to drive them')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed buildbot checkconfig')
        return self.run_step()


class TestRunEWSUnitTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RunEWSUnitTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport',
                        timeout=120,
                        log_environ=False,
                        command=['python3', 'runUnittests.py', 'ews-build', '--autoinstall'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed EWS unit tests')
        return self.run_step()

    def test_failure(self):
        self.setup_step(RunEWSUnitTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport',
                        timeout=120,
                        log_environ=False,
                        command=['python3', 'runUnittests.py', 'ews-build', '--autoinstall'],
                        )
            .log('stdio', stdout='Unhandled Error. Traceback (most recent call last): Keys in cmd missing from expectation: [logfiles.json]')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed EWS unit tests')
        return self.run_step()


class TestRunResultsdbpyTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RunResultsdbpyTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=900,
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/libraries/resultsdbpy/resultsdbpy/run-tests', '--verbose', '--no-selenium', '--fast-tests'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed resultsdbpy unit tests')
        return self.run_step()

    def test_failure(self):
        self.setup_step(RunResultsdbpyTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=900,
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/libraries/resultsdbpy/resultsdbpy/run-tests', '--verbose', '--no-selenium', '--fast-tests'],
                        )
            .log('stdio', stdout='FAILED (errors=5, skipped=224)')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed resultsdbpy unit tests')
        return self.run_step()


class TestRunBuildWebKitOrgUnitTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RunBuildWebKitOrgUnitTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport',
                        timeout=120,
                        log_environ=False,
                        command=['python3', 'runUnittests.py', 'build-webkit-org', '--autoinstall'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed build.webkit.org unit tests')
        return self.run_step()

    def test_failure(self):
        self.setup_step(RunBuildWebKitOrgUnitTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport',
                        timeout=120,
                        log_environ=False,
                        command=['python3', 'runUnittests.py', 'build-webkit-org', '--autoinstall'],
                        )
            .log('stdio', stdout='Unhandled Error. Traceback (most recent call last): Keys in cmd missing from expectation: [logfiles.json]')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed build.webkit.org unit tests')
        return self.run_step()


class TestKillOldProcesses(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(KillOldProcesses())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/kill-old-processes', 'buildbot'],
                        log_environ=False,
                        timeout=120,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Killed old processes')
        return self.run_step()

    def test_failure(self):
        self.setup_step(KillOldProcesses())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/kill-old-processes', 'buildbot'],
                        log_environ=False,
                        timeout=120,
                        )
            .log('stdio', stdout='Unexpected error.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to kill old processes')
        return self.run_step()


class TestTriggerCrashLogSubmission(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(TriggerCrashLogSubmission())
        self.assertEqual(TriggerCrashLogSubmission.haltOnFailure, False)
        self.assertEqual(TriggerCrashLogSubmission.flunkOnFailure, False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/trigger-crash-log-submission'],
                        log_environ=False,
                        timeout=60,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Triggered crash log submission')
        return self.run_step()

    def test_failure(self):
        self.setup_step(TriggerCrashLogSubmission())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/trigger-crash-log-submission'],
                        log_environ=False,
                        timeout=60,
                        )
            .log('stdio', stdout='Unexpected error.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to trigger crash log submission')
        return self.run_step()


class TestWaitForCrashCollection(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(WaitForCrashCollection())
        self.assertEqual(WaitForCrashCollection.haltOnFailure, False)
        self.assertEqual(WaitForCrashCollection.flunkOnFailure, False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/wait-for-crash-collection', '--timeout', str(5 * 60)],
                        log_environ=False,
                        timeout=360,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Crash collection has quiesced')
        return self.run_step()

    def test_failure(self):
        self.setup_step(WaitForCrashCollection())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/wait-for-crash-collection', '--timeout', str(5 * 60)],
                        log_environ=False,
                        timeout=360,
                        )
            .log('stdio', stdout='Unexpected error.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Crash log collection process still running')
        return self.run_step()


class TestCleanBuild(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CleanBuild())
        self.setProperty('fullPlatform', 'ios-11')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/clean-build', '--platform=ios-11', '--release'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Deleted WebKitBuild directory')
        return self.run_step()

    def test_failure(self):
        self.setup_step(CleanBuild())
        self.setProperty('fullPlatform', 'ios-simulator-11')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/clean-build', '--platform=ios-simulator-11', '--debug'],
                        )
            .log('stdio', stdout='Unexpected error.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Deleted WebKitBuild directory (failure)')
        return self.run_step()


class TestCleanDerivedSources(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CleanDerivedSources())
        self.setProperty('platform', 'gtk')
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/clean-webkit', '--derived-sources-only'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Cleaned derived sources directories')
        return self.run_step()


class TestCleanUpGitIndexLock(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CleanUpGitIndexLock())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=120,
                        log_environ=False,
                        command=['rm', '-f', '.git/index.lock'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Deleted .git/index.lock')
        return self.run_step()

    def test_success_different_workdir(self):
        self.setup_step(CleanUpGitIndexLock(workdir='build/OpenSource'))
        self.expectRemoteCommands(
            ExpectShell(workdir='build/OpenSource',
                        timeout=120,
                        log_environ=False,
                        command=['rm', '-f', '.git/index.lock'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Deleted .git/index.lock')
        return self.run_step()

    def test_success_ios(self):
        self.setup_step(CleanUpGitIndexLock())
        self.setProperty('platform', 'ios-16')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=120,
                        log_environ=False,
                        command=['rm', '-f', '.git/index.lock'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Deleted .git/index.lock')
        return self.run_step()

    def test_success_win(self):
        self.setup_step(CleanUpGitIndexLock())
        self.setProperty('platform', 'win')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=120,
                        log_environ=False,
                        command=['del', r'.git\index.lock'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Deleted .git/index.lock')
        return self.run_step()

    def test_failure(self):
        self.setup_step(CleanUpGitIndexLock())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=120,
                        log_environ=False,
                        command=['rm', '-f', '.git/index.lock'],
                        )
            .log('stdio', stdout='Unexpected error.')
            .exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='Deleted .git/index.lock (failure)')
        return self.run_step()


class TestInstallGtkDependencies(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(InstallGtkDependencies())
        self.setProperty('configuration', 'release')
        self.assertEqual(InstallGtkDependencies.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['perl', 'Tools/Scripts/update-webkitgtk-libs', '--release'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Updated gtk dependencies')
        return self.run_step()

    def test_failure(self):
        self.setup_step(InstallGtkDependencies())
        self.setProperty('configuration', 'release')
        self.assertEqual(InstallGtkDependencies.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['perl', 'Tools/Scripts/update-webkitgtk-libs', '--release'],
                        )
            .log('stdio', stdout='Unexpected error.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Updated gtk dependencies (failure)')
        return self.run_step()


class TestInstallWpeDependencies(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(InstallWpeDependencies())
        self.setProperty('configuration', 'release')
        self.assertEqual(InstallWpeDependencies.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['perl', 'Tools/Scripts/update-webkitwpe-libs', '--release'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Updated wpe dependencies')
        return self.run_step()

    def test_failure(self):
        self.setup_step(InstallWpeDependencies())
        self.setProperty('configuration', 'release')
        self.assertEqual(InstallWpeDependencies.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['perl', 'Tools/Scripts/update-webkitwpe-libs', '--release'],
                        )
            .log('stdio', stdout='Unexpected error.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Updated wpe dependencies (failure)')
        return self.run_step()


class TestCompileWebKit(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CompileWebKit())
        self.setProperty('platform', 'ios')
        self.setProperty('fullPlatform', 'ios-simulator-11')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=3600,
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'perl Tools/Scripts/build-webkit --release -hideShellScriptEnvironment WK_VALIDATE_DEPENDENCIES=YES --ios-simulator 2>&1 | perl Tools/Scripts/filter-build-webkit -logfile build-log.txt'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Compiled WebKit')
        return self.run_step()

    def test_success_architecture(self):
        self.setup_step(CompileWebKit())
        self.setProperty('platform', 'mac')
        self.setProperty('fullPlatform', 'mac-monterey')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64 arm64')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=3600,
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'perl Tools/Scripts/build-webkit --release --architecture "x86_64 arm64" -hideShellScriptEnvironment WK_VALIDATE_DEPENDENCIES=YES 2>&1 | perl Tools/Scripts/filter-build-webkit -logfile build-log.txt'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Compiled WebKit')
        return self.run_step()

    def test_success_gtk(self):
        self.setup_step(CompileWebKit())
        self.setProperty('platform', 'gtk')
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=3600,
                        log_environ=False,
                        command=['perl', 'Tools/Scripts/build-webkit', '--release', '--gtk'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Compiled WebKit')
        return self.run_step()

    def test_success_wpe(self):
        self.setup_step(CompileWebKit())
        self.setProperty('platform', 'wpe')
        self.setProperty('fullPlatform', 'wpe')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=3600,
                        log_environ=False,
                        command=['perl', 'Tools/Scripts/build-webkit', '--release', '--wpe'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Compiled WebKit')
        return self.run_step()

    def test_failure(self):
        self.setup_step(CompileWebKit())
        self.setProperty('platform', 'mac')
        self.setProperty('fullPlatform', 'mac-monterey')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=3600,
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'perl Tools/Scripts/build-webkit --debug -hideShellScriptEnvironment WK_VALIDATE_DEPENDENCIES=YES 2>&1 | perl Tools/Scripts/filter-build-webkit -logfile build-log.txt'],
                        )
            .log('stdio', stdout='1 error generated.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to compile WebKit')
        return self.run_step()

    def test_skip_for_revert_patches_on_commit_queue(self):
        self.setup_step(CompileWebKit())
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('configuration', 'debug')
        self.setProperty('fast_commit_queue', True)
        self.expect_outcome(result=SKIPPED, state_string='Skipped compiling WebKit in fast-cq mode')
        return self.run_step()


class TestCompileWebKitWithoutChange(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CompileWebKitWithoutChange())
        self.setProperty('platform', 'ios')
        self.setProperty('fullPlatform', 'ios-simulator-11')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=3600,
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'perl Tools/Scripts/build-webkit --release -hideShellScriptEnvironment WK_VALIDATE_DEPENDENCIES=YES --ios-simulator 2>&1 | perl Tools/Scripts/filter-build-webkit -logfile build-log.txt'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Compiled WebKit')
        return self.run_step()

    def test_failure(self):
        self.setup_step(CompileWebKitWithoutChange())
        self.setProperty('platform', 'mac')
        self.setProperty('fullPlatform', 'mac-monterey')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=3600,
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'perl Tools/Scripts/build-webkit --debug -hideShellScriptEnvironment WK_VALIDATE_DEPENDENCIES=YES 2>&1 | perl Tools/Scripts/filter-build-webkit -logfile build-log.txt'],
                        )
            .log('stdio', stdout='1 error generated.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to compile WebKit')
        return self.run_step()


class TestAnalyzeCompileWebKitResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        GitHub.credentials = lambda user=None: ('webkit-commit-queue', 'password')
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_patch_with_build_failure(self):
        previous_steps = [
            mock_step(CompileWebKit(), results=FAILURE),
            mock_step(CompileWebKitWithoutChange(), results=SUCCESS),
        ]
        self.setup_step(AnalyzeCompileWebKitResults(), previous_steps=previous_steps)
        self.setProperty('patch_id', '1234')
        self.expect_outcome(result=FAILURE, state_string='Patch 1234 does not build (failure)')
        rc = self.run_step()
        self.expect_property('comment_text', None)
        self.expect_property('build_finish_summary', 'Patch 1234 does not build')
        return rc

    def test_pull_request_with_build_failure(self):
        previous_steps = [
            mock_step(CompileWebKit(), results=FAILURE),
            mock_step(CompileWebKitWithoutChange(), results=SUCCESS),
        ]
        self.setup_step(AnalyzeCompileWebKitResults(), previous_steps=previous_steps)
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expect_outcome(result=FAILURE, state_string='Hash 7496f8ec for PR 1234 does not build (failure)')
        rc = self.run_step()
        self.expect_property('build_finish_summary', 'Hash 7496f8ec for PR 1234 does not build')
        return rc

    def test_patch_with_build_failure_on_commit_queue(self):
        previous_steps = [
            mock_step(CompileWebKit(), results=FAILURE),
            mock_step(CompileWebKitWithoutChange(), results=SUCCESS),
        ]
        self.setup_step(AnalyzeCompileWebKitResults(), previous_steps=previous_steps)
        self.setProperty('patch_id', '1234')
        self.setProperty('buildername', 'commit-queue')
        self.expect_outcome(result=FAILURE, state_string='Patch 1234 does not build (failure)')
        rc = self.run_step()
        self.expect_property('comment_text', 'Patch 1234 does not build')
        self.expect_property('build_finish_summary', 'Patch 1234 does not build')
        return rc

    @expectedFailure
    def test_patch_with_trunk_failure(self):
        previous_steps = [
            mock_step(CompileWebKit(), results=FAILURE),
            mock_step(CompileWebKitWithoutChange(), results=FAILURE),
        ]
        self.setup_step(AnalyzeCompileWebKitResults(), previous_steps=previous_steps)
        self.expect_outcome(result=FAILURE, state_string='Unable to build WebKit without patch, retrying build (failure)')
        return self.run_step()

    @expectedFailure
    def test_pr_with_main_failure(self):
        previous_steps = [
            mock_step(CompileWebKit(), results=FAILURE),
            mock_step(CompileWebKitWithoutChange(), results=FAILURE),
        ]
        self.setup_step(AnalyzeCompileWebKitResults(), previous_steps=previous_steps)
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.expect_outcome(result=FAILURE, state_string='Unable to build WebKit without PR, retrying build (failure)')
        return self.run_step()

    @expectedFailure
    def test_pr_with_branch_failure(self):
        previous_steps = [
            mock_step(CompileWebKit(), results=FAILURE),
            mock_step(CompileWebKitWithoutChange(), results=FAILURE),
        ]
        self.setup_step(AnalyzeCompileWebKitResults(), previous_steps=previous_steps)
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'safari-7614-branch')
        self.expect_outcome(result=FAILURE, state_string='Unable to build WebKit without PR, please check manually (failure)')
        return self.run_step()

    def test_filter_logs_containing_error(self):
        logs = 'In file included from WebCore/unified-sources/UnifiedSource263.cpp:4:\nImageBufferIOSurfaceBackend.cpp:108:30: error: definition of implicitly declared destructor'
        expected_output = 'ImageBufferIOSurfaceBackend.cpp:108:30: error: definition of implicitly declared destructor'
        output = AnalyzeCompileWebKitResults().filter_logs_containing_error(logs)
        self.assertEqual(expected_output, output)

    def test_filter_logs_containing_error_with_too_many_errors(self):
        logs = 'Error:1\nError:2\nerror:3\nerror:4\nerror:5\nrandom-string\nerror:6\nerror:7\nerror8\nerror:9\nerror:10\nerror:11\nerror:12\nerror:13'
        expected_output = 'error:3\nerror:4\nerror:5\nerror:6\nerror:7\nerror:9\nerror:10\nerror:11\nerror:12\nerror:13'
        output = AnalyzeCompileWebKitResults().filter_logs_containing_error(logs)
        self.assertEqual(expected_output, output)

    def test_filter_logs_containing_error_with_no_error(self):
        logs = 'CompileC /Volumes/Data/worker/macOS-Catalina-Release-Build-EWS'
        expected_output = ''
        output = AnalyzeCompileWebKitResults().filter_logs_containing_error(logs)
        self.assertEqual(expected_output, output)


class TestCompileJSC(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CompileJSC())
        self.setProperty('platform', 'mac')
        self.setProperty('fullPlatform', 'mac-monterey')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=3600,
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'perl Tools/Scripts/build-jsc --release WK_VALIDATE_DEPENDENCIES=YES 2>&1 | perl Tools/Scripts/filter-build-webkit -logfile build-log.txt'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Compiled JSC')
        return self.run_step()

    def test_failure(self):
        self.setup_step(CompileJSC())
        self.setProperty('platform', 'mac')
        self.setProperty('fullPlatform', 'mac-monterey')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=3600,
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'perl Tools/Scripts/build-jsc --debug WK_VALIDATE_DEPENDENCIES=YES 2>&1 | perl Tools/Scripts/filter-build-webkit -logfile build-log.txt'],
                        )
            .log('stdio', stdout='1 error generated.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to compile JSC')
        return self.run_step()


class TestCompileJSCWithoutChange(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CompileJSCWithoutChange())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=3600,
                        log_environ=False,
                        command=['perl', 'Tools/Scripts/build-jsc', '--release'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Compiled JSC')
        return self.run_step()

    def test_failure(self):
        self.setup_step(CompileJSCWithoutChange())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=3600,
                        log_environ=False,
                        command=['perl', 'Tools/Scripts/build-jsc', '--debug'],
                        )
            .log('stdio', stdout='1 error generated.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to compile JSC')
        return self.run_step()


class TestRunJavaScriptCoreTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'jsc_results.json'
        self.jsc_masm_failure = '''{"allDFGTestsPassed":true,"allMasmTestsPassed":false,"allB3TestsPassed":true,"allAirTestsPassed":true,"stressTestFailures":[],"allApiTestsPassed":true}\n'''
        self.jsc_b3_and_stress_test_failure = '''{"allDFGTestsPassed":true,"allMasmTestsPassed":true,"allB3TestsPassed":false,"allAirTestsPassed":true,"allApiTestsPassed":true,"stressTestFailures":["stress/weakset-gc.js"]}\n'''
        self.jsc_dfg_air_and_stress_test_failure = '''{"allDFGTestsPassed":false,"allMasmTestsPassed":true,"allB3TestsPassed":true,"allAirTestsPassed":false,"allApiTestsPassed":true,"stressTestFailures":["stress/weakset-gc.js"]}\n'''
        self.jsc_single_stress_test_failure = '''{"allDFGTestsPassed":true,"allMasmTestsPassed":true,"allB3TestsPassed":true,"allAirTestsPassed":true,"stressTestFailures":["stress/switch-on-char-llint-rope.js.dfg-eager"],"allApiTestsPassed":true}\n'''
        self.jsc_multiple_stress_test_failures = '''{"allDFGTestsPassed":true,"allMasmTestsPassed":true,"allB3TestsPassed":true,"allAirTestsPassed":true,"stressTestFailures":["stress/switch-on-char-llint-rope.js.dfg-eager","stress/switch-on-char-llint-rope.js.dfg-eager-no-cjit-validate","stress/switch-on-char-llint-rope.js.eager-jettison-no-cjit","stress/switch-on-char-llint-rope.js.ftl-eager","stress/switch-on-char-llint-rope.js.ftl-eager-no-cjit","stress/switch-on-char-llint-rope.js.ftl-eager-no-cjit-b3o1","stress/switch-on-char-llint-rope.js.ftl-no-cjit-b3o0","stress/switch-on-char-llint-rope.js.ftl-no-cjit-no-inline-validate","stress/switch-on-char-llint-rope.js.ftl-no-cjit-no-put-stack-validate","stress/switch-on-char-llint-rope.js.ftl-no-cjit-small-pool","stress/switch-on-char-llint-rope.js.ftl-no-cjit-validate-sampling-profiler","stress/switch-on-char-llint-rope.js.no-cjit-collect-continuously","stress/switch-on-char-llint-rope.js.no-cjit-validate-phases","stress/switch-on-char-llint-rope.js.no-ftl"],"allApiTestsPassed":true}\n'''
        self.jsc_passed_with_flaky = '''{"allDFGTestsPassed":true,"allMasmTestsPassed":true,"allB3TestsPassed":true,"allAirTestsPassed":true,"stressTestFailures":[],"flakyAndPassed":{"stress/switch-on-char-llint-rope.js.default":{"P":"7","T":"10"}},"allApiTestsPassed":true}\n'''
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self, platform=None, fullPlatform=None, configuration=None):
        self.setup_step(RunJavaScriptCoreTests())
        self.prefix = RunJavaScriptCoreTests.prefix
        self.command_extra = RunJavaScriptCoreTests.command_extra
        if platform:
            self.setProperty('platform', platform)
        if fullPlatform:
            self.setProperty('fullPlatform', fullPlatform)
        if configuration:
            self.setProperty('configuration', configuration)

    def test_success(self):
        self.configureStep(platform='mac', fullPlatform='mac-highsierra', configuration='release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=3 * 60 * 60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'perl Tools/Scripts/run-javascriptcore-tests --no-build --no-fail-fast --json-output={self.jsonFileName} --release --treat-failing-as-flaky=0.6,10,200 2>&1 | Tools/Scripts/filter-test-logs jsc'],
                        logfiles={'json': self.jsonFileName},
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed JSC tests')
        return self.run_step()

    def test_remote_success(self):
        self.configureStep(platform='jsc-only', fullPlatform='jsc-only', configuration='release')
        self.setProperty('remotes', 'remote-machines.json')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=3 * 60 * 60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'perl Tools/Scripts/run-javascriptcore-tests --no-build --no-fail-fast --json-output={self.jsonFileName} --release --remote-config-file=remote-machines.json --no-testmasm --no-testair --no-testb3 --no-testdfg --no-testapi --memory-limited --verbose --jsc-only --treat-failing-as-flaky=0.6,10,200 2>&1 | Tools/Scripts/filter-test-logs jsc'],
                        logfiles={'json': self.jsonFileName},
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed JSC tests')
        return self.run_step()

    def test_failure(self):
        self.configureStep(platform='mac', fullPlatform='mac-highsierra', configuration='debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=3 * 60 * 60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'perl Tools/Scripts/run-javascriptcore-tests --no-build --no-fail-fast --json-output={self.jsonFileName} --debug --treat-failing-as-flaky=0.6,10,200 2>&1 | Tools/Scripts/filter-test-logs jsc'],
                        logfiles={'json': self.jsonFileName},
                        )
            .log('stdio', stdout='9 failures found.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='jscore-tests (failure)')
        return self.run_step()

    @expectedFailure
    def test_single_stress_test_failure(self):
        self.configureStep(platform='mac', fullPlatform='mac-highsierra', configuration='debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=3 * 60 * 60,
                        logfiles={'json': self.jsonFileName},
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'perl Tools/Scripts/run-javascriptcore-tests --no-build --no-fail-fast --json-output={self.jsonFileName} --debug --treat-failing-as-flaky=0.6,10,200 2>&1 | Tools/Scripts/filter-test-logs jsc'],
                        )
            .exit(2)
            .log('json', stdout=self.jsc_single_stress_test_failure),
        )
        self.expect_outcome(result=FAILURE, state_string='Found 1 jsc stress test failure: stress/switch-on-char-llint-rope.js.dfg-eager')
        rc = self.run_step()
        self.expect_property(self.prefix + 'stress_test_failures', ['stress/switch-on-char-llint-rope.js.dfg-eager'])
        self.expect_property(self.prefix + 'binary_failures', None)
        return rc

    @expectedFailure
    def test_lot_of_stress_test_failure(self):
        self.configureStep(platform='mac', fullPlatform='mac-highsierra', configuration='debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=3 * 60 * 60,
                        logfiles={'json': self.jsonFileName},
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'perl Tools/Scripts/run-javascriptcore-tests --no-build --no-fail-fast --json-output={self.jsonFileName} --debug --treat-failing-as-flaky=0.6,10,200 2>&1 | Tools/Scripts/filter-test-logs jsc'],
                        )
            .exit(2)
            .log('json', stdout=self.jsc_multiple_stress_test_failures),
        )
        self.expect_outcome(result=FAILURE, state_string='Found 14 jsc stress test failures: stress/switch-on-char-llint-rope.js.dfg-eager, stress/switch-on-char-llint-rope.js.dfg-eager-no-cjit-validate, stress/switch-on-char-llint-rope.js.eager-jettison-no-cjit, stress/switch-on-char-llint-rope.js.ftl-eager, stress/switch-on-char-llint-rope.js.ftl-eager-no-cjit ...')
        rc = self.run_step()
        self.expect_property(self.prefix + 'stress_test_failures', ["stress/switch-on-char-llint-rope.js.dfg-eager", "stress/switch-on-char-llint-rope.js.dfg-eager-no-cjit-validate", "stress/switch-on-char-llint-rope.js.eager-jettison-no-cjit", "stress/switch-on-char-llint-rope.js.ftl-eager", "stress/switch-on-char-llint-rope.js.ftl-eager-no-cjit", "stress/switch-on-char-llint-rope.js.ftl-eager-no-cjit-b3o1", "stress/switch-on-char-llint-rope.js.ftl-no-cjit-b3o0", "stress/switch-on-char-llint-rope.js.ftl-no-cjit-no-inline-validate", "stress/switch-on-char-llint-rope.js.ftl-no-cjit-no-put-stack-validate", "stress/switch-on-char-llint-rope.js.ftl-no-cjit-small-pool", "stress/switch-on-char-llint-rope.js.ftl-no-cjit-validate-sampling-profiler", "stress/switch-on-char-llint-rope.js.no-cjit-collect-continuously", "stress/switch-on-char-llint-rope.js.no-cjit-validate-phases", "stress/switch-on-char-llint-rope.js.no-ftl"])
        self.expect_property(self.prefix + 'binary_failures', None)
        return rc

    @expectedFailure
    def test_masm_failure(self):
        self.configureStep(platform='mac', fullPlatform='mac-highsierra', configuration='debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=3 * 60 * 60,
                        logfiles={'json': self.jsonFileName},
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'perl Tools/Scripts/run-javascriptcore-tests --no-build --no-fail-fast --json-output={self.jsonFileName} --debug --treat-failing-as-flaky=0.6,10,200 2>&1 | Tools/Scripts/filter-test-logs jsc'],
                        )
            .exit(2)
            .log('json', stdout=self.jsc_masm_failure),
        )
        self.expect_outcome(result=FAILURE, state_string='JSC test binary failure: testmasm')
        rc = self.run_step()
        self.expect_property(self.prefix + 'stress_test_failures', None)
        self.expect_property(self.prefix + 'binary_failures', ['testmasm'])
        return rc

    def test_b3_and_stress_test_failure(self):
        self.configureStep(platform='mac', fullPlatform='mac-highsierra', configuration='release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=3 * 60 * 60,
                        logfiles={'json': self.jsonFileName},
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'perl Tools/Scripts/run-javascriptcore-tests --no-build --no-fail-fast --json-output={self.jsonFileName} --release --treat-failing-as-flaky=0.6,10,200 2>&1 | Tools/Scripts/filter-test-logs jsc'],
                        )
            .exit(2)
            .log('json', stdout=self.jsc_b3_and_stress_test_failure),
        )
        self.expect_outcome(result=FAILURE, state_string='Found 1 jsc stress test failure: stress/weakset-gc.js, JSC test binary failure: testb3')
        rc = self.run_step()
        self.expect_property(self.prefix + 'stress_test_failures', ['stress/weakset-gc.js'])
        self.expect_property(self.prefix + 'binary_failures', ['testb3'])
        return rc

    def test_dfg_air_and_stress_test_failure(self):
        self.configureStep(platform='jsc-only', fullPlatform='jsc-only', configuration='release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=3 * 60 * 60,
                        logfiles={'json': self.jsonFileName},
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'perl Tools/Scripts/run-javascriptcore-tests --no-build --no-fail-fast --json-output={self.jsonFileName} --release --memory-limited --verbose --jsc-only --treat-failing-as-flaky=0.6,10,200 2>&1 | Tools/Scripts/filter-test-logs jsc'],
                        )
            .exit(2)
            .log('json', stdout=self.jsc_dfg_air_and_stress_test_failure),
        )
        self.expect_outcome(result=FAILURE, state_string='Found 1 jsc stress test failure: stress/weakset-gc.js, JSC test binary failures: testair, testdfg')
        rc = self.run_step()
        self.expect_property(self.prefix + 'stress_test_failures', ['stress/weakset-gc.js'])
        self.expect_property(self.prefix + 'binary_failures', ['testair', 'testdfg'])
        return rc

    @expectedFailure
    def test_success_with_flaky(self):
        self.configureStep(platform='jsc-only', fullPlatform='jsc-only', configuration='release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=3 * 60 * 60,
                        logfiles={'json': self.jsonFileName},
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'perl Tools/Scripts/run-javascriptcore-tests --no-build --no-fail-fast --json-output={self.jsonFileName} --release --memory-limited --verbose --jsc-only --treat-failing-as-flaky=0.6,10,200 2>&1 | Tools/Scripts/filter-test-logs jsc'],
                        )
            .exit(0)
            .log('json', stdout=self.jsc_passed_with_flaky),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed JSC tests (1 flaky)')
        rc = self.run_step()
        self.expect_property(self.prefix + 'flaky_and_passed', {'stress/switch-on-char-llint-rope.js.default': {'P': '7', 'T': '10'}})
        self.expect_property(self.prefix + 'stress_test_failures', None)
        self.expect_property(self.prefix + 'binary_test_failures', None)
        return rc


class TestRunJSCTestsWithoutChange(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'jsc_results.json'
        self.command_extra = RunJSCTestsWithoutChange.command_extra
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RunJSCTestsWithoutChange())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'perl Tools/Scripts/run-javascriptcore-tests --no-build --no-fail-fast --json-output={self.jsonFileName} --release --treat-failing-as-flaky=0.6,10,200 2>&1 | Tools/Scripts/filter-test-logs jsc'],
                        logfiles={'json': self.jsonFileName},
                        timeout=3 * 60 * 60,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='jscore-tests')
        return self.run_step()

    def test_failure(self):
        self.setup_step(RunJSCTestsWithoutChange())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'perl Tools/Scripts/run-javascriptcore-tests --no-build --no-fail-fast --json-output={self.jsonFileName} --debug --treat-failing-as-flaky=0.6,10,200 2>&1 | Tools/Scripts/filter-test-logs jsc'],
                        logfiles={'json': self.jsonFileName},
                        timeout=3 * 60 * 60,
                        )
            .log('stdio', stdout='9 failures found.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='jscore-tests (failure)')
        return self.run_step()


class TestAnalyzeJSCTestsResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(AnalyzeJSCTestsResults())
        self.setProperty('jsc_stress_test_failures', [])
        self.setProperty('jsc_binary_failures', [])
        self.setProperty('jsc_flaky_and_passed', {})
        self.setProperty('jsc_clean_tree_stress_test_failures', [])
        self.setProperty('jsc_clean_tree_binary_failures', [])
        self.setProperty('jsc_clean_tree_flaky_and_passed', {})

    def test_single_new_stress_failure(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', ['stress/force-error.js.bytecode-cache'])
        self.expect_outcome(result=FAILURE, state_string='Found 1 new JSC stress test failure: stress/force-error.js.bytecode-cache (failure)')
        return self.run_step()

    def test_single_new_binary_failure(self):
        self.configureStep()
        self.setProperty('jsc_binary_failures', ['testmasm'])
        self.expect_outcome(result=FAILURE, state_string='Found 1 new JSC binary failure: testmasm (failure)')
        return self.run_step()

    def test_multiple_new_stress_failure(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', [f'test{i}' for i in range(0, 30)])
        self.expect_outcome(result=FAILURE, state_string='Found 30 new JSC stress test failures: test0, test1, test10, test11, test12, test13, test14, test15, test16, test17 ... (failure)')
        return self.run_step()

    def test_multiple_new_binary_failure(self):
        self.configureStep()
        self.setProperty('jsc_binary_failures', ['testmasm', 'testair', 'testb3', 'testdfg', 'testapi'])
        self.expect_outcome(result=FAILURE, state_string='Found 5 new JSC binary failures: testair, testapi, testb3, testdfg, testmasm (failure)')
        return self.run_step()

    def test_new_stress_and_binary_failure(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', ['es6.yaml/es6/Set_iterator_closing.js.default'])
        self.setProperty('jsc_binary_failures', ['testmasm'])
        self.expect_outcome(result=FAILURE, state_string='Found 1 new JSC binary failure: testmasm, Found 1 new JSC stress test failure: es6.yaml/es6/Set_iterator_closing.js.default (failure)')
        return self.run_step()

    def test_stress_failure_on_clean_tree(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', ['stress/force-error.js.default'])
        self.setProperty('jsc_clean_tree_stress_test_failures', ['stress/force-error.js.default'])
        self.expect_outcome(result=SUCCESS, state_string='Passed JSC tests')
        return self.run_step()

    def test_binary_failure_on_clean_tree(self):
        self.configureStep()
        self.setProperty('jsc_binary_failures', ['testdfg'])
        self.setProperty('jsc_rerun_binary_failures', ['testdfg'])
        self.setProperty('jsc_clean_tree_binary_failures', ['testdfg'])
        self.expect_outcome(result=SUCCESS, state_string='Passed JSC tests')
        return self.run_step()

    def test_stress_and_binary_failure_on_clean_tree(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', ['es6.yaml/es6/Set_iterator_closing.js.default'])
        self.setProperty('jsc_binary_failures', ['testair'])
        self.setProperty('jsc_clean_tree_stress_test_failures', ['es6.yaml/es6/Set_iterator_closing.js.default'])
        self.setProperty('jsc_clean_tree_binary_failures', ['testair'])
        self.expect_outcome(result=SUCCESS, state_string='Passed JSC tests')
        return self.run_step()

    def test_flaky_and_consistent_stress_failures(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', ['test1'])
        self.setProperty('jsc_flaky_and_passed', {'test2': {'P': '7', 'T': '10'}})
        self.expect_outcome(result=FAILURE, state_string='Found 1 new JSC stress test failure: test1 (failure)')
        return self.run_step()

    def test_flaky_and_consistent_failures_with_clean_tree_failures(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', ['test1'])
        self.setProperty('jsc_flaky_and_passed', {'test2': {'P': 7, 'T': 10}})
        self.setProperty('jsc_clean_tree_stress_test_failures', ['test1'])
        self.expect_outcome(result=SUCCESS, state_string='Passed JSC tests')
        return self.run_step()

    def test_unexpected_infra_issue(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', [])
        self.setProperty('jsc_clean_tree_stress_test_failures', [])
        self.setProperty('clean_tree_run_status', FAILURE)
        self.expect_outcome(result=RETRY, state_string='Unexpected infrastructure issue, retrying build (retry)')
        return self.run_step()

    def test_change_breaking_jsc_test_suite(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', [])
        self.setProperty('jsc_flaky_and_passed', {})
        self.setProperty('jsc_clean_tree_stress_test_failures', [])
        self.setProperty('jsc_clean_tree_flaky_and_passed', {})
        self.setProperty('clean_tree_run_status', SUCCESS)
        self.expect_outcome(result=FAILURE, state_string='Found unexpected failure with change (failure)')
        return self.run_step()


class TestRunWebKitTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        self.results_json_regressions = '''ADD_RESULTS({"tests":{"imported":{"w3c":{"web-platform-tests":{"IndexedDB":{"interleaved-cursors-large.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}},"wasm":{"jsapi":{"interface.any.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"instance":{"constructor-bad-imports.any.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}},"global":{"constructor.any.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"constructor.any.worker.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"toString.any.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}},"constructor":{"instantiate-bad-imports.any.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"instantiate-bad-imports.any.worker.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}},"interface.any.worker.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}}},"blink":{"storage":{"indexeddb":{"blob-valid-before-commit.html":{"report":"REGRESSION","expected":"PASS","actual":"TIMEOUT","has_stderr":true}}}}}},"skipped":23256,"num_regressions":10,"other_crashes":{},"interrupted":true,"num_missing":0,"layout_tests_dir":"/Volumes/Data/worker/iOS-12-Simulator-WK2-Tests-EWS/build/LayoutTests","version":4,"num_passes":32056,"pixel_tests_enabled":false,"date":"06:21AM on July 15, 2019","has_pretty_patch":true,"fixable":23267,"num_flaky":0,"uses_expectations_file":true});
        '''
        self.results_json_flakes = '''ADD_RESULTS({"tests":{"http":{"tests":{"workers":{"service":{"service-worker-resource-timing.https.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}},"xmlhttprequest":{"post-content-type-document.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS"}}}},"imported":{"blink":{"storage":{"indexeddb":{"blob-valid-before-commit.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}},"fast":{"text":{"international":{"repaint-glyph-bounds.html":{"report":"FLAKY","expected":"PASS","actual":"IMAGE PASS","reftest_type":["=="],"image_diff_percent":0.08}}}}}}},"skipped":13176,"num_regressions":0,"other_crashes":{},"interrupted":false,"num_missing":0,"layout_tests_dir":"/Volumes/Data/worker/iOS-12-Simulator-WK2-Tests-EWS/build/LayoutTests","version":4,"num_passes":42185,"pixel_tests_enabled":false,"date":"06:54AM on July 17, 2019","has_pretty_patch":true,"fixable":55356,"num_flaky":4,"uses_expectations_file":true});
        '''
        self.results_json_mix_flakes_and_regression = '''ADD_RESULTS({"tests":{"http":{"tests":{"IndexedDB":{"collect-IDB-objects.https.html":{"report":"FLAKY","expected":"PASS","actual":"TEXT PASS"}},"xmlhttprequest":{"on-network-timeout-error-during-preflight.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS"}}}},"transitions":{"lengthsize-transition-to-from-auto.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS"}},"imported":{"blink":{"storage":{"indexeddb":{"blob-valid-before-commit.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}}}},"fast":{"text":{"font-weight-fallback.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true,"reftest_type":["=="]}},"scrolling":{"ios":{"reconcile-layer-position-recursive.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}}},"skipped":13174,"num_regressions":1,"other_crashes":{},"interrupted":false,"num_missing":0,"layout_tests_dir":"/Volumes/Data/worker/iOS-12-Simulator-WK2-Tests-EWS/build/LayoutTests","version":4,"num_passes":42158,"pixel_tests_enabled":false,"date":"11:28AM on July 16, 2019","has_pretty_patch":true,"fixable":55329,"num_flaky":5,"uses_expectations_file":true});
        '''

        self.results_json_with_newlines = '''ADD_RESULTS({"tests":{"http":{"tests":{"IndexedDB":{"collect-IDB-objects.https.html":{"report":"FLAKY","expected":"PASS","actual":"TEXT PASS"}},"xmlhttprequest":{"on-network-timeout-error-during-preflight.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS"}}}},"transitions":{"lengthsize-trans
ition-to-from-auto.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS"}},"imported":{"blink":{"storage":{"indexeddb":{"blob-valid-before-commit.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}}}},"fast":{"text":{"font-weight-fallback.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true,"reftest_type":["=="]}},"scrolling":{"ios":{"reconcile-layer-position-recursive.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}}},"skipped":13174,"num_regressions":1,"other_crashes":{},"interrupted":false,"num_missing":0,"layout_tests_dir":"/Volumes/Data/worker/iOS-12-Simulator-WK2-Tests-EWS/build/LayoutTes
ts","version":4,"num_passes":42158,"pixel_tests_enabled":false,"date":"11:28AM on July 16, 2019","has_pretty_patch":true,"fixable":55329,"num_flaky":5,"uses_expectations_file":true});
        '''

        self.results_with_missing_results = '''ADD_RESULTS({"tests":{"http":{"wpt":{"css":{"css-highlight-api":{"highlight-image-expected-mismatched.html":{"report":"MISSING","expected":"PASS","is_missing_text":true,"actual":"MISSING"},"highlight-image.html":{"report":"MISSING","expected":"PASS","is_missing_text":true,"actual":"MISSING"}}}}}}, "interrupted":false});
        '''

        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(RunWebKitTests())
        self.property_exceed_failure_limit = 'first_results_exceed_failure_limit'
        self.property_failures = 'first_run_failures'
        RunWebKitTests.filter_failures_using_results_db = lambda x, failing_tests: ''

    def test_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        return self.run_step()

    def test_warnings(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(0)
            .log('stdio', stdout='''Unexpected flakiness: timeouts (2)
                              imported/blink/storage/indexeddb/blob-valid-before-commit.html [ Timeout Pass ]
                              storage/indexeddb/modern/deleteindex-2.html [ Timeout Pass ]'''),
        )
        self.expect_outcome(result=WARNINGS, state_string='2 flakes')
        return self.run_step()

    def test_success_additional_arguments(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('additionalArguments', ['--exclude-tests', 'imported/w3c/web-platform-tests'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests --exclude-tests imported/w3c/web-platform-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        return self.run_step()

    def test_skip_for_revert_patches_on_commit_queue(self):
        self.configureStep()
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('fullPlatform', 'mac')
        self.setProperty('configuration', 'debug')
        self.setProperty('fast_commit_queue', True)
        self.expect_outcome(result=SKIPPED, state_string='Skipped layout-tests in fast-cq mode')
        return self.run_step()

    def test_skip_for_mac_wk2_passed_change_on_commit_queue(self):
        self.configureStep()
        self.setProperty('patch_id', '1234')
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('fullPlatform', 'mac')
        self.setProperty('configuration', 'debug')
        self.setProperty('passed_mac_wk2', True)
        self.expect_outcome(result=SKIPPED, state_string='Skipped layout-tests')
        return self.run_step()

    def test_skip_for_mac_wk2_passed_change_on_merge_queue(self):
        self.configureStep()
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88a')
        self.setProperty('buildername', 'Merge-Queue')
        self.setProperty('fullPlatform', 'mac')
        self.setProperty('configuration', 'debug')
        self.setProperty('passed_mac_wk2', True)
        self.expect_outcome(result=SKIPPED, state_string='Skipped layout-tests')
        return self.run_step()

    def test_parse_results_json_regression(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(2)
            .log('json', stdout=self.results_json_regressions),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.run_step()
        self.expect_property(self.property_exceed_failure_limit, True)
        self.expect_property(self.property_failures,
                             ['imported/blink/storage/indexeddb/blob-valid-before-commit.html',
                              'imported/w3c/web-platform-tests/IndexedDB/interleaved-cursors-large.html',
                              'imported/w3c/web-platform-tests/wasm/jsapi/constructor/instantiate-bad-imports.any.html',
                              'imported/w3c/web-platform-tests/wasm/jsapi/constructor/instantiate-bad-imports.any.worker.html',
                              'imported/w3c/web-platform-tests/wasm/jsapi/global/constructor.any.html',
                              'imported/w3c/web-platform-tests/wasm/jsapi/global/constructor.any.worker.html',
                              'imported/w3c/web-platform-tests/wasm/jsapi/global/toString.any.html',
                              'imported/w3c/web-platform-tests/wasm/jsapi/instance/constructor-bad-imports.any.html',
                              'imported/w3c/web-platform-tests/wasm/jsapi/interface.any.html',
                              'imported/w3c/web-platform-tests/wasm/jsapi/interface.any.worker.html'])
        return rc

    def test_parse_results_json_flakes(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(0)
            .log('json', stdout=self.results_json_flakes),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.run_step()
        self.expect_property(self.property_exceed_failure_limit, False)
        self.expect_property(self.property_failures, [])
        return rc

    def test_parse_results_json_flakes_and_regressions(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(2)
            .log('json', stdout=self.results_json_mix_flakes_and_regression),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.run_step()
        self.expect_property(self.property_exceed_failure_limit, False)
        self.expect_property(self.property_failures, ['fast/scrolling/ios/reconcile-layer-position-recursive.html'])
        return rc

    def test_parse_results_json_with_newlines(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(2)
            .log('json', stdout=self.results_json_with_newlines),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.run_step()
        self.expect_property(self.property_exceed_failure_limit, False)
        self.expect_property(self.property_failures, ['fast/scrolling/ios/reconcile-layer-position-recursive.html'])
        return rc

    @expectedFailure
    def test_parse_results_invalid_json(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(2)
            .log('json', stdout=self.results_json_with_newlines + " non-JSON nonsense\n"),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.run_step()
        self.expect_property(self.property_exceed_failure_limit, None)
        self.expect_property(self.property_failures, None)
        return rc

    def test_parse_results_json_with_missing_results(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(2)
            .log('json', stdout=self.results_with_missing_results),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.run_step()
        self.expect_property(self.property_exceed_failure_limit, False)
        self.expect_property(self.property_failures,
                             ['http/wpt/css/css-highlight-api/highlight-image-expected-mismatched.html',
                              'http/wpt/css/css-highlight-api/highlight-image.html'])
        return rc

    def test_unexpected_error(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'mac-highsierra')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --debug --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .log('stdio', stdout='Unexpected error.')
            .exit(254),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.run_step()

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .log('stdio', stdout='9 failures found.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.run_step()

    def test_success_wpt_import_bot(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('patch_author', 'webkit-wpt-import-bot@igalia.com')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging imported/w3c/web-platform-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        return self.run_step()

    def test_failure_no_failure_limits(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('github_labels', ['no-failure-limits'])

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        max_time=60 * 90,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --no-retry 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .log('stdio', stdout='9 failures found.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.run_step()

    def test_success_no_failure_limits(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('github_labels', ['no-failure-limits'])

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        max_time=60 * 90,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --no-retry 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        return self.run_step()


class TestReRunWebKitTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        # Copied from TestRunWebKitTests.setUp()
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        self.results_json_regressions = '''ADD_RESULTS({"tests":{"imported":{"w3c":{"web-platform-tests":{"IndexedDB":{"interleaved-cursors-large.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}},"wasm":{"jsapi":{"interface.any.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"instance":{"constructor-bad-imports.any.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}},"global":{"constructor.any.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"constructor.any.worker.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"toString.any.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}},"constructor":{"instantiate-bad-imports.any.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"},"instantiate-bad-imports.any.worker.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}},"interface.any.worker.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}}},"blink":{"storage":{"indexeddb":{"blob-valid-before-commit.html":{"report":"REGRESSION","expected":"PASS","actual":"TIMEOUT","has_stderr":true}}}}}},"skipped":23256,"num_regressions":10,"other_crashes":{},"interrupted":true,"num_missing":0,"layout_tests_dir":"/Volumes/Data/worker/iOS-12-Simulator-WK2-Tests-EWS/build/LayoutTests","version":4,"num_passes":32056,"pixel_tests_enabled":false,"date":"06:21AM on July 15, 2019","has_pretty_patch":true,"fixable":23267,"num_flaky":0,"uses_expectations_file":true});
        '''
        self.results_json_flakes = '''ADD_RESULTS({"tests":{"http":{"tests":{"workers":{"service":{"service-worker-resource-timing.https.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}},"xmlhttprequest":{"post-content-type-document.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS"}}}},"imported":{"blink":{"storage":{"indexeddb":{"blob-valid-before-commit.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}},"fast":{"text":{"international":{"repaint-glyph-bounds.html":{"report":"FLAKY","expected":"PASS","actual":"IMAGE PASS","reftest_type":["=="],"image_diff_percent":0.08}}}}}}},"skipped":13176,"num_regressions":0,"other_crashes":{},"interrupted":false,"num_missing":0,"layout_tests_dir":"/Volumes/Data/worker/iOS-12-Simulator-WK2-Tests-EWS/build/LayoutTests","version":4,"num_passes":42185,"pixel_tests_enabled":false,"date":"06:54AM on July 17, 2019","has_pretty_patch":true,"fixable":55356,"num_flaky":4,"uses_expectations_file":true});
        '''
        self.results_json_mix_flakes_and_regression = '''ADD_RESULTS({"tests":{"http":{"tests":{"IndexedDB":{"collect-IDB-objects.https.html":{"report":"FLAKY","expected":"PASS","actual":"TEXT PASS"}},"xmlhttprequest":{"on-network-timeout-error-during-preflight.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS"}}}},"transitions":{"lengthsize-transition-to-from-auto.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS"}},"imported":{"blink":{"storage":{"indexeddb":{"blob-valid-before-commit.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}}}},"fast":{"text":{"font-weight-fallback.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true,"reftest_type":["=="]}},"scrolling":{"ios":{"reconcile-layer-position-recursive.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}}},"skipped":13174,"num_regressions":1,"other_crashes":{},"interrupted":false,"num_missing":0,"layout_tests_dir":"/Volumes/Data/worker/iOS-12-Simulator-WK2-Tests-EWS/build/LayoutTests","version":4,"num_passes":42158,"pixel_tests_enabled":false,"date":"11:28AM on July 16, 2019","has_pretty_patch":true,"fixable":55329,"num_flaky":5,"uses_expectations_file":true});
        '''

        self.results_json_with_newlines = '''ADD_RESULTS({"tests":{"http":{"tests":{"IndexedDB":{"collect-IDB-objects.https.html":{"report":"FLAKY","expected":"PASS","actual":"TEXT PASS"}},"xmlhttprequest":{"on-network-timeout-error-during-preflight.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS"}}}},"transitions":{"lengthsize-trans
ition-to-from-auto.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS"}},"imported":{"blink":{"storage":{"indexeddb":{"blob-valid-before-commit.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}}}},"fast":{"text":{"font-weight-fallback.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true,"reftest_type":["=="]}},"scrolling":{"ios":{"reconcile-layer-position-recursive.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}}},"skipped":13174,"num_regressions":1,"other_crashes":{},"interrupted":false,"num_missing":0,"layout_tests_dir":"/Volumes/Data/worker/iOS-12-Simulator-WK2-Tests-EWS/build/LayoutTes
ts","version":4,"num_passes":42158,"pixel_tests_enabled":false,"date":"11:28AM on July 16, 2019","has_pretty_patch":true,"fixable":55329,"num_flaky":5,"uses_expectations_file":true});
        '''

        self.results_with_missing_results = '''ADD_RESULTS({"tests":{"http":{"wpt":{"css":{"css-highlight-api":{"highlight-image-expected-mismatched.html":{"report":"MISSING","expected":"PASS","is_missing_text":true,"actual":"MISSING"},"highlight-image.html":{"report":"MISSING","expected":"PASS","is_missing_text":true,"actual":"MISSING"}}}}}}, "interrupted":false});
        '''

        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(ReRunWebKitTests())
        self.property_exceed_failure_limit = 'second_results_exceed_failure_limit'
        self.property_failures = 'second_run_failures'
        ReRunWebKitTests.filter_failures_using_results_db = lambda x, failing_tests: ''

    def test_flaky_failures_in_first_run(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.run_step()
        self.expect_property('build_summary', 'Found flaky tests: test1, test2')
        return rc

    def test_first_run_failed_unexpectedly(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('first_run_failures', [])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.run_step()
        self.expect_property('build_summary', 'Passed layout tests')
        return rc

    def test_too_many_flaky_failures_in_first_and_second_run(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('first_run_failures', ['test1', 'test2', 'test3', 'test4', 'test5'])
        self.setProperty('second_run_failures', ['test6', 'test7', 'test8', 'test9', 'test10', 'test11'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.run_step()


class TestRunWebKitTestsInStressMode(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(RunWebKitTestsInStressMode())
        self.property_exceed_failure_limit = 'first_results_exceed_failure_limit'
        self.property_failures = 'first_run_failures'

    def test_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('modified_tests', ['test1', 'test2'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results',
                                 '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging',
                                 '--exit-after-n-failures', '10',
                                 '--skipped', 'always',
                                 '--iterations', 100, 'test1', 'test2'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        return self.run_step()

    def test_success_wk1(self):
        self.setup_step(RunWebKitTestsInStressMode(layout_test_class=RunWebKit1Tests))
        self.property_exceed_failure_limit = 'first_results_exceed_failure_limit'
        self.property_failures = 'first_run_failures'
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('modified_tests', ['test1', 'test2'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --dump-render-tree --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 10 --skipped always --iterations 100 test1 test2 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        return self.run_step()

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('modified_tests', ['test'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 10 --skipped always --iterations 100 test 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .log('stdio', stdout='9 failures found.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.run_step()
        self.expect_property('build_summary', 'Found test failures in stress mode')
        return rc

    def test_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('modified_tests', ['test1', 'test2'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 10 --skipped always --iterations 100 test1 test2 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        return self.run_step()

    def test_success_additional_arguments(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('modified_tests', ['test1', 'test2'])
        self.setProperty('additionalArguments', ['--child-processes=5', '--exclude-tests', 'imported/w3c/web-platform-tests'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 10 --skipped always --iterations 100 test1 test2 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        return self.run_step()


class TestRunWebKitTestsInStressGuardmallocMode(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(RunWebKitTestsInStressGuardmallocMode())
        self.property_exceed_failure_limit = 'first_results_exceed_failure_limit'
        self.property_failures = 'first_run_failures'

    def test_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('modified_tests', ['test1', 'test2'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 10 --skipped always --guard-malloc --iterations 100 test1 test2 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        return self.run_step()

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('modified_tests', ['test'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 10 --skipped always --guard-malloc --iterations 100 test 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .log('stdio', stdout='9 failures found.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.run_step()
        self.expect_property('build_summary', 'Found test failures in stress mode')
        return rc


class TestRunWebKitTestsWithoutChange(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        os.environ['RESULTS_SERVER_API_KEY'] = 'test-api-key'
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(RunWebKitTestsWithoutChange())
        self.property_exceed_failure_limit = 'clean_tree_results_exceed_failure_limit'
        self.property_failures = 'clean_tree_run_failures'
        self.setProperty('buildername', 'iOS-13-Simulator-WK2-Tests-EWS')
        self.setProperty('buildnumber', '123')
        self.setProperty('workername', 'ews126')

    def test_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests --builder-name iOS-13-Simulator-WK2-Tests-EWS --build-number 123 --buildbot-worker ews126 --buildbot-master {EWS_BUILD_HOSTNAMES[0]} --report https://results.webkit.org/ 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'},
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='layout-tests')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    def test_run_subtest_tests_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('first_run_failures', ['test1.html', 'test2.html', 'test3.html'])
        self.setProperty('second_run_failures', ['test3.html', 'test4.html', 'test5.html'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests --builder-name iOS-13-Simulator-WK2-Tests-EWS --build-number 123 --buildbot-worker ews126 --buildbot-master {EWS_BUILD_HOSTNAMES[0]} --report https://results.webkit.org/ --skipped=always test1.html test2.html test3.html test4.html test5.html 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'},
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='layout-tests')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    def test_run_subtest_tests_removes_skipped_that_fails(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('first_run_failures', ['test-was-skipped-patch-removed-expectation-but-still-fails.html'])
        self.setProperty('second_run_failures', ['test-was-skipped-patch-removed-expectation-but-still-fails.html'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests --builder-name iOS-13-Simulator-WK2-Tests-EWS --build-number 123 --buildbot-worker ews126 --buildbot-master {EWS_BUILD_HOSTNAMES[0]} --report https://results.webkit.org/ --skipped=always test-was-skipped-patch-removed-expectation-but-still-fails.html 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'},
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='layout-tests')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    def test_run_subtest_tests_fail(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('first_run_failures', ['test-fails-withpatch1.html', 'test-pre-existent-failure1.html'])
        self.setProperty('second_run_failures', ['test-fails-withpatch2.html', 'test-pre-existent-failure2.html'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests --builder-name iOS-13-Simulator-WK2-Tests-EWS --build-number 123 --buildbot-worker ews126 --buildbot-master {EWS_BUILD_HOSTNAMES[0]} --report https://results.webkit.org/ --skipped=always test-fails-withpatch1.html test-fails-withpatch2.html test-pre-existent-failure1.html test-pre-existent-failure2.html 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'},
                        )
            .log('stdio', stdout='2 failures found.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    def test_run_subtest_tests_limit_exceeded(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('first_run_failures', ['test1.html', 'test2.html', 'test3.html'])
        self.setProperty('second_results_exceed_failure_limit', True)
        self.setProperty('second_run_failures', [f'test{i}.html' for i in range(0, 30)])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests --builder-name iOS-13-Simulator-WK2-Tests-EWS --build-number 123 --buildbot-worker ews126 --buildbot-master {EWS_BUILD_HOSTNAMES[0]} --report https://results.webkit.org/ 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'},
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='layout-tests')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests --builder-name iOS-13-Simulator-WK2-Tests-EWS --build-number 123 --buildbot-worker ews126 --buildbot-master {EWS_BUILD_HOSTNAMES[0]} --report https://results.webkit.org/ 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
                        )
            .log('stdio', stdout='9 failures found.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()


class TestRunWebKit1Tests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RunWebKit1Tests())
        self.setProperty('fullPlatform', 'ios-11')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --debug --dump-render-tree --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        return self.run_step()

    def test_failure(self):
        self.setup_step(RunWebKit1Tests())
        self.setProperty('fullPlatform', 'ios-11')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --dump-render-tree --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 60 --skip-failing-tests 2>&1 | Tools/Scripts/filter-test-logs layout'],
                        )
            .log('stdio', stdout='9 failures found.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.run_step()

    def test_skip_for_revert_patches_on_commit_queue(self):
        self.setup_step(RunWebKit1Tests())
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('fullPlatform', 'mac')
        self.setProperty('configuration', 'debug')
        self.setProperty('fast_commit_queue', True)
        self.expect_outcome(result=SKIPPED, state_string='Skipped layout-tests in fast-cq mode')
        return self.run_step()


class TestAnalyzeLayoutTestsResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(AnalyzeLayoutTestsResults())
        self.setProperty('first_results_exceed_failure_limit', False)
        self.setProperty('second_results_exceed_failure_limit', False)
        self.setProperty('clean_tree_results_exceed_failure_limit', False)
        self.setProperty('clean_tree_run_failures', [])

    def test_failure_introduced_by_change(self):
        self.configureStep()
        self.setProperty('first_run_failures', ["jquery/offset.html"])
        self.setProperty('second_run_failures', ["jquery/offset.html"])
        self.expect_outcome(result=FAILURE, state_string='Found 1 new test failure: jquery/offset.html (failure)')
        return self.run_step()

    def test_failure_on_clean_tree(self):
        self.configureStep()
        self.setProperty('first_run_failures', ["jquery/offset.html"])
        self.setProperty('second_run_failures', ["jquery/offset.html"])
        self.setProperty('clean_tree_run_failures', ["jquery/offset.html"])
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.run_step()
        self.expect_property('build_summary', 'Found 1 pre-existing test failure: jquery/offset.html')
        return rc

    def test_flaky_and_consistent_failures_without_clean_tree_failures(self):
        self.configureStep()
        self.setProperty('buildername', 'iOS-13-Simulator-WK2-Tests-EWS')
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.setProperty('second_run_failures', ['test1'])
        self.expect_outcome(result=FAILURE, state_string='Found 1 new test failure: test1 (failure)')
        rc = self.run_step()
        self.expect_property('comment_text', None)
        self.expect_property('build_finish_summary', 'Found 1 new test failure: test1')
        return rc

    def test_consistent_failure_without_clean_tree_failures_commit_queue(self):
        self.configureStep()
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('first_run_failures', ['test1'])
        self.setProperty('second_run_failures', ['test1'])
        self.expect_outcome(result=FAILURE, state_string='Found 1 new test failure: test1 (failure)')
        rc = self.run_step()
        self.expect_property('comment_text', 'Found 1 new test failure: test1')
        self.expect_property('build_finish_summary', 'Found 1 new test failure: test1')
        return rc

    def test_flaky_and_inconsistent_failures_without_clean_tree_failures(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.setProperty('second_run_failures', ['test3'])
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        return self.run_step()

    def test_flaky_failures_in_first_run(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.setProperty('second_run_failures', [])
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.run_step()
        self.expect_property('build_summary', ' Found flaky tests: test1, test2')
        return rc

    def test_flaky_and_inconsistent_failures_with_clean_tree_failures(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.setProperty('second_run_failures', ['test3'])
        self.setProperty('clean_tree_run_failures', ['test1', 'test2', 'test3'])
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.run_step()
        self.expect_property('build_summary', 'Found 3 pre-existing test failures: test1, test2, test3 Found flaky tests: test1, test2, test3')
        return rc

    def test_flaky_and_consistent_failures_with_clean_tree_failures(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.setProperty('second_run_failures', ['test1'])
        self.setProperty('clean_tree_run_failures', ['test1', 'test2'])
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        return self.run_step()

    def test_mildly_flaky_change_with_some_tree_redness_and_flakiness(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1', 'test2', 'test3'])
        self.setProperty('second_run_failures', ['test1', 'test2'])
        self.setProperty('clean_tree_run_failures', ['test1', 'test2', 'test4'])
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.run_step()
        self.expect_property('build_summary', 'Found 3 pre-existing test failures: test1, test2, test4 Found flaky test: test3')
        return rc

    def test_first_run_exceed_failure_limit(self):
        self.configureStep()
        self.setProperty('first_results_exceed_failure_limit', True)
        self.setProperty('first_run_failures',  [f'test{i}' for i in range(0, 30)])
        self.setProperty('second_run_failures', [])
        self.expect_outcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by change, retrying build (retry)')
        return self.run_step()

    def test_second_run_exceed_failure_limit(self):
        self.configureStep()
        self.setProperty('first_run_failures', [])
        self.setProperty('second_results_exceed_failure_limit', True)
        self.setProperty('second_run_failures',  [f'test{i}' for i in range(0, 30)])
        self.expect_outcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by change, retrying build (retry)')
        return self.run_step()

    def test_clean_tree_exceed_failure_limit(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1'])
        self.setProperty('second_run_failures', ['test1'])
        self.setProperty('clean_tree_results_exceed_failure_limit', True)
        self.setProperty('clean_tree_run_failures',  [f'test{i}' for i in range(0, 30)])
        self.expect_outcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by change, retrying build (retry)')
        return self.run_step()

    def test_clean_tree_exceed_failure_limit_with_triggered_by(self):
        self.configureStep()
        self.setProperty('buildername', 'iOS-13-Simulator-WK2-Tests-EWS')
        self.setProperty('triggered_by', 'ios-13-sim-build-ews')
        self.setProperty('first_run_failures', ['test1'])
        self.setProperty('second_run_failures', ['test1'])
        self.setProperty('clean_tree_results_exceed_failure_limit', True)
        self.setProperty('clean_tree_run_failures',  [f'test{i}' for i in range(0, 30)])
        message = 'Unable to confirm if test failures are introduced by change, retrying build'
        self.expect_outcome(result=SUCCESS, state_string=message)
        rc = self.run_step()
        self.expect_property('build_summary', message)
        return rc

    def test_clean_tree_has_lot_of_failures(self):
        self.configureStep()
        self.setProperty('first_results_exceed_failure_limit', True)
        self.setProperty('first_run_failures', [f'test{i}' for i in range(0, 30)])
        self.setProperty('second_results_exceed_failure_limit', True)
        self.setProperty('second_run_failures', [f'test{i}' for i in range(0, 30)])
        self.setProperty('clean_tree_run_failures', [f'test{i}' for i in range(0, 27)])
        self.expect_outcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by change, retrying build (retry)')
        return self.run_step()

    def test_clean_tree_has_some_failures(self):
        self.configureStep()
        self.setProperty('first_results_exceed_failure_limit', True)
        self.setProperty('first_run_failures', [f'test{i}' for i in range(0, 30)])
        self.setProperty('second_results_exceed_failure_limit', True)
        self.setProperty('second_run_failures', [f'test{i}' for i in range(0, 30)])
        self.setProperty('clean_tree_run_failures', [f'test{i}' for i in range(0, 10)])
        self.expect_outcome(result=FAILURE, state_string='Found 30 new test failures: test0 test1 test10 test11 test12 test13 test14 test15 test16 test17 ... (failure)')
        return self.run_step()

    def test_clean_tree_has_lot_of_failures_and_no_new_failure(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1'])
        self.setProperty('second_run_failures', ['test1'])
        self.setProperty('clean_tree_run_failures', [f'test{i}' for i in range(0, 20)])
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.run_step()
        self.expect_property('build_summary', 'Found 20 pre-existing test failures: test0, test1, test10, test11, test12, test13, test14, test15, test16, test17 ...')
        return rc

    def test_change_introduces_lot_of_failures(self):
        self.configureStep()
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('first_results_exceed_failure_limit', True)
        self.setProperty('first_run_failures', [f'test{i}' for i in range(0, 300)])
        self.setProperty('second_results_exceed_failure_limit', True)
        self.setProperty('second_run_failures', [f'test{i}' for i in range(0, 300)])
        failure_message = 'Found 300 new test failures: test0 test1 test10 test100 test101 test102 test103 test104 test105 test106 ...'
        self.expect_outcome(result=FAILURE, state_string=failure_message + ' (failure)')
        rc = self.run_step()
        self.expect_property('comment_text', failure_message)
        self.expect_property('build_finish_summary', failure_message)
        return rc

    def test_change_introduces_lot_of_flakiness(self):
        self.configureStep()
        self.setProperty('first_results_exceed_failure_limit', False)
        self.setProperty('first_run_failures', [f'test{i}' for i in range(0, 5)])
        self.setProperty('second_results_exceed_failure_limit', False)
        self.setProperty('second_run_failures', [f'test{i}' for i in range(5, 12)])
        failure_message = 'Too many flaky failures: test0, test1, test10, test11, test2, test3, test4, test5, test6, test7 (failure)'
        self.expect_outcome(result=FAILURE, state_string=failure_message)
        return self.run_step()

    def test_unexpected_infra_issue(self):
        self.configureStep()
        self.setProperty('first_run_failures', [])
        self.setProperty('second_run_failures', [])
        self.setProperty('clean_tree_run_status', FAILURE)
        self.expect_outcome(result=RETRY, state_string='Unexpected infrastructure issue, retrying build (retry)')
        return self.run_step()

    def test_change_breaks_layout_tests1(self):
        self.configureStep()
        self.setProperty('first_run_failures', [])
        self.setProperty('second_run_failures', [])
        self.setProperty('clean_tree_run_failures', [])
        self.setProperty('clean_tree_run_status', SUCCESS)
        self.expect_outcome(result=FAILURE, state_string='Found unexpected failure with change (failure)')
        return self.run_step()

    def test_change_breaks_layout_tests2(self):
        self.configureStep()
        self.setProperty('first_run_failures', [])
        self.setProperty('second_run_failures', [])
        self.setProperty('clean_tree_run_failures', [])
        self.setProperty('clean_tree_run_status', WARNINGS)
        self.expect_outcome(result=FAILURE, state_string='Found unexpected failure with change (failure)')
        return self.run_step()

    def test_change_removes_skipped_test_that_fails(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test-was-skipped-change-removed-expectation-but-still-fails.html'])
        self.setProperty('second_run_failures', ['test-was-skipped-change-removed-expectation-but-still-fails.html'])
        self.setProperty('clean_tree_run_failures', [])
        self.setProperty('clean_tree_run_status', SUCCESS)
        self.expect_outcome(result=FAILURE, state_string='Found 1 new test failure: test-was-skipped-change-removed-expectation-but-still-fails.html (failure)')
        return self.run_step()


class MockLayoutTestFailures(object):
    def __init__(self, failing_tests, flaky_tests, did_exceed_test_failure_limit):
        self.failing_tests = failing_tests
        self.flaky_tests = flaky_tests
        self.did_exceed_test_failure_limit = did_exceed_test_failure_limit


class TestRunWebKitTestsRedTree(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(RunWebKitTestsRedTree())
        self.setProperty('platform', 'wpe')
        self.setProperty('fullPlatform', 'wpe')
        self.setProperty('configuration', 'release')

    def test_success(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --wpe --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 500 --skip-failing-tests --enable-core-dumps-nolimit 2>&1 | Tools/Scripts/filter-test-logs layout']
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        return self.run_step()

    def test_set_properties_when_executed_scope_this_class(self):
        self.configureStep()
        first_run_failures = ['fast/css/test1.html', 'fast/svg/test2.svg', 'imported/test/test3.html']
        first_run_flakies = ['fast/css/flaky1.html', 'fast/svg/flaky2.svg', 'imported/test/flaky3.html']
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --wpe --results-directory layout-test-results --debug-rwt-logging --exit-after-n-failures 500 --skip-failing-tests --enable-core-dumps-nolimit 2>&1 | Tools/Scripts/filter-test-logs layout']
                        )
            .exit(2)
        )
        # Patch LayoutTestFailures.results_from_string() to report the expected values
        # Check this values end on the properties this class should define
        mock_test_failures = MockLayoutTestFailures(first_run_failures, first_run_flakies, False)
        self.patch(LayoutTestFailures, 'results_from_string', lambda f: mock_test_failures)
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.run_step()
        # Note: first_run properties are considered to belong to RunWebKitTestsRedTree() in this case, so they should be set to mock_test_failures
        self.expect_property('first_run_failures', first_run_failures)
        self.expect_property('first_run_flakies', first_run_flakies)
        self.assertFalse(self.getProperty('first_results_exceed_failure_limit'))
        return rc

    def test_last_try_unexpected_failure_without_list_of_failing_tests_then_schedule_update_libs_and_test_without_patch(self):
        self.configureStep()
        self.setProperty('first_run_failures', [])
        self.setProperty('first_run_flakies', [])
        self.setProperty('retry_count', AnalyzeLayoutTestsResultsRedTree.MAX_RETRY)
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.patch(RunWebKitTestsRedTree, 'evaluateResult', lambda s, r: r)
        self.get_nth_step(0).evaluateCommand(FAILURE)
        self.assertTrue(RevertAppliedChanges() in next_steps)
        self.assertTrue(InstallWpeDependencies() in next_steps)
        self.assertTrue(CompileWebKitWithoutChange(retry_build_on_failure=True))
        self.assertTrue(RunWebKitTestsWithoutChangeRedTree() in next_steps)

    def test_flakies_but_no_failures_then_go_to_analyze_results(self):
        self.configureStep()
        self.setProperty('first_run_failures', [])
        self.setProperty('first_run_flakies', ['fast/css/flaky1.html', 'fast/svg/flaky2.svg', 'imported/test/flaky3.html'])
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.patch(RunWebKitTestsRedTree, 'evaluateResult', lambda s, r: r)
        self.get_nth_step(0).evaluateCommand(SUCCESS)
        self.assertFalse(RevertAppliedChanges() in next_steps)
        self.assertFalse(InstallWpeDependencies() in next_steps)
        self.assertFalse(RunWebKitTestsWithoutChangeRedTree() in next_steps)
        self.assertTrue(AnalyzeLayoutTestsResultsRedTree() in next_steps)


class TestRunWebKitTestsRepeatFailuresRedTree(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(RunWebKitTestsRepeatFailuresRedTree())
        self.setProperty('platform', 'wpe')
        self.setProperty('fullPlatform', 'wpe')
        self.setProperty('configuration', 'release')

    def test_success(self):
        self.configureStep()
        first_run_failures = ['fast/css/test1.html', 'imported/test/test2.html', 'fast/svg/test3.svg']
        first_run_flakies = ['fast/css/flaky1.html', 'imported/test/flaky2.html', 'fast/svg/flaky3.svg']
        self.setProperty('first_run_failures', first_run_failures)
        self.setProperty('first_run_flakies', first_run_flakies)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        max_time=18000,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --wpe --results-directory layout-test-results --debug-rwt-logging --skip-failing-tests --fully-parallel --repeat-each=10 fast/css/test1.html fast/svg/test3.svg imported/test/test2.html 2>&1 | Tools/Scripts/filter-test-logs layout']
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='layout-tests')
        return self.run_step()

    def test_success_tests_names_with_shell_conflictive_chars(self):
        self.configureStep()
        first_run_failures = ['imported/w3c/web-platform-tests/html/dom/idlharness.https.html?exclude=(Document|Window|HTML.*)',
                              'imported/w3c/web-platform-tests/html/dom/idlharness.https.html?include=HTML.*',
                              'try/crash/for/test_with_brackets[]{}',
                              'try/crash/for/test_with spaces " and \' quotes'
                              ]
        first_run_flakies = ['fast/css/flaky1.html', 'imported/test/flaky2.html', 'fast/svg/flaky3.svg']
        self.setProperty('first_run_failures', first_run_failures)
        self.setProperty('first_run_flakies', first_run_flakies)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        max_time=18000,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --wpe --results-directory layout-test-results --debug-rwt-logging --skip-failing-tests --fully-parallel --repeat-each=10 \'imported/w3c/web-platform-tests/html/dom/idlharness.https.html?exclude=(Document|Window|HTML.*)\' \'imported/w3c/web-platform-tests/html/dom/idlharness.https.html?include=HTML.*\' \'try/crash/for/test_with spaces " and \'"\'"\' quotes\' \'try/crash/for/test_with_brackets[]{}\' 2>&1 | Tools/Scripts/filter-test-logs layout']
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='layout-tests')
        return self.run_step()

    @defer.inlineCallbacks
    def test_set_properties_when_executed_scope_this_class(self):
        self.configureStep()
        first_run_failures = ['fast/css/test1.html', 'imported/test/test2.html', 'fast/svg/test3.svg']
        first_run_flakies = ['fast/css/flaky1.html', 'imported/test/flaky2.html', 'fast/svg/flaky3.svg']
        # Set good values for properties that only the superclass should set
        self.setProperty('first_run_failures', first_run_failures)
        self.setProperty('first_run_flakies', first_run_flakies)
        self.setProperty('first_results_exceed_failure_limit', False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        max_time=18000,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --wpe --results-directory layout-test-results --debug-rwt-logging --skip-failing-tests --fully-parallel --repeat-each=10 fast/css/test1.html fast/svg/test3.svg imported/test/test2.html 2>&1 | Tools/Scripts/filter-test-logs layout']
                        )
            .exit(2)
        )
        # Patch LayoutTestFailures.results_from_string() so it always reports fake values.
        # Check this fake values do not end on the properties that belong to the superclass.
        fake_failing_tests = ['fake/should/not/happen/failure1.html', 'imported/fake/failure2.html']
        fake_flaky_tests = ['fake/should/not/happen/flaky1.html', 'imported/fake/flaky2.html']
        fake_layout_test_failures = MockLayoutTestFailures(fake_failing_tests, fake_flaky_tests, True)
        self.patch(LayoutTestFailures, 'results_from_string', lambda f: fake_layout_test_failures)
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = yield self.run_step()
        # first_run properties should not be set to fake_layout_test_failures when running RunWebKitTestsRepeatFailuresRedTree()
        self.expect_property('first_run_failures', first_run_failures)
        self.expect_property('first_run_flakies', first_run_flakies)
        self.assertFalse(self.getProperty('first_results_exceed_failure_limit'))
        # Test also that this fake values are set _only_ for the properties this class should define
        self.expect_property('with_change_repeat_failures_results_nonflaky_failures', fake_failing_tests)
        self.expect_property('with_change_repeat_failures_results_flakies', fake_flaky_tests)
        self.assertTrue(self.getProperty('with_change_repeat_failures_results_exceed_failure_limit'))
        return rc

    def test_last_run_with_patch_ends_with_list_of_failing_tests_then_schedule_update_libs_and_test_without_patch(self):
        self.configureStep()
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', ['fake/should/not/happen/failure1.html', 'imported/fake/failure2.html'])
        self.setProperty('with_change_repeat_failures_results_flakies', [])
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.patch(RunWebKitTestsRepeatFailuresRedTree, 'evaluateResult', lambda s, r: r)
        self.get_nth_step(0).evaluateCommand(FAILURE)
        self.assertTrue(RevertAppliedChanges() in next_steps)
        self.assertTrue(InstallWpeDependencies() in next_steps)
        self.assertTrue(CompileWebKitWithoutChange(retry_build_on_failure=True) in next_steps)
        self.assertTrue(RunWebKitTestsRepeatFailuresWithoutChangeRedTree() in next_steps)
        self.assertFalse(AnalyzeLayoutTestsResultsRedTree() in next_steps)

    def test_last_run_with_patch_ends_with_no_failing_tests_then_go_to_analyze(self):
        self.configureStep()
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', [])
        self.setProperty('with_change_repeat_failures_results_flakies', ['fake/should/not/happen/flaky1.html', 'imported/fake/flaky2.html'])
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.patch(RunWebKitTestsRepeatFailuresRedTree, 'evaluateResult', lambda s, r: r)
        self.get_nth_step(0).evaluateCommand(FAILURE)
        self.assertFalse(RevertAppliedChanges() in next_steps)
        self.assertFalse(InstallWpeDependencies() in next_steps)
        self.assertFalse(CompileWebKitWithoutChange(retry_build_on_failure=True) in next_steps)
        self.assertFalse(RunWebKitTestsRepeatFailuresWithoutChangeRedTree() in next_steps)
        self.assertTrue(AnalyzeLayoutTestsResultsRedTree() in next_steps)


class TestRunWebKitTestsRepeatFailuresWithoutChangeRedTree(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(RunWebKitTestsRepeatFailuresWithoutChangeRedTree())
        self.setProperty('platform', 'wpe')
        self.setProperty('fullPlatform', 'wpe')
        self.setProperty('configuration', 'release')

    def test_success(self):
        self.configureStep()
        first_run_failures = ['fast/css/test1.html', 'imported/test/test2.html', 'fast/svg/test3.svg']
        first_run_flakies = ['fast/css/flaky1.html', 'imported/test/flaky2.html', 'fast/svg/flaky3.svg']
        with_change_repeat_failures_results_nonflaky_failures = ['fast/css/test1.html']
        with_change_repeat_failures_results_flakies = ['imported/test/test2.html', 'fast/svg/test3.svg']
        self.setProperty('first_run_failures', first_run_failures)
        self.setProperty('first_run_flakies', first_run_flakies)
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', with_change_repeat_failures_results_nonflaky_failures)
        self.setProperty('with_change_repeat_failures_results_flakies', with_change_repeat_failures_results_flakies)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        max_time=10800,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --wpe --results-directory layout-test-results --debug-rwt-logging --skip-failing-tests --fully-parallel --repeat-each=10 --skipped=always fast/css/test1.html 2>&1 | Tools/Scripts/filter-test-logs layout']
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='layout-tests')
        return self.run_step()

    def test_step_with_change_did_timeout(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        first_run_failures = ['fast/css/test1.html', 'imported/test/test2.html', 'fast/svg/test3.svg']
        first_run_flakies = ['fast/css/flaky1.html', 'imported/test/flaky2.html', 'fast/svg/flaky3.svg']
        with_change_repeat_failures_results_nonflaky_failures = ['fast/css/test1.html']
        with_change_repeat_failures_results_flakies = ['imported/test/test2.html', 'fast/svg/test3.svg']
        self.setProperty('first_run_failures', first_run_failures)
        self.setProperty('first_run_flakies', first_run_flakies)
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', with_change_repeat_failures_results_nonflaky_failures)
        self.setProperty('with_change_repeat_failures_results_flakies', with_change_repeat_failures_results_flakies)
        self.setProperty('with_change_repeat_failures_timedout', True)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        max_time=10800,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --wpe --results-directory layout-test-results --debug-rwt-logging --skip-failing-tests --fully-parallel --repeat-each=10 --skipped=always fast/css/test1.html fast/svg/test3.svg imported/test/test2.html 2>&1 | Tools/Scripts/filter-test-logs layout']
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='layout-tests')
        return self.run_step()

    @defer.inlineCallbacks
    def test_set_properties_when_executed_scope_this_class(self):
        self.configureStep()
        first_run_failures = ['fast/css/test1.html', 'imported/test/test2.html', 'fast/svg/test3.svg']
        first_run_flakies = ['fast/css/flaky1.html', 'imported/test/flaky2.html', 'fast/svg/flaky3.svg']
        with_change_repeat_failures_results_nonflaky_failures = ['fast/css/test1.html']
        with_change_repeat_failures_results_flakies = ['imported/test/test2.html', 'fast/svg/test3.svg']
        # Set good values for properties that only the superclass should set
        self.setProperty('first_run_failures', first_run_failures)
        self.setProperty('first_run_flakies', first_run_flakies)
        self.setProperty('first_results_exceed_failure_limit', False)
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', with_change_repeat_failures_results_nonflaky_failures)
        self.setProperty('with_change_repeat_failures_results_flakies', with_change_repeat_failures_results_flakies)
        self.setProperty('with_change_repeat_failures_timedout', False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        log_environ=False,
                        max_time=10800,
                        timeout=19800,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --release --wpe --results-directory layout-test-results --debug-rwt-logging --skip-failing-tests --fully-parallel --repeat-each=10 --skipped=always fast/css/test1.html 2>&1 | Tools/Scripts/filter-test-logs layout']
                        )
            .exit(2)
        )
        # Patch LayoutTestFailures.results_from_string() so it always reports fake values.
        # Check this fake values do not end on the properties that belong to the superclass.
        fake_failing_tests = ['fake/should/not/happen/failure1.html', 'imported/fake/failure2.html']
        fake_flaky_tests = ['fake/should/not/happen/flaky1.html', 'imported/fake/flaky2.html']
        fake_layout_test_failures = MockLayoutTestFailures(fake_failing_tests, fake_flaky_tests, True)
        self.patch(LayoutTestFailures, 'results_from_string', lambda f: fake_layout_test_failures)
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = yield self.run_step()
        # first_run properties should not be set to fake_layout_test_failures when running RunWebKitTestsRepeatFailuresWithoutChangeRedTree()
        self.expect_property('first_run_failures', first_run_failures)
        self.expect_property('first_run_flakies', first_run_flakies)
        self.assertFalse(self.getProperty('first_results_exceed_failure_limit'))
        self.expect_property('with_change_repeat_failures_results_nonflaky_failures', with_change_repeat_failures_results_nonflaky_failures)
        self.expect_property('with_change_repeat_failures_results_flakies', with_change_repeat_failures_results_flakies)
        self.assertFalse(self.getProperty('with_change_repeat_failures_timedout'))
        # Test also that this fake values are set _only_ for the properties this class should define
        self.expect_property('without_change_repeat_failures_results_nonflaky_failures', fake_failing_tests)
        self.expect_property('without_change_repeat_failures_results_flakies', fake_flaky_tests)
        self.assertTrue(self.getProperty('without_change_repeat_failures_results_exceed_failure_limit'))
        return rc


class TestAnalyzeLayoutTestsResultsRedTree(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(AnalyzeLayoutTestsResultsRedTree())

    def configureCommonProperties(self):
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        self.setProperty('patch_author', 'test@igalia.com')
        self.setProperty('patch_id', '404044')

    def test_failure_introduced_by_change_clean_tree_green(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', ["test/failure1.html", "test/failure2.html"])
        self.setProperty('first_run_flakies', ["test/flaky1.html", "test/flaky2.html"])
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', ["test/failure1.html"])
        self.setProperty('with_change_repeat_failures_results_flakies', ["test/failure2.html"])
        self.setProperty('without_change_repeat_failures_results_nonflaky_failures', [])
        self.setProperty('without_change_repeat_failures_results_flakies', ["test/pre-existent/flaky.html"])
        self.expect_outcome(result=FAILURE, state_string='Found 1 new test failure: test/failure1.html (failure)')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 2)
        self.assertTrue('Subject: Info about 4 flaky failures' in self._emails_list[0])
        for flaky_test in ["test/flaky1.html", "test/flaky2.html", "test/failure2.html", "test/pre-existent/flaky.html"]:
            self.assertTrue(f'Test name: <a href="https://github.com/WebKit/WebKit/blob/main/LayoutTests/{flaky_test}">{flaky_test}</a>' in self._emails_list[0])
        self.assertFalse('Test name: <a href="https://github.com/WebKit/WebKit/blob/main/LayoutTests/test/failure1.html">test/failure1.html</a>' in self._emails_list[0])
        self.assertTrue('Subject: Layout test failure for Patch' in self._emails_list[1])
        self.assertTrue('test/failure1.html' in self._emails_list[1])
        return step_result

    def test_failure_introduced_by_change_clean_tree_red(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', ["test/failure1.html", "test/failure2.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('first_run_flakies', ["test/flaky1.html", "test/flaky2.html"])
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', ["test/failure1.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('with_change_repeat_failures_results_flakies', ["test/failure2.html"])
        self.setProperty('without_change_repeat_failures_results_nonflaky_failures', ["test/pre-existent/failure.html"])
        self.setProperty('without_change_repeat_failures_results_flakies', ["test/pre-existent/flaky.html"])
        self.expect_outcome(result=FAILURE, state_string='Found 1 new test failure: test/failure1.html (failure)')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 3)
        self.assertTrue('Subject: Info about 4 flaky failures' in self._emails_list[0])
        for flaky_test in ["test/flaky1.html", "test/flaky2.html", "test/failure2.html", "test/pre-existent/flaky.html"]:
            self.assertTrue(f'Test name: <a href="https://github.com/WebKit/WebKit/blob/main/LayoutTests/{flaky_test}">{flaky_test}</a>' in self._emails_list[0])
        self.assertFalse('Test name: <a href="https://github.com/WebKit/WebKit/blob/main/LayoutTests/preexisting-test/pre-existent/failure.html">preexisting-test/pre-existent/failure.html</a>' in self._emails_list[0])
        self.assertTrue('Subject: Info about 1 pre-existent failure at' in self._emails_list[1])
        self.assertTrue('preexisting-test/pre-existent/failure.html' in self._emails_list[1])
        self.assertTrue('Subject: Layout test failure for Patch' in self._emails_list[2])
        self.assertTrue('test/failure1.html' in self._emails_list[2])
        return step_result

    def test_pre_existent_failures(self):
        self.configureStep()
        self.configureCommonProperties()
        # MARK HERE
        self.setProperty('first_run_failures', ["test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('first_run_flakies', ["test/pre-existent/flaky2.html", "test/pre-existent/flaky3.html"])
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', ["test/pre-existent/failure.html"])
        self.setProperty('with_change_repeat_failures_results_flakies', ["test/pre-existent/flaky.html"])
        self.setProperty('without_change_repeat_failures_results_nonflaky_failures', ["test/pre-existent/failure.html"])
        self.setProperty('without_change_repeat_failures_results_flakies', [])
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 2)
        self.assertTrue('Subject: Info about 3 flaky failures' in self._emails_list[0])
        for flaky_test in ["test/pre-existent/flaky.html", "test/pre-existent/flaky2.html", "test/pre-existent/flaky3.html"]:
            self.assertTrue(f'Test name: <a href="https://github.com/WebKit/WebKit/blob/main/LayoutTests/{flaky_test}">{flaky_test}</a>' in self._emails_list[0])
        self.assertFalse('Test name: <a href="https://github.com/WebKit/WebKit/blob/main/LayoutTests/preexisting-test/pre-existent/failure.html">preexisting-test/pre-existent/failure.html</a>' in self._emails_list[0])
        self.assertTrue('Subject: Info about 1 pre-existent failure at' in self._emails_list[1])
        self.assertTrue('preexisting-test/pre-existent/failure.html' in self._emails_list[1])
        return step_result

    def test_pre_existent_flakies(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', ["test/pre-existent/flaky1.html"])
        self.setProperty('first_run_flakies', ["test/pre-existent/flaky2.html", "test/pre-existent/flaky3.html"])
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', [])
        self.setProperty('with_change_repeat_failures_results_flakies', ["test/pre-existent/flaky1.html"])
        self.setProperty('without_change_repeat_failures_results_nonflaky_failures', [])
        self.setProperty('without_change_repeat_failures_results_flakies', [])
        self.setProperty('without_change_repeat_failures_retcode', SUCCESS)
        self.expect_outcome(result=SUCCESS, state_string='Passed layout tests')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 1)
        self.assertTrue('Subject: Info about 3 flaky failures' in self._emails_list[0])
        for flaky_test in ["test/pre-existent/flaky1.html", "test/pre-existent/flaky2.html", "test/pre-existent/flaky3.html"]:
            self.assertTrue(f'Test name: <a href="https://github.com/WebKit/WebKit/blob/main/LayoutTests/{flaky_test}">{flaky_test}</a>' in self._emails_list[0])
        return step_result

    def test_first_step_gives_unexpected_failure_and_clean_tree_pass_last_try(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', [])
        self.setProperty('first_run_flakies', [])
        self.setProperty('retry_count', AnalyzeLayoutTestsResultsRedTree.MAX_RETRY)
        self.setProperty('clean_tree_run_failures', [])
        self.setProperty('clean_tree_run_flakies', ['test/pre-existent/flaky.html'])
        self.setProperty('clean_tree_run_status', WARNINGS)
        self.expect_outcome(result=FAILURE, state_string='Found unexpected failure with change (failure)')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 0)
        return step_result

    def test_first_step_gives_unexpected_failure_and_clean_tree_unexpected_failure_last_try(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', [])
        self.setProperty('first_run_flakies', [])
        self.setProperty('retry_count', AnalyzeLayoutTestsResultsRedTree.MAX_RETRY)
        self.setProperty('clean_tree_run_failures', [])
        self.setProperty('clean_tree_run_flakies', [])
        self.setProperty('clean_tree_run_status', FAILURE)
        expected_infrastructure_error = 'The layout-test run with change generated no list of results and exited with error, and the clean_tree without change run did the same thing.'
        self.expect_outcome(
            result=WARNINGS,
            state_string=f'{expected_infrastructure_error}\nReached the maximum number of retries (3). Unable to determine if change is bad or there is a pre-existent infrastructure issue. (warnings)')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 1)
        self.assertTrue(expected_infrastructure_error in self._emails_list[0])
        return step_result

    def test_first_step_gives_unexpected_failure_retry(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', [])
        self.setProperty('first_run_flakies', [])
        self.setProperty('clean_tree_run_failures', [])
        self.setProperty('retry_count', AnalyzeLayoutTestsResultsRedTree.MAX_RETRY - 1)
        self.setProperty('clean_tree_run_flakies', ['test/pre-existent/flaky.html'])
        self.setProperty('clean_tree_run_status', WARNINGS)
        expected_infrastructure_error = 'The layout-test run with change generated no list of results and exited with error, retrying with the hope it was a random infrastructure error.'
        self.expect_outcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 2 of 3] (retry)')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 1)
        self.assertTrue(expected_infrastructure_error in self._emails_list[0])
        return step_result

    def test_step_retry_with_change_exits_early_error(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', ["test/failure1.html", "test/failure2.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('first_run_flakies', ["test/flaky1.html", "test/flaky2.html"])
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', ["test/failure1.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('with_change_repeat_failures_results_flakies', ["test/failure2.html"])
        self.setProperty('without_change_repeat_failures_results_nonflaky_failures', ["test/pre-existent/failure.html"])
        self.setProperty('without_change_repeat_failures_results_flakies', ["test/pre-existent/flaky.html"])
        self.setProperty('with_change_repeat_failures_results_exceed_failure_limit', True)
        expected_infrastructure_error = 'One of the steps for retrying the failed tests has exited early, but this steps should run without "--exit-after-n-failures" switch, so they should not exit early.'
        self.expect_outcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 0 of 3] (retry)')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 1)
        self.assertTrue(expected_infrastructure_error in self._emails_list[0])
        return step_result

    def test_step_retry_without_change_exits_early_error(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', ["test/failure1.html", "test/failure2.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('first_run_flakies', ["test/flaky1.html", "test/flaky2.html"])
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', ["test/failure1.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('with_change_repeat_failures_results_flakies', ["test/failure2.html"])
        self.setProperty('without_change_repeat_failures_results_nonflaky_failures', ["test/pre-existent/failure.html"])
        self.setProperty('without_change_repeat_failures_results_flakies', ["test/pre-existent/flaky.html"])
        self.setProperty('without_change_repeat_failures_results_exceed_failure_limit', True)
        expected_infrastructure_error = 'One of the steps for retrying the failed tests has exited early, but this steps should run without "--exit-after-n-failures" switch, so they should not exit early.'
        self.expect_outcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 0 of 3] (retry)')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 1)
        self.assertTrue(expected_infrastructure_error in self._emails_list[0])
        return step_result

    def test_step_retry_with_change_timeouts(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', ["test/failure1.html", "test/failure2.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('first_run_flakies', ["test/flaky1.html", "test/flaky2.html"])
        self.setProperty('with_change_repeat_failures_timedout', True)
        self.setProperty('without_change_repeat_failures_results_nonflaky_failures', ["test/pre-existent/failure.html"])
        self.setProperty('without_change_repeat_failures_results_flakies', ["test/pre-existent/flaky.html"])
        self.expect_outcome(result=FAILURE, state_string='Found 2 new test failures: test/failure1.html test/failure2.html (failure)')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 2)
        expected_infrastructure_error = 'The step "layout-tests-repeat-failures-with-change" reached the timeout but the step "layout-tests-repeat-failures-without-change" ended. Not trying to repeat this. Reporting 2 failures from the first run.'
        self.assertTrue(expected_infrastructure_error in self._emails_list[0])
        self.assertTrue('Subject: Layout test failure for Patch' in self._emails_list[1])
        for failed_test in ['test/failure1.html', 'test/failure2.html']:
            self.assertTrue(failed_test in self._emails_list[1])
        return step_result

    def test_step_retry_with_change_unexpected_error(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', ["test/failure1.html", "test/failure2.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('first_run_flakies', ["test/flaky1.html", "test/flaky2.html"])
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', [])
        self.setProperty('with_change_repeat_failures_results_flakies', [])
        self.setProperty('with_change_repeat_failures_retcode', FAILURE)
        expected_infrastructure_error = 'The step "layout-tests-repeat-failures" failed to generate any list of failures or flakies and returned an error code.'
        self.expect_outcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 0 of 3] (retry)')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 1)
        self.assertTrue(expected_infrastructure_error in self._emails_list[0])
        return step_result

    def test_step_retry_without_change_unexpected_error(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', ["test/failure1.html", "test/failure2.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('first_run_flakies', ["test/flaky1.html", "test/flaky2.html"])
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', ["test/failure1.html", "test/failure2.html"])
        self.setProperty('with_change_repeat_failures_results_flakies', [])
        self.setProperty('with_change_repeat_failures_retcode', FAILURE)
        self.setProperty('without_change_repeat_failures_results_nonflaky_failures', [])
        self.setProperty('without_change_repeat_failures_results_flakies', [])
        self.setProperty('without_change_repeat_failures_retcode', FAILURE)
        expected_infrastructure_error = 'The step "layout-tests-repeat-failures-without-change" failed to generate any list of failures or flakies and returned an error code.'
        self.expect_outcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 0 of 3] (retry)')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 1)
        self.assertTrue(expected_infrastructure_error in self._emails_list[0])
        return step_result

    def test_step_retry_without_change_timeouts(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', ["test/failure1.html", "test/failure2.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('first_run_flakies', ["test/flaky1.html", "test/flaky2.html"])
        self.setProperty('with_change_repeat_failures_timedout', True)
        self.setProperty('without_change_repeat_failures_timedout', True)
        expected_infrastructure_error = 'The step "layout-tests-repeat-failures-without-change" was interrumped because it reached the timeout.'
        self.expect_outcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 0 of 3] (retry)')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 1)
        self.assertTrue(expected_infrastructure_error in self._emails_list[0])
        return step_result

    def test_step_retry_with_change_timeouts_and_without_change_timeouts(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', ["test/failure1.html", "test/failure2.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('first_run_flakies', ["test/flaky1.html", "test/flaky2.html"])
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', ["test/failure1.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('with_change_repeat_failures_results_flakies', ["test/failure2.html"])
        self.setProperty('without_change_repeat_failures_timedout', True)
        expected_infrastructure_error = 'The step "layout-tests-repeat-failures-without-change" was interrumped because it reached the timeout.'
        self.expect_outcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 0 of 3] (retry)')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 1)
        self.assertTrue(expected_infrastructure_error in self._emails_list[0])
        return step_result

    def test_retry_third_time(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', ["test/failure1.html", "test/failure2.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('first_run_flakies', ["test/flaky1.html", "test/flaky2.html"])
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', ["test/failure1.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('with_change_repeat_failures_results_flakies', ["test/failure2.html"])
        self.setProperty('without_change_repeat_failures_timedout', True)
        self.setProperty('retry_count', 2)
        expected_infrastructure_error = 'The step "layout-tests-repeat-failures-without-change" was interrumped because it reached the timeout.'
        self.expect_outcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 2 of 3] (retry)')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 1)
        self.assertTrue(expected_infrastructure_error in self._emails_list[0])
        return step_result

    def test_retry_finish(self):
        self.configureStep()
        self.configureCommonProperties()
        self.setProperty('first_run_failures', ["test/failure1.html", "test/failure2.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('first_run_flakies', ["test/flaky1.html", "test/flaky2.html"])
        self.setProperty('with_change_repeat_failures_results_nonflaky_failures', ["test/failure1.html", "test/pre-existent/failure.html", "test/pre-existent/flaky.html"])
        self.setProperty('with_change_repeat_failures_results_flakies', ["test/failure2.html"])
        self.setProperty('without_change_repeat_failures_timedout', True)
        self.setProperty('retry_count', 3)
        expected_infrastructure_error = 'The step "layout-tests-repeat-failures-without-change" was interrumped because it reached the timeout.'
        self.expect_outcome(result=WARNINGS, state_string=f'{expected_infrastructure_error}\nReached the maximum number of retries (3). Unable to determine if change is bad or there is a pre-existent infrastructure issue. (warnings)')
        step_result = self.run_step()
        self.assertEqual(len(self._emails_list), 1)
        self.assertTrue(expected_infrastructure_error in self._emails_list[0])
        return step_result


class TestCheckOutSpecificRevision(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CheckOutSpecificRevision())
        self.setProperty('ews_revision', '1a3425cb92dbcbca12a10aa9514f1b77c76dc26')
        self.expect_hidden(False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=1200,
                        log_environ=False,
                        command=['git', 'checkout', '--progress', '1a3425cb92dbcbca12a10aa9514f1b77c76dc26'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Checked out required revision')
        return self.run_step()

    def test_failure(self):
        self.setup_step(CheckOutSpecificRevision())
        self.setProperty('ews_revision', '1a3425cb92dbcbca12a10aa9514f1b77c76dc26')
        self.expect_hidden(False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=1200,
                        log_environ=False,
                        command=['git', 'checkout', '--progress', '1a3425cb92dbcbca12a10aa9514f1b77c76dc26'],
                        )
            .log('stdio', stdout='Unexpected failure')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Checked out required revision (failure)')
        return self.run_step()

    def test_skip(self):
        self.setup_step(CheckOutSpecificRevision())
        self.expect_hidden(True)
        self.expect_outcome(result=SKIPPED, state_string='Checked out required revision (skipped)')
        return self.run_step()


class TestCleanWorkingDirectory(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CleanWorkingDirectory())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/clean-webkit'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Cleaned working directory')
        return self.run_step()

    def test_success_wpe(self):
        self.setup_step(CleanWorkingDirectory())
        self.setProperty('platform', 'wpe')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/clean-webkit', '--keep-jhbuild-directory'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Cleaned working directory')
        return self.run_step()

    def test_failure(self):
        self.setup_step(CleanWorkingDirectory())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/clean-webkit'],
                        )
            .log('stdio', stdout='Unexpected failure.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Cleaned working directory (failure)')
        return self.run_step()


class TestUpdateWorkingDirectory(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(UpdateWorkingDirectory())
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                command=['git', 'checkout', '--progress', 'remotes/origin/main', '-f'],
            ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git branch -D main || true'],
                        ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'checkout', '--progress', '-b', 'main'],
                        ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Updated working directory')
        return self.run_step()

    def test_success_branch(self):
        self.setup_step(UpdateWorkingDirectory())
        self.setProperty('github.base.ref', 'safari-xxx-branch')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                command=['git', 'checkout', '--progress', 'remotes/origin/safari-xxx-branch', '-f'],
            ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git branch -D safari-xxx-branch || true'],
                        ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'checkout', '--progress', '-b', 'safari-xxx-branch'],
                        ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git branch -D main || true'],
                        ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'branch', '--track', 'main', 'remotes/origin/main'],
                        ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Updated working directory')
        return self.run_step()

    def test_success_remote(self):
        self.setup_step(UpdateWorkingDirectory())
        self.setProperty('remote', 'security')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                command=['git', 'checkout', '--progress', 'remotes/security/main', '-f'],
            ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git branch -D main || true'],
                        ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'checkout', '--progress', '-b', 'main'],
                        ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Updated working directory')
        return self.run_step()

    def test_success_remote_branch(self):
        self.setup_step(UpdateWorkingDirectory())
        self.setProperty('remote', 'security')
        self.setProperty('github.base.ref', 'safari-xxx-branch')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                command=['git', 'checkout', '--progress', 'remotes/security/safari-xxx-branch', '-f'],
            ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git branch -D safari-xxx-branch || true'],
                        ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'checkout', '--progress', '-b', 'safari-xxx-branch'],
                        ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git branch -D main || true'],
                        ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'branch', '--track', 'main', 'remotes/origin/main'],
                        ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Updated working directory')
        return self.run_step()

    def test_failure(self):
        self.setup_step(UpdateWorkingDirectory())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'checkout', '--progress', 'remotes/origin/main', '-f'],
                        )
            .log('stdio', stdout='Unexpected failure.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to updated working directory')
        return self.run_step()


class TestApplyPatch(BuildStepMixinAdditions, unittest.TestCase):
    READ_LIMIT = 1000
    ENV = dict(FILTER_BRANCH_SQUELCH_WARNING='1')

    @staticmethod
    def downloadFileRecordingContents(limit, recorder):
        def behavior(command):
            reader = command.args['reader']
            data = reader.remote_read(limit)
            recorder(data)
            reader.remote_close()
        return behavior

    def setUp(self):
        self.longMessage = True

        def mock_start(cls, *args, **kwargs):
            from buildbot.steps import shell
            return shell.ShellCommand.start(cls)
        ApplyPatch.start = mock_start
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    @expectedFailure
    def test_success(self):
        self.setup_step(ApplyPatch())
        self.setProperty('patch_id', '1234')
        self.assertEqual(ApplyPatch.flunkOnFailure, True)
        self.assertEqual(ApplyPatch.haltOnFailure, True)
        buf = []
        self.expectRemoteCommands(
            Expect('downloadFile', dict(
                workerdest='.buildbot-diff', workdir='wkdir',
                blocksize=1024 * 32, maxsize=None, mode=None,
                reader=ExpectRemoteRef(remotetransfer.FileReader),
            ))
            + Expect.behavior(self.downloadFileRecordingContents(self.READ_LIMIT, buf.append))
            .exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['curl', '-L', 'https://bugs.webkit.org/attachment.cgi?id=1234', '-o', '.buildbot-diff'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'config', 'user.name', 'EWS'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'config', 'user.email', 'ews@webkit.org'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'am', '--keep-non-patch', '.buildbot-diff'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'filter-branch', '-f', '--msg-filter', 'python3 -c "{}"'.format(ApplyPatch.FILTER_BRANCH_PROGRAM), 'HEAD...HEAD~1'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Applied patch')
        return self.run_step()

    @expectedFailure
    def test_success_win(self):
        self.setup_step(ApplyPatch())
        self.setProperty('patch_id', '1234')
        self.setProperty('platform', 'win')
        self.assertEqual(ApplyPatch.flunkOnFailure, True)
        self.assertEqual(ApplyPatch.haltOnFailure, True)
        buf = []
        self.expectRemoteCommands(
            Expect('downloadFile', dict(
                workerdest='.buildbot-diff', workdir='wkdir',
                blocksize=1024 * 32, maxsize=None, mode=None,
                reader=ExpectRemoteRef(remotetransfer.FileReader),
            ))
            + Expect.behavior(self.downloadFileRecordingContents(self.READ_LIMIT, buf.append))
            .exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['curl', '-L', 'https://bugs.webkit.org/attachment.cgi?id=1234', '-o', '.buildbot-diff'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'config', 'user.name', 'EWS'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'config', 'user.email', 'ews@webkit.org'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'am', '--keep-non-patch', '.buildbot-diff'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Applied patch')
        return self.run_step()

    @expectedFailure
    def test_failure(self):
        self.setup_step(ApplyPatch())
        self.setProperty('patch_id', '1234')
        buf = []
        self.expectRemoteCommands(
            Expect('downloadFile', dict(
                workerdest='.buildbot-diff', workdir='wkdir',
                blocksize=1024 * 32, maxsize=None, mode=None,
                reader=ExpectRemoteRef(remotetransfer.FileReader),
            ))
            + Expect.behavior(self.downloadFileRecordingContents(self.READ_LIMIT, buf.append))
            .exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['curl', '-L', 'https://bugs.webkit.org/attachment.cgi?id=1234', '-o', '.buildbot-diff'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'config', 'user.name', 'EWS'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'config', 'user.email', 'ews@webkit.org'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'am', '--keep-non-patch', '.buildbot-diff'],
            ).exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='git failed to apply patch to trunk')
        rc = self.run_step()
        self.expect_property('comment_text', None)
        self.expect_property('build_finish_summary', None)
        return rc

    def test_skipped(self):
        self.setup_step(ApplyPatch())
        self.expect_hidden(True)
        self.expect_outcome(result=SKIPPED, state_string="Skipping applying patch since patch_id isn't provided")
        return self.run_step()


class TestCheckOutPullRequest(BuildStepMixinAdditions, unittest.TestCase):
    ENV = dict(
        GIT_COMMITTER_NAME='EWS',
        GIT_COMMITTER_EMAIL='ews@webkit.org',
        GIT_USER='webkit-commit-queue',
        GIT_PASSWORD='password',
    )

    def setUp(self):
        self.longMessage = True
        GitHub.credentials = lambda user=None: ('webkit-commit-queue', 'password')
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CheckOutPullRequest())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.repo.full_name', 'Contributor/WebKit')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('github.base.sha', 'aaebef7312238f3ad1d25e8894916a1aaea45ba1')
        self.setProperty('got_revision', '59dab0396721db221c264aad3c0cea37ef0d297b')
        self.assertEqual(CheckOutPullRequest.flunkOnFailure, True)
        self.assertEqual(CheckOutPullRequest.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'config', 'credential.helper', '!echo_credentials() { sleep 1; echo "username=${GIT_USER}"; echo "password=${GIT_PASSWORD}"; }; echo_credentials'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git remote add Contributor https://github.com/Contributor/WebKit.git || true'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'remote', 'set-url', 'Contributor', 'https://github.com/Contributor/WebKit.git'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'fetch', 'Contributor', 'eng/pull-request-branch'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'checkout', '--progress', '-b', 'eng/pull-request-branch'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'cherry-pick', '--allow-empty', 'HEAD..remotes/Contributor/eng/pull-request-branch'],
            ).exit(0)
        )
        self.expect_outcome(result=SUCCESS, state_string='Checked out pull request')
        return self.run_step()

    def test_success_apple(self):
        self.setup_step(CheckOutPullRequest())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.repo.full_name', 'Contributor/WebKit-apple')
        self.setProperty('remote', 'apple')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('github.base.sha', 'aaebef7312238f3ad1d25e8894916a1aaea45ba1')
        self.setProperty('got_revision', '59dab0396721db221c264aad3c0cea37ef0d297b')
        self.assertEqual(CheckOutPullRequest.flunkOnFailure, True)
        self.assertEqual(CheckOutPullRequest.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'config', 'credential.helper', '!echo_credentials() { sleep 1; echo "username=${GIT_USER}"; echo "password=${GIT_PASSWORD}"; }; echo_credentials'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git remote add Contributor-apple https://github.com/Contributor/WebKit-apple.git || true'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'remote', 'set-url', 'Contributor-apple', 'https://github.com/Contributor/WebKit-apple.git'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'fetch', 'Contributor-apple', 'eng/pull-request-branch'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'checkout', '--progress', '-b', 'eng/pull-request-branch'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'cherry-pick', '--allow-empty', 'HEAD..remotes/Contributor-apple/eng/pull-request-branch']
            ).exit(0)
        )
        self.expect_outcome(result=SUCCESS, state_string='Checked out pull request')
        return self.run_step()

    def test_success_integration_remote(self):
        self.setup_step(CheckOutPullRequest())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.repo.full_name', 'WebKit/WebKit-integration')
        self.setProperty('remote', 'origin')
        self.setProperty('github.head.ref', 'integration/ci/1234')
        self.setProperty('github.base.sha', 'aaebef7312238f3ad1d25e8894916a1aaea45ba1')
        self.setProperty('got_revision', '59dab0396721db221c264aad3c0cea37ef0d297b')
        self.assertEqual(CheckOutPullRequest.flunkOnFailure, True)
        self.assertEqual(CheckOutPullRequest.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'config', 'credential.helper', '!echo_credentials() { sleep 1; echo "username=${GIT_USER}"; echo "password=${GIT_PASSWORD}"; }; echo_credentials'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git remote add WebKit-integration https://github.com/WebKit/WebKit-integration.git || true'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'remote', 'set-url', 'WebKit-integration', 'https://github.com/WebKit/WebKit-integration.git'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'fetch', 'WebKit-integration', 'integration/ci/1234'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'checkout', '--progress', '-b', 'integration/ci/1234'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'cherry-pick', '--allow-empty', 'HEAD..remotes/WebKit-integration/integration/ci/1234'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Checked out pull request')
        return self.run_step()

    def test_success_win(self):
        self.setup_step(CheckOutPullRequest())
        self.setProperty('platform', 'win')
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.repo.full_name', 'Contributor/WebKit')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('github.base.sha', 'aaebef7312238f3ad1d25e8894916a1aaea45ba1')
        self.setProperty('got_revision', '59dab0396721db221c264aad3c0cea37ef0d297b')
        self.assertEqual(CheckOutPullRequest.flunkOnFailure, True)
        self.assertEqual(CheckOutPullRequest.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'config', 'credential.helper', '!echo_credentials() { sleep 1; echo "username=${GIT_USER}"; echo "password=${GIT_PASSWORD}"; }; echo_credentials'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['bash', '--posix', '-o', 'pipefail', '-c', 'git remote add Contributor https://github.com/Contributor/WebKit.git || exit 0'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'remote', 'set-url', 'Contributor', 'https://github.com/Contributor/WebKit.git'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'fetch', 'Contributor', 'eng/pull-request-branch'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'checkout', '--progress', '-b', 'eng/pull-request-branch'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'cherry-pick', '--allow-empty', 'HEAD..remotes/Contributor/eng/pull-request-branch'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Checked out pull request')
        return self.run_step()

    def test_failure(self):
        self.setup_step(CheckOutPullRequest())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.repo.full_name', 'Contributor/WebKit')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('github.base.sha', 'aaebef7312238f3ad1d25e8894916a1aaea45ba1')
        self.setProperty('got_revision', '59dab0396721db221c264aad3c0cea37ef0d297b')
        self.assertEqual(CheckOutPullRequest.flunkOnFailure, True)
        self.assertEqual(CheckOutPullRequest.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'config', 'credential.helper', '!echo_credentials() { sleep 1; echo "username=${GIT_USER}"; echo "password=${GIT_PASSWORD}"; }; echo_credentials'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git remote add Contributor https://github.com/Contributor/WebKit.git || true'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=600,
                log_environ=False,
                env=self.ENV,
                command=['git', 'remote', 'set-url', 'Contributor', 'https://github.com/Contributor/WebKit.git'],
            ).exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to checkout and rebase branch from PR 1234')
        return self.run_step()

    def test_skipped(self):
        self.setup_step(CheckOutPullRequest())
        self.expect_hidden(True)
        self.expect_outcome(result=SKIPPED, state_string='No pull request to checkout')
        return self.run_step()


class TestRevertAppliedChanges(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RevertAppliedChanges())
        self.setProperty('got_revision', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c')
        self.setProperty('github.number', 1234)
        self.expect_hidden(False)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=5 * 60,
                command=['git', 'clean', '-f', '-d'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=5 * 60,
                command=['git', 'checkout', '--progress', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Reverted applied changes')
        return self.run_step()

    def test_success_exclude(self):
        self.setup_step(RevertAppliedChanges(exclude=['directory*']))
        self.setProperty('got_revision', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c')
        self.setProperty('github.number', 1234)
        self.expect_hidden(False)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=5 * 60,
                command=['git', 'clean', '-f', '-d', '-e', 'directory*'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=5 * 60,
                command=['git', 'checkout', '--progress', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Reverted applied changes')
        return self.run_step()

    def test_failure(self):
        self.setup_step(RevertAppliedChanges())
        self.setProperty('ews_revision', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c')
        self.setProperty('github.number', 1234)
        self.expect_hidden(False)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=5 * 60,
                command=['git', 'clean', '-f', '-d'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=5 * 60,
                command=['git', 'checkout', '--progress', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c'],
            )
            .log('stdio', stdout='Unexpected failure.').exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Reverted applied changes (failure)')
        return self.run_step()

    def test_patch(self):
        self.setup_step(RevertAppliedChanges())
        self.setProperty('got_revision', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c')
        self.expect_hidden(False)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=5 * 60,
                command=['git', 'clean', '-f', '-d'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=5 * 60,
                command=['git', 'checkout', '--progress', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Reverted applied changes')
        return self.run_step()

    def test_glib_cleanup(self):
        self.setup_step(RevertAppliedChanges())
        self.setProperty('got_revision', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c')
        self.setProperty('github.number', 1234)
        self.setProperty('platform', 'gtk')
        self.setProperty('configuration', 'release')
        self.expect_hidden(False)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=5 * 60,
                command=['git', 'clean', '-f', '-d'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=5 * 60,
                command=['git', 'checkout', '--progress', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=5 * 60,
                command=['rm', '-f', 'WebKitBuild/GTK/Release/build-webkit-options.txt'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Reverted applied changes')
        return self.run_step()


class TestCheckChangeRelevance(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_relevant_jsc_patch(self):
        file_names = ['JSTests/', 'Source/JavaScriptCore/', 'Source/WTF/', 'Source/bmalloc/', 'Makefile', 'Makefile.shared',
                      'Source/Makefile', 'Source/Makefile.shared', 'Tools/Scripts/build-webkit', 'Tools/Scripts/build-jsc',
                      'Tools/Scripts/jsc-stress-test-helpers/', 'Tools/Scripts/run-jsc', 'Tools/Scripts/run-jsc-benchmarks',
                      'Tools/Scripts/run-jsc-stress-tests', 'Tools/Scripts/run-javascriptcore-tests', 'Tools/Scripts/run-layout-jsc',
                      'Tools/Scripts/update-javascriptcore-test-results', 'Tools/Scripts/webkitdirs.pm',
                      'Source/cmake/OptionsJSCOnly.cmake']

        self.setup_step(CheckChangeRelevance())
        self.setProperty('buildername', 'JSC-Tests-EWS')
        self.assertEqual(CheckChangeRelevance.haltOnFailure, True)
        self.assertEqual(CheckChangeRelevance.flunkOnFailure, True)
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: f'Sample patch; file: {file_name}'
            self.expect_outcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.run_step()
        return rc

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_relevant_jsc_arm64_patch(self):
        file_names = ['JSTests/', 'Source/JavaScriptCore/', 'Source/WTF/', 'Source/bmalloc/', 'Makefile', 'Makefile.shared',
                      'Source/Makefile', 'Source/Makefile.shared', 'Tools/Scripts/build-webkit', 'Tools/Scripts/build-jsc',
                      'Tools/Scripts/jsc-stress-test-helpers/', 'Tools/Scripts/run-jsc', 'Tools/Scripts/run-jsc-benchmarks',
                      'Tools/Scripts/run-jsc-stress-tests', 'Tools/Scripts/run-javascriptcore-tests', 'Tools/Scripts/run-layout-jsc',
                      'Tools/Scripts/update-javascriptcore-test-results', 'Tools/Scripts/webkitdirs.pm',
                      'Source/cmake/OptionsJSCOnly.cmake']

        self.setup_step(CheckChangeRelevance())
        self.setProperty('buildername', 'JSC-Tests-arm64-EWS')
        self.assertEqual(CheckChangeRelevance.haltOnFailure, True)
        self.assertEqual(CheckChangeRelevance.flunkOnFailure, True)
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: f'Sample patch; file: {file_name}'
            self.expect_outcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.run_step()
        return rc

    def test_relevant_wk1_patch(self):
        file_names = ['Source/WebKitLegacy', 'Source/WebCore', 'Source/WebInspectorUI', 'Source/WebDriver', 'Source/WTF',
                      'Source/bmalloc', 'Source/JavaScriptCore', 'Source/ThirdParty', 'LayoutTests', 'Tools']

        self.setup_step(CheckChangeRelevance())
        self.setProperty('buildername', 'macOS-Catalina-Release-WK1-Tests-EWS')
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: f'Sample patch; file: {file_name}'
            self.expect_outcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.run_step()
        return rc

    @expectedFailure
    def test_relevant_monterey_builder_patch(self):
        file_names = ['Source/xyz', 'Tools/abc']
        self.setup_step(CheckChangeRelevance())
        self.setProperty('buildername', 'macOS-Monterey-Release-Build-EWS')
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: f'Sample patch; file: {file_name}'
            self.expect_outcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.run_step()
        return rc

    def test_relevant_wk1_patch(self):
        CheckChangeRelevance._get_patch = lambda x: b'Sample patch; file: Source/WebKitLegacy'
        self.setup_step(CheckChangeRelevance())
        self.setProperty('buildername', 'macOS-Monterey-Release-WK1-Tests-EWS')
        self.expect_outcome(result=SUCCESS, state_string='Patch contains relevant changes')
        return self.run_step()

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_relevant_webkitpy_patch(self):
        file_names = ['Tools/Scripts/webkitpy', 'Tools/Scripts/libraries', 'Tools/Scripts/commit-log-editor']
        self.setup_step(CheckChangeRelevance())
        self.setProperty('buildername', 'WebKitPy-Tests-EWS')
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: f'Sample patch; file: {file_name}'
            self.expect_outcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.run_step()
        return rc

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_relevant_services_patch(self):
        file_names = ['Tools/CISupport/build-webkit-org', 'Tools/CISupport/ews-build', 'Tools/CISupport/Shared',
                      'Tools/Scripts/libraries/resultsdbpy', 'Tools/Scripts/libraries/webkitcorepy', 'Tools/Scripts/libraries/webkitscmpy']
        self.setup_step(CheckChangeRelevance())
        self.setProperty('buildername', 'Services-EWS')
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: f'Sample patch; file: {file_name}'
            self.expect_outcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.run_step()
        return rc

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_relevant_services_pull_request(self):
        file_names = ['Tools/CISupport/build-webkit-org', 'Tools/CISupport/ews-build', 'Tools/CISupport/Shared',
                      'Tools/Scripts/libraries/resultsdbpy', 'Tools/Scripts/libraries/webkitcorepy', 'Tools/Scripts/libraries/webkitscmpy']
        self.setup_step(CheckChangeRelevance())
        self.setProperty('buildername', 'Services-EWS')
        self.setProperty('github.number', 1234)
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: file_name
            self.expect_outcome(result=SUCCESS, state_string='Pull request contains relevant changes')
            rc = self.run_step()
        return rc

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_relevant_safer_cpp_pull_request(self):
        file_names = ['Tools/CISupport/Shared/download-and-install-build-tools', 'Tools/Scripts/build-and-analyze', 'Source/WebKit']
        self.setup_step(CheckChangeRelevance())
        self.setProperty('buildername', 'Safer-CPP-Checks-EWS')
        self.setProperty('github.number', 1234)
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: file_name
            self.expect_outcome(result=SUCCESS, state_string='Pull request contains relevant changes')
            rc = self.run_step()
        return rc

    @expectedFailure
    def test_relevant_bindings_tests_patch(self):
        file_names = ['Source/WebCore', 'Tools']
        self.setup_step(CheckChangeRelevance())
        self.setProperty('buildername', 'Bindings-Tests-EWS')
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: f'Sample patch; file: {file_name}'
            self.expect_outcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.run_step()
        return rc

    @expectedFailure
    def test_relevant_bindings_tests_pull_request(self):
        file_names = ['Source/WebCore', 'Tools']
        self.setup_step(CheckChangeRelevance())
        self.setProperty('buildername', 'Bindings-Tests-EWS')
        self.setProperty('github.number', 1234)
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: file_name
            self.expect_outcome(result=SUCCESS, state_string='Pull request contains relevant changes')
            rc = self.run_step()
        return rc

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_queues_without_relevance_info(self):
        CheckChangeRelevance._get_patch = lambda x: 'Sample patch'
        queues = ['Commit-Queue', 'Style-EWS', 'GTK-Build-EWS', 'GTK-WK2-Tests-EWS',
                  'iOS-13-Build-EWS', 'iOS-13-Simulator-Build-EWS', 'iOS-13-Simulator-WK2-Tests-EWS',
                  'macOS-Catalina-Release-Build-EWS', 'macOS-Catalina-Release-WK2-Tests-EWS', 'macOS-Catalina-Debug-Build-EWS',
                  'PlayStation-Build-EWS', 'Win-Build-EWS', 'WPE-Build-EWS', 'WebKitPerl-Tests-EWS', 'WPE-Cairo-LibWebRTC-Build-EWS']
        for queue in queues:
            self.setup_step(CheckChangeRelevance())
            self.setProperty('buildername', queue)
            self.expect_outcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.run_step()
        return rc

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_non_relevant_patch_on_various_queues(self):
        CheckChangeRelevance._get_patch = lambda x: 'Sample patch'
        queues = ['Bindings-Tests-EWS', 'JSC-Tests-EWS', 'macOS-Monterey-Release-Build-EWS',
                  'macOS-Catalina-Debug-WK1-Tests-EWS', 'macOS-Safer-CPP-Checks-EWS', 'Services-EWS', 'WebKitPy-Tests-EWS']
        for queue in queues:
            self.setup_step(CheckChangeRelevance())
            self.setProperty('buildername', queue)
            self.expect_outcome(result=FAILURE, state_string='Patch doesn\'t have relevant changes')
            rc = self.run_step()
        return rc

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_non_relevant_pull_request_on_various_queues(self):
        CheckChangeRelevance._get_patch = lambda x: '\n'
        queues = ['Bindings-Tests-EWS', 'JSC-Tests-EWS', 'macOS-Monterey-Release-Build-EWS',
                  'macOS-Catalina-Debug-WK1-Tests-EWS', 'macOS-Safer-CPP-Checks-EWS', 'Services-EWS', 'WebKitPy-Tests-EWS']
        for queue in queues:
            self.setup_step(CheckChangeRelevance())
            self.setProperty('buildername', queue)
            self.setProperty('github.number', 1234)
            self.expect_outcome(result=FAILURE, state_string='Pull request doesn\'t have relevant changes')
            rc = self.run_step()
        return rc


class TestGetTestExpectationsBaseline(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(GetTestExpectationsBaseline())
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir', log_environ=True, command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --print-expectations --debug > base-expectations.txt']).exit(0)
        )
        self.expect_outcome(result=SUCCESS, state_string='Found baseline expectations for layout tests')
        return self.run_step()

    def test_additional_args(self):
        self.setup_step(GetTestExpectationsBaseline())
        self.setProperty('configuration', 'release')
        self.setProperty('additionalArguments', ["--child-processes=6", "--exclude-tests", "imported/w3c/web-platform-tests"])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir', log_environ=True, command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --print-expectations --release --child-processes=6 --exclude-tests imported/w3c/web-platform-tests > base-expectations.txt']).exit(0)
        )
        self.expect_outcome(result=SUCCESS, state_string='Found baseline expectations for layout tests')
        return self.run_step()


class TestGetUpdatedTestExpectations(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(GetUpdatedTestExpectations())
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=True,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --print-expectations --debug > new-expectations.txt'],
                        )
            .exit(0),
            ExpectShell(workdir='wkdir', command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "perl -p -i -e 's/\\].*/\\]/' base-expectations.txt"]).exit(0),
            ExpectShell(workdir='wkdir', command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "perl -p -i -e 's/\\].*/\\]/' new-expectations.txt"]).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Found updated expectations for layout tests')
        rc = self.run_step()
        return rc

    def test_additional_args(self):
        self.setup_step(GetUpdatedTestExpectations())
        self.setProperty('configuration', 'release')
        self.setProperty('additionalArguments', ["--child-processes=6", "--exclude-tests", "imported/w3c/web-platform-tests"])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=True,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webkit-tests --print-expectations --release --child-processes=6 --exclude-tests imported/w3c/web-platform-tests > new-expectations.txt'],
                        )
            .exit(0),
            ExpectShell(workdir='wkdir', command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "perl -p -i -e 's/\\].*/\\]/' base-expectations.txt"]).exit(0),
            ExpectShell(workdir='wkdir', command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "perl -p -i -e 's/\\].*/\\]/' new-expectations.txt"]).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Found updated expectations for layout tests')
        rc = self.run_step()
        return rc


class TestFindModifiedLayoutTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(FindModifiedLayoutTests())
        self.assertEqual(FindModifiedLayoutTests.haltOnFailure, True)
        self.assertEqual(FindModifiedLayoutTests.flunkOnFailure, True)
        FindModifiedLayoutTests._get_patch = lambda x: b'+++ LayoutTests/http/tests/events/device-orientation-motion-insecure-context.html'
        self.expect_outcome(result=SUCCESS, state_string='Patch contains relevant changes')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir', log_environ=False, command=['diff', '-u', '-w', 'base-expectations.txt', 'new-expectations.txt']).exit(0)
        )
        rc = self.run_step()
        self.expect_property('modified_tests', ['LayoutTests/http/tests/events/device-orientation-motion-insecure-context.html'])
        return rc

    def test_success_svg(self):
        self.setup_step(FindModifiedLayoutTests())
        self.assertEqual(FindModifiedLayoutTests.haltOnFailure, True)
        self.assertEqual(FindModifiedLayoutTests.flunkOnFailure, True)
        FindModifiedLayoutTests._get_patch = lambda x: b'+++ LayoutTests/svg/filters/feConvolveMatrix-clipped.svg'
        self.expect_outcome(result=SUCCESS, state_string='Patch contains relevant changes')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir', log_environ=False, command=['diff', '-u', '-w', 'base-expectations.txt', 'new-expectations.txt']).exit(0)
        )
        rc = self.run_step()
        self.expect_property('modified_tests', ['LayoutTests/svg/filters/feConvolveMatrix-clipped.svg'])
        return rc

    def test_success_xml(self):
        self.setup_step(FindModifiedLayoutTests())
        self.assertEqual(FindModifiedLayoutTests.haltOnFailure, True)
        self.assertEqual(FindModifiedLayoutTests.flunkOnFailure, True)
        FindModifiedLayoutTests._get_patch = lambda x: b'+++ LayoutTests/fast/table/037.xml'
        self.expect_outcome(result=SUCCESS, state_string='Patch contains relevant changes')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir', log_environ=False, command=['diff', '-u', '-w', 'base-expectations.txt', 'new-expectations.txt']).exit(0)
        )
        rc = self.run_step()
        self.expect_property('modified_tests', ['LayoutTests/fast/table/037.xml'])
        return rc

    @expectedFailure
    def test_ignore_certain_directories(self):
        self.setup_step(FindModifiedLayoutTests())
        dir_names = ['reference', 'reftest', 'resources', 'support', 'script-tests', 'tools']
        FindModifiedLayoutTests._get_patch = lambda x: f'+++ LayoutTests/reference/test-name.html'.encode('utf-8')
        self.expect_outcome(result=SKIPPED, state_string='Patch doesn\'t have relevant changes')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir', log_environ=False, command=['diff', '-u', '-w', 'base-expectations.txt', 'new-expectations.txt']).exit(0)
        )
        rc = self.run_step()
        self.expect_property('modified_tests', None)
        return rc

    @expectedFailure
    def test_ignore_certain_directories_svg(self):
        self.setup_step(FindModifiedLayoutTests())
        dir_names = ['reference', 'reftest', 'resources', 'support', 'script-tests', 'tools']
        FindModifiedLayoutTests._get_patch = lambda x: f'+++ LayoutTests/reference/test-name.svg'.encode('utf-8')
        self.expect_outcome(result=SKIPPED, state_string='Patch doesn\'t have relevant changes')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir', log_environ=False, command=['diff', '-u', '-w', 'base-expectations.txt', 'new-expectations.txt']).exit(0)
        )
        rc = self.run_step()
        self.expect_property('modified_tests', None)
        return rc

    @expectedFailure
    def test_ignore_certain_directories_xml(self):
        self.setup_step(FindModifiedLayoutTests())
        dir_names = ['reference', 'reftest', 'resources', 'support', 'script-tests', 'tools']
        FindModifiedLayoutTests._get_patch = lambda x: f'+++ LayoutTests/reference/test-name.xml'.encode('utf-8')
        self.expect_outcome(result=SKIPPED, state_string='Patch doesn\'t have relevant changes')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir', log_environ=False, command=['diff', '-u', '-w', 'base-expectations.txt', 'new-expectations.txt']).exit(0)
        )
        rc = self.run_step()
        self.expect_property('modified_tests', None)
        return rc

    @expectedFailure
    def test_ignore_certain_suffixes(self):
        self.setup_step(FindModifiedLayoutTests())
        FindModifiedLayoutTests._get_patch = lambda x: f'+++ LayoutTests/http/tests/events/device-motion-expected-mismatch.html'.encode('utf-8')
        self.expect_outcome(result=SKIPPED, state_string='Patch doesn\'t have relevant changes')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir', log_environ=False, command=['diff', '-u', '-w', 'base-expectations.txt', 'new-expectations.txt']).exit(0)
        )
        rc = self.run_step()
        self.expect_property('modified_tests', None)
        return rc

    @expectedFailure
    def test_ignore_non_layout_test_in_html_directory(self):
        self.setup_step(FindModifiedLayoutTests())
        FindModifiedLayoutTests._get_patch = lambda x: '+++ LayoutTests/html/test.txt'.encode('utf-8')
        self.expect_outcome(result=SKIPPED, state_string='Patch doesn\'t have relevant changes')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir', log_environ=False, command=['diff', '-u', '-w', 'base-expectations.txt', 'new-expectations.txt']).exit(0)
        )
        rc = self.run_step()
        self.expect_property('modified_tests', None)
        return rc

    @expectedFailure
    def test_non_relevant_patch(self):
        self.setup_step(FindModifiedLayoutTests())
        FindModifiedLayoutTests._get_patch = lambda x: b'Sample patch which does not modify any layout test'
        self.expect_outcome(result=SKIPPED, state_string='Patch doesn\'t have relevant changes')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir', log_environ=False, command=['diff', '-u', '-w', 'base-expectations.txt', 'new-expectations.txt']).exit(0)
        )
        rc = self.run_step()
        self.expect_property('modified_tests', None)
        return rc

    @expectedFailure
    def test_non_accessible_patch(self):
        self.setup_step(FindModifiedLayoutTests())
        FindModifiedLayoutTests._get_patch = lambda x: b''
        self.expect_outcome(result=WARNINGS, state_string='Patch could not be accessed')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir', log_environ=False, command=['diff', '-u', '-w', 'base-expectations.txt', 'new-expectations.txt']).exit(0)
        )
        rc = self.run_step()
        self.expect_property('modified_tests', None)
        return rc


class TestArchiveBuiltProduct(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(ArchiveBuiltProduct())
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/CISupport/built-product-archive', '--platform=ios-simulator',  '--release', 'archive'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Archived built product')
        return self.run_step()

    def test_failure(self):
        self.setup_step(ArchiveBuiltProduct())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/CISupport/built-product-archive', '--platform=mac-sierra',  '--debug', 'archive'],
                        )
            .log('stdio', stdout='Unexpected failure.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Archived built product (failure)')
        return self.run_step()


class TestArchiveStaticAnalysis(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(ArchiveStaticAnalyzerResults())
        self.setProperty('buildnumber', 123)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['Tools/Scripts/generate-static-analysis-archive', '--id-string', 'Build #123', '--output-root', 'scan-build-output', '--destination', '/tmp/static-analysis.zip'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='archived static analyzer results')
        return self.run_step()

    def test_failure(self):
        self.setup_step(ArchiveStaticAnalyzerResults())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.setProperty('buildnumber', 123)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['Tools/Scripts/generate-static-analysis-archive', '--id-string', 'Build #123', '--output-root', 'scan-build-output', '--destination', '/tmp/static-analysis.zip'],
                        )
            .log('stdio', stdout='Unexpected failure.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='archived static analyzer results (failure)')
        return self.run_step()


class TestUploadBuiltProduct(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    @expectedFailure
    def test_success(self):
        self.setup_step(UploadBuiltProduct())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '1234')
        self.expect_hidden(False)
        self.expectRemoteCommands(
            Expect('uploadFile', dict(workersrc='WebKitBuild/release.zip', workdir='wkdir',
                                      blocksize=1024 * 256, maxsize=None, keepstamp=False,
                                      writer=ExpectRemoteRef(remotetransfer.FileWriter)))
            + Expect.behavior(uploadFileWithContentsOfString('Dummy zip file content.'))
            .exit(0),
        )
        self.expectUploadedFile('public_html/archives/mac-sierra-x86_64-release/1234.zip')

        self.expect_outcome(result=SUCCESS, state_string='Uploaded built product')
        return self.run_step()

    @expectedFailure
    def test_failure(self):
        self.setup_step(UploadBuiltProduct())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '1234')
        self.expect_hidden(False)
        self.expectRemoteCommands(
            Expect('uploadFile', dict(workersrc='WebKitBuild/release.zip', workdir='wkdir',
                                      blocksize=1024 * 256, maxsize=None, keepstamp=False,
                                      writer=ExpectRemoteRef(remotetransfer.FileWriter)))
            + Expect.behavior(uploadFileWithContentsOfString('Dummy zip file content.'))
            .exit(1),
        )
        self.expectUploadedFile('public_html/archives/mac-sierra-x86_64-release/1234.zip')

        self.expect_outcome(result=FAILURE, state_string='Failed to upload built product')
        return self.run_step()


class TestDownloadBuiltProduct(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_success(self):
        self.setup_step(DownloadBuiltProduct())
        self.setProperty('fullPlatform', 'ios-simulator-12')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '1234')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/CISupport/download-built-product', '--release', 'https://s3-us-west-2.amazonaws.com/ews-archives.webkit.org/ios-simulator-12-x86_64-release/1234.zip'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Downloaded built product')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_failure(self):
        self.setup_step(DownloadBuiltProduct())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '123456')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/CISupport/download-built-product', '--debug', 'https://s3-us-west-2.amazonaws.com/ews-archives.webkit.org/mac-sierra-x86_64-debug/123456.zip'],
                        )
            .log('stdio', stdout='Unexpected failure.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to download built product from S3')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_deployment_skipped(self):
        self.setup_step(DownloadBuiltProduct())
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '123456')
        self.expect_outcome(result=SKIPPED)
        with current_hostname('test-ews-deployment.igalia.com'):
            return self.run_step()


class TestDownloadBuiltProductFromMaster(BuildStepMixinAdditions, unittest.TestCase):
    READ_LIMIT = 1000

    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    @staticmethod
    def downloadFileRecordingContents(limit, recorder):
        def behavior(command):
            reader = command.args['reader']
            data = reader.remote_read(limit)
            recorder(data)
            reader.remote_close()
        return behavior

    @defer.inlineCallbacks
    @expectedFailure
    def test_success(self):
        self.setup_step(DownloadBuiltProductFromMaster(mastersrc=__file__))
        self.setProperty('fullPlatform', 'ios-simulator-12')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '1234')
        self.expect_hidden(False)
        buf = []
        self.expectRemoteCommands(
            Expect('downloadFile', dict(
                workerdest='WebKitBuild/release.zip', workdir='wkdir',
                blocksize=1024 * 256, maxsize=None, mode=0o0644,
                reader=ExpectRemoteRef(remotetransfer.FileReader),
            ))
            + Expect.behavior(self.downloadFileRecordingContents(self.READ_LIMIT, buf.append))
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='downloading to release.zip')

        yield self.run_step()

        buf = b''.join(buf)
        self.assertEqual(len(buf), self.READ_LIMIT)
        with open(__file__, 'rb') as masterFile:
            data = masterFile.read(self.READ_LIMIT)
            if data != buf:
                self.assertEqual(buf, data)

    @defer.inlineCallbacks
    @expectedFailure
    def test_failure(self):
        self.setup_step(DownloadBuiltProductFromMaster(mastersrc=__file__))
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '123456')
        buf = []
        self.expectRemoteCommands(
            Expect('downloadFile', dict(
                workerdest='WebKitBuild/debug.zip', workdir='wkdir',
                blocksize=1024 * 256, maxsize=None, mode=0o0644,
                reader=ExpectRemoteRef(remotetransfer.FileReader),
            ))
            + Expect.behavior(self.downloadFileRecordingContents(self.READ_LIMIT, buf.append))
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to download built product from build master')
        yield self.run_step()


class TestExtractBuiltProduct(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(ExtractBuiltProduct())
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/CISupport/built-product-archive', '--platform=ios-simulator',  '--release', 'extract'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Extracted built product')
        return self.run_step()

    def test_failure(self):
        self.setup_step(ExtractBuiltProduct())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/CISupport/built-product-archive', '--platform=mac-sierra',  '--debug', 'extract'],
                        )
            .log('stdio', stdout='Unexpected failure.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Extracted built product (failure)')
        return self.run_step()


class current_hostname(object):
    def __init__(self, hostname):
        self.hostname = hostname
        self.saved_hostname = None

    def __enter__(self):
        from . import steps
        self.saved_hostname = steps.CURRENT_HOSTNAME
        steps.CURRENT_HOSTNAME = self.hostname

    def __exit__(self, type, value, tb):
        from . import steps
        steps.CURRENT_HOSTNAME = self.saved_hostname


class TestGenerateS3URL(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self, identifier='mac-highsierra-x86_64-release', extension='zip', additions=None, content_type=None):
        self.setup_step(GenerateS3URL(identifier, extension=extension, additions=additions, content_type=content_type))
        self.setProperty('change_id', '1234')

    def disabled_test_success(self):
        # TODO: Figure out how to pass logs to unit-test for MasterShellCommand steps
        self.configureStep()
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['python3',
                                              '../Shared/generate-s3-url',
                                              '--change-id', '1234',
                                              '--identifier', 'mac-highsierra-x86_64-release',
                                              '--extension', 'zip',
                                              ])
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Generated S3 URL')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    @expectedFailure
    def test_failure(self):
        self.configureStep('ios-simulator-16-x86_64-debug', additions='123')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['python3',
                                              '../Shared/generate-s3-url',
                                              '--change-id', '1234',
                                              '--identifier', 'ios-simulator-16-x86_64-debug',
                                              '--extension', 'zip',
                                              '--additions', '123'
                                              ])
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to generate S3 URL')

        try:
            with current_hostname(EWS_BUILD_HOSTNAMES[0]), open(os.devnull, 'w') as null:
                sys.stdout = null
                return self.run_step()
        finally:
            sys.stdout = sys.__stdout__

    @expectedFailure
    def test_failure_with_extension(self):
        self.configureStep('macos-arm64-release-compile-webkit', extension='txt', content_type='text/plain')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['python3',
                                              '../Shared/generate-s3-url',
                                              '--change-id', '1234',
                                              '--identifier', 'macos-arm64-release-compile-webkit',
                                              '--extension', 'txt',
                                              '--content-type', 'text/plain',
                                              ])
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to generate S3 URL')

        try:
            with current_hostname(EWS_BUILD_HOSTNAMES[0]), open(os.devnull, 'w') as null:
                sys.stdout = null
                return self.run_step()
        finally:
            sys.stdout = sys.__stdout__

    def test_skipped(self):
        self.configureStep()
        self.expect_outcome(result=SKIPPED, state_string='Generated S3 URL (skipped)')
        with current_hostname('something-other-than-steps.EWS_BUILD_HOSTNAME'):
            return self.run_step()


class TestTransferToS3(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    @expectedFailure
    def test_success(self):
        self.setup_step(TransferToS3())
        self.setProperty('fullPlatform', 'mac-highsierra')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '1234')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['python3',
                                              '../Shared/transfer-archive-to-s3',
                                              '--change-id', '1234',
                                              '--identifier', 'mac-highsierra-x86_64-release',
                                              '--archive', 'public_html/archives/mac-highsierra-x86_64-release/1234.zip',
                                              ])
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Transferred archive to S3')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    @expectedFailure
    def test_failure(self):
        self.setup_step(TransferToS3())
        self.setProperty('fullPlatform', 'ios-simulator-12')
        self.setProperty('configuration', 'debug')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '1234')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['python3',
                                              '../Shared/transfer-archive-to-s3',
                                              '--change-id', '1234',
                                              '--identifier', 'ios-simulator-12-x86_64-debug',
                                              '--archive', 'public_html/archives/ios-simulator-12-x86_64-debug/1234.zip',
                                              ])
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to transfer archive to S3')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_skipped(self):
        self.setup_step(TransferToS3())
        self.setProperty('fullPlatform', 'mac-highsierra')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '1234')
        self.expect_outcome(result=SKIPPED, state_string='Transferred archive to S3 (skipped)')
        with current_hostname('something-other-than-steps.EWS_BUILD_HOSTNAME'):
            return self.run_step()


class TestUploadFileToS3(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self, file='WebKitBuild/release.zip', content_type=None):
        self.setup_step(UploadFileToS3(file, content_type=content_type))
        self.build.s3url = 'https://test-s3-url'

    def test_success(self):
        self.configureStep()
        self.assertEqual(UploadFileToS3.haltOnFailure, True)
        self.assertEqual(UploadFileToS3.flunkOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        env=dict(UPLOAD_URL='https://test-s3-url'),
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/upload-file-to-url', '--filename', 'WebKitBuild/release.zip'],
                        timeout=1860,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Uploaded WebKitBuild/release.zip to S3')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    def test_success_content_type(self):
        self.configureStep(file='build-log.txt', content_type='text/plain')
        self.assertEqual(UploadFileToS3.haltOnFailure, True)
        self.assertEqual(UploadFileToS3.flunkOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        env=dict(UPLOAD_URL='https://test-s3-url'),
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/upload-file-to-url', '--filename', 'build-log.txt', '--content-type', 'text/plain'],
                        timeout=1860,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Uploaded build-log.txt to S3')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    def test_failure(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        env=dict(UPLOAD_URL='https://test-s3-url'),
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/upload-file-to-url', '--filename', 'WebKitBuild/release.zip'],
                        timeout=1860,
                        )
            .log('stdio', stdout='''Uploading WebKitBuild/release.zip
response: <Response [403]>, 403, Forbidden
exit 1''')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to upload WebKitBuild/release.zip to S3. Please inform an admin.')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    def test_skipped(self):
        self.configureStep()
        self.expect_outcome(result=SKIPPED, state_string='Skipped upload to S3')
        with current_hostname('something-other-than-steps.EWS_BUILD_HOSTNAME'):
            return self.run_step()


class TestRunAPITests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'api_test_results.json'
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success_mac(self):
        self.setup_step(RunAPITests())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'release')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-api-tests --timestamps --no-build --release --verbose --json-output={self.jsonFileName} > logs.txt 2>&1 ; ret=$? ; grep "Ran " logs.txt ; exit $ret'],
                        logfiles={'json': self.jsonFileName},
                        timeout=3 * 60 * 60
                        )
            .log('stdio', stdout='''...
worker/0 TestWTF.WTF_Variant.OperatorAmpersand Passed
worker/0 TestWTF.WTF_Variant.Ref Passed
worker/0 TestWTF.WTF_Variant.RefPtr Passed
worker/0 TestWTF.WTF_Variant.RetainPtr Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingMakeVisitor Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingSwitchOn Passed
Ran 1888 tests of 1888 with 1888 successful
------------------------------
All tests successfully passed!
''')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='run-api-tests')
        return self.run_step()

    def test_success_ios_simulator(self):
        self.setup_step(RunAPITests())
        self.setProperty('fullPlatform', 'ios-simulator-11')
        self.setProperty('platform', 'ios')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-api-tests --timestamps --no-build --debug --verbose --json-output={self.jsonFileName} --ios-simulator > logs.txt 2>&1 ; ret=$? ; grep "Ran " logs.txt ; exit $ret'],
                        logfiles={'json': self.jsonFileName},
                        timeout=3 * 60 * 60
                        )
            .log('stdio', stdout='''...
worker/0 TestWTF.WTF_Variant.OperatorAmpersand Passed
worker/0 TestWTF.WTF_Variant.Ref Passed
worker/0 TestWTF.WTF_Variant.RefPtr Passed
worker/0 TestWTF.WTF_Variant.RetainPtr Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingMakeVisitor Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingSwitchOn Passed
Ran 1888 tests of 1888 with 1888 successful
------------------------------
All tests successfully passed!
''')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='run-api-tests')
        return self.run_step()

    def test_success_gtk(self):
        self.setup_step(RunAPITests())
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('platform', 'gtk')
        self.setProperty('configuration', 'release')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-gtk-tests --release --json-output={self.jsonFileName} > logs.txt 2>&1 ; ret=$? ; grep "Ran " logs.txt ; exit $ret'],
                        logfiles={'json': self.jsonFileName},
                        timeout=3 * 60 * 60
                        )
            .log('stdio', stdout='''...
**PASS** TransformationMatrix.Blend
**PASS** TransformationMatrix.Blend2
**PASS** TransformationMatrix.Blend4
**PASS** TransformationMatrix.Equality
**PASS** TransformationMatrix.Casting
**PASS** TransformationMatrix.MakeMapBetweenRects
**PASS** URLParserTextEncodingTest.QueryEncoding
**PASS** GStreamerTest.mappedBufferBasics
**PASS** GStreamerTest.mappedBufferReadSanity
**PASS** GStreamerTest.mappedBufferWriteSanity
**PASS** GStreamerTest.mappedBufferCachesSharedBuffers
**PASS** GStreamerTest.mappedBufferDoesNotAddExtraRefs

Ran 1316 tests of 1318 with 1316 successful
''')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='run-api-tests')
        return self.run_step()

    def test_success_wpe(self):
        self.setup_step(RunAPITests())
        self.setProperty('fullPlatform', 'wpe')
        self.setProperty('platform', 'wpe')
        self.setProperty('configuration', 'release')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-wpe-tests --release --json-output={self.jsonFileName} > logs.txt 2>&1 ; ret=$? ; grep "Ran " logs.txt ; exit $ret'],
                        logfiles={'json': self.jsonFileName},
                        timeout=3 * 60 * 60
                        )
            .log('stdio', stdout='''...
**PASS** TransformationMatrix.Blend
**PASS** TransformationMatrix.Blend2
**PASS** TransformationMatrix.Blend4
**PASS** TransformationMatrix.Equality
**PASS** TransformationMatrix.Casting
**PASS** TransformationMatrix.MakeMapBetweenRects
**PASS** URLParserTextEncodingTest.QueryEncoding
**PASS** GStreamerTest.mappedBufferBasics
**PASS** GStreamerTest.mappedBufferReadSanity
**PASS** GStreamerTest.mappedBufferWriteSanity
**PASS** GStreamerTest.mappedBufferCachesSharedBuffers
**PASS** GStreamerTest.mappedBufferDoesNotAddExtraRefs

Ran 1316 tests of 1318 with 1316 successful
''')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='run-api-tests')
        return self.run_step()

    def test_one_failure(self):
        self.setup_step(RunAPITests())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-api-tests --timestamps --no-build --debug --verbose --json-output={self.jsonFileName} > logs.txt 2>&1 ; ret=$? ; grep "Ran " logs.txt ; exit $ret'],
                        logfiles={'json': self.jsonFileName},
                        timeout=3 * 60 * 60
                        )
            .log('stdio', stdout='''
worker/0 TestWTF.WTF_Variant.OperatorAmpersand Passed
worker/0 TestWTF.WTF_Variant.Ref Passed
worker/0 TestWTF.WTF_Variant.RefPtr Passed
worker/0 TestWTF.WTF_Variant.RetainPtr Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingMakeVisitor Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingSwitchOn Passed
worker/0 exiting
Ran 1888 tests of 1888 with 1887 successful
------------------------------
Test suite failed

Crashed

    TestWTF.WTF.StringConcatenate_Unsigned
        **FAIL** WTF.StringConcatenate_Unsigned

        Tools\\TestWebKitAPI\\Tests\\WTF\\StringConcatenate.cpp:84
        Value of: makeString('hello ', static_cast<unsigned short>(42) , ' world')
          Actual: hello 42 world
        Expected: 'hello * world'
        Which is: 74B00C9C

Testing completed, Exit status: 3
''')
            .exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='1 api test failed or timed out')
        return self.run_step()

    def test_multiple_failures_and_timeouts(self):
        self.setup_step(RunAPITests())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-api-tests --timestamps --no-build --debug --verbose --json-output={self.jsonFileName} > logs.txt 2>&1 ; ret=$? ; grep "Ran " logs.txt ; exit $ret'],
                        logfiles={'json': self.jsonFileName},
                        timeout=3 * 60 * 60
                        )
            .log('stdio', stdout='''...
worker/0 TestWTF.WTF_Variant.OperatorAmpersand Passed
worker/0 TestWTF.WTF_Variant.Ref Passed
worker/0 TestWTF.WTF_Variant.RefPtr Passed
worker/0 TestWTF.WTF_Variant.RetainPtr Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingMakeVisitor Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingSwitchOn Passed
worker/0 exiting
Ran 1888 tests of 1888 with 1884 successful
------------------------------
Test suite failed

Failed

    TestWTF.WTF.StringConcatenate_Unsigned
        **FAIL** WTF.StringConcatenate_Unsigned

        Tools\\TestWebKitAPI\\Tests\\WTF\\StringConcatenate.cpp:84
        Value of: makeString('hello ', static_cast<unsigned short>(42) , ' world')
          Actual: hello 42 world
        Expected: 'hello * world'
        Which is: 74B00C9C

    TestWTF.WTF_Expected.Unexpected
        **FAIL** WTF_Expected.Unexpected

        Tools\\TestWebKitAPI\\Tests\\WTF\\Expected.cpp:96
        Value of: s1
          Actual: oops
        Expected: s0
        Which is: oops

Timeout

    TestWTF.WTF_PoisonedUniquePtrForTriviallyDestructibleArrays.Assignment
    TestWTF.WTF_Lock.ContendedShortSection

Testing completed, Exit status: 3
''')
            .exit(4),
        )
        self.expect_outcome(result=FAILURE, state_string='4 api tests failed or timed out')
        return self.run_step()

    def test_unexpected_failure(self):
        self.setup_step(RunAPITests())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-api-tests --timestamps --no-build --debug --verbose --json-output={self.jsonFileName} > logs.txt 2>&1 ; ret=$? ; grep "Ran " logs.txt ; exit $ret'],
                        logfiles={'json': self.jsonFileName},
                        timeout=3 * 60 * 60
                        )
            .log('stdio', stdout='Unexpected failure. Failed to run api tests.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='run-api-tests (failure)')
        return self.run_step()

    def test_no_failures_or_timeouts_with_disabled(self):
        self.setup_step(RunAPITests())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-api-tests --timestamps --no-build --debug --verbose --json-output={self.jsonFileName} > logs.txt 2>&1 ; ret=$? ; grep "Ran " logs.txt ; exit $ret'],
                        logfiles={'json': self.jsonFileName},
                        timeout=3 * 60 * 60
                        )
            .log('stdio', stdout='''...
worker/0 TestWTF.WTF_Variant.OperatorAmpersand Passed
worker/0 TestWTF.WTF_Variant.Ref Passed
worker/0 TestWTF.WTF_Variant.RefPtr Passed
worker/0 TestWTF.WTF_Variant.RetainPtr Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingMakeVisitor Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingSwitchOn Passed
worker/0 exiting
Ran 1881 tests of 1888 with 1881 successful
------------------------------
All tests successfully passed!
''')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='run-api-tests')
        return self.run_step()


class TestRunAPITestsWithoutChange(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'api_test_results.json'
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success_mac(self):
        self.setup_step(RunAPITestsWithoutChange())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'release')
        self.setProperty('buildername', 'API-Tests-macOS-EWS')
        self.setProperty('buildnumber', '11525')
        self.setProperty('workername', 'ews155')
        self.setProperty('first_run_failures', ['suite.test1', 'suite.test2', 'suite.test3'])
        self.setProperty('second_run_failures', ['suite.test3', 'suite.test4', 'suite.test5'])

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-api-tests --timestamps --no-build --release --verbose --json-output={self.jsonFileName} suite.test1 suite.test2 suite.test3 suite.test4 suite.test5 > logs.txt 2>&1 ; ret=$? ; grep "Ran " logs.txt ; exit $ret'],
                        logfiles={'json': self.jsonFileName},
                        timeout=3 * 60 * 60
                        )
            .log('stdio', stdout='''...
worker/0 TestWTF.WTF_Variant.OperatorAmpersand Passed
worker/0 TestWTF.WTF_Variant.Ref Passed
worker/0 TestWTF.WTF_Variant.RefPtr Passed
worker/0 TestWTF.WTF_Variant.RetainPtr Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingMakeVisitor Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingSwitchOn Passed
Ran 1888 tests of 1888 with 1888 successful
------------------------------
All tests successfully passed!
''')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='run-api-tests-without-change')
        return self.run_step()

    def test_one_failure(self):
        self.setup_step(RunAPITestsWithoutChange())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'ios-simulator')
        self.setProperty('configuration', 'debug')
        self.setProperty('buildername', 'API-Tests-iOS-EWS')
        self.setProperty('buildnumber', '123')
        self.setProperty('workername', 'ews156')
        self.setProperty('first_run_failures', ['suite.test-one-failure1'])
        self.setProperty('second_run_failures', ['suite.test-one-failure2'])

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-api-tests --timestamps --no-build --debug --verbose --json-output={self.jsonFileName} suite.test-one-failure1 suite.test-one-failure2 > logs.txt 2>&1 ; ret=$? ; grep "Ran " logs.txt ; exit $ret'],
                        logfiles={'json': self.jsonFileName},
                        timeout=3 * 60 * 60
                        )
            .log('stdio', stdout='''
worker/0 TestWTF.WTF_Variant.OperatorAmpersand Passed
worker/0 TestWTF.WTF_Variant.Ref Passed
worker/0 TestWTF.WTF_Variant.RefPtr Passed
worker/0 TestWTF.WTF_Variant.RetainPtr Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingMakeVisitor Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingSwitchOn Passed
worker/0 exiting
Ran 1888 tests of 1888 with 1887 successful
------------------------------
Test suite failed

Crashed

    TestWTF.WTF.StringConcatenate_Unsigned
        **FAIL** WTF.StringConcatenate_Unsigned

        Tools\\TestWebKitAPI\\Tests\\WTF\\StringConcatenate.cpp:84
        Value of: makeString('hello ', static_cast<unsigned short>(42) , ' world')
          Actual: hello 42 world
        Expected: 'hello * world'
        Which is: 74B00C9C

Testing completed, Exit status: 3
''')
            .exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='1 api test failed or timed out')
        return self.run_step()

    def test_multiple_failures_gtk(self):
        self.setup_step(RunAPITestsWithoutChange())
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('platform', 'gtk')
        self.setProperty('configuration', 'debug')
        self.setProperty('buildername', 'API-Tests-GTK-EWS')
        self.setProperty('buildnumber', '13529')
        self.setProperty('workername', 'igalia4-gtk-wk2-ews')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-gtk-tests --debug --json-output={self.jsonFileName} > logs.txt 2>&1 ; ret=$? ; grep "Ran " logs.txt ; exit $ret'],
                        logfiles={'json': self.jsonFileName},
                        timeout=3 * 60 * 60
                        )
            .log('stdio', stdout='''
**PASS** GStreamerTest.mappedBufferBasics
**PASS** GStreamerTest.mappedBufferReadSanity
**PASS** GStreamerTest.mappedBufferWriteSanity
**PASS** GStreamerTest.mappedBufferCachesSharedBuffers
**PASS** GStreamerTest.mappedBufferDoesNotAddExtraRefs

Unexpected failures (3)
    /TestWTF
        WTF_DateMath.calculateLocalTimeOffset
    /WebKit2Gtk/TestPrinting
        /webkit/WebKitPrintOperation/close-after-print
    /WebKit2Gtk/TestWebsiteData
        /webkit/WebKitWebsiteData/databases

Unexpected passes (1)
    /WebKit2Gtk/TestUIClient
        /webkit/WebKitWebView/usermedia-enumeratedevices-permission-check

Ran 1296 tests of 1298 with 1293 successful
''')
            .exit(3),
        )
        self.expect_outcome(result=FAILURE, state_string='3 api tests failed or timed out')
        return self.run_step()

    def test_multiple_failures_wpe(self):
        self.setup_step(RunAPITestsWithoutChange())
        self.setProperty('fullPlatform', 'wpe')
        self.setProperty('platform', 'wpe')
        self.setProperty('configuration', 'debug')
        self.setProperty('buildername', 'API-Tests-WPE-EWS')
        self.setProperty('buildnumber', '11529')
        self.setProperty('workername', 'igalia14-wpe-ews')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'python3 Tools/Scripts/run-wpe-tests --debug --json-output={self.jsonFileName} > logs.txt 2>&1 ; ret=$? ; grep "Ran " logs.txt ; exit $ret'],
                        logfiles={'json': self.jsonFileName},
                        timeout=3 * 60 * 60
                        )
            .log('stdio', stdout='''
**PASS** GStreamerTest.mappedBufferBasics
**PASS** GStreamerTest.mappedBufferReadSanity
**PASS** GStreamerTest.mappedBufferWriteSanity
**PASS** GStreamerTest.mappedBufferCachesSharedBuffers
**PASS** GStreamerTest.mappedBufferDoesNotAddExtraRefs

Unexpected failures (3)
    /TestWTF
        WTF_DateMath.calculateLocalTimeOffset
    /WebKit2WPE/TestPrinting
        /webkit/WebKitPrintOperation/close-after-print
    /WebKit2WPE/TestWebsiteData
        /webkit/WebKitWebsiteData/databases

Unexpected passes (1)
    /WebKit2WPE/TestUIClient
        /webkit/WebKitWebView/usermedia-enumeratedevices-permission-check

Ran 1296 tests of 1298 with 1293 successful
''')
            .exit(3),
        )
        self.expect_outcome(result=FAILURE, state_string='3 api tests failed or timed out')
        return self.run_step()


class TestArchiveTestResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(ArchiveTestResults())
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('platform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/CISupport/test-result-archive', '--platform=ios-simulator',  '--release', 'archive'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Archived test results')
        return self.run_step()

    def test_failure(self):
        self.setup_step(ArchiveTestResults())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/CISupport/test-result-archive', '--platform=mac',  '--debug', 'archive'],
                        )
            .log('stdio', stdout='Unexpected failure.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Archived test results (failure)')
        return self.run_step()


class TestUploadTestResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    @expectedFailure
    def test_success(self):
        self.setup_step(UploadTestResults())
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '1234')
        self.setProperty('buildername', 'macOS-Sierra-Release-WK2-Tests-EWS')
        self.setProperty('buildnumber', '12')
        self.expect_hidden(False)
        self.expectRemoteCommands(
            Expect('uploadFile', dict(workersrc='layout-test-results.zip', workdir='wkdir',
                                      blocksize=1024 * 256, maxsize=None, keepstamp=False,
                                      writer=ExpectRemoteRef(remotetransfer.FileWriter)))
            + Expect.behavior(uploadFileWithContentsOfString('Dummy zip file content.'))
            .exit(0),
        )
        self.expectUploadedFile('public_html/results/macOS-Sierra-Release-WK2-Tests-EWS/1234-12.zip')

        self.expect_outcome(result=SUCCESS, state_string='Uploaded test results')
        return self.run_step()

    @expectedFailure
    def test_success_hash(self):
        self.setup_step(UploadTestResults())
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '8f75a5fa')
        self.setProperty('buildername', 'macOS-Sierra-Release-WK2-Tests-EWS')
        self.setProperty('buildnumber', '12')
        self.expect_hidden(False)
        self.expectRemoteCommands(
            Expect('uploadFile', dict(workersrc='layout-test-results.zip', workdir='wkdir',
                                      blocksize=1024 * 256, maxsize=None, keepstamp=False,
                                      writer=ExpectRemoteRef(remotetransfer.FileWriter)))
            + Expect.behavior(uploadFileWithContentsOfString('Dummy zip file content.'))
            .exit(0),
        )
        self.expectUploadedFile('public_html/results/macOS-Sierra-Release-WK2-Tests-EWS/8f75a5fa-12.zip')

        self.expect_outcome(result=SUCCESS, state_string='Uploaded test results')
        return self.run_step()

    @expectedFailure
    def test_success_with_identifier(self):
        self.setup_step(UploadTestResults(identifier='clean-tree'))
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '37be32c5')
        self.setProperty('buildername', 'iOS-12-Simulator-WK2-Tests-EWS')
        self.setProperty('buildnumber', '120')
        self.expect_hidden(False)
        self.expectRemoteCommands(
            Expect('uploadFile', dict(workersrc='layout-test-results.zip', workdir='wkdir',
                                      blocksize=1024 * 256, maxsize=None, keepstamp=False,
                                      writer=ExpectRemoteRef(remotetransfer.FileWriter)))
            + Expect.behavior(uploadFileWithContentsOfString('Dummy zip file content.'))
            .exit(0),
        )
        self.expectUploadedFile('public_html/results/iOS-12-Simulator-WK2-Tests-EWS/37be32c5-120-clean-tree.zip')

        self.expect_outcome(result=SUCCESS, state_string='Uploaded test results')
        return self.run_step()


class TestExtractTestResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    @expectedFailure
    def test_success(self):
        self.setup_step(ExtractTestResults())
        self.setProperty('configuration', 'release')
        self.setProperty('change_id', '1234')
        self.setProperty('buildername', 'macOS-Sierra-Release-WK2-Tests-EWS')
        self.setProperty('buildnumber', '12')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['unzip',
                                              '-q',
                                              '-o',
                                              'public_html/results/macOS-Sierra-Release-WK2-Tests-EWS/1234-12.zip',
                                              '-d',
                                              'public_html/results/macOS-Sierra-Release-WK2-Tests-EWS/1234-12',
                                              ])
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Extracted test results')
        self.expectAddedURLs([call('view layout test results', 'https://ews-build.s3-us-west-2.amazonaws.com/macOS-Sierra-Release-WK2-Tests-EWS/r2468-12/results.html')])
        return self.run_step()

    @expectedFailure
    def test_success_with_identifier(self):
        self.setup_step(ExtractTestResults(identifier='rerun'))
        self.setProperty('configuration', 'release')
        self.setProperty('change_id', '1234')
        self.setProperty('buildername', 'iOS-12-Simulator-WK2-Tests-EWS')
        self.setProperty('buildnumber', '12')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['unzip',
                                              '-q',
                                              '-o',
                                              'public_html/results/iOS-12-Simulator-WK2-Tests-EWS/1234-12-rerun.zip',
                                              '-d',
                                              'public_html/results/iOS-12-Simulator-WK2-Tests-EWS/1234-12-rerun',
                                              ])
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Extracted test results')
        self.expectAddedURLs([call('view layout test results', 'https://ews-build.s3-us-west-2.amazonaws.com/iOS-12-Simulator-WK2-Tests-EWS/1234-12/results.html')])
        return self.run_step()

    @expectedFailure
    def test_failure(self):
        self.setup_step(ExtractTestResults())
        self.setProperty('configuration', 'debug')
        self.setProperty('change_id', '1234')
        self.setProperty('buildername', 'macOS-Sierra-Release-WK2-Tests-EWS')
        self.setProperty('buildnumber', '12')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['unzip',
                                              '-q',
                                              '-o',
                                              'public_html/results/macOS-Sierra-Release-WK2-Tests-EWS/1234-12.zip',
                                              '-d',
                                              'public_html/results/macOS-Sierra-Release-WK2-Tests-EWS/1234-12',
                                              ])
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='failed (2) (failure)')
        self.expectAddedURLs([call('view layout test results', 'https://ews-build.s3-us-west-2.amazonaws.com/macOS-Sierra-Release-WK2-Tests-EWS/1234-12/results.html')])
        return self.run_step()


class TestPrintConfiguration(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success_mac(self):
        self.setup_step(PrintConfiguration())
        self.setProperty('buildername', 'macOS-Sequoia-Release-WK2-Tests-EWS')
        self.setProperty('platform', 'mac-sequoia')

        self.expectRemoteCommands(
            ExpectShell(command=['hostname'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='ews150.apple.com'),
            ExpectShell(command=['df', '-hl'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='''Filesystem     Size   Used  Avail Capacity iused  ifree %iused  Mounted on
/dev/disk1s1  119Gi   95Gi   23Gi    81%  937959 9223372036853837848    0%   /
/dev/disk1s4  119Gi   20Ki   23Gi     1%       0 9223372036854775807    0%   /private/var/vm
/dev/disk0s3  119Gi   22Gi   97Gi    19%  337595          4294629684    0%   /Volumes/Data'''),
            ExpectShell(command=['date'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='Tue Apr  9 15:30:52 PDT 2019'),
            ExpectShell(command=['sw_vers'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='''ProductName:	macOS
ProductVersion:	15.0
BuildVersion:	24A335'''),
            ExpectShell(command=['system_profiler', 'SPSoftwareDataType', 'SPHardwareDataType'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='Configuration version: Software: System Software Overview: System Version: macOS 11.4 (20F71) Kernel Version: Darwin 20.5.0 Boot Volume: Macintosh HD Boot Mode: Normal Computer Name: bot1020 User Name: WebKit Build Worker (buildbot) Secure Virtual Memory: Enabled System Integrity Protection: Enabled Time since boot: 27 seconds Hardware: Hardware Overview: Model Name: Mac mini Model Identifier: Macmini8,1 Processor Name: 6-Core Intel Core i7 Processor Speed: 3.2 GHz Number of Processors: 1 Total Number of Cores: 6 L2 Cache (per Core): 256 KB L3 Cache: 12 MB Hyper-Threading Technology: Enabled Memory: 32 GB System Firmware Version: 1554.120.19.0.0 (iBridge: 18.16.14663.0.0,0) Serial Number (system): C07DXXXXXXXX Hardware UUID: F724DE6E-706A-5A54-8D16-000000000000 Provisioning UDID: E724DE6E-006A-5A54-8D16-000000000000 Activation Lock Status: Disabled Xcode 12.5 Build version 12E262'),
            ExpectShell(command=['cat', '/usr/share/zoneinfo/+VERSION'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['xcodebuild', '-sdk', '-version'], workdir='wkdir', timeout=60, log_environ=False)
            .log('stdio', stdout='''MacOSX15.sdk - macOS 15.0 (macosx15.0)
SDKVersion: 15.0
Path: /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX15.sdk
PlatformVersion: 15.0
PlatformPath: /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform
BuildID: E7931D9A-726E-11EF-B57C-DCEFEEF80074
ProductBuildVersion: 24A336
ProductCopyright: 1983-2024 Apple Inc.
ProductName: macOS
ProductUserVisibleVersion: 15.0
ProductVersion: 15.0
iOSSupportVersion: 18.0

Xcode 16.0
Build version 16A242d''')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='OS: Sequoia (15.0), Xcode: 16.0')
        return self.run_step()

    def test_success_ios_simulator(self):
        self.setup_step(PrintConfiguration())
        self.setProperty('buildername', 'Apple-iOS-17-Simulator-Release-WK2-Tests')
        self.setProperty('platform', 'ios-simulator-17')

        self.expectRemoteCommands(
            ExpectShell(command=['hostname'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='ews152.apple.com'),
            ExpectShell(command=['df', '-hl'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='''Filesystem     Size   Used  Avail Capacity iused  ifree %iused  Mounted on
/dev/disk1s1  119Gi   95Gi   23Gi    81%  937959 9223372036853837848    0%   /
/dev/disk1s4  119Gi   20Ki   23Gi     1%       0 9223372036854775807    0%   /private/var/vm
/dev/disk0s3  119Gi   22Gi   97Gi    19%  337595          4294629684    0%   /Volumes/Data'''),
            ExpectShell(command=['date'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='Tue Apr  9 15:30:52 PDT 2019'),
            ExpectShell(command=['sw_vers'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='''ProductName:	macOS
ProductVersion:	14.5
BuildVersion:	23F79'''),
            ExpectShell(command=['system_profiler', 'SPSoftwareDataType', 'SPHardwareDataType'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='Sample system information'),
            ExpectShell(command=['cat', '/usr/share/zoneinfo/+VERSION'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['xcodebuild', '-sdk', '-version'], workdir='wkdir', timeout=60, log_environ=False)
            .log('stdio', stdout='''iPhoneSimulator17.5.sdk - Simulator - iOS 17.5 (iphonesimulator17.5)
SDKVersion: 17.5
Path: /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator17.5.sdk
PlatformVersion: 17.5
PlatformPath: /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform
BuildID: 8EFDDFDC-08C7-11EF-A0A9-DD3864AEFA1C
ProductBuildVersion: 21F77
ProductCopyright: 1983-2024 Apple Inc.
ProductName: iPhone OS
ProductVersion: 17.5

Xcode 15.4
Build version 15F31d''')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='OS: Sonoma (14.5), Xcode: 15.4')
        return self.run_step()

    def test_success_webkitpy(self):
        self.setup_step(PrintConfiguration())
        self.setProperty('platform', '*')

        self.expectRemoteCommands(
            ExpectShell(command=['hostname'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['df', '-hl'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['date'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['sw_vers'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='''ProductName:	macOS
ProductVersion:	14.5
BuildVersion:	23F79'''),
            ExpectShell(command=['system_profiler', 'SPSoftwareDataType', 'SPHardwareDataType'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='Sample system information'),
            ExpectShell(command=['cat', '/usr/share/zoneinfo/+VERSION'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['xcodebuild', '-sdk', '-version'], workdir='wkdir', timeout=60,
                        log_environ=False).exit(0)
            .log('stdio', stdout='''Xcode 15.4\nBuild version 15F31d'''),
        )
        self.expect_outcome(result=SUCCESS, state_string='OS: Sonoma (14.5), Xcode: 15.4')
        return self.run_step()

    def test_success_linux_wpe(self):
        self.setup_step(PrintConfiguration())
        self.setProperty('platform', 'wpe')

        self.expectRemoteCommands(
            ExpectShell(command=['hostname'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='ews190'),
            ExpectShell(command=['df', '-hl', '--exclude-type=fuse.portal'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='''Filesystem     Size   Used  Avail Capacity iused  ifree %iused  Mounted on
/dev/disk0s3  119Gi   22Gi   97Gi    19%  337595          4294629684    0%   /'''),
            ExpectShell(command=['date'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='Tue Apr  9 15:30:52 PDT 2019'),
            ExpectShell(command=['uname', '-a'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='''Linux kodama-ews 5.0.4-arch1-1-ARCH #1 SMP PREEMPT Sat Mar 23 21:00:33 UTC 2019 x86_64 GNU/Linux'''),
            ExpectShell(command=['uptime'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout=' 6:31  up 22 seconds, 12:05, 2 users, load averages: 3.17 7.23 5.45'),
        )
        self.expect_outcome(result=SUCCESS, state_string='Printed configuration')
        return self.run_step()

    def test_success_linux_gtk(self):
        self.setup_step(PrintConfiguration())
        self.setProperty('platform', 'gtk')

        self.expectRemoteCommands(
            ExpectShell(command=['hostname'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['df', '-hl', '--exclude-type=fuse.portal'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['date'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['uname', '-a'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['uptime'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Printed configuration')
        return self.run_step()

    def test_failure(self):
        self.setup_step(PrintConfiguration())
        self.setProperty('platform', 'ios-12')
        self.expectRemoteCommands(
            ExpectShell(command=['hostname'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['df', '-hl'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['date'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['sw_vers'], workdir='wkdir', timeout=60, log_environ=False).exit(1)
            .log('stdio', stdout='''Upon execvpe sw_vers ['sw_vers'] in environment id 7696545650400
:Traceback (most recent call last):
  File "/usr/lib/python2.7/site-packages/twisted/internet/process.py", line 445, in _fork
    environment)
  File "/usr/lib/python2.7/site-packages/twisted/internet/process.py", line 523, in _execChild
    os.execvpe(executable, args, environment)
  File "/usr/lib/python2.7/os.py", line 355, in execvpe
    _execvpe(file, args, env)
  File "/usr/lib/python2.7/os.py", line 382, in _execvpe
    func(fullname, *argrest)
OSError: [Errno 2] No such file or directory'''),
            ExpectShell(command=['system_profiler', 'SPSoftwareDataType', 'SPHardwareDataType'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['cat', '/usr/share/zoneinfo/+VERSION'], workdir='wkdir', timeout=60, log_environ=False).exit(0),
            ExpectShell(command=['xcodebuild', '-sdk', '-version'], workdir='wkdir', timeout=60, log_environ=False)
            .log('stdio', stdout='''Upon execvpe xcodebuild ['xcodebuild', '-sdk', '-version'] in environment id 7696545612416
:Traceback (most recent call last):
  File "/usr/lib/python2.7/site-packages/twisted/internet/process.py", line 445, in _fork
    environment)
  File "/usr/lib/python2.7/site-packages/twisted/internet/process.py", line 523, in _execChild
    os.execvpe(executable, args, environment)
  File "/usr/lib/python2.7/os.py", line 355, in execvpe
    _execvpe(file, args, env)
  File "/usr/lib/python2.7/os.py", line 382, in _execvpe
    func(fullname, *argrest)
OSError: [Errno 2] No such file or directory''')
            .exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to print configuration')
        return self.run_step()


class TestCleanGitRepo(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CleanGitRepo())
        self.setProperty('buildername', 'Style-EWS')

        self.expectRemoteCommands(
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'rm -f .git/gc.log || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git rebase --abort || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git am --abort || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git cherry-pick --abort || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'clean', '-f', '-d'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'checkout', '--progress', 'origin/main', '-f'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout='You are in detached HEAD state.'),
            ExpectShell(command=['git', 'branch', '-D', 'main'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout='Deleted branch main (was 57015967fef9).'),
            ExpectShell(command=['git', 'branch', 'main'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout="Switched to a new branch 'main'"),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git branch | grep -v ' main$' | grep -v 'HEAD detached at' | xargs git branch -D || true"], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git remote | grep -v 'origin$' | xargs -L 1 git remote rm || true"], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'prune'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
        )
        self.expect_outcome(result=SUCCESS, state_string='Cleaned up git repository')
        return self.run_step()

    def test_success_win(self):
        self.setup_step(CleanGitRepo())
        self.setProperty('buildername', 'Win-Build-EWS')
        self.setProperty('platform', 'win')

        self.expectRemoteCommands(
            ExpectShell(command=['bash', '--posix', '-o', 'pipefail', '-c', r'del .git\gc.log || exit 0'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['bash', '--posix', '-o', 'pipefail', '-c', 'git rebase --abort || exit 0'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['bash', '--posix', '-o', 'pipefail', '-c', 'git am --abort || exit 0'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['bash', '--posix', '-o', 'pipefail', '-c', 'git cherry-pick --abort || exit 0'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'clean', '-f', '-d'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'checkout', '--progress', 'origin/main', '-f'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout='You are in detached HEAD state.'),
            ExpectShell(command=['git', 'branch', '-D', 'main'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout='Deleted branch main (was 57015967fef9).'),
            ExpectShell(command=['git', 'branch', 'main'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout="Switched to a new branch 'main'"),
            ExpectShell(command=['bash', '--posix', '-o', 'pipefail', '-c', "git branch | grep -v ' main$' | grep -v 'HEAD detached at' | xargs git branch -D || exit 0"], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['bash', '--posix', '-o', 'pipefail', '-c', "git remote | grep -v 'origin$' | xargs -L 1 git remote rm || exit 0"], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'prune'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
        )
        self.expect_outcome(result=SUCCESS, state_string='Cleaned up git repository')
        return self.run_step()

    def test_success_master(self):
        self.setup_step(CleanGitRepo(default_branch='master'))
        self.setProperty('buildername', 'Commit-Queue')

        self.expectRemoteCommands(
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'rm -f .git/gc.log || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git rebase --abort || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git am --abort || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git cherry-pick --abort || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'clean', '-f', '-d'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'checkout', '--progress', 'origin/master', '-f'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout='You are in detached HEAD state.'),
            ExpectShell(command=['git', 'branch', '-D', 'master'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout='Deleted branch master (was 57015967fef9).'),
            ExpectShell(command=['git', 'branch', 'master'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout="Switched to a new branch 'master'"),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git branch | grep -v ' master$' | grep -v 'HEAD detached at' | xargs git branch -D || true"], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git remote | grep -v 'origin$' | xargs -L 1 git remote rm || true"], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'prune'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
        )
        self.expect_outcome(result=SUCCESS, state_string='Cleaned up git repository')
        return self.run_step()

    def test_failure(self):
        self.setup_step(CleanGitRepo())
        self.setProperty('buildername', 'Commit-Queue')

        self.expectRemoteCommands(
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'rm -f .git/gc.log || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git rebase --abort || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git am --abort || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git cherry-pick --abort || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'clean', '-f', '-d'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'checkout', '--progress', 'origin/main', '-f'], workdir='wkdir', timeout=300, log_environ=False).exit(128)
            .log('stdio', stdout='You are in detached HEAD state.'),
            ExpectShell(command=['git', 'branch', '-D', 'main'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout='Deleted branch main (was 57015967fef9).'),
            ExpectShell(command=['git', 'branch', 'main'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout="Switched to a new branch 'main'"),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git branch | grep -v ' main$' | grep -v 'HEAD detached at' | xargs git branch -D || true"], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git remote | grep -v 'origin$' | xargs -L 1 git remote rm || true"], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'prune'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
        )
        self.expect_outcome(result=FAILURE, state_string='Encountered some issues during cleanup')
        return self.run_step()

    def test_branch(self):
        self.setup_step(CleanGitRepo())
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('basename', 'safari-612-branch')

        self.expectRemoteCommands(
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'rm -f .git/gc.log || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git rebase --abort || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git am --abort || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git cherry-pick --abort || true'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'clean', '-f', '-d'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'checkout', '--progress', 'origin/main', '-f'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout='You are in detached HEAD state.'),
            ExpectShell(command=['git', 'branch', '-D', 'main'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout='Deleted branch main (was 57015967fef9).'),
            ExpectShell(command=['git', 'branch', 'main'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout="Switched to a new branch 'main'"),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git branch | grep -v ' main$' | grep -v 'HEAD detached at' | xargs git branch -D || true"], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git remote | grep -v 'origin$' | xargs -L 1 git remote rm || true"], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
            ExpectShell(command=['git', 'prune'], workdir='wkdir', timeout=300, log_environ=False).exit(0)
            .log('stdio', stdout=''),
        )
        self.expect_outcome(result=SUCCESS, state_string='Cleaned up git repository')
        return self.run_step()


class TestValidateChange(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def get_patch(self, title='Patch', obsolete=0):
        return json.loads('''{{"bug_id": 224460,
                     "creator":"reviewer@apple.com",
                     "data": "patch-contents",
                     "file_name":"bug-224460-20210412192105.patch",
                     "flags": [{{"creation_date" : "2021-04-12T23:21:06Z", "id": 445872, "modification_date": "2021-04-12T23:55:36Z", "name": "review", "setter": "ap@webkit.org", "status": "+", "type_id": 1}}],
                     "id": 425806,
                     "is_obsolete": {},
                     "is_patch": 1,
                     "summary": "{}"}}'''.format(obsolete, title))

    def get_pr(self, pr_number, title='Sample pull request', closed=False, labels=None, draft=False):
        return dict(
            number=pr_number,
            state='closed' if closed else 'open',
            title=title,
            user=dict(login='JonWBedard'),
            draft=draft,
            head=dict(
                sha='7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c',
                ref='eng/pull-request',
                repo=dict(
                    name='WebKit',
                    full_name='JonWBedard/WebKit',
                ),
            ), base=dict(
                sha='528b99575eebf7fa5b94f1fc51de81977f265005',
                ref='main',
                repo=dict(
                    name='WebKit',
                    full_name='WebKit/WebKit',
                ),
            ), labels=[dict(name=label) for label in labels or []],
        )

    def test_skipped_patch(self):
        self.setup_step(ValidateChange())
        self.setProperty('patch_id', '1234')
        self.setProperty('bug_id', '5678')
        self.setProperty('skip_validation', True)
        self.expect_outcome(result=SKIPPED, state_string='Validated change (skipped)')
        return self.run_step()

    def test_skipped_pr(self):
        self.setup_step(ValidateChange())
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('skip_validation', True)
        self.expect_outcome(result=SKIPPED, state_string='Validated change (skipped)')
        return self.run_step()

    def test_success_patch(self):
        self.setup_step(ValidateChange(verifyBugClosed=False))
        ValidateChange.get_patch_json = lambda x, patch_id: self.get_patch()
        self.setProperty('patch_id', '425806')
        self.expect_outcome(result=SUCCESS, state_string='Validated change')
        rc = self.run_step()
        self.expect_property('fast_commit_queue', None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_success_pr(self):
        self.setup_step(ValidateChange(verifyBugClosed=False))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request)
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expect_outcome(result=SUCCESS, state_string='Validated change')
        rc = self.run_step()
        self.expect_property('fast_commit_queue', None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_success_pr_blocked(self):
        self.setup_step(ValidateChange(verifyBugClosed=False))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request, labels=['merging-blocked'])
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expect_outcome(result=SUCCESS, state_string='Validated change')
        rc = self.run_step()
        self.expect_property('fast_commit_queue', None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_obsolete_patch(self):
        self.setup_step(ValidateChange(verifyBugClosed=False))
        ValidateChange.get_patch_json = lambda x, patch_id: self.get_patch(obsolete=1)
        self.setProperty('patch_id', '425806')
        self.expect_outcome(result=FAILURE, state_string='Patch 425806 is obsolete')
        rc = self.run_step()
        self.expect_property('fast_commit_queue', None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_obsolete_pr(self):
        self.setup_step(ValidateChange(verifyBugClosed=False))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request)
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '1ad60d45a112301f7b9f93dac06134524dae8480')
        self.expect_outcome(result=FAILURE, state_string='Hash 1ad60d45 on PR 1234 is outdated')
        rc = self.run_step()
        self.expect_property('fast_commit_queue', None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_deleted_pr(self):
        self.setup_step(ValidateChange(verifyBugClosed=True))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: False
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '1ad60d45a112301f7b9f93dac06134524dae8480')
        self.expect_outcome(result=FAILURE, state_string='Pull request 1234 is already closed')
        rc = self.run_step()
        self.expect_property('fast_commit_queue', None, 'fast_commit_queue is unexpectedly set')
        return rc

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_fast_cq_patches_trigger_fast_cq_mode(self):
        fast_cq_patch_titles = ('REVERT OF r1234', 'revert of r1234', 'REVERT of 123456@main', '[fast-cq]Patch', '[FAST-cq] patch', 'fast-cq-patch', 'FAST-CQ Patch')
        for fast_cq_patch_title in fast_cq_patch_titles:
            self.setup_step(ValidateChange(verifyBugClosed=False))
            ValidateChange.get_patch_json = lambda x, patch_id: self.get_patch(title=fast_cq_patch_title)
            self.setProperty('patch_id', '425806')
            self.expect_outcome(result=SUCCESS, state_string='Validated change')
            rc = self.run_step()
            self.expect_property('fast_commit_queue', True, f'fast_commit_queue is not set, patch title: {fast_cq_patch_title}')
        return rc

    def test_merge_queue(self):
        self.setup_step(ValidateChange(verifyMergeQueue=True))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request, labels=['merge-queue'])
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expect_outcome(result=SUCCESS, state_string='Validated change')
        rc = self.run_step()
        self.expect_property('fast_commit_queue', None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_merge_queue_blocked(self):
        self.setup_step(ValidateChange(verifyMergeQueue=True))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request, labels=['merge-queue', 'merging-blocked'])
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expect_outcome(result=FAILURE, state_string="PR 1234 has been marked as 'merging-blocked'")
        rc = self.run_step()
        self.expect_property('fast_commit_queue', None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_no_merge_queue(self):
        self.setup_step(ValidateChange(verifyMergeQueue=True))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request)
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expect_outcome(result=FAILURE, state_string='PR 1234 does not have a merge queue label')
        rc = self.run_step()
        self.expect_property('fast_commit_queue', None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_draft(self):
        self.setup_step(ValidateChange(verifyNoDraftForMergeQueue=True))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request, draft=True)
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expect_outcome(result=FAILURE, state_string='PR 1234 is a draft pull request')
        rc = self.run_step()
        self.expect_property('fast_commit_queue', None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_no_draft(self):
        self.setup_step(ValidateChange(verifyNoDraftForMergeQueue=True))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request, draft=False)
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expect_outcome(result=SUCCESS, state_string='Validated change')
        rc = self.run_step()
        self.expect_property('fast_commit_queue', None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_sensative_patch(self):
        self.setup_step(ValidateChange(verifyBugClosed=False))
        ValidateChange.get_patch_json = lambda x, patch_id: self.get_patch()
        self.setProperty('patch_id', '425806')
        self.setProperty('sensitive', True)
        self.setProperty('buildername', 'Commit-Queue')

        message = 'Cannot land security changes with Commit-Queue, please use a GitHub PR against a secret remote'
        self.expect_outcome(result=FAILURE, state_string=message)
        rc = self.run_step()
        self.expect_property('fast_commit_queue', None, 'fast_commit_queue is unexpectedly set')
        self.expect_property('build_finish_summary', message)
        self.expect_property('comment_text', message)
        return rc

    def test_skipped_branch(self):
        self.setup_step(ValidateChange(verifyBugClosed=False, branches=[r'main']))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request)
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.setProperty('github.base.ref', 'safari-123-branch')

        self.expect_outcome(result=FAILURE, state_string="Changes to 'safari-123-branch' are not tested")
        rc = self.run_step()
        self.expect_property('fast_commit_queue', None, 'fast_commit_queue is unexpectedly set')
        return rc


class TestRetrievePRDataFromLabel(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RetrievePRDataFromLabel(project='WebKit/WebKit'))
        GitHubMixin.get_number_of_prs_with_label = lambda self, label, retry=0: 4
        query_result = {'data': {'repository': {'pullRequests': {'edges': [
            {'node':
                {'title': 'Fix `test-webkitpy webkitflaskpy`', 'number': 17412, 'commits':
                    {'nodes': [{'commit': {'commitUrl': 'https://github.com/WebKit/WebKit/commit/582fb8b4f85cc9f385c0e0809170cadc48c7fed5', 'status': {'state': 'SUCCESS', 'contexts': [
                        {'context': 'api-gtk', 'state': 'SUCCESS'},
                        {'context': 'api-ios', 'state': 'SUCCESS'},
                        {'context': 'api-mac', 'state': 'SUCCESS'},
                        {'context': 'api-mac-debug', 'state': 'SUCCESS'},
                        {'context': 'bindings', 'state': 'SUCCESS'},
                        {'context': 'gtk', 'state': 'SUCCESS'},
                        {'context': 'gtk-wk2', 'state': 'SUCCESS'},
                        {'context': 'ios', 'state': 'SUCCESS'},
                        {'context': 'ios-sim', 'state': 'SUCCESS'},
                        {'context': 'ios-wk2', 'state': 'SUCCESS'},
                        {'context': 'ios-wk2-wpt', 'state': 'SUCCESS'},
                        {'context': 'jsc', 'state': 'SUCCESS'},
                        {'context': 'jsc-arm64', 'state': 'SUCCESS'},
                        {'context': 'jsc-armv7', 'state': 'SUCCESS'},
                        {'context': 'jsc-i386', 'state': 'SUCCESS'},
                        {'context': 'mac', 'state': 'SUCCESS'},
                        {'context': 'mac-AS-debug', 'state': 'SUCCESS'},
                        {'context': 'mac-AS-debug-wk2', 'state': 'SUCCESS'},
                        {'context': 'mac-wk1', 'state': 'SUCCESS'},
                        {'context': 'mac-wk2', 'state': 'SUCCESS'},
                        {'context': 'mac-wk2-stress', 'state': 'SUCCESS'},
                        {'context': 'services', 'state': 'SUCCESS'},
                        {'context': 'style', 'state': 'SUCCESS'},
                        {'context': 'tv', 'state': 'SUCCESS'},
                        {'context': 'tv-sim', 'state': 'SUCCESS'},
                        {'context': 'vision', 'state': 'SUCCESS'},
                        {'context': 'vision-sim', 'state': 'SUCCESS'},
                        {'context': 'watch', 'state': 'SUCCESS'},
                        {'context': 'watch-sim', 'state': 'SUCCESS'},
                        {'context': 'webkitperl', 'state': 'SUCCESS'},
                        {'context': 'webkitpy', 'state': 'SUCCESS'},
                        {'context': 'win', 'state': 'SUCCESS'},
                        {'context': 'wpe', 'state': 'SUCCESS'},
                        {'context': 'wpe-wk2', 'state': 'SUCCESS'}]}}}]}}},
            {'node':
                {'title': 'Import WPT css/compositing directory', 'number': 17418, 'commits':
                    {'nodes': [{'commit': {'commitUrl': 'https://github.com/WebKit/WebKit/commit/b8df1771f6cb7197bcc8c3940670a24b8cd77d47', 'status': {'state': 'SUCCESS', 'contexts': [
                        {'context': 'bindings', 'state': 'SUCCESS'},
                        {'context': 'ios', 'state': 'SUCCESS'},
                        {'context': 'jsc-arm64', 'state': 'SUCCESS'},
                        {'context': 'jsc-armv7', 'state': 'SUCCESS'},
                        {'context': 'jsc-i386', 'state': 'SUCCESS'},
                        {'context': 'services', 'state': 'SUCCESS'},
                        {'context': 'style', 'state': 'SUCCESS'},
                        {'context': 'tv', 'state': 'SUCCESS'},
                        {'context': 'watch-sim', 'state': 'SUCCESS'},
                        {'context': 'webkitperl', 'state': 'SUCCESS'},
                        {'context': 'webkitpy', 'state': 'SUCCESS'},
                        {'context': 'wpe', 'state': 'SUCCESS'}]}}}]}}},
            {'node':
                {'title': 'Test safe-merge-queue labelling', 'number': 17451, 'commits':
                    {'nodes': [{'commit': {'commitUrl': 'https://github.com/WebKit/WebKit/commit/b12b5ee6709e3ce6a9019342a12eded04975af18', 'status': {'state': 'FAILURE', 'contexts': [
                        {'context': 'style', 'state': 'FAILURE'},
                        {'context': 'api-gtk', 'state': 'SUCCESS'},
                        {'context': 'api-ios', 'state': 'SUCCESS'},
                        {'context': 'api-mac', 'state': 'SUCCESS'},
                        {'context': 'bindings', 'state': 'SUCCESS'},
                        {'context': 'gtk', 'state': 'SUCCESS'},
                        {'context': 'ios', 'state': 'SUCCESS'},
                        {'context': 'ios-sim', 'state': 'SUCCESS'},
                        {'context': 'ios-wk2', 'state': 'SUCCESS'},
                        {'context': 'jsc', 'state': 'SUCCESS'},
                        {'context': 'jsc-arm64', 'state': 'SUCCESS'},
                        {'context': 'jsc-armv7', 'state': 'SUCCESS'},
                        {'context': 'jsc-i386', 'state': 'SUCCESS'},
                        {'context': 'mac', 'state': 'SUCCESS'},
                        {'context': 'mac-AS-debug', 'state': 'SUCCESS'},
                        {'context': 'mac-wk1', 'state': 'SUCCESS'},
                        {'context': 'mac-wk2', 'state': 'SUCCESS'},
                        {'context': 'mac-wk2-stress', 'state': 'SUCCESS'},
                        {'context': 'services', 'state': 'SUCCESS'},
                        {'context': 'tv', 'state': 'SUCCESS'},
                        {'context': 'tv-sim', 'state': 'SUCCESS'},
                        {'context': 'watch', 'state': 'SUCCESS'},
                        {'context': 'watch-sim', 'state': 'SUCCESS'},
                        {'context': 'webkitperl', 'state': 'SUCCESS'},
                        {'context': 'webkitpy', 'state': 'SUCCESS'},
                        {'context': 'wpe', 'state': 'SUCCESS'}]}}}]}}},
            {'node':
                {'title': 'Split webkitpy.common.net.bugzilla.TestExpectationUpdater', 'number': 17454, 'commits':
                    {'nodes': [{'commit': {'commitUrl': 'https://github.com/WebKit/WebKit/commit/4b36be2acc1b4b3190f93c69acd6484e58b35ef0', 'status': {'state': 'SUCCESS', 'contexts': [
                        {'context': 'api-gtk', 'state': 'SUCCESS'},
                        {'context': 'bindings', 'state': 'SUCCESS'},
                        {'context': 'gtk', 'state': 'SUCCESS'},
                        {'context': 'ios', 'state': 'SUCCESS'},
                        {'context': 'ios-sim', 'state': 'SUCCESS'},
                        {'context': 'jsc-arm64', 'state': 'SUCCESS'},
                        {'context': 'jsc-armv7', 'state': 'SUCCESS'},
                        {'context': 'jsc-i386', 'state': 'SUCCESS'},
                        {'context': 'mac-AS-debug', 'state': 'SUCCESS'},
                        {'context': 'services', 'state': 'SUCCESS'},
                        {'context': 'style', 'state': 'SUCCESS'},
                        {'context': 'tv', 'state': 'SUCCESS'},
                        {'context': 'watch-sim', 'state': 'SUCCESS'},
                        {'context': 'webkitperl', 'state': 'SUCCESS'},
                        {'context': 'webkitpy', 'state': 'SUCCESS'},
                        {'context': 'wpe', 'state': 'SUCCESS'}]}}}]}}}]}}}}
        GitHubMixin.query_graph_ql = lambda self, query: query_result
        self.expect_outcome(result=SUCCESS, state_string="Successfully retrieved pull request data")
        rc = self.run_step()
        self.expect_property('project', 'WebKit/WebKit')
        self.expect_property('repository', 'https://github.com/WebKit/WebKit')
        self.expect_property('list_of_prs', [17412, 17418, 17451, 17454])
        return rc

    def test_success_project(self):
        self.setup_step(RetrievePRDataFromLabel(project='testRepo/WebKit'))
        GitHubMixin.get_number_of_prs_with_label = lambda self, label, retry=0: 4
        query_result = {'data': {'repository': {'pullRequests': {'edges': [
            {'node':
                {'title': 'Fix `test-webkitpy webkitflaskpy`', 'number': 17412, 'commits':
                    {'nodes': [{'commit': {'commitUrl': 'https://github.com/WebKit/WebKit/commit/582fb8b4f85cc9f385c0e0809170cadc48c7fed5',
                        'status': {'state': 'SUCCESS', 'contexts': [
                            {'context': 'api-gtk', 'state': 'SUCCESS'},
                            {'context': 'api-ios', 'state': 'SUCCESS'},
                            {'context': 'api-mac', 'state': 'SUCCESS'},
                            {'context': 'bindings', 'state': 'SUCCESS'},
                            {'context': 'gtk', 'state': 'SUCCESS'},
                            {'context': 'gtk-wk2', 'state': 'SUCCESS'},
                            {'context': 'ios', 'state': 'SUCCESS'},
                            {'context': 'ios-sim', 'state': 'SUCCESS'},
                            {'context': 'ios-wk2', 'state': 'SUCCESS'},
                            {'context': 'ios-wk2-wpt', 'state': 'SUCCESS'},
                            {'context': 'jsc', 'state': 'SUCCESS'},
                            {'context': 'jsc-arm64', 'state': 'SUCCESS'},
                            {'context': 'jsc-armv7', 'state': 'SUCCESS'},
                            {'context': 'jsc-i386', 'state': 'SUCCESS'},
                            {'context': 'mac', 'state': 'SUCCESS'},
                            {'context': 'mac-AS-debug', 'state': 'SUCCESS'},
                            {'context': 'mac-AS-debug-wk2', 'state': 'SUCCESS'},
                            {'context': 'mac-wk1', 'state': 'SUCCESS'},
                            {'context': 'mac-wk2', 'state': 'SUCCESS'},
                            {'context': 'mac-wk2-stress', 'state': 'SUCCESS'},
                            {'context': 'services', 'state': 'SUCCESS'},
                            {'context': 'style', 'state': 'SUCCESS'},
                            {'context': 'tv', 'state': 'SUCCESS'},
                            {'context': 'tv-sim', 'state': 'SUCCESS'},
                            {'context': 'watch', 'state': 'SUCCESS'},
                            {'context': 'watch-sim', 'state': 'SUCCESS'},
                            {'context': 'webkitperl', 'state': 'SUCCESS'},
                            {'context': 'webkitpy', 'state': 'SUCCESS'},
                            {'context': 'win', 'state': 'SUCCESS'},
                            {'context': 'wpe', 'state': 'SUCCESS'},
                            {'context': 'wpe-wk2', 'state': 'SUCCESS'}]}}}]}}},
            {'node':
                {'title': 'Import WPT css/compositing directory', 'number': 17418, 'commits':
                    {'nodes': [{'commit': {'commitUrl': 'https://github.com/WebKit/WebKit/commit/b8df1771f6cb7197bcc8c3940670a24b8cd77d47', 'status': {'state': 'SUCCESS', 'contexts': [
                        {'context': 'bindings', 'state': 'SUCCESS'},
                        {'context': 'ios', 'state': 'SUCCESS'},
                        {'context': 'jsc-arm64', 'state': 'SUCCESS'},
                        {'context': 'jsc-armv7', 'state': 'SUCCESS'},
                        {'context': 'jsc-i386', 'state': 'SUCCESS'},
                        {'context': 'services', 'state': 'SUCCESS'},
                        {'context': 'style', 'state': 'SUCCESS'},
                        {'context': 'tv', 'state': 'SUCCESS'},
                        {'context': 'watch-sim', 'state': 'SUCCESS'},
                        {'context': 'webkitperl', 'state': 'SUCCESS'},
                        {'context': 'webkitpy', 'state': 'SUCCESS'},
                        {'context': 'wpe', 'state': 'SUCCESS'}]}}}]}}},
            {'node':
                {'title': 'Test safe-merge-queue labelling', 'number': 17451, 'commits':
                    {'nodes': [{'commit': {'commitUrl': 'https://github.com/WebKit/WebKit/commit/b12b5ee6709e3ce6a9019342a12eded04975af18', 'status': {'state': 'FAILURE', 'contexts': [
                        {'context': 'style', 'state': 'FAILURE'},
                        {'context': 'api-gtk', 'state': 'SUCCESS'},
                        {'context': 'api-ios', 'state': 'SUCCESS'},
                        {'context': 'api-mac', 'state': 'SUCCESS'},
                        {'context': 'bindings', 'state': 'SUCCESS'},
                        {'context': 'gtk', 'state': 'SUCCESS'},
                        {'context': 'ios', 'state': 'SUCCESS'},
                        {'context': 'ios-sim', 'state': 'SUCCESS'},
                        {'context': 'ios-wk2', 'state': 'SUCCESS'},
                        {'context': 'jsc', 'state': 'SUCCESS'},
                        {'context': 'jsc-arm64', 'state': 'SUCCESS'},
                        {'context': 'jsc-armv7', 'state': 'SUCCESS'},
                        {'context': 'jsc-i386', 'state': 'SUCCESS'},
                        {'context': 'mac', 'state': 'SUCCESS'},
                        {'context': 'mac-AS-debug', 'state': 'SUCCESS'},
                        {'context': 'mac-wk1', 'state': 'SUCCESS'},
                        {'context': 'mac-wk2', 'state': 'SUCCESS'},
                        {'context': 'mac-wk2-stress', 'state': 'SUCCESS'},
                        {'context': 'services', 'state': 'SUCCESS'},
                        {'context': 'tv', 'state': 'SUCCESS'},
                        {'context': 'tv-sim', 'state': 'SUCCESS'},
                        {'context': 'watch', 'state': 'SUCCESS'},
                        {'context': 'watch-sim', 'state': 'SUCCESS'},
                        {'context': 'webkitperl', 'state': 'SUCCESS'},
                        {'context': 'webkitpy', 'state': 'SUCCESS'},
                        {'context': 'wpe', 'state': 'SUCCESS'}]}}}]}}},
            {'node':
                {'title': 'Split webkitpy.common.net.bugzilla.TestExpectationUpdater', 'number': 17454, 'commits':
                    {'nodes': [{'commit': {'commitUrl': 'https://github.com/WebKit/WebKit/commit/4b36be2acc1b4b3190f93c69acd6484e58b35ef0', 'status': {'state': 'SUCCESS', 'contexts': [
                        {'context': 'api-gtk', 'state': 'SUCCESS'},
                        {'context': 'bindings', 'state': 'SUCCESS'},
                        {'context': 'gtk', 'state': 'SUCCESS'},
                        {'context': 'ios', 'state': 'SUCCESS'},
                        {'context': 'ios-sim', 'state': 'SUCCESS'},
                        {'context': 'jsc-arm64', 'state': 'SUCCESS'},
                        {'context': 'jsc-armv7', 'state': 'SUCCESS'},
                        {'context': 'jsc-i386', 'state': 'SUCCESS'},
                        {'context': 'mac-AS-debug', 'state': 'SUCCESS'},
                        {'context': 'services', 'state': 'SUCCESS'},
                        {'context': 'style', 'state': 'SUCCESS'},
                        {'context': 'tv', 'state': 'SUCCESS'},
                        {'context': 'watch-sim', 'state': 'SUCCESS'},
                        {'context': 'webkitperl', 'state': 'SUCCESS'},
                        {'context': 'webkitpy', 'state': 'SUCCESS'},
                        {'context': 'wpe', 'state': 'SUCCESS'}]}}}]}}}]}}}}
        GitHubMixin.query_graph_ql = lambda self, query: query_result
        self.expect_outcome(result=SUCCESS, state_string="Successfully retrieved pull request data")
        rc = self.run_step()
        self.expect_property('project', 'testRepo/WebKit')
        self.expect_property('repository', 'https://github.com/testRepo/WebKit')
        self.expect_property('list_of_prs', [17412, 17418, 17451, 17454])
        return rc

    def test_failure(self):
        self.timeout = 1000
        self.setup_step(RetrievePRDataFromLabel(project='testRepo/WebKit'))
        GitHubMixin.get_number_of_prs_with_label = lambda self, label, retry=0: None
        query_result = {'errors': [{'message': 'Error'}]}
        GitHubMixin.query_graph_ql = lambda self, query: query_result
        self.expect_outcome(result=FAILURE, state_string="Failed to retrieve pull request data")
        rc = self.run_step()
        self.expect_property('project', 'testRepo/WebKit')
        self.expect_property('repository', 'https://github.com/testRepo/WebKit')
        return rc


class TestCheckStatusOfPR(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CheckStatusOfPR())
        self.setProperty('github.number', 12345)
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('list_of_prs', [12344, 12346])
        CheckStatusOfPR.validateCommitterStatus = lambda self, pr_number: True
        CheckStatusOfPR.checkPRStatus = lambda self, pr_number: True
        self.expect_outcome(result=SUCCESS, state_string="PR 12345 marked safe for merge-queue")
        rc = self.run_step()
        return rc


class TestAddMergeLabelsToPRs(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(AddMergeLabelsToPRs())
        self.setProperty('passed_status_check', [12345, 12344])
        self.setProperty('failed_status_check', [12345])
        self.expect_outcome(result=SUCCESS, state_string="Started PR labelling process successfully")
        rc = self.run_step()
        return rc


class TestRemoveAndAddLabels(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success_blocked(self):
        self.setup_step(RemoveAndAddLabels(label_to_add='merging-blocked'))
        self.setProperty('buildername', 'Merge-Queue')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('failed_status_check', [17451])
        self.setProperty('passed_status_check', [12345])
        RemoveAndAddLabels.update_labels = lambda self, pr_number: SUCCESS
        self.expect_outcome(result=SUCCESS, state_string='Labelled PR 17451 with merging-blocked')
        rc = self.run_step()
        self.expect_property('failed_status_check', [])
        self.expect_property('passed_status_check', [12345])
        return rc

    def test_failure_blocked(self):
        self.setup_step(RemoveAndAddLabels(label_to_add='merging-blocked'))
        self.setProperty('buildername', 'Merge-Queue')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('failed_status_check', [])
        self.expect_outcome(result=FAILURE, state_string='Failed to label PR  with merging-blocked')
        rc = self.run_step()
        self.expect_property('failed_status_check', [])
        return rc

    def test_success_merge_queue(self):
        self.setup_step(RemoveAndAddLabels(label_to_add='merge-queue'))
        self.setProperty('buildername', 'Merge-Queue')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('passed_status_check', [17451])
        RemoveAndAddLabels.update_labels = lambda self, pr_number: SUCCESS
        self.expect_outcome(result=SUCCESS, state_string='Labelled PR 17451 with merge-queue')
        rc = self.run_step()
        self.expect_property('passed_status_check', [])
        return rc

    def test_failure_merge_queue(self):
        self.setup_step(RemoveAndAddLabels(label_to_add='merge-queue'))
        self.setProperty('buildername', 'Merge-Queue')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('passed_status_check', [])
        self.expect_outcome(result=FAILURE, state_string='Failed to label PR  with merge-queue')
        rc = self.run_step()
        self.expect_property('passed_status_check', [])
        return rc


class TestValidateUserForQueue(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        Contributors.load = mock_load_contributors
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success_patch(self):
        self.setup_step(ValidateUserForQueue())
        self.setProperty('patch_committer', 'committer@webkit.org')
        self.expect_outcome(result=SUCCESS, state_string='Validated user for queue')
        return self.run_step()

    def test_success_pr(self):
        self.setup_step(ValidateUserForQueue())
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.expect_outcome(result=SUCCESS, state_string='Validated user for queue')
        return self.run_step()

    def test_failure_load_contributors_patch(self):
        self.setup_step(ValidateUserForQueue())
        self.setProperty('patch_committer', 'abc@webkit.org')
        Contributors.load = lambda *args, **kwargs: ({}, [])
        self.expect_outcome(result=FAILURE, state_string='Failed to get contributors information (failure)')
        return self.run_step()

    def test_failure_load_contributors_pr(self):
        self.setup_step(ValidateUserForQueue())
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['abc'])
        Contributors.load = lambda *args, **kwargs: ({}, [])
        self.expect_outcome(result=FAILURE, state_string='Failed to get contributors information (failure)')
        return self.run_step()

    def test_failure_invalid_committer_patch(self):
        self.setup_step(ValidateUserForQueue())
        self.setProperty('patch_committer', 'abc@webkit.org')
        self.expect_outcome(result=FAILURE, state_string='Skipping queue, as abc@webkit.org lacks committer status (failure)')
        return self.run_step()

    def test_failure_invalid_committer_pr(self):
        self.setup_step(ValidateUserForQueue())
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['abc'])
        self.expect_outcome(result=FAILURE, state_string='Skipping queue, as abc lacks committer status (failure)')
        return self.run_step()


class TestValidateCommitterAndReviewer(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        Contributors.load = mock_load_contributors
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success_patch(self):
        self.setup_step(ValidateCommitterAndReviewer())
        self.setProperty('patch_id', '1234')
        self.setProperty('patch_committer', 'committer@webkit.org')
        self.setProperty('reviewer', 'reviewer@apple.com')
        self.expect_hidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expect_outcome(result=SUCCESS, state_string='Validated committer and reviewer')
        rc = self.run_step()
        self.expect_property('valid_reviewers', ['WebKit Reviewer'])
        return rc

    def test_success_pr(self):
        self.setup_step(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-reviewer']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.expect_hidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expect_outcome(result=SUCCESS, state_string='Validated committer and reviewer')
        rc = self.run_step()
        self.expect_property('valid_reviewers', ['WebKit Reviewer'])
        return rc

    def test_success_pr_duplicate(self):
        self.setup_step(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-reviewer', 'webkit-reviewer']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.expect_hidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expect_outcome(result=SUCCESS, state_string='Validated committer and reviewer')
        rc = self.run_step()
        self.expect_property('valid_reviewers', ['WebKit Reviewer'])
        return rc

    def test_success_no_reviewer_patch(self):
        self.setup_step(ValidateCommitterAndReviewer())
        self.setProperty('patch_id', '1234')
        self.setProperty('patch_committer', 'reviewer@apple.com')
        self.expect_hidden(False)
        self.expect_outcome(result=SUCCESS, state_string='Validated committer, valid reviewer not found')
        return self.run_step()

    def test_success_no_reviewer_pr(self):
        self.setup_step(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: []
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-reviewer'])
        self.expect_hidden(False)
        self.expect_outcome(result=SUCCESS, state_string='Validated committer, valid reviewer not found')
        return self.run_step()

    def test_success_integration(self):
        self.setup_step(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-reviewer']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-integration'])
        self.expect_hidden(False)
        self.expect_outcome(result=SUCCESS, state_string='Validated committer and reviewer')
        return self.run_step()

    def test_failure_load_contributors_patch(self):
        self.setup_step(ValidateCommitterAndReviewer())
        self.setProperty('patch_id', '1234')
        self.setProperty('patch_committer', 'abc@webkit.org')
        Contributors.load = lambda *args, **kwargs: ({}, [])
        self.expect_hidden(False)
        self.expect_outcome(result=FAILURE, state_string='Failed to get contributors information')
        return self.run_step()

    def test_failure_load_contributors_pr(self):
        self.setup_step(ValidateCommitterAndReviewer())
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['abc'])
        Contributors.load = lambda *args, **kwargs: ({}, [])
        self.expect_hidden(False)
        self.expect_outcome(result=FAILURE, state_string='Failed to get contributors information')
        return self.run_step()

    def test_failure_invalid_committer_patch(self):
        self.setup_step(ValidateCommitterAndReviewer())
        self.setProperty('patch_id', '1234')
        self.setProperty('patch_committer', 'abc@webkit.org')
        self.expect_hidden(False)
        self.expect_outcome(result=FAILURE, state_string='abc@webkit.org does not have committer permissions')
        return self.run_step()

    def test_failure_invalid_committer_pr(self):
        self.setup_step(ValidateCommitterAndReviewer())
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['abc'])
        self.expect_hidden(False)
        self.expect_outcome(result=FAILURE, state_string='abc does not have committer permissions')
        return self.run_step()

    def test_success_invalid_reviewer_patch(self):
        self.setup_step(ValidateCommitterAndReviewer())
        self.setProperty('patch_id', '1234')
        self.setProperty('patch_committer', 'reviewer@apple.com')
        self.setProperty('reviewer', 'committer@webkit.org')
        self.expect_hidden(False)
        self.expect_outcome(result=SUCCESS, state_string='Validated committer, valid reviewer not found')
        rc = self.run_step()
        self.expect_property('valid_reviewers', [])
        self.expect_property('invalid_reviewers', ['WebKit Committer'])
        return rc

    def test_success_invalid_reviewer_pr(self):
        self.setup_step(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-commit-queue']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-reviewer'])
        self.expect_hidden(False)
        self.expect_outcome(result=SUCCESS, state_string='Validated committer, valid reviewer not found')
        rc = self.run_step()
        self.expect_property('valid_reviewers', [])
        self.expect_property('invalid_reviewers', ['WebKit Committer'])
        return rc

    def test_load_contributors_from_disk(self):
        contributors = filter(lambda element: element.get('name') == 'Aakash Jain', Contributors().load_from_disk()[0])
        self.assertEqual(list(contributors)[0]['emails'][0], 'aakash_jain@apple.com')

    def test_success_pr_validators(self):
        self.setup_step(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-reviewer', 'webkit-bug-bridge']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'apple')
        self.expect_hidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expect_outcome(result=SUCCESS, state_string='Validated committer and reviewer')
        rc = self.run_step()
        self.expect_property('valid_reviewers', ['WebKit Reviewer'])
        return rc

    def test_success_pr_validators_case(self):
        self.setup_step(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-reviewer', 'Webkit-Bug-Bridge']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'apple')
        self.expect_hidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expect_outcome(result=SUCCESS, state_string='Validated committer and reviewer')
        rc = self.run_step()
        self.expect_property('valid_reviewers', ['WebKit Reviewer'])
        return rc

    def test_success_pr_validators_not_reviewer(self):
        self.setup_step(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-bug-bridge']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'apple')
        self.expect_hidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expect_outcome(result=SUCCESS, state_string='Validated committer, valid reviewer not found')
        rc = self.run_step()
        self.expect_property('valid_reviewers', [])
        return rc

    def test_success_no_pr_validators(self):
        self.setup_step(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-reviewer']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'security')
        self.expect_hidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expect_outcome(result=SUCCESS, state_string='Validated committer and reviewer')
        rc = self.run_step()
        self.expect_property('valid_reviewers', ['WebKit Reviewer'])
        return rc

    def test_failure_pr_validators(self):
        self.setup_step(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-reviewer']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'apple')
        self.expect_hidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expect_outcome(result=FAILURE, state_string="Landing changes on 'apple' remote requires validation from @webkit-bug-bridge")
        return self.run_step()


class TestCheckStatusOnEWSQueues(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        CheckStatusOnEWSQueues.get_change_status = lambda cls, change_id, queue: SUCCESS
        self.setup_step(CheckStatusOnEWSQueues())
        self.expect_outcome(result=SUCCESS, state_string='mac-wk2 tests already passed')
        rc = self.run_step()
        self.expect_property('passed_mac_wk2', True)
        return rc

    def test_failure(self):
        self.setup_step(CheckStatusOnEWSQueues())
        CheckStatusOnEWSQueues.get_change_status = lambda cls, change_id, queue: FAILURE
        self.expect_outcome(result=SUCCESS, state_string='mac-wk2 tests failed')
        rc = self.run_step()
        self.expect_property('passed_mac_wk2', False)
        return rc

    def test_mac_wk2_not_finished_yet(self):
        self.setup_step(CheckStatusOnEWSQueues())
        CheckStatusOnEWSQueues.get_change_status = lambda cls, change_id, queue: None
        self.expect_outcome(result=SUCCESS, state_string="mac-wk2 tests haven't completed")
        rc = self.run_step()
        self.expect_property('passed_mac_wk2', None)
        return rc


class TestPushCommitToWebKitRepo(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        GitHub.credentials = lambda user=None: ('webkit-commit-queue', 'password')
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(PushCommitToWebKitRepo())
        self.setProperty('patch_id', '1234')
        self.setProperty('remote', 'origin')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        log_environ=False,
                        env=dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password'),
                        command=['git', 'push', 'origin', 'HEAD:main'])
            .log('stdio', stdout=' 4c3bac1de151...b94dc426b331 \n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = self.run_step()
        self.expect_property('landed_hash', 'b94dc426b331')
        return rc

    @expectedFailure
    def test_failure_retry(self):
        self.setup_step(PushCommitToWebKitRepo())
        self.setProperty('patch_id', '2345')
        self.setProperty('remote', 'origin')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        log_environ=False,
                        env=dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password'),
                        command=['git', 'push', 'origin', 'HEAD:main'])
            .log('stdio', stdout='Unexpected failure')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to push commit to Webkit repository')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = self.run_step()
        self.expect_property('retry_count', 1)
        self.expect_property('landed_hash', None)
        return rc

    def test_failure_patch(self):
        self.setup_step(PushCommitToWebKitRepo())
        self.setProperty('remote', 'origin')
        self.setProperty('patch_id', '2345')
        self.setProperty('retry_count', PushCommitToWebKitRepo.MAX_RETRY)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        log_environ=False,
                        env=dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password'),
                        command=['git', 'push', 'origin', 'HEAD:main'])
            .log('stdio', stdout='Unexpected failure')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to push commit to Webkit repository')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = self.run_step()
        self.expect_property('build_finish_summary', 'Failed to commit to WebKit repository')
        self.expect_property('comment_text', 'commit-queue failed to commit attachment 2345 to WebKit repository. To retry, please set cq+ flag again.')
        return rc

    def test_failure_pr(self):
        self.setup_step(PushCommitToWebKitRepo())
        self.setProperty('github.number', '1234')
        self.setProperty('remote', 'origin')
        self.setProperty('retry_count', PushCommitToWebKitRepo.MAX_RETRY)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        log_environ=False,
                        env=dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password'),
                        command=['git', 'push', 'origin', 'HEAD:main'])
            .log('stdio', stdout='Unexpected failure')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to push commit to Webkit repository')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = self.run_step()
        self.expect_property('build_finish_summary', 'Failed to commit to WebKit repository')
        self.expect_property('comment_text', 'merge-queue failed to commit PR to repository. To retry, remove any blocking labels and re-apply merge-queue label')
        return rc


class TestDetermineLabelOwner(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success_merge_queue(self):
        self.setup_step(DetermineLabelOwner())
        self.setProperty('github.number', 17518)
        self.setProperty('buildername', 'Merge-Queue')
        response = {"data": {
                    "repository": {
                        "pullRequest": {
                            "timelineItems": {
                                "nodes": [
                                    {
                                        "actor": {
                                            "login": "JonWBedard"
                                        },
                                        "label": {
                                            "name": "safe-merge-queue"
                                        },
                                        "createdAt": "2023-09-07T00:24:06Z"
                                    },
                                    {
                                        "actor": {
                                            "login": "webkit-ews-buildbot"
                                        },
                                        "label": {
                                            "name": "merge-queue"
                                        },
                                        "createdAt": "2023-09-11T20:53:23Z"}]}}}}}
        GitHubMixin.query_graph_ql = lambda self, query: response
        self.expect_outcome(result=SUCCESS, state_string='Owner of PR 17518 determined to be JonWBedard\n')
        self.run_step()
        self.expect_property('owners', ['JonWBedard'])

    def test_failure(self):
        self.setup_step(DetermineLabelOwner())
        self.setProperty('github.number', 17518)
        self.setProperty('buildername', 'Merge-Queue')
        response = {"data": {
                    "repository": {
                        "pullRequest": {
                            "timelineItems": {
                                "nodes": [
                                    {
                                        "actor": {
                                            "login": "JonWBedard"
                                        },
                                        "label": {
                                            "name": "Tools / Tests"
                                        },
                                        "createdAt": "2023-09-07T00:24:06Z"
                                    },
                                    {
                                        "actor": {
                                            "login": "webkit-commit-queue"
                                        },
                                        "label": {
                                            "name": "merging-blocked"
                                        },
                                        "createdAt": "2023-09-11T20:53:23Z"}]}}}}}
        GitHubMixin.query_graph_ql = lambda self, query: response
        self.expect_outcome(result=FAILURE, state_string="Unable to determine owner of PR 17518\n")
        self.run_step()


class TestDetermineLandedIdentifier(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def mock_commits_webkit_org(self, identifier=None):
        return patch('ews-build.steps.TwistedAdditions.request', lambda *args, **kwargs: TwistedAdditions.Response(
            status_code=200,
            content=json.dumps(dict(identifier=identifier) if identifier else dict(status='Not Found')).encode('utf-8'),
        ))

    @classmethod
    def mock_sleep(cls):
        return patch('twisted.internet.task.deferLater', lambda *_, **__: None)

    @defer.inlineCallbacks
    def test_success_pr(self):
        with self.mock_commits_webkit_org(), self.mock_sleep():
            self.setup_step(DetermineLandedIdentifier())
            self.setProperty('landed_hash', '14dbf1155cf5')
            self.setProperty('github.number', '1234')
            self.expectRemoteCommands(
                ExpectShell(workdir='wkdir',
                            timeout=300,
                            log_environ=False,
                            command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git log -1 --no-decorate | grep 'Canonical link: https://commits\\.webkit\\.org/'"])
                .log('stdio', stdout='    Canonical link: https://commits.webkit.org/220797@main\n')
                .exit(0),
            )
            self.expect_outcome(result=SUCCESS, state_string='Identifier: 220797@main')
            with current_hostname(EWS_BUILD_HOSTNAMES[0]):
                yield self.run_step()

        self.expect_property('comment_text', 'Committed 220797@main (14dbf1155cf5): <https://commits.webkit.org/220797@main>\n\nReviewed commits have been landed. Closing PR #1234 and removing active labels.')
        self.expect_property('build_summary', 'Committed 220797@main')

    @defer.inlineCallbacks
    def test_success_gardening_pr(self):
        with self.mock_commits_webkit_org(), self.mock_sleep():
            self.setup_step(DetermineLandedIdentifier())
            self.setProperty('landed_hash', '5dc27962b4c5')
            self.setProperty('github.number', '1234')
            self.setProperty('is_test_gardening', True)
            self.expectRemoteCommands(
                ExpectShell(workdir='wkdir',
                            timeout=300,
                            log_environ=False,
                            command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git log -1 --no-decorate | grep 'Canonical link: https://commits\\.webkit\\.org/'"])
                .log('stdio', stdout='    Canonical link: https://commits.webkit.org/249903@main\n')
                .exit(0),
            )
            self.expect_outcome(result=SUCCESS, state_string='Identifier: 249903@main')
            with current_hostname(EWS_BUILD_HOSTNAMES[0]):
                yield self.run_step()

        self.expect_property('comment_text', 'Test gardening commit 249903@main (5dc27962b4c5): <https://commits.webkit.org/249903@main>\n\nReviewed commits have been landed. Closing PR #1234 and removing active labels.')
        self.expect_property('build_summary', 'Committed 249903@main')

    @defer.inlineCallbacks
    def test_success_pr_fallback(self):
        with self.mock_commits_webkit_org(identifier='220797@main'), self.mock_sleep():
            self.setup_step(DetermineLandedIdentifier())
            self.setProperty('landed_hash', '5dc27962b4c5')
            self.setProperty('github.number', '1234')
            self.expectRemoteCommands(
                ExpectShell(workdir='wkdir',
                            timeout=300,
                            log_environ=False,
                            command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git log -1 --no-decorate | grep 'Canonical link: https://commits\\.webkit\\.org/'"])
                .log('stdio', stdout='')
                .exit(1),
            )
            self.expect_outcome(result=SUCCESS, state_string='Identifier: 220797@main')
            with current_hostname(EWS_BUILD_HOSTNAMES[0]):
                yield self.run_step()

        self.expect_property('comment_text', 'Committed 220797@main (5dc27962b4c5): <https://commits.webkit.org/220797@main>\n\nReviewed commits have been landed. Closing PR #1234 and removing active labels.')
        self.expect_property('build_summary', 'Committed 220797@main')

    @defer.inlineCallbacks
    def test_pr_no_identifier(self):
        with self.mock_commits_webkit_org(), self.mock_sleep():
            self.setup_step(DetermineLandedIdentifier())
            self.setProperty('landed_hash', '5dc27962b4c5')
            self.setProperty('github.number', '1234')
            self.expectRemoteCommands(
                ExpectShell(workdir='wkdir',
                            timeout=300,
                            log_environ=False,
                            command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git log -1 --no-decorate | grep 'Canonical link: https://commits\\.webkit\\.org/'"])
                .log('stdio', stdout='')
                .exit(1),
            )
            self.expect_outcome(result=FAILURE, state_string='Failed to determine identifier')
            with current_hostname(EWS_BUILD_HOSTNAMES[0]):
                yield self.run_step()

        self.expect_property('comment_text', 'Committed ? (5dc27962b4c5): <https://commits.webkit.org/5dc27962b4c5>\n\nReviewed commits have been landed. Closing PR #1234 and removing active labels.')
        self.expect_property('build_summary', 'Committed 5dc27962b4c5')

    @defer.inlineCallbacks
    def test_success_patch(self):
        with self.mock_commits_webkit_org(identifier='220797@main'), self.mock_sleep():
            self.setup_step(DetermineLandedIdentifier())
            self.setProperty('landed_hash', '5dc27962b4c5')
            self.setProperty('patch_id', '1234')
            self.expectRemoteCommands(
                ExpectShell(workdir='wkdir',
                            timeout=300,
                            log_environ=False,
                            command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git log -1 --no-decorate | grep 'Canonical link: https://commits\\.webkit\\.org/'"])
                .log('stdio', stdout='')
                .exit(1),
            )
            self.expect_outcome(result=SUCCESS, state_string='Identifier: 220797@main')
            with current_hostname(EWS_BUILD_HOSTNAMES[0]):
                yield self.run_step()

        self.expect_property('comment_text', 'Committed 220797@main (5dc27962b4c5): <https://commits.webkit.org/220797@main>\n\nAll reviewed patches have been landed. Closing bug and clearing flags on attachment 1234.')
        self.expect_property('build_summary', 'Committed 220797@main')

    @defer.inlineCallbacks
    def test_patch_no_identifier(self):
        with self.mock_commits_webkit_org(), self.mock_sleep():
            self.setup_step(DetermineLandedIdentifier())
            self.setProperty('landed_hash', '5dc27962b4c5')
            self.setProperty('patch_id', '1234')
            self.expectRemoteCommands(
                ExpectShell(workdir='wkdir',
                            timeout=300,
                            log_environ=False,
                            command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git log -1 --no-decorate | grep 'Canonical link: https://commits\\.webkit\\.org/'"])
                .log('stdio', stdout='')
                .exit(1),
            )
            self.expect_outcome(result=FAILURE, state_string='Failed to determine identifier')
            with current_hostname(EWS_BUILD_HOSTNAMES[0]):
                yield self.run_step()

        self.expect_property('comment_text', 'Committed ? (5dc27962b4c5): <https://commits.webkit.org/5dc27962b4c5>\n\nAll reviewed patches have been landed. Closing bug and clearing flags on attachment 1234.')
        self.expect_property('build_summary', 'Committed 5dc27962b4c5')


class TestCheckOutSource(BuildStepMixinAdditions, unittest.TestCase):
    ENV = dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password')

    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_success(self):
        self.setup_step(CheckOutSource())
        self.setProperty('project', 'WebKit/WebKit')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', '--version'],
            )
            .log('stdio', stdout='git version 2.32.3 (Apple Git-135)\n').exit(0),
            Expect(
                'stat', dict(
                    file='wkdir/.buildbot-patched',
                    log_environ=False,
                ),
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', 'clean', '-f', '-f', '-d', '-x'],
            ).exit(0),
            Expect(
                'listdir', dict(
                    dir='wkdir',
                    timeout=7200,
                    log_environ=False,
                ),
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', 'clone', 'https://github.com/WebKit/WebKit.git', '.', '--progress'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', 'rev-parse', 'HEAD'],
            )
            .log('stdio', stdout='3b84731a5f6a0a38b6f48a16ab927e5dbcb5c770\n').exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', 'remote', 'set-url', '--push', 'origin', 'PUSH_DISABLED_BY_ADMIN'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Cleaned and updated working directory')
        return self.run_step()

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_success_merge_queue(self):
        self.setup_step(CheckOutSource())
        self.setProperty('project', 'WebKit/WebKit')
        self.setProperty('buildername', 'Merge-Queue')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', '--version'],
            )
            .log('stdio', stdout='git version 2.32.3 (Apple Git-135)\n').exit(0),
            Expect(
                'stat', dict(
                    file='wkdir/.buildbot-patched',
                    log_environ=False,
                ),
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', 'clean', '-f', '-f', '-d', '-x'],
            ).exit(0),
            Expect(
                'listdir', dict(
                    dir='wkdir',
                    timeout=7200,
                    log_environ=False,
                ),
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', 'clone', 'https://github.com/WebKit/WebKit.git', '.', '--progress'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', 'rev-parse', 'HEAD'],
            )
            .log('stdio', stdout='3b84731a5f6a0a38b6f48a16ab927e5dbcb5c770\n').exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Cleaned and updated working directory')
        return self.run_step()

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_success_security(self):
        self.setup_step(CheckOutSource())
        self.setProperty('project', 'WebKit/WebKit-security')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', '--version'],
            )
            .log('stdio', stdout='git version 2.32.3 (Apple Git-135)\n').exit(0),
            Expect(
                'stat', dict(
                    file='wkdir/.buildbot-patched',
                    log_environ=False,
                ),
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', 'clean', '-f', '-f', '-d', '-x'],
            ).exit(0),
            Expect(
                'listdir', dict(
                    dir='wkdir',
                    timeout=7200,
                    log_environ=False,
                ),
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', 'clone', 'https://github.com/WebKit/WebKit.git', '.', '--progress'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', 'rev-parse', 'HEAD'],
            )
            .log('stdio', stdout='3b84731a5f6a0a38b6f48a16ab927e5dbcb5c770\n').exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', 'remote', 'set-url', '--push', 'origin', 'PUSH_DISABLED_BY_ADMIN'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Cleaned and updated working directory')
        return self.run_step()

    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_failure(self):
        self.setup_step(CheckOutSource())
        self.setProperty('project', 'WebKit/WebKit')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', '--version'],
            )
            .log('stdio', stdout='git version 2.32.3 (Apple Git-135)\n').exit(0),
            Expect(
                'stat', dict(
                    file='wkdir/.buildbot-patched',
                    log_environ=False,
                ),
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', 'clean', '-f', '-f', '-d', '-x'],
            ).exit(0),
            Expect(
                'listdir', dict(
                    dir='wkdir',
                    timeout=7200,
                    log_environ=False,
                ),
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                env=self.ENV,
                command=['git', 'clone', 'https://github.com/WebKit/WebKit.git', '.', '--progress'],
            ).exit(1),
            Expect(
                'rmdir', dict(
                    dir='wkdir',
                    timeout=7200,
                    log_environ=False,
                ),
            )
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to updated working directory')
        return self.run_step()


class TestShowIdentifier(BuildStepMixinAdditions, unittest.TestCase):
    class MockPreviousStep(object):
        def __init__(self):
            self.text = None
            self.url = None

        def addURL(self, text, url):
            self.text = text
            self.url = url

    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    @expectedFailure
    def test_success(self):
        previous_steps = {
            CheckOutSpecificRevision.name: self.MockPreviousStep(),
            CheckOutSource.name: self.MockPreviousStep(),
        }
        ShowIdentifier.getLastBuildStepByName = lambda _, name: previous_steps.get(name, self.MockPreviousStep())
        self.setup_step(ShowIdentifier())
        self.setProperty('ews_revision', '51a6aec9f664')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/git-webkit', 'find', '51a6aec9f664'])
            .log('stdio', stdout='Identifier: 233175@main\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Identifier: 233175@main')
        rc = self.run_step()
        self.expect_property('identifier', '233175@main')
        self.assertEqual(previous_steps[CheckOutSpecificRevision.name].text, 'Updated to 233175@main')
        self.assertEqual(previous_steps[CheckOutSpecificRevision.name].url, 'https://commits.webkit.org/233175@main')
        self.assertEqual(previous_steps[CheckOutSource.name].text, None)
        self.assertEqual(previous_steps[CheckOutSource.name].url, None)
        return rc

    @expectedFailure
    def test_success_pull_request(self):
        previous_steps = {
            CheckOutSpecificRevision.name: self.MockPreviousStep(),
            CheckOutSource.name: self.MockPreviousStep(),
        }
        ShowIdentifier.getLastBuildStepByName = lambda _, name: previous_steps.get(name, self.MockPreviousStep())
        self.setup_step(ShowIdentifier())
        self.setProperty('got_revision', '51a6aec9f664')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/git-webkit', 'find', '51a6aec9f664'])
            .log('stdio', stdout='Identifier: 233175@main\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Identifier: 233175@main')
        rc = self.run_step()
        self.expect_property('identifier', '233175@main')
        self.assertEqual(previous_steps[CheckOutSpecificRevision.name].text, None)
        self.assertEqual(previous_steps[CheckOutSpecificRevision.name].url, None)
        self.assertEqual(previous_steps[CheckOutSource.name].text, 'Updated to 233175@main')
        self.assertEqual(previous_steps[CheckOutSource.name].url, 'https://commits.webkit.org/233175@main')
        return rc

    def test_prioritized(self):
        self.setup_step(ShowIdentifier())
        self.setProperty('ews_revision', '51a6aec9f664')
        self.setProperty('got_revision', '9f66451a6aec')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/git-webkit', 'find', '51a6aec9f664'])
            .log('stdio', stdout='Identifier: 233175@main\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Identifier: 233175@main')
        rc = self.run_step()
        self.expect_property('identifier', '233175@main')
        return rc

    def test_failure(self):
        self.setup_step(ShowIdentifier())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/git-webkit', 'find', 'HEAD'])
            .log('stdio', stdout='Unexpected failure')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to find identifier')
        return self.run_step()


class TestInstallHooks(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_no_remote(self):
        self.setup_step(InstallHooks())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=30,
                        log_environ=False,
                        command=['git', 'config', 'include.path', '../metadata/git_config_extension'])
            .log('stdio', stdout='Unexpected failure').exit(0),
            ExpectShell(workdir='wkdir',
                        timeout=30,
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/git-webkit', 'install-hooks', 'pre-push', '--mode', 'no-radar'])
            .log('stdio', stdout='Unexpected failure').exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Installed hooks to checkout')
        return self.run_step()

    def test_unknown_remote(self):
        self.setup_step(InstallHooks())
        self.setProperty('github.head.repo.full_name', 'JonWBedard/WebKit-igalia')
        self.setProperty('project', 'Igalia/WebKit')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=30,
                        log_environ=False,
                        command=['git', 'config', 'include.path', '../metadata/git_config_extension'])
            .log('stdio', stdout='Unexpected failure').exit(0),
            ExpectShell(workdir='wkdir',
                        timeout=30,
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/git-webkit', 'install-hooks', 'pre-push', '--mode', 'no-radar'])
            .log('stdio', stdout='Unexpected failure').exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Installed hooks to checkout')
        return self.run_step()

    def test_origin_remote(self):
        self.setup_step(InstallHooks())
        self.setProperty('github.head.repo.full_name', 'JonWBedard/WebKit')
        self.setProperty('project', 'WebKit/WebKit')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=30,
                        log_environ=False,
                        command=['git', 'config', 'include.path', '../metadata/git_config_extension'])
            .log('stdio', stdout='Unexpected failure').exit(0),
            ExpectShell(workdir='wkdir',
                        timeout=30,
                        log_environ=False,
                        command=[
                            'python3', 'Tools/Scripts/git-webkit',
                            'install-hooks', 'pre-push', '--mode', 'no-radar',
                            '--level', 'github.com:JonWBedard/WebKit=0',
                        ])
            .log('stdio', stdout='Unexpected failure').exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Installed hooks to checkout')
        return self.run_step()

    def test_apple_remote(self):
        self.setup_step(InstallHooks())
        self.setProperty('github.head.repo.full_name', 'JonWBedard/WebKit-security')
        self.setProperty('project', 'WebKit/WebKit-security')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=30,
                        log_environ=False,
                        command=['git', 'config', 'include.path', '../metadata/git_config_extension'])
            .log('stdio', stdout='Unexpected failure').exit(0),
            ExpectShell(workdir='wkdir',
                        timeout=30,
                        log_environ=False,
                        command=[
                            'python3', 'Tools/Scripts/git-webkit',
                            'install-hooks', 'pre-push', '--mode', 'no-radar',
                            '--level', 'github.com:JonWBedard/WebKit-security=1',
                        ])
            .log('stdio', stdout='Unexpected failure').exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Installed hooks to checkout')
        return self.run_step()

    def test_failure(self):
        self.setup_step(InstallHooks())
        self.setProperty('github.head.repo.full_name', 'JonWBedard/WebKit')
        self.setProperty('project', 'WebKit/WebKit')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=30,
                        log_environ=False,
                        command=['git', 'config', 'include.path', '../metadata/git_config_extension'])
            .log('stdio', stdout='Unexpected failure').exit(0),
            ExpectShell(workdir='wkdir',
                        timeout=30,
                        log_environ=False,
                        command=[
                            'python3', 'Tools/Scripts/git-webkit',
                            'install-hooks', 'pre-push', '--mode', 'no-radar',
                            '--level', 'github.com:JonWBedard/WebKit=0',
                        ])
            .log('stdio', stdout='Unexpected failure').exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to install hooks to checkout')
        return self.run_step()


class TestSetCredentialHelper(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(SetCredentialHelper())
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                command=['git', 'config', '--global', 'credential.helper', '!echo_credentials() { sleep 1; echo "username=${GIT_USER}"; echo "password=${GIT_PASSWORD}"; }; echo_credentials'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS)
        return self.run_step()


class TestFetchBranches(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(FetchBranches())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        log_environ=False,
                        command=['git', 'fetch', 'origin', '--prune'])
            .log('stdio', stdout='   fb192c1de607..afb17ed1708b  main       -> origin/main\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS)
        return self.run_step()

    def test_success_remote(self):
        GitHub.credentials = lambda user=None: ('webkit-commit-queue', 'password')
        self.setup_step(FetchBranches())
        self.setProperty('project', 'WebKit/WebKit-security')
        self.setProperty('remote', 'security')
        env = dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password')

        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=env,
                command=['git', 'fetch', 'origin', '--prune'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=env,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git remote add security https://github.com/WebKit/WebKit-security.git || true'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=env,
                command=['git', 'remote', 'set-url', 'security', 'https://github.com/WebKit/WebKit-security.git'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=env,
                command=['git', 'fetch', 'security', '--prune'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS)
        return self.run_step()

    def test_failure(self):
        self.setup_step(FetchBranches())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        log_environ=False,
                        command=['git', 'fetch',  'origin', '--prune'])
            .log('stdio', stdout="fatal: unable to access 'https://github.com/WebKit/WebKit/': Could not resolve host: github.com\n")
            .exit(2),
        )
        self.expect_outcome(result=FAILURE)
        return self.run_step()


class TestValidateRemote(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_patch(self):
        self.setup_step(ValidateRemote())
        self.setProperty('patch_id', '1234')
        self.expect_outcome(result=SKIPPED, state_string='finished (skipped)')
        return self.run_step()

    def test_origin(self):
        self.setup_step(ValidateRemote())
        self.setProperty('remote', 'origin')
        self.expect_outcome(result=SKIPPED, state_string='finished (skipped)')
        return self.run_step()

    def test_success(self):
        self.setup_step(ValidateRemote())
        self.setProperty('remote', 'security')
        self.setProperty('github.base.ref', 'safari-000-branch')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                command=['git', 'merge-base', '--is-ancestor', 'remotes/security/safari-000-branch', 'remotes/origin/safari-000-branch'],
            ).exit(1),
        )
        self.expect_outcome(result=SUCCESS, state_string="Verified 'WebKit/WebKit' does not own 'safari-000-branch'")
        return self.run_step()

    def test_failure(self):
        self.setup_step(ValidateRemote())
        self.setProperty('remote', 'security')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                command=['git', 'merge-base', '--is-ancestor', 'remotes/security/main', 'remotes/origin/main'],
            ).exit(0),
        )
        self.expect_outcome(result=FAILURE, state_string="Cannot land on 'main', it is owned by 'WebKit/WebKit'")
        rc = self.run_step()
        self.expect_property('comment_text', "Cannot land on 'main', it is owned by 'WebKit/WebKit', blocking PR #1234.\nMake a pull request against 'WebKit/WebKit' to land this change.")
        self.expect_property('build_finish_summary', "Cannot land on 'main', it is owned by 'WebKit/WebKit'")
        return rc


class TestMapBranchAlias(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_patch(self):
        self.setup_step(MapBranchAlias())
        self.setProperty('patch_id', '1234')
        self.expect_outcome(result=SKIPPED, state_string='finished (skipped)')
        return self.run_step()

    def test_main(self):
        self.setup_step(MapBranchAlias())
        self.setProperty('remote', 'origin')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.number', '1234')
        self.expect_outcome(result=SKIPPED, state_string='finished (skipped)')
        return self.run_step()

    def test_prod_branch(self):
        self.setup_step(MapBranchAlias())
        self.setProperty('remote', 'origin')
        self.setProperty('github.base.ref', 'safari-000-branch')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=60,
                command=['git', 'branch', '-a', '--contains', 'remotes/origin/safari-000-branch'],
            ).exit(0)
            .log('stdio', stdout='  safari-000-branch\n  remotes/origin/safari-000-branch\n  remotes/origin/safari-alias\n  remotes/origin/eng/pr-branch\n'),
        )
        self.expect_outcome(result=SUCCESS, state_string="'safari-000-branch' is the prevailing alias")
        rc = self.run_step()
        self.expect_property('github.base.ref', 'safari-000-branch')
        return rc

    def test_main_override(self):
        self.setup_step(MapBranchAlias())
        self.setProperty('remote', 'origin')
        self.setProperty('github.base.ref', 'safari-000-branch')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=60,
                command=['git', 'branch', '-a', '--contains', 'remotes/origin/safari-000-branch'],
            ).exit(0)
            .log('stdio', stdout='  safari-000-branch\n  remotes/origin/safari-000-branch\n  remotes/origin/safari-alias\n  remotes/origin/eng/pr-branch\n  remotes/origin/main\n'),
        )
        self.expect_outcome(result=SUCCESS, state_string="'main' is the prevailing alias")
        rc = self.run_step()
        self.expect_property('github.base.ref', 'main')
        return rc

    def test_alias_branch(self):
        self.setup_step(MapBranchAlias())
        self.setProperty('remote', 'origin')
        self.setProperty('github.base.ref', 'safari-alias')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=60,
                command=['git', 'branch', '-a', '--contains', 'remotes/origin/safari-alias'],
            ).exit(0)
            .log('stdio', stdout='  safari-alias\n  remotes/origin/safari-000-branch\n  remotes/origin/safari-alias\n  remotes/origin/eng/pr-branch\n'),
        )
        self.expect_outcome(result=SUCCESS, state_string="'safari-000-branch' is the prevailing alias")
        rc = self.run_step()
        self.expect_property('github.base.ref', 'safari-000-branch')
        return rc

    def test_prod_branch_alternate_remote(self):
        self.setup_step(MapBranchAlias())
        self.setProperty('remote', 'security')
        self.setProperty('github.base.ref', 'safari-000-branch')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=60,
                command=['git', 'branch', '-a', '--contains', 'remotes/security/safari-000-branch'],
            ).exit(0)
            .log('stdio', stdout='  safari-000-branch\n  remotes/security/safari-000-branch\n  remotes/security/safari-alias\n  remotes/security/eng/pr-branch\n  remotes/origin/main\n'),
        )
        self.expect_outcome(result=SUCCESS, state_string="'safari-000-branch' is the prevailing alias")
        rc = self.run_step()
        self.expect_property('github.base.ref', 'safari-000-branch')
        return rc

    def test_alias_branch_alternate_remote(self):
        self.setup_step(MapBranchAlias())
        self.setProperty('remote', 'security')
        self.setProperty('github.base.ref', 'safari-alias')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=60,
                command=['git', 'branch', '-a', '--contains', 'remotes/security/safari-alias'],
            ).exit(0)
            .log('stdio', stdout='  safari-alias\n  remotes/security/safari-000-branch\n  remotes/security/safari-alias\n  remotes/security/eng/pr-branch\n  remotes/origin/main\n'),
        )
        self.expect_outcome(result=SUCCESS, state_string="'safari-000-branch' is the prevailing alias")
        rc = self.run_step()
        self.expect_property('github.base.ref', 'safari-000-branch')
        return rc

    def test_failure(self):
        self.setup_step(MapBranchAlias())
        self.setProperty('remote', 'origin')
        self.setProperty('github.base.ref', 'safari-000-branch')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=60,
                command=['git', 'branch', '-a', '--contains', 'remotes/origin/safari-000-branch'],
            ).exit(129)
            .log('stdio', stdout='error: malformed object name remotes/origin/safari-000-branch\n'),
        )
        self.expect_outcome(result=FAILURE, state_string="Failed to query checkout for aliases of 'safari-000-branch'")
        return self.run_step()


class TestValidateSquashed(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_patch(self):
        self.setup_step(ValidateSquashed())
        self.setProperty('patch_id', '1234')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'log', '--format=format:%H', 'HEAD', '^origin/main', '--max-count=51'],
                        )
            .exit(0)
            .log('stdio', stdout='e1eb24603493\n'),
        )
        self.expect_outcome(result=SUCCESS, state_string='Verified commit is squashed')
        return self.run_step()

    def test_failure_patch(self):
        self.setup_step(ValidateSquashed())
        self.setProperty('patch_id', '1234')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'log', '--format=format:%H', 'HEAD', '^origin/main', '--max-count=51'],
                        )
            .exit(0)
            .log('stdio', stdout='e1eb24603493\n08abb9ddcbb5\n45cf3efe4dfb\n'),
        )
        self.expect_outcome(result=FAILURE, state_string='Can only land squashed commits')
        rc = self.run_step()
        self.expect_property('comment_text', 'This change contains multiple commits which are not squashed together, rejecting attachment 1234 from commit queue. Please squash the commits to land.')
        self.expect_property('build_finish_summary', 'Can only land squashed commits')
        return rc

    def test_success(self):
        self.setup_step(ValidateSquashed())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'log', '--format=format:%H', 'eng/pull-request-branch', '^main', '--max-count=51'],
                        )
            .exit(0)
            .log('stdio', stdout='e1eb24603493\n'),
        )
        self.expect_outcome(result=SUCCESS, state_string='Verified commit is squashed')
        return self.run_step()

    def test_failure_multiple_commits(self):
        self.setup_step(ValidateSquashed())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'log', '--format=format:%H', 'eng/pull-request-branch', '^main', '--max-count=51'],
                        )
            .exit(0)
            .log('stdio', stdout='e1eb24603493\n08abb9ddcbb5\n45cf3efe4dfb\n'),
        )
        self.expect_outcome(result=FAILURE, state_string='Can only land squashed commits')
        rc = self.run_step()
        self.expect_property('comment_text', 'This change contains multiple commits which are not squashed together, blocking PR #1234. Please squash the commits to land.')
        self.expect_property('build_finish_summary', 'Can only land squashed commits')
        return rc

    def test_failure_merged(self):
        self.setup_step(ValidateSquashed())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'log', '--format=format:%H', 'eng/pull-request-branch', '^main', '--max-count=51'],
                        )
            .exit(0)
            .log('stdio', stdout=''),
        )
        self.expect_outcome(result=FAILURE, state_string='Can only land squashed commits')
        rc = self.run_step()
        self.expect_property('comment_text', 'This change contains multiple commits which are not squashed together, blocking PR #1234. Please squash the commits to land.')
        self.expect_property('build_finish_summary', 'Can only land squashed commits')
        return rc

    def test_success_multiple_commits_cherry_pick(self):
        self.setup_step(ValidateSquashed())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('classification', ['Cherry-pick'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'log', '--format=format:%H', 'eng/pull-request-branch', '^main', '--max-count=51'],
                        )
            .exit(0)
            .log('stdio', stdout='e1eb24603493\n08abb9ddcbb5\n45cf3efe4dfb\n'),
        )
        self.expect_outcome(result=SUCCESS, state_string='Commit sequence is entirely cherry-picks')
        rc = self.run_step()
        self.expect_property('commit_count', 3)
        return rc

    def test_failure_too_many_commits_cherry_pick(self):
        self.setup_step(ValidateSquashed())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('classification', ['Cherry-pick'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'log', '--format=format:%H', 'eng/pull-request-branch', '^main', '--max-count=51'],
                        )
            .exit(0)
            .log('stdio', stdout='e1eb24603493\n' + 50 * '08abb9ddcbb5\n'),
        )
        self.expect_outcome(result=FAILURE, state_string='Too many commits in a pull-request')
        rc = self.run_step()
        self.expect_property('commit_count', 51)
        self.expect_property('comment_text', 'Policy allows for multiple cherry-picks to be landed simultaneously but there is a limit of 50, blocking PR #1234 because it has 51 commits. Please break this change into multiple pull requests.')
        self.expect_property('build_finish_summary', 'Too many commits in a pull-request')
        return rc


class TestAddReviewerToCommitMessage(BuildStepMixinAdditions, unittest.TestCase):
    ENV = dict(
        GIT_COMMITTER_NAME='WebKit Committer',
        GIT_COMMITTER_EMAIL='committer@webkit.org',
        FILTER_BRANCH_SQUELCH_WARNING='1',
    )

    def setUp(self):
        self.longMessage = True
        Contributors.load = mock_load_contributors
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_skipped_patch(self):
        self.setup_step(AddReviewerToCommitMessage())
        self.setProperty('patch_id', '1234')
        self.expect_outcome(result=SKIPPED, state_string='Skipped because there are no valid reviewers')
        return self.run_step()

    def test_success(self):
        gmtoffset = int(time.localtime().tm_gmtoff * 100 / (60 * 60))
        fixed_time = int(time.time())
        date = f'{int(time.time())} {gmtoffset}'
        time.time = lambda: fixed_time

        self.setup_step(AddReviewerToCommitMessage())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('valid_reviewers', ['WebKit Reviewer', 'Other Reviewer'])
        self.setProperty('owners', ['webkit-commit-queue'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        env=self.ENV,
                        timeout=60,
                        command=[
                            'git', 'filter-branch', '-f',
                            '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}'".format(date=date),
                            '--msg-filter', 'sed "s/NOBODY (OO*PP*S!*)/WebKit Reviewer and Other Reviewer/g"',
                            'eng/pull-request-branch...main',
                        ])
            .exit(0)
            .log('stdio', stdout="Ref 'refs/heads/eng/pull-request-branch' was rewritten\n"),
        )
        self.expect_outcome(result=SUCCESS, state_string='Reviewed by WebKit Reviewer and Other Reviewer')
        return self.run_step()

    def test_success_more_than_two_reviewers(self):
        gmtoffset = int(time.localtime().tm_gmtoff * 100 / (60 * 60))
        fixed_time = int(time.time())
        date = f'{int(time.time())} {gmtoffset}'
        time.time = lambda: fixed_time

        self.setup_step(AddReviewerToCommitMessage())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('valid_reviewers', ['WebKit Reviewer', 'Other Reviewer', 'Another Reviewer'])
        self.setProperty('owners', ['webkit-commit-queue'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        env=self.ENV,
                        timeout=60,
                        command=[
                            'git', 'filter-branch', '-f',
                            '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}'".format(date=date),
                            '--msg-filter', 'sed "s/NOBODY (OO*PP*S!*)/WebKit Reviewer, Other Reviewer, and Another Reviewer/g"',
                            'eng/pull-request-branch...main',
                        ])
            .exit(0)
            .log('stdio', stdout="Ref 'refs/heads/eng/pull-request-branch' was rewritten\n"),
        )
        self.expect_outcome(result=SUCCESS, state_string='Reviewed by WebKit Reviewer, Other Reviewer, and Another Reviewer')
        return self.run_step()

    def test_failure(self):
        gmtoffset = int(time.localtime().tm_gmtoff * 100 / (60 * 60))
        fixed_time = int(time.time())
        date = f'{int(time.time())} {gmtoffset}'
        time.time = lambda: fixed_time

        self.setup_step(AddReviewerToCommitMessage())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('valid_reviewers', ['WebKit Reviewer', 'Other Reviewer'])
        self.setProperty('owners', ['webkit-commit-queue'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        env=self.ENV,
                        timeout=60,
                        command=[
                            'git', 'filter-branch', '-f',
                            '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}'".format(date=date),
                            '--msg-filter', 'sed "s/NOBODY (OO*PP*S!*)/WebKit Reviewer and Other Reviewer/g"',
                            'eng/pull-request-branch...main',
                        ])
            .exit(2)
            .log('stdio', stdout="Failed to rewrite 'refs/heads/eng/pull-request-branch'\n"),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to apply reviewers')
        return self.run_step()

    def test_no_reviewers(self):
        self.setup_step(AddReviewerToCommitMessage())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('valid_reviewers', [])
        self.expect_outcome(result=SKIPPED, state_string='Skipped because there are no valid reviewers')
        return self.run_step()

    def test_skip_cherry_pick(self):
        self.setup_step(AddReviewerToCommitMessage())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('valid_reviewers', ['WebKit Reviewer', 'Other Reviewer'])
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('classification', ['Cherry-pick'])
        self.expect_outcome(result=SKIPPED, state_string='Skipped because commit is a cherry-pick')
        return self.run_step()


class TestValidateCommitMessage(BuildStepMixinAdditions, unittest.TestCase):
    def expectCommonRemoteCommandsWithOutput(self, expected_remote_command_output):
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                                 "git log eng/pull-request-branch ^main | grep -q 'OO*PP*S!' && echo 'Commit message contains (OOPS!) and no valid reviewer found' || test $? -eq 1"])
            .exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                                 "git log eng/pull-request-branch ^main > commit_msg.txt; grep -q '\\(Reviewed by\\|Rubber-stamped by\\|Rubber stamped by\\|Unreviewed\\|Versioning.\\)' commit_msg.txt || echo 'No reviewer information in commit message';"])
            .exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                                 "git log eng/pull-request-branch ^main | grep '\\(Reviewed by\\|Rubber-stamped by\\|Rubber stamped by\\)' || true"])
            .exit(0)
            .log('stdio', stdout=expected_remote_command_output),
        )

    def setUp(self):
        self.longMessage = True
        Contributors.load = mock_load_contributors
        return self.setup_test_build_step()

    def setUpCommonProperties(self):
        self.setProperty('buildnumber', '2345')
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_patch(self):
        self.setup_step(ValidateCommitMessage())
        self.setProperty('patch_id', '1234')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git log HEAD ^origin/main | grep -q 'OO*PP*S!' && echo 'Commit message contains (OOPS!) and no valid reviewer found' || test $? -eq 1"])
            .exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git log HEAD ^origin/main > commit_msg.txt; grep -q '\\(Reviewed by\\|Rubber-stamped by\\|Rubber stamped by\\|Unreviewed\\|Versioning.\\)' commit_msg.txt || echo 'No reviewer information in commit message';"])
            .exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git log HEAD ^origin/main | grep '\\(Reviewed by\\|Rubber-stamped by\\|Rubber stamped by\\)' || true"])
            .exit(0)
            .log('stdio', stdout='    Reviewed by WebKit Reviewer.\n'),
        )
        self.expect_outcome(result=SUCCESS, state_string='Validated commit message')
        return self.run_step()

    def test_success(self):
        self.setup_step(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        expected_remote_command_output = '    Reviewed by WebKit Reviewer.\n'
        self.expectCommonRemoteCommandsWithOutput(expected_remote_command_output)
        self.expect_outcome(result=SUCCESS, state_string='Validated commit message')
        return self.run_step()

    def test_success_period(self):
        self.setup_step(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        expected_remote_command_output = '    Reviewed by Myles C. Maxfield.\n'
        self.expectCommonRemoteCommandsWithOutput(expected_remote_command_output)
        self.expect_outcome(result=SUCCESS, state_string='Validated commit message')
        return self.run_step()

    def test_success_two_reviewers(self):
        self.setup_step(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        expected_remote_command_output = '    Reviewed by WebKit Reviewer and Abrar Protyasha.\n'
        self.expectCommonRemoteCommandsWithOutput(expected_remote_command_output)
        self.expect_outcome(result=SUCCESS, state_string='Validated commit message')
        return self.run_step()

    def test_success_more_than_two_reviewers(self):
        self.setup_step(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        expected_remote_command_output = '    Reviewed by WebKit Reviewer, Abrar Protyasha, and Myles C. Maxfield.\n'
        self.expectCommonRemoteCommandsWithOutput(expected_remote_command_output)
        self.expect_outcome(result=SUCCESS, state_string='Validated commit message')
        return self.run_step()

    def test_success_with_invalid(self):
        self.setup_step(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        self.setProperty('invalid_reviewers', ['Web Kit'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git log eng/pull-request-branch ^main | grep -q 'OO*PP*S!' && echo 'Commit message contains (OOPS!) and Web Kit is not a reviewer' || test $? -eq 1"])
            .exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git log eng/pull-request-branch ^main > commit_msg.txt; grep -q '\\(Reviewed by\\|Rubber-stamped by\\|Rubber stamped by\\|Unreviewed\\|Versioning.\\)' commit_msg.txt || echo 'No reviewer information in commit message';"])
            .exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git log eng/pull-request-branch ^main | grep '\\(Reviewed by\\|Rubber-stamped by\\|Rubber stamped by\\)' || true"])
            .exit(0)
            .log('stdio', stdout='    Reviewed by Myles C. Maxfield.\n'),
        )
        self.expect_outcome(result=SUCCESS, state_string='Validated commit message')
        return self.run_step()

    @expectedFailure
    def test_failure_oops(self):
        self.setup_step(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git log eng/pull-request-branch ^main | grep -q 'OO*PP*S!' && echo 'Commit message contains (OOPS!) and no valid reviewer found' || test $? -eq 1"])
            .exit(1)
            .log('stdio', stdout='Commit message contains (OOPS!) and no valid reviewer found\n'),
        )
        self.expect_outcome(result=FAILURE, state_string='Commit message contains (OOPS!) and no valid reviewer found')
        rc = self.run_step()
        self.assertRegex(self.getProperty('comment_text'), r'Commit message contains \(OOPS!\) and no valid reviewer found, blocking PR #1234. Details: \[Build #2345\]\(http.*/#/builders/1/builds/13\)')
        return rc

    def test_unoffical_reviewers(self):
        self.setup_step(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        self.setProperty('invalid_reviewers', ['Web Kit', 'Kit Web'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', "git log eng/pull-request-branch ^main | grep -q 'OO*PP*S!' && echo 'Commit message contains (OOPS!) and Web Kit, Kit Web are not reviewers' || test $? -eq 1"])
            .exit(1)
            .log('stdio', stdout='Commit message contains (OOPS!) and Web Kit, Kit Web are not reviewers\n'),
        )
        self.expect_outcome(result=FAILURE, state_string="Commit message contains (OOPS!) and Web Kit, Kit Web are not reviewers")
        return self.run_step()

    @expectedFailure
    def test_failure_multiple(self):
        self.setup_step(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        self.expectCommonRemoteCommandsWithOutput('No reviewer information in commit message\nFailed to access https://raw.githubusercontent.com/WebKit/WebKit/main/metadata/contributors.json\n')
        self.expect_outcome(result=FAILURE, state_string='No reviewer information in commit message')
        rc = self.run_step()
        self.assertRegex(self.getProperty('comment_text'), r'No reviewer information in commit message, blocking PR #1234. Details: \[Build #2345\]\(http.*/#/builders/1/builds/13\)')
        return rc

    @expectedFailure
    def test_failure_no_reviewer(self):
        self.setup_step(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        self.expectCommonRemoteCommandsWithOutput('No reviewer information in commit message\n')
        self.expect_outcome(result=FAILURE, state_string='No reviewer information in commit message')
        rc = self.run_step()
        self.assertRegex(self.getProperty('comment_text'), r'No reviewer information in commit message, blocking PR #1234. Details: \[Build #2345\]\(http.*/#/builders/1/builds/13\)')
        return rc

    def test_failure_no_changelog(self):
        self.setup_step(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/ChangeLog', '+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        expected_remote_command_output = '    Reviewed by WebKit Reviewer.\n'
        self.expectCommonRemoteCommandsWithOutput(expected_remote_command_output)
        self.expect_outcome(result=FAILURE, state_string='ChangeLog modified, WebKit only allows commit messages')
        return self.run_step()

    def test_success_with_changelog_tools(self):
        self.setup_step(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: [
            '+++ Tools/Scripts/prepare-ChangeLog',
            '+++ Tools/Scripts/webkitperl/prepare-ChangeLog_unittest/resources/swift_unittests-expected.txt',
            '+++ Tools/Scripts/webkitperl/prepare-ChangeLog_unittest/resources/swift_unittests.swift']
        self.setUpCommonProperties()
        expected_remote_command_output = '    Reviewed by WebKit Reviewer.\n'
        self.expectCommonRemoteCommandsWithOutput(expected_remote_command_output)
        self.expect_outcome(result=SUCCESS, state_string='Validated commit message')
        return self.run_step()

    def test_invalid_reviewer(self):
        self.setup_step(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        expected_remote_command_output = '    Reviewed by WebKit Contributor.\n'
        self.expectCommonRemoteCommandsWithOutput(expected_remote_command_output)
        self.expect_outcome(result=SUCCESS, state_string="'WebKit Contributor' is not a reviewer, still continuing")
        return self.run_step()

    def test_self_reviewer(self):
        self.setup_step(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        self.setProperty('author', 'WebKit Reviewer <reviewer@apple.com>')
        expected_remote_command_output = '    Reviewed by WebKit Reviewer.\n'
        self.expectCommonRemoteCommandsWithOutput(expected_remote_command_output)
        self.expect_outcome(result=FAILURE, state_string="'WebKit Reviewer <reviewer@apple.com>' cannot review their own change")
        return self.run_step()


class TestCanonicalize(BuildStepMixinAdditions, unittest.TestCase):
    ENV = dict(
        FILTER_BRANCH_SQUELCH_WARNING='1',
        GIT_USER='webkit-commit-queue',
        GIT_PASSWORD='password',
    )

    def setUp(self):
        self.longMessage = True
        Contributors.load = mock_load_contributors
        GitHub.credentials = lambda user=None: ('webkit-commit-queue', 'password')
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_patch(self):
        self.setup_step(Canonicalize())
        self.setProperty('patch_id', '1234')
        self.setProperty('patch_committer', 'committer@webkit.org')
        self.setProperty('remote', 'origin')

        gmtoffset = int(time.localtime().tm_gmtoff * 100 / (60 * 60))
        fixed_time = int(time.time())
        date = f'{int(time.time())} {gmtoffset}'
        time.time = lambda: fixed_time

        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'rm .git/identifiers.json || true'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['git', 'pull', 'origin', 'main', '--rebase'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['git', 'checkout', '--progress', 'main'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['python3', 'Tools/Scripts/git-webkit', 'canonicalize', '-n', '1'],
            ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        env=self.ENV,
                        timeout=300,
                        command=[
                            'git', 'filter-branch', '-f',
                            '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}';GIT_COMMITTER_NAME='WebKit Committer';GIT_COMMITTER_EMAIL='committer@webkit.org'".format(date=date),
                            'HEAD...HEAD~1',
                        ],
                        ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Canonicalized commit')
        return self.run_step()

    def test_success(self):
        self.setup_step(Canonicalize())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'origin')

        gmtoffset = int(time.localtime().tm_gmtoff * 100 / (60 * 60))
        fixed_time = int(time.time())
        date = f'{int(time.time())} {gmtoffset}'
        time.time = lambda: fixed_time

        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'rm .git/identifiers.json || true'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['git', 'pull', 'origin', 'main', '--rebase'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['git', 'branch', '-f', 'main', 'eng/pull-request-branch'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['git', 'checkout', '--progress', 'main'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['python3', 'Tools/Scripts/git-webkit', 'canonicalize', '-n', '1'],
            ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        env=self.ENV,
                        timeout=300,
                        command=[
                            'git', 'filter-branch', '-f',
                            '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}';GIT_COMMITTER_NAME='WebKit Committer';GIT_COMMITTER_EMAIL='committer@webkit.org'".format(date=date),
                            'HEAD...HEAD~1',
                        ],
                        ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Canonicalized commit')
        return self.run_step()

    def test_success_multiple_commits(self):
        self.setup_step(Canonicalize())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'origin')
        self.setProperty('commit_count', 4)

        gmtoffset = int(time.localtime().tm_gmtoff * 100 / (60 * 60))
        fixed_time = int(time.time())
        date = f'{int(time.time())} {gmtoffset}'
        time.time = lambda: fixed_time

        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'rm .git/identifiers.json || true'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['git', 'pull', 'origin', 'main', '--rebase'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['git', 'branch', '-f', 'main', 'eng/pull-request-branch'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['git', 'checkout', '--progress', 'main'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['python3', 'Tools/Scripts/git-webkit', 'canonicalize', '-n', '4'],
            ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        env=self.ENV,
                        timeout=300,
                        command=[
                            'git', 'filter-branch', '-f',
                            '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}';GIT_COMMITTER_NAME='WebKit Committer';GIT_COMMITTER_EMAIL='committer@webkit.org'".format(date=date),
                            'HEAD...HEAD~4',
                        ],
                        ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Canonicalized commits')
        return self.run_step()

    def test_success_branch(self):
        self.setup_step(Canonicalize())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'safari-000-branch')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'security')

        gmtoffset = int(time.localtime().tm_gmtoff * 100 / (60 * 60))
        fixed_time = int(time.time())
        date = f'{int(time.time())} {gmtoffset}'
        time.time = lambda: fixed_time

        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'rm .git/identifiers.json || true'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['git', 'pull', 'security', 'safari-000-branch', '--rebase'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['git', 'branch', '-f', 'safari-000-branch', 'eng/pull-request-branch'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['git', 'checkout', '--progress', 'safari-000-branch'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['python3', 'Tools/Scripts/git-webkit', 'canonicalize', '-n', '1'],
            ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        env=self.ENV,
                        timeout=300,
                        command=[
                            'git', 'filter-branch', '-f',
                            '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}';GIT_COMMITTER_NAME='WebKit Committer';GIT_COMMITTER_EMAIL='committer@webkit.org'".format(date=date),
                            'HEAD...HEAD~1',
                        ],
                        ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Canonicalized commit')
        return self.run_step()

    def test_success_no_rebase(self):
        self.setup_step(Canonicalize(rebase_enabled=False))
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'origin')

        gmtoffset = int(time.localtime().tm_gmtoff * 100 / (60 * 60))
        fixed_time = int(time.time())
        date = f'{int(time.time())} {gmtoffset}'
        time.time = lambda: fixed_time

        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'rm .git/identifiers.json || true'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['python3', 'Tools/Scripts/git-webkit', 'canonicalize', '-n', '3'],
            ).exit(0),
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        env=self.ENV,
                        timeout=300,
                        command=[
                            'git', 'filter-branch', '-f',
                            '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}';GIT_COMMITTER_NAME='WebKit Committer';GIT_COMMITTER_EMAIL='committer@webkit.org'".format(date=date),
                            'HEAD...HEAD~1',
                        ],
                        ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Canonicalized commits')
        return self.run_step()

    def test_failure(self):
        self.setup_step(Canonicalize())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'origin')

        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'rm .git/identifiers.json || true'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['git', 'pull', 'origin', 'main', '--rebase'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['git', 'branch', '-f', 'main', 'eng/pull-request-branch'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['git', 'checkout', '--progress', 'main'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                log_environ=False,
                env=self.ENV,
                command=['python3', 'Tools/Scripts/git-webkit', 'canonicalize', '-n', '1'],
            ).exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to canonicalize commit')
        return self.run_step()


class TestPushPullRequestBranch(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_skipped_patch(self):
        self.setup_step(PushPullRequestBranch())
        self.setProperty('patch_id', '1234')
        self.expect_outcome(result=SKIPPED, state_string='finished (skipped)')
        return self.run_step()

    def test_success(self):
        GitHub.credentials = lambda user=None: ('webkit-commit-queue', 'password')
        self.setup_step(PushPullRequestBranch())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.repo.full_name', 'Contributor/WebKit')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=300,
                        env=dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password'),
                        command=['git', 'push', '-f', 'Contributor', 'HEAD:eng/pull-request-branch'])
            .exit(0)
            .log('stdio', stdout='To https://github.com/Contributor/WebKit.git\n37b7da95723b...9e2cb83b07b6 eng/pull-request-branch -> eng/pull-request-branch (forced update)\n'),
        )
        self.expect_outcome(result=SUCCESS, state_string='Pushed to pull request branch')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    def test_success_apple(self):
        GitHub.credentials = lambda user=None: ('webkit-commit-queue', 'password')
        self.setup_step(PushPullRequestBranch())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.repo.full_name', 'Contributor/WebKit-apple')
        self.setProperty('remote', 'apple')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=300,
                        env=dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password'),
                        command=['git', 'push', '-f', 'Contributor-apple', 'HEAD:eng/pull-request-branch'])
            .exit(0)
            .log('stdio', stdout='To https://github.com/Contributor/WebKit.git\n37b7da95723b...9e2cb83b07b6 eng/pull-request-branch -> eng/pull-request-branch (forced update)\n'),
        )
        self.expect_outcome(result=SUCCESS, state_string='Pushed to pull request branch')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    def test_success_integration(self):
        GitHub.credentials = lambda user=None: ('webkit-commit-queue', 'password')
        self.setup_step(PushPullRequestBranch())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.repo.full_name', 'WebKit/WebKit-integration')
        self.setProperty('remote', 'origin')
        self.setProperty('github.head.ref', 'integration/ci/1234')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=300,
                        env=dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password'),
                        command=['git', 'push', '-f', 'WebKit-integration', 'HEAD:integration/ci/1234'])
            .exit(0)
            .log('stdio', stdout='To https://github.com/Contributor/WebKit.git\n37b7da95723b...9e2cb83b07b6 eng/pull-request-branch -> eng/pull-request-branch (forced update)\n'),
        )
        self.expect_outcome(result=SUCCESS, state_string='Pushed to pull request branch')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    def test_failure(self):
        GitHub.credentials = lambda user=None: ('webkit-commit-queue', 'password')
        self.setup_step(PushPullRequestBranch())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.repo.full_name', 'Contributor/WebKit')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('build_summary', 'Test summary.')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=300,
                        env=dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password'),
                        command=['git', 'push', '-f', 'Contributor', 'HEAD:eng/pull-request-branch'])
            .exit(1)
            .log('stdio', stdout="fatal: could not read Username for 'https://github.com': Device not configured\n"),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to push to pull request branch')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()
        self.expect_property('build_summary', '')


class TestUpdatePullRequest(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_skipped_patch(self):
        self.setup_step(UpdatePullRequest())
        self.setProperty('patch_id', '1234')
        self.expect_outcome(result=SKIPPED, state_string="'git log ...' (skipped)")
        return self.run_step()

    def test_success(self):
        def update_pr(x, pr_number, title, description, base=None, head=None, repository_url=None):
            self.assertEqual(pr_number, '1234')
            self.assertEqual(title, '[Merge-Queue] Add http credential helper')
            self.assertEqual(base, 'main')
            self.assertEqual(head, 'JonWBedard:eng/pull-request-branch')

            self.assertEqual(
                description,
                '''#### 44a3b7100bd5dba51c57d874d3e89f89081e7886
<pre>
[Merge-Queue] Add http credential helper
<a href="https://bugs.webkit.org/show_bug.cgi?id=238553">https://bugs.webkit.org/show_bug.cgi?id=238553</a>
&lt;<a href="https://rdar.apple.com/problem/91044821">rdar://problem/91044821</a>&gt;

Reviewed by NOBODY (OOPS!).

* Tools/CISupport/ews-build/steps.py:
(CheckOutPullRequest.run): Add credential helper that pulls http credentials
from environment variables.
* Tools/CISupport/ews-build/steps_unittest.py:

Canonical link: <a href="https://commits.webkit.org/249006@main">https://commits.webkit.org/249006@main</a>
</pre>
''',
            )

            return defer.succeed(True)

        UpdatePullRequest.update_pr = update_pr
        self.setup_step(UpdatePullRequest())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.user.login', 'JonWBedard')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('github.base.ref', 'main')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=300,
                        command=['git', 'log', '--no-decorate', '-1'])
            .exit(0)
            .log('stdio', stdout='''commit 44a3b7100bd5dba51c57d874d3e89f89081e7886
Author: Jonathan Bedard <jbedard@apple.com>
Date:   Tue Mar 29 16:04:35 2022 -0700

    [Merge-Queue] Add http credential helper
    https://bugs.webkit.org/show_bug.cgi?id=238553
    <rdar://problem/91044821>

    Reviewed by NOBODY (OOPS!).

    * Tools/CISupport/ews-build/steps.py:
    (CheckOutPullRequest.run): Add credential helper that pulls http credentials
    from environment variables.
    * Tools/CISupport/ews-build/steps_unittest.py:

    Canonical link: https://commits.webkit.org/249006@main
'''),
        )
        self.expect_outcome(result=SUCCESS, state_string='Updated pull request')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = self.run_step()
            self.expect_property('bug_id', '238553')
            self.expect_property('is_test_gardening', False)
            return rc

    def test_bugs_url_with_angle_bracket(self):
        def update_pr(x, pr_number, title, description, base=None, head=None, repository_url=None):
            return defer.succeed(True)

        UpdatePullRequest.update_pr = update_pr
        self.setup_step(UpdatePullRequest())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.user.login', 'JonWBedard')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('github.base.ref', 'main')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=300,
                        command=['git', 'log', '--no-decorate', '-1'])
            .exit(0)
            .log('stdio', stdout='''commit 44a3b7100bd5dba51c57d874d3e89f89081e7886
Author: Jonathan Bedard <jbedard@apple.com>
Date:   Tue Mar 29 16:04:35 2022 -0700
    [Merge-Queue] Add http credential helper
    <https://bugs.webkit.org/show_bug.cgi?id=238553>
    <rdar://problem/91044821>

    Reviewed by NOBODY (OOPS!).

    * Tools/CISupport/ews-build/steps.py:
    Canonical link: https://commits.webkit.org/249006@main
'''),
        )
        self.expect_outcome(result=SUCCESS, state_string='Updated pull request')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = self.run_step()
            self.expect_property('bug_id', '238553')
            self.expect_property('is_test_gardening', False)
            return rc

    @defer.inlineCallbacks
    def test_success_gardening(self):
        def update_pr(x, pr_number, title, description, base=None, head=None, repository_url=None):
            self.assertEqual(pr_number, '1234')
            self.assertEqual(title, '[ macOS wk2 ] some/test/path.html a flaky failure')
            self.assertEqual(base, 'main')
            self.assertEqual(head, 'karlrackler:eng/pull-request-branch')

            self.assertEqual(
                description,
                '''#### 6a50b47fd71d922f753c06f46917086c839520b
<pre>
[ macOS wk2 ] some/test/path.html a flaky failure
<a href="https://bugs.webkit.org/show_bug.cgi?id=239577">https://bugs.webkit.org/show_bug.cgi?id=239577</a>

Unreviewed test gardening.

* LayoutTests/platform/mac-wk2/TestExpectations:

Canonical link: <a href="https://commits.webkit.org/249833@main">https://commits.webkit.org/249833@main</a>
</pre>
''',
            )

            return defer.succeed(True)

        UpdatePullRequest.update_pr = update_pr
        self.setup_step(UpdatePullRequest())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.user.login', 'karlrackler')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('github.base.ref', 'main')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=300,
                        command=['git', 'log', '--no-decorate', '-1'])
            .exit(0)
            .log('stdio', stdout='''commit 6a50b47fd71d922f753c06f46917086c839520b
Author: Karl Rackler <rackler@apple.com>
Date:   Thu Apr 21 00:25:03 2022 +0000

    [ macOS wk2 ] some/test/path.html a flaky failure
    https://bugs.webkit.org/show_bug.cgi?id=239577

    Unreviewed test gardening.

    * LayoutTests/platform/mac-wk2/TestExpectations:

    Canonical link: https://commits.webkit.org/249833@main
'''),
        )
        self.expect_outcome(result=SUCCESS, state_string='Updated pull request')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = yield self.run_step()
            self.expect_property('bug_id', '239577')
            self.expect_property('is_test_gardening', True)
            return rc

    @defer.inlineCallbacks
    def test_failure(self):
        def update_pr(x, pr_number, title, description, base=None, head=None, repository_url=None):
            return defer.succeed(False)

        UpdatePullRequest.update_pr = update_pr
        self.setup_step(UpdatePullRequest())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.user.login', 'JonWBedard')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('github.base.ref', 'main')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=300,
                        command=['git', 'log', '--no-decorate', '-1'])
            .exit(0)
            .log('stdio', stdout='''commit 44a3b7100bd5dba51c57d874d3e89f89081e7886
Author: Jonathan Bedard <jbedard@apple.com>
Date:   Tue Mar 29 16:04:35 2022 -0700

    [Merge-Queue] Add http credential helper
    https://bugs.webkit.org/show_bug.cgi?id=238553
    <rdar://problem/91044821>

    Reviewed by NOBODY (OOPS!).

    * Tools/CISupport/ews-build/steps.py:
    (CheckOutPullRequest.run): Add credential helper that pulls http credentials
    from environment variables.
    * Tools/CISupport/ews-build/steps_unittest.py:

    Canonical link: https://commits.webkit.org/249006@main
'''),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to update pull request')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = yield self.run_step()
            self.expect_property('bug_id', '238553')
            self.expect_property('is_test_gardening', False)
            return rc

    def test_success_series(self):

        def update_pr(x, pr_number, title, description, base=None, head=None, repository_url=None):
            self.assertEqual(pr_number, '1234')
            self.assertEqual(title, 'Cherry-pick 252432.1026@safari-7614-branch (2a8469e53b2f). rdar://107367418')
            self.assertEqual(base, 'main')
            self.assertEqual(head, 'JonWBedard:eng/pull-request-branch')

            self.assertEqual(
                description,
                '''#### 9140b95e718e7342366bbcdc29cb1ba0f9328422
<pre>
Cherry-pick 252432.1026@safari-7614-branch (2a8469e53b2f). <a href="https://rdar.apple.com/107367418">rdar://107367418</a>

    Remove inheritance of designMode attribute
    <a href="https://bugs.webkit.org/show_bug.cgi?id=248615">https://bugs.webkit.org/show_bug.cgi?id=248615</a>
    <a href="https://rdar.apple.com/102868995">rdar://102868995</a>

    Reviewed by Wenson Hsieh and Jonathan Bedard.

    Stop making design mode inherit across frame boundaries.

    This will prevent a form element from being injected into a victim page via drag &amp; drop
    and the new behavior matches that of Firefox and Chrome.

    * LayoutTests/editing/editability/design-mode-does-not-inherit-across-frames-expected.txt: Added.
    * LayoutTests/editing/editability/design-mode-does-not-inherit-across-frames.html: Added.
    * LayoutTests/fast/dom/HTMLElement/iscontenteditable-designmodeon-allinherit-subframe-expected.txt:
    * LayoutTests/fast/dom/HTMLElement/iscontenteditable-designmodeon-allinherit-subframe.html:
    * Source/WebCore/dom/Document.cpp:
    (WebCore::Document::setDesignMode):
    (WebCore::Document::inDesignMode const): Deleted.
    * Source/WebCore/dom/Document.h:
    (WebCore::Document::inDesignMode const):

    Canonical link: <a href="https://commits.webkit.org/252432.1026@safari-7614-branch">https://commits.webkit.org/252432.1026@safari-7614-branch</a>

Canonical link: <a href="https://commits.webkit.org/262299@main">https://commits.webkit.org/262299@main</a>
</pre>
----------------------------------------------------------------------
#### 6ec5319be307db36a27ea61d208cf68ce84abd67
<pre>
Cherry-pick 252432.1024@safari-7614-branch (2ea437d75522). <a href="https://rdar.apple.com/107367090">rdar://107367090</a>

    Use-after-free in ContactsManager::select
    <a href="https://bugs.webkit.org/show_bug.cgi?id=250351">https://bugs.webkit.org/show_bug.cgi?id=250351</a>
    <a href="https://rdar.apple.com/101241436">rdar://101241436</a>

    Reviewed by Wenson Hsieh and Jonathan Bedard.

    `ContactsManager` can be destroyed prior to receiving the user&apos;s selection, which
    is performed asynchronously. Deploy `WeakPtr` to avoid a use-after-free in this
    scenario.

    A test was unable to be added, as the failure scenario involves opening a new
    Window, using the new Window object&apos;s `navigator.contacts`, and performing user
    interaction. Creating a new Window results in the creation of a new web view,
    however all of our existing UIScriptController hooks only apply to the original
    (main) web view. Consequently, it is not possible to use our testing
    infrastructure to dismiss the contact picker and trigger the callback in the
    failure scenario.

    * Source/WebCore/Modules/contact-picker/ContactsManager.cpp:
    (WebCore::ContactsManager::select):
    * Source/WebCore/Modules/contact-picker/ContactsManager.h:

    Canonical link: <a href="https://commits.webkit.org/252432.1024@safari-7614-branch">https://commits.webkit.org/252432.1024@safari-7614-branch</a>

Canonical link: <a href="https://commits.webkit.org/262298@main">https://commits.webkit.org/262298@main</a>
</pre>
''',
            )

            return defer.succeed(True)

        UpdatePullRequest.update_pr = update_pr
        self.setup_step(UpdatePullRequest())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.user.login', 'JonWBedard')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('commit_count', 2)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=300,
                        command=['git', 'log', '--no-decorate', '-2'])
            .exit(0)
            .log('stdio', stdout='''commit 9140b95e718e7342366bbcdc29cb1ba0f9328422
Author: Jonathan Bedard <jbedard@apple.com>
Date:   Tue Mar 29 16:04:35 2023 -0700

    Cherry-pick 252432.1026@safari-7614-branch (2a8469e53b2f). rdar://107367418

        Remove inheritance of designMode attribute
        https://bugs.webkit.org/show_bug.cgi?id=248615
        rdar://102868995

        Reviewed by Wenson Hsieh and Jonathan Bedard.

        Stop making design mode inherit across frame boundaries.

        This will prevent a form element from being injected into a victim page via drag & drop
        and the new behavior matches that of Firefox and Chrome.

        * LayoutTests/editing/editability/design-mode-does-not-inherit-across-frames-expected.txt: Added.
        * LayoutTests/editing/editability/design-mode-does-not-inherit-across-frames.html: Added.
        * LayoutTests/fast/dom/HTMLElement/iscontenteditable-designmodeon-allinherit-subframe-expected.txt:
        * LayoutTests/fast/dom/HTMLElement/iscontenteditable-designmodeon-allinherit-subframe.html:
        * Source/WebCore/dom/Document.cpp:
        (WebCore::Document::setDesignMode):
        (WebCore::Document::inDesignMode const): Deleted.
        * Source/WebCore/dom/Document.h:
        (WebCore::Document::inDesignMode const):

        Canonical link: https://commits.webkit.org/252432.1026@safari-7614-branch

    Canonical link: https://commits.webkit.org/262299@main

commit 6ec5319be307db36a27ea61d208cf68ce84abd67
Author: Jonathan Bedard <jbedard@apple.com>
Date:   Tue Mar 29 16:04:35 2023 -0700

    Cherry-pick 252432.1024@safari-7614-branch (2ea437d75522). rdar://107367090

        Use-after-free in ContactsManager::select
        https://bugs.webkit.org/show_bug.cgi?id=250351
        rdar://101241436

        Reviewed by Wenson Hsieh and Jonathan Bedard.

        `ContactsManager` can be destroyed prior to receiving the user's selection, which
        is performed asynchronously. Deploy `WeakPtr` to avoid a use-after-free in this
        scenario.

        A test was unable to be added, as the failure scenario involves opening a new
        Window, using the new Window object's `navigator.contacts`, and performing user
        interaction. Creating a new Window results in the creation of a new web view,
        however all of our existing UIScriptController hooks only apply to the original
        (main) web view. Consequently, it is not possible to use our testing
        infrastructure to dismiss the contact picker and trigger the callback in the
        failure scenario.

        * Source/WebCore/Modules/contact-picker/ContactsManager.cpp:
        (WebCore::ContactsManager::select):
        * Source/WebCore/Modules/contact-picker/ContactsManager.h:

        Canonical link: https://commits.webkit.org/252432.1024@safari-7614-branch

    Canonical link: https://commits.webkit.org/262298@main
'''),
        )
        self.expect_outcome(result=SUCCESS, state_string='Updated pull request')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = self.run_step()
            self.expect_property('bug_id', '248615')
            self.expect_property('is_test_gardening', False)
            return rc


class TestScanBuild(BuildStepMixinAdditions, unittest.TestCase):
    WORK_DIR = 'wkdir'
    EXPECTED_BUILD_COMMAND = ['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'Tools/Scripts/build-and-analyze --output-dir wkdir/build/{SCAN_BUILD_OUTPUT_DIR} --configuration release --only-smart-pointers --analyzer-path=wkdir/llvm-project/build/bin/clang --scan-build-path=../llvm-project/clang/tools/scan-build/bin/scan-build --sdkroot=macosx --preprocessor-additions=CLANG_WEBKIT_BRANCH=1 2>&1 | python3 Tools/Scripts/filter-test-logs scan-build --output build-log.txt']

    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(ScanBuild())
        self.setProperty('configuration', 'release')
        self.setProperty('builddir', self.WORK_DIR)
        self.setProperty('fullPlatform', 'mac')
        self.setProperty('architecture', 'arm64')

    @expectedFailure
    def test_failure(self):
        self.configureStep()
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir=self.WORK_DIR,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'/bin/rm -rf wkdir/build/{SCAN_BUILD_OUTPUT_DIR}'],
                        log_environ=False,
                        timeout=2 * 60 * 60).exit(0),
            ExpectShell(workdir=self.WORK_DIR,
                        command=self.EXPECTED_BUILD_COMMAND,
                        log_environ=False,
                        timeout=2 * 60 * 60)
            .log('stdio', stdout='scan-build-static-analyzer: No bugs found.\nTotal issue count: 123\n')
            .exit(0)
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to build and analyze WebKit')
        rc = self.run_step()
        expected_steps = [
            GenerateS3URL('mac-arm64-release-scan-build', extension='txt', content_type='text/plain'),
            UploadFileToS3('build-log.txt', links={'scan-build': 'Full build log'}, content_type='text/plain'),
            ValidateChange(verifyBugClosed=False, addURLs=False),
            RevertAppliedChanges(exclude=['new*', 'scan-build-output*']),
            ScanBuildWithoutChange(analyze_safercpp_results=False)
        ]
        self.assertEqual(expected_steps, next_steps)
        return rc

    @expectedFailure
    def test_success(self):
        self.configureStep()
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir=self.WORK_DIR,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'/bin/rm -rf wkdir/build/{SCAN_BUILD_OUTPUT_DIR}'],
                        log_environ=False,
                        timeout=2 * 60 * 60)
            .exit(0),
            ExpectShell(workdir=self.WORK_DIR,
                        command=self.EXPECTED_BUILD_COMMAND,
                        log_environ=False,
                        timeout=2 * 60 * 60)
            .log('stdio', stdout='ANALYZE SUCCEEDED No issues found.\n')
            .exit(0)
        )
        self.expect_outcome(result=SUCCESS, state_string='Found 0 issues')
        rc = self.run_step()
        expected_steps = [
            GenerateS3URL('mac-arm64-release-scan-build', extension='txt', content_type='text/plain'),
            UploadFileToS3('build-log.txt', links={'scan-build': 'Full build log'}, content_type='text/plain'),
            ParseStaticAnalyzerResults(),
            FindUnexpectedStaticAnalyzerResults()
        ]
        self.assertEqual(expected_steps, next_steps)
        return rc

    @expectedFailure
    def test_success_with_issues(self):
        self.configureStep()
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir=self.WORK_DIR,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'/bin/rm -rf wkdir/build/{SCAN_BUILD_OUTPUT_DIR}'],
                        log_environ=False,
                        timeout=2 * 60 * 60)
            .exit(0),
            ExpectShell(workdir=self.WORK_DIR,
                        command=self.EXPECTED_BUILD_COMMAND,
                        log_environ=False,
                        timeout=2 * 60 * 60)
            .log('stdio', stdout='ANALYZE SUCCEEDED\n Total issue count: 300\n')
            .exit(0)
        )
        self.expect_outcome(result=SUCCESS, state_string='Found 300 issues')
        rc = self.run_step()
        expected_steps = [
            GenerateS3URL('mac-arm64-release-scan-build', extension='txt', content_type='text/plain'),
            UploadFileToS3('build-log.txt', links={'scan-build': 'Full build log'}, content_type='text/plain'),
            ParseStaticAnalyzerResults(),
            FindUnexpectedStaticAnalyzerResults()
        ]
        self.assertEqual(expected_steps, next_steps)
        return rc


class TestScanBuildWithoutChange(BuildStepMixinAdditions, unittest.TestCase):
    WORK_DIR = 'wkdir'
    EXPECTED_BUILD_COMMAND = ['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'Tools/Scripts/build-and-analyze --output-dir wkdir/build/{SCAN_BUILD_OUTPUT_DIR}-baseline --configuration release --only-smart-pointers --analyzer-path=wkdir/llvm-project/build/bin/clang --scan-build-path=../llvm-project/clang/tools/scan-build/bin/scan-build --sdkroot=macosx --preprocessor-additions=CLANG_WEBKIT_BRANCH=1 2>&1 | python3 Tools/Scripts/filter-test-logs scan-build --output build-log.txt']

    def setUp(self):
        self.maxDiff = None

        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self, analyze_safercpp_results=True):
        self.setup_step(ScanBuildWithoutChange(analyze_safercpp_results=analyze_safercpp_results))
        self.setProperty('configuration', 'release')
        self.setProperty('builddir', self.WORK_DIR)
        self.setProperty('fullPlatform', 'mac')
        self.setProperty('architecture', 'arm64')

    @expectedFailure
    def test_failure_no_analyze(self):
        self.configureStep(analyze_safercpp_results=False)
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir=self.WORK_DIR,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'/bin/rm -rf wkdir/build/{SCAN_BUILD_OUTPUT_DIR}-baseline'],
                        log_environ=False,
                        timeout=2 * 60 * 60).exit(0),
            ExpectShell(workdir=self.WORK_DIR,
                        command=self.EXPECTED_BUILD_COMMAND,
                        log_environ=False,
                        timeout=2 * 60 * 60)
            .log('stdio', stdout='ANALYZE FAILED\nNo issues found.')
            .exit(0)
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to build and analyze WebKit')
        rc = self.run_step()
        expected_steps = [
            GenerateS3URL('mac-arm64-release-scan-build-without-change', extension='txt', content_type='text/plain'),
            UploadFileToS3('build-log.txt', links={'scan-build-without-change': 'Full build log'}, content_type='text/plain'),
        ]
        self.assertEqual(expected_steps, next_steps)
        return rc

    @expectedFailure
    def test_success_with_issues(self):
        self.configureStep()
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir=self.WORK_DIR,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'/bin/rm -rf wkdir/build/{SCAN_BUILD_OUTPUT_DIR}-baseline'],
                        log_environ=False,
                        timeout=2 * 60 * 60)
            .exit(0),
            ExpectShell(workdir=self.WORK_DIR,
                        command=self.EXPECTED_BUILD_COMMAND,
                        log_environ=False,
                        timeout=2 * 60 * 60)
            .log('stdio', stdout='ANALYZE SUCCEEDED\n Total issue count: 300\n')
            .exit(0)
        )
        self.expect_outcome(result=SUCCESS, state_string='Found 300 issues')
        rc = self.run_step()
        expected_steps = [
            GenerateS3URL('mac-arm64-release-scan-build-without-change', extension='txt', content_type='text/plain'),
            UploadFileToS3('build-log.txt', links={'scan-build-without-change': 'Full build log'}, content_type='text/plain'),
            ParseStaticAnalyzerResultsWithoutChange(),
            FindUnexpectedStaticAnalyzerResultsWithoutChange()
        ]
        self.assertEqual(expected_steps, next_steps)
        return rc


class TestParseStaticAnalyzerResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(ParseStaticAnalyzerResults())

    def test_success(self):
        self.configureStep()
        self.setProperty('builddir', 'wkdir')
        self.setProperty('buildnumber', 1234)

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/generate-dirty-files', f'wkdir/build/{SCAN_BUILD_OUTPUT_DIR}', '--output-dir', 'wkdir/build/new', '--build-dir', 'wkdir/build'])
            .log('stdio', stdout='Total (24247) WebKit (327) WebCore (23920)\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string=' Issues: Total (24247) WebKit (327) WebCore (23920)')
        return self.run_step()


class TestFindModifiedSaferCPPExpectations(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(FindModifiedSaferCPPExpectations())

    def test_success(self):
        self.configureStep()
        self.setProperty('builddir', 'wkdir')
        self.setProperty('buildnumber', 1234)

        commit_diff = '''
diff --git a/Source/WebCore/SaferCPPExpectations/NoUncountedMemberCheckerExpectations b/Source/WebCore/SaferCPPExpectations/NoUncountedMemberCheckerExpectations
--- a/Source/WebCore/SaferCPPExpectations/NoUncountedMemberCheckerExpectations
+++ b/Source/WebCore/SaferCPPExpectations/NoUncountedMemberCheckerExpectations
-css/ShorthandSerializer.cpp
@@ -53,6 +52,7 @@ inspector/InspectorStyleSheet.cpp
 inspector/agents/worker/WorkerAuditAgent.h
+inspector/agents/worker/WorkerWorkerAgent.h
diff --git a/Source/WebCore/SaferCPPExpectations/UncountedCallArgsCheckerExpectations b/Source/WebCore/SaferCPPExpectations/UncountedCallArgsCheckerExpectations
--- a/Source/WebCore/SaferCPPExpectations/UncountedCallArgsCheckerExpectations
+++ b/Source/WebCore/SaferCPPExpectations/UncountedCallArgsCheckerExpectations
@@ -17,7 +17,6 @@ Modules/WebGPU/GPUExternalTexture.cpp
-Modules/WebGPU/GPUQueue.cpp
 Modules/WebGPU/GPURenderBundle.cpp
'''
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'diff', 'HEAD~1', '--', '*Expectations'])
            .log('stdio', stdout=commit_diff)
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Found modified expectations')
        rc = self.run_step()
        self.expect_property('user_added_tests', ['WebCore/inspector/agents/worker/WorkerWorkerAgent.h/NoUncountedMemberChecker'])
        self.expect_property('user_removed_tests', ['WebCore/css/ShorthandSerializer.cpp/NoUncountedMemberChecker', 'WebCore/Modules/WebGPU/GPUQueue.cpp/UncountedCallArgsChecker'])
        return rc

    def test_replace(self):
        self.configureStep()
        self.setProperty('builddir', 'wkdir')
        self.setProperty('buildnumber', 1234)

        commit_diff = '''
diff --git a/Source/WebCore/SaferCPPExpectations/NoUncountedMemberCheckerExpectations b/Source/WebCore/SaferCPPExpectations/NoUncountedMemberCheckerExpectations
index 8a2d2375b8d2..f7ebc3b11b94 100644
--- a/Source/WebCore/SaferCPPExpectations/NoUncountedMemberCheckerExpectations
+++ b/Source/WebCore/SaferCPPExpectations/NoUncountedMemberCheckerExpectations
@@ -51,7 +51,7 @@ inspector/InspectorShaderProgram.h
 inspector/InspectorStyleSheet.cpp
 inspector/InspectorStyleSheet.h
 inspector/InspectorWebAgentBase.h
-inspector/agents/worker/WorkerAuditAgent.h
+inspector/agents/worker/WorkerWorkerAgent.h
 layout/formattingContexts/inline/InlineItemsBuilder.h
 loader/appcache/ApplicationCacheStorage.cpp
 page/FrameSnapshotting.cpp
'''
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'diff', 'HEAD~1', '--', '*Expectations'])
            .log('stdio', stdout=commit_diff)
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Found modified expectations')
        rc = self.run_step()
        self.expect_property('user_added_tests', ['WebCore/inspector/agents/worker/WorkerWorkerAgent.h/NoUncountedMemberChecker'])
        self.expect_property('user_removed_tests', ['WebCore/inspector/agents/worker/WorkerAuditAgent.h/NoUncountedMemberChecker'])
        return rc

    @expectedFailure
    def test_unmodified(self):
        self.configureStep()
        self.setProperty('builddir', 'wkdir')
        self.setProperty('buildnumber', 1234)

        commit_diff = ''
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'diff', 'HEAD~1', '--', '*Expectations'])
            .log('stdio', stdout=commit_diff)
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='No modified expectations')
        rc = self.run_step()
        self.expect_property('user_added_tests', None)
        self.expect_property('user_removed_tests', None)
        return rc

    @expectedFailure
    def test_failure(self):
        self.configureStep()
        self.setProperty('builddir', 'wkdir')
        self.setProperty('buildnumber', 1234)

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['git', 'diff', 'HEAD~1', '--', '*Expectations'])
            .log('stdio', stdout='Failure')
            .exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='Unable to find modified expectations')
        rc = self.run_step()
        self.expect_property('user_added_tests', None)
        self.expect_property('user_removed_tests', None)
        return rc


class TestFindUnexpectedStaticAnalyzerResults(BuildStepMixinAdditions, unittest.TestCase):
    command = ['python3', 'Tools/Scripts/compare-static-analysis-results', 'wkdir/build/new', '--build-output', SCAN_BUILD_OUTPUT_DIR, '--archived-dir', 'wkdir/build/baseline', '--scan-build-path', '../llvm-project/clang/tools/scan-build/bin/scan-build']
    upload_options = ['--builder-name', 'Safer-CPP-Checks', '--build-number', 1234, '--buildbot-worker', 'ews123', '--buildbot-master', EWS_BUILD_HOSTNAMES[0], '--report', 'https://results.webkit.org/']
    configuration = ['--architecture', 'arm64', '--platform', 'mac', '--version', '14.6.1', '--version-name', 'Sonoma', '--style', 'release', '--sdk', '23G93']
    jsonFileName = f'{SCAN_BUILD_OUTPUT_DIR}/unexpected_results.json'

    def setUp(self):
        os.environ['RESULTS_SERVER_API_KEY'] = 'test-api-key'
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self, use_expectations, was_filtered=False):
        if use_expectations:
            self.setup_step(FindUnexpectedStaticAnalyzerResults(was_filtered=was_filtered))
        else:
            self.setup_step(FindUnexpectedStaticAnalyzerResultsWithoutChange(was_filtered=was_filtered))
        FindUnexpectedStaticAnalyzerResults._get_patch = lambda x: b'Test patch 123'
        FindUnexpectedStaticAnalyzerResults.decode_results_data = lambda self: {'passes': {'project': {'checker': ['filename']}}, 'failures': {'project': {'checker': ['filename2']}}}
        self.setProperty('builddir', 'wkdir')
        self.setProperty('buildnumber', 1234)
        self.setProperty('architecture', 'arm64')
        self.setProperty('platform', 'mac')
        self.setProperty('os_version', '14.6.1')
        self.setProperty('os_name', 'Sonoma')
        self.setProperty('configuration', 'release')
        self.setProperty('build_version', '23G93')
        self.setProperty('got_revision', '1234567')
        self.setProperty('branch', 'main')
        self.setProperty('buildername', 'Safer-CPP-Checks')
        self.setProperty('workername', 'ews123')
        self.setProperty('builddir', 'wkdir')
        self.setProperty('buildnumber', 1234)
        self.setProperty('change_id', 1234)

    def test_success_no_issues(self):
        self.configureStep(False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        logfiles={'json': self.jsonFileName},
                        command=self.command + self.upload_options + self.configuration,
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'})
            .log('stdio', stdout='')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Found no unexpected results')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()

    @expectedFailure
    def test_new_issues(self):
        self.configureStep(False)
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        logfiles={'json': self.jsonFileName},
                        command=self.command + self.upload_options + self.configuration,
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'})
            .log('stdio', stdout='Total new issues: 19\nTotal fixed files: 3\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='19 new issues 3 fixed files')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = self.run_step()
        self.assertEqual([DeleteStaticAnalyzerResults(), ArchiveStaticAnalyzerResults(), UploadStaticAnalyzerResults(), ExtractStaticAnalyzerTestResults(), DisplaySaferCPPResults()], next_steps)
        return rc

    @expectedFailure
    def test_with_expectations_results_failure(self):
        self.configureStep(True)
        FindUnexpectedStaticAnalyzerResults.filter_results_using_results_db = lambda self, logText: False
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        logfiles={'json': self.jsonFileName},
                        command=['python3', 'Tools/Scripts/compare-static-analysis-results', 'wkdir/build/new', '--build-output', SCAN_BUILD_OUTPUT_DIR, '--check-expectations'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'})
            .log('stdio', stdout='Total new issues: 19\nTotal fixed files: 3\n')
            .exit(0)
        )
        self.expect_outcome(result=SUCCESS, state_string='19 new issues 3 fixed files')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = self.run_step()
        self.assertEqual([ValidateChange(verifyBugClosed=False, addURLs=False), RevertAppliedChanges(exclude=['new*', 'scan-build-output*']), ScanBuildWithoutChange()], next_steps)
        return rc

    @expectedFailure
    def test_with_expectations_results_success_unexpected(self):
        self.configureStep(True, was_filtered=True)
        FindUnexpectedStaticAnalyzerResults.filter_results_using_results_db = lambda self, logText: True
        FindUnexpectedStaticAnalyzerResults.write_unexpected_results_file_to_master = lambda self: None
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        logfiles={'json': self.jsonFileName},
                        command=['python3', 'Tools/Scripts/compare-static-analysis-results', 'wkdir/build/new', '--build-output', SCAN_BUILD_OUTPUT_DIR, '--check-expectations'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'})
            .log('stdio', stdout='Total new issues: 19\nTotal new files: 3\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='3 failing files ')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = self.run_step()
        self.assertEqual([DownloadUnexpectedResultsFromMaster(), DeleteStaticAnalyzerResults(results_dir='StaticAnalyzerUnexpectedRegressions'), GenerateSaferCPPResultsIndex(), DeleteStaticAnalyzerResults(), ArchiveStaticAnalyzerResults(), UploadStaticAnalyzerResults(), ExtractStaticAnalyzerTestResults(), DisplaySaferCPPResults()], next_steps)
        return rc

    @expectedFailure
    def test_with_expectations_results_success_without_unexpected(self):
        self.configureStep(True)
        FindUnexpectedStaticAnalyzerResults.filter_results_using_results_db = lambda self, logText: True
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        logfiles={'json': self.jsonFileName},
                        command=['python3', 'Tools/Scripts/compare-static-analysis-results', 'wkdir/build/new', '--build-output', SCAN_BUILD_OUTPUT_DIR, '--check-expectations'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'})
            .log('stdio', stdout='Total new issues: 19\nTotal fixed files: 3\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='19 new issues 3 fixed files')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = self.run_step()
        self.assertEqual([GenerateSaferCPPResultsIndex(), DeleteStaticAnalyzerResults(), ArchiveStaticAnalyzerResults(), UploadStaticAnalyzerResults(), ExtractStaticAnalyzerTestResults(), DisplaySaferCPPResults()], next_steps)
        return rc

    @expectedFailure
    def test_changed_expectations_match(self):
        self.configureStep(True)
        FindUnexpectedStaticAnalyzerResults.decode_results_data = lambda self: {'passes': {'WebCore': {'NoUncountedMemberChecker': ['css/ShorthandSerializer.cpp']}}, 'failures': {'WebCore': {'NoUncountedMemberChecker': ['inspector/agents/worker/WorkerWorkerAgent.h']}}}
        self.setProperty('user_removed_tests', ['WebCore/inspector/agents/worker/WorkerWorkerAgent.h/NoUncountedMemberChecker'])
        self.setProperty('user_added_tests', ['WebCore/css/ShorthandSerializer.cpp/NoUncountedMemberChecker'])
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        logfiles={'json': self.jsonFileName},
                        command=['python3', 'Tools/Scripts/compare-static-analysis-results', 'wkdir/build/new', '--build-output', SCAN_BUILD_OUTPUT_DIR, '--check-expectations'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'})
            .log('stdio', stdout='Total new issues: 1\nTotal fixed files: 1\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='1 new issue 1 fixed file')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = self.run_step()
        self.assertEqual([GenerateSaferCPPResultsIndex(), DeleteStaticAnalyzerResults(), ArchiveStaticAnalyzerResults(), UploadStaticAnalyzerResults(), ExtractStaticAnalyzerTestResults(), DisplaySaferCPPResults()], next_steps)
        return rc

    def test_changed_expectations_no_match(self):
        self.configureStep(True)
        FindUnexpectedStaticAnalyzerResults.decode_results_data = lambda self: {'passes': {'WebCore': {'NoUncountedMemberChecker': ['css/ShorthandSerializer.cpp']}}, 'failures': {'WebCore': {'NoUncountedMemberChecker': ['inspector/agents/worker/WorkerWorkerAgent.h']}}}
        self.setProperty('user_removed_tests', [])
        self.setProperty('user_added_tests', ['WebCore/css/ShorthandSerializer.cpp/NoUncountedMemberChecker'])
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        logfiles={'json': self.jsonFileName},
                        command=['python3', 'Tools/Scripts/compare-static-analysis-results', 'wkdir/build/new', '--build-output', SCAN_BUILD_OUTPUT_DIR, '--check-expectations'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'})
            .log('stdio', stdout='Total new issues: 1\nTotal fixed files: 2\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='1 new issue 2 fixed files')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            return self.run_step()
        self.assertEqual([ValidateChange(verifyBugClosed=False, addURLs=False), RevertAppliedChanges(exclude=['new*', 'scan-build-output*']), ScanBuildWithoutChange()], next_steps)
        return rc

    @expectedFailure
    def test_second_pass_changed_expectations(self):
        self.configureStep(False)
        FindUnexpectedStaticAnalyzerResults.decode_results_data = lambda self: {'passes': {'WebCore': {'NoUncountedMemberChecker': []}}, 'failures': {'WebCore': {'NoUncountedMemberChecker': []}}}
        self.setProperty('test_failures', ['WebCore/inspector/agents/worker/WorkerWorkerAgent.h/NoUncountedMemberChecker'])
        self.setProperty('test_passes', [])
        self.setProperty('user_removed_tests', ['WebCore/inspector/agents/worker/WorkerWorkerAgent.h/NoUncountedMemberChecker'])
        self.setProperty('user_added_tests', [])
        self._expected_uploaded_files = ['public_html/results/Safer-CPP-Checks/1234-1234/unexpected_results.json']
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        logfiles={'json': self.jsonFileName},
                        command=self.command + self.upload_options + self.configuration,
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'})
            .log('stdio', stdout='No unexpected results\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='1 failing file ')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = self.run_step()
        self.assertEqual([DownloadUnexpectedResultsFromMaster(), DeleteStaticAnalyzerResults(results_dir='StaticAnalyzerUnexpectedRegressions'), GenerateSaferCPPResultsIndex(), DeleteStaticAnalyzerResults(), ArchiveStaticAnalyzerResults(), UploadStaticAnalyzerResults(), ExtractStaticAnalyzerTestResults(), DisplaySaferCPPResults()], next_steps)
        return rc

    def test_second_pass_no_results(self):
        self.configureStep(False)
        FindUnexpectedStaticAnalyzerResults.decode_results_data = lambda self: {'passes': {'WebCore': {'NoUncountedMemberChecker': []}}, 'failures': {'WebCore': {'NoUncountedMemberChecker': ['css/ShorthandSerializer.cpp']}}}
        self.setProperty('user_added_tests', ['WebCore/css/ShorthandSerializer.cpp/NoUncountedMemberChecker'])
        next_steps = []
        self._expected_uploaded_files = ['public_html/results/Safer-CPP-Checks/1234-1234/unexpected_results.json']
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        logfiles={'json': self.jsonFileName},
                        command=self.command + self.upload_options + self.configuration,
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'})
            .log('stdio', stdout='Total failing files: 1\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Found no unexpected results')
        with current_hostname(EWS_BUILD_HOSTNAMES[0]):
            rc = self.run_step()
        self.assertEqual([], next_steps)
        return rc


class TestDownloadUnexpectedResultsfromMaster(BuildStepMixinAdditions, unittest.TestCase):
    READ_LIMIT = 1000

    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(DownloadUnexpectedResultsFromMaster(mastersrc=__file__))
        self.setProperty('buildername', 'Safer-CPP-EWS')
        self.setProperty('change_id', '123456')
        self.setProperty('buildnumber', '123')

    @staticmethod
    def downloadFileRecordingContents(limit, recorder):
        def behavior(command):
            reader = command.args['reader']
            data = reader.remote_read(limit)
            recorder(data)
            reader.remote_close()
        return behavior

    @defer.inlineCallbacks
    @expectedFailure
    def test_success(self):
        self.configureStep()
        self.expect_hidden(False)
        buf = []
        self.expectRemoteCommands(
            Expect('downloadFile', dict(
                workerdest=f'{SCAN_BUILD_OUTPUT_DIR}/unexpected_results.json', workdir='wkdir',
                blocksize=1024 * 256, maxsize=None, mode=0o0644,
                reader=ExpectRemoteRef(remotetransfer.FileReader),
            ))
            + Expect.behavior(self.downloadFileRecordingContents(self.READ_LIMIT, buf.append))
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='downloading to unexpected_results.json')

        yield self.run_step()

        buf = b''.join(buf)
        self.assertEqual(len(buf), self.READ_LIMIT)
        with open(__file__, 'rb') as masterFile:
            data = masterFile.read(self.READ_LIMIT)
            if data != buf:
                self.assertEqual(buf, data)


class TestDeleteStaticAnalyzerResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(DeleteStaticAnalyzerResults())
        self.setProperty('builddir', 'wkdir')

    def test_success(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['rm', '-r', os.path.join(self.getProperty('builddir'), f'build/{SCAN_BUILD_OUTPUT_DIR}/StaticAnalyzer')])
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='deleted static analyzer results')
        return self.run_step()


class TestGenerateSaferCPPResultsIndex(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        os.environ['RESULTS_SERVER_API_KEY'] = 'test-api-key'
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(GenerateSaferCPPResultsIndex())
        self.setProperty('builddir', 'wkdir')

    def test_success(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/compare-static-analysis-results', 'wkdir/build/new', '--build-output', SCAN_BUILD_OUTPUT_DIR, '--scan-build-path', '../llvm-project/clang/tools/scan-build/bin/scan-build', '--generate-results-only'])
            .log('stdio', stdout='scan-build: 1 bug found\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='generated safer cpp results')
        rc = self.run_step()
        self.expect_property('num_unexpected_issues', 1)
        return rc


class TestDisplaySaferCPPResults(BuildStepMixinAdditions, unittest.TestCase):
    HEADER = '### Safer C++ Build [#123](http://localhost:8080/#/builders/1/builds/13) (https://github.com/WebKit/WebKit/commit/7e4dc83588490a785f71acac4724e4e43a705077)\n'

    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.maxDiff = None
        self.setup_step(DisplaySaferCPPResults())
        self.setProperty('buildnumber', '123')
        self.setProperty('github.number', '17')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7e4dc83588490a785f71acac4724e4e43a705077')

        def loadResultsData(self, path):
            return {
                "passes": {
                    "WebCore": {
                        "NoUncountedMemberChecker": [],
                        "RefCntblBaseVirtualDtor": [],
                        "UncountedCallArgsChecker": [],
                        "UncountedLocalVarsChecker": []
                    },
                    "WebKit": {
                        "NoUncountedMemberChecker": [],
                        "RefCntblBaseVirtualDtor": ['File17.cpp'],
                        "UncountedCallArgsChecker": [],
                        "UncountedLocalVarsChecker": []
                    }
                },
                "failures": {
                    "WebCore": {
                        "NoUncountedMemberChecker": ['File1.cpp'],
                        "RefCntblBaseVirtualDtor": [],
                        "UncountedCallArgsChecker": [],
                        "UncountedLocalVarsChecker": []
                    },
                    "WebKit": {
                        "NoUncountedMemberChecker": [],
                        "RefCntblBaseVirtualDtor": [],
                        "UncountedCallArgsChecker": [],
                        "UncountedLocalVarsChecker": []
                    }
                }
            }

        DisplaySaferCPPResults.loadResultsData = loadResultsData

    def test_success_preexisting_failures(self):
        self.configureStep()
        self.setProperty('num_unexpected_issues', 10)
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))

        self.expect_outcome(result=SUCCESS, state_string='Ignored 10 pre-existing failures')
        rc = self.run_step()
        self.expect_property('build_summary', 'Ignored 10 pre-existing failures')
        self.expect_property('comment_text', None)
        self.assertEqual([], next_steps)
        return rc

    def test_success_only_fixes(self):
        self.configureStep()
        self.setProperty('num_passing_files', 1)
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))

        self.expect_outcome(result=SUCCESS, state_string='Found 1 fixed file: File17.cpp')
        rc = self.run_step()
        self.expect_property('passes', ['File17.cpp'])
        expected_comment = self.HEADER + "\n:warning: Found 1 fixed file! Please update expectations in `Source/[Project]/SaferCPPExpectations` by running the following command and update your pull request:\n"
        expected_comment += "- `Tools/Scripts/update-safer-cpp-expectations -p WebKit --RefCntblBaseVirtualDtor File17.cpp`"
        self.expect_property('comment_text', expected_comment)
        self.expect_property('build_summary', 'Found 1 fixed file: File17.cpp')
        self.assertEqual([LeaveComment(), SetBuildSummary()], next_steps)
        return rc

    def test_failure_new_failures(self):
        self.configureStep()
        self.setProperty('num_unexpected_issues', 10)
        self.setProperty('num_failing_files', 1)
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))

        self.expect_outcome(result=FAILURE, state_string='Found 10 new failures in File1.cpp')
        rc = self.run_step()
        expected_comment = self.HEADER + ":x: Found [1 failing file with 10 issues](https://ews-build.s3-us-west-2.amazonaws.com/None/None-123/scan-build-output/new-results.html). "
        expected_comment += "Please address these issues before landing. See [WebKit Guidelines for Safer C++ Programming](https://github.com/WebKit/WebKit/wiki/Safer-CPP-Guidelines).\n(cc @rniwa)\n"
        self.expect_property('comment_text', expected_comment)
        self.expect_property('build_finish_summary', 'Found 10 new failures in File1.cpp')
        self.assertEqual([LeaveComment(), SetBuildSummary()], next_steps)
        return rc

    def test_failure_mixed(self):
        self.configureStep()
        self.setProperty('num_unexpected_issues', 10)
        self.setProperty('num_passing_files', 1)
        self.setProperty('num_failing_files', 1)
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))

        self.expect_outcome(result=FAILURE, state_string='Found 10 new failures in File1.cpp and found 1 fixed file: File17.cpp')
        rc = self.run_step()
        expected_comment = self.HEADER + ":x: Found [1 failing file with 10 issues](https://ews-build.s3-us-west-2.amazonaws.com/None/None-123/scan-build-output/new-results.html). "
        expected_comment += "Please address these issues before landing. See [WebKit Guidelines for Safer C++ Programming](https://github.com/WebKit/WebKit/wiki/Safer-CPP-Guidelines).\n(cc @rniwa)\n"
        expected_comment += "\n:warning: Found 1 fixed file! Please update expectations in `Source/[Project]/SaferCPPExpectations` by running the following command and update your pull request:\n"
        expected_comment += '- `Tools/Scripts/update-safer-cpp-expectations -p WebKit --RefCntblBaseVirtualDtor File17.cpp`'
        self.expect_property('comment_text', expected_comment)
        self.expect_property('build_finish_summary', 'Found 10 new failures in File1.cpp')
        self.assertEqual([LeaveComment(), SetBuildSummary()], next_steps)


if __name__ == '__main__':
    unittest.main()
