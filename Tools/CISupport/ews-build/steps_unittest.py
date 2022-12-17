# Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
import operator
import os
import shutil
import sys
import tempfile
import time

from buildbot.process import remotetransfer
from buildbot.process.results import Results, SUCCESS, FAILURE, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.test.fake.remotecommand import Expect, ExpectRemoteRef, ExpectShell
from buildbot.test.util.misc import TestReactorMixin
from buildbot.test.util.steps import BuildStepMixin
from buildbot.util import identifiers as buildbot_identifiers
from datetime import date
from mock import call, patch
from twisted.internet import defer, error, reactor
from twisted.python import failure, log
from twisted.trial import unittest
import send_email

from steps import (AddReviewerToCommitMessage, AnalyzeAPITestsResults, AnalyzeCompileWebKitResults,
                   AnalyzeJSCTestsResults, AnalyzeLayoutTestsResults, ApplyPatch, ApplyWatchList, ArchiveBuiltProduct, ArchiveTestResults, BugzillaMixin,
                   Canonicalize, CheckOutPullRequest, CheckOutSource, CheckOutSpecificRevision, CheckChangeRelevance, CheckStatusOnEWSQueues, CheckStyle,
                   CleanBuild, CleanUpGitIndexLock, CleanGitRepo, CleanWorkingDirectory, CompileJSC, CommitPatch, CompileJSCWithoutChange,
                   CompileWebKit, CompileWebKitWithoutChange, ConfigureBuild, ConfigureBuild, Contributors,
                   DeleteStaleBuildFiles, DetermineLandedIdentifier, DownloadBuiltProduct, DownloadBuiltProductFromMaster,
                   EWS_BUILD_HOSTNAME, ExtractBuiltProduct, ExtractTestResults,
                   FetchBranches, FindModifiedLayoutTests, GitHub, GitHubMixin,
                   InstallBuiltProduct, InstallGtkDependencies, InstallWpeDependencies,
                   KillOldProcesses, PrintConfiguration, PushCommitToWebKitRepo, PushPullRequestBranch, ReRunAPITests, ReRunWebKitPerlTests,
                   MapBranchAlias, ReRunWebKitTests, RevertPullRequestChanges, RunAPITests, RunAPITestsWithoutChange, RunBindingsTests, RunBuildWebKitOrgUnitTests,
                   RunBuildbotCheckConfigForBuildWebKit, RunBuildbotCheckConfigForEWS, RunEWSUnitTests, RunResultsdbpyTests,
                   RunJavaScriptCoreTests, RunJSCTestsWithoutChange, RunWebKit1Tests, RunWebKitPerlTests, RunWebKitPyPython2Tests,
                   RunWebKitPyPython3Tests, RunWebKitTests, RunWebKitTestsInStressMode, RunWebKitTestsInStressGuardmallocMode,
                   RunWebKitTestsWithoutChange, RunWebKitTestsRedTree, RunWebKitTestsRepeatFailuresRedTree, RunWebKitTestsRepeatFailuresWithoutChangeRedTree,
                   RunWebKitTestsWithoutChangeRedTree, AnalyzeLayoutTestsResultsRedTree, TestWithFailureCount, ShowIdentifier,
                   Trigger, TransferToS3, TwistedAdditions, UnApplyPatch, UpdatePullRequest, UpdateWorkingDirectory, UploadBuiltProduct,
                   UploadTestResults, ValidateCommitMessage, ValidateCommitterAndReviewer, ValidateChange, ValidateRemote, ValidateSquashed)

# Workaround for https://github.com/buildbot/buildbot/issues/4669
from buildbot.test.fake.fakebuild import FakeBuild
FakeBuild.addStepsAfterCurrentStep = lambda FakeBuild, step_factories: None
FakeBuild._builderid = 1

def mock_step(step, logs='', results=SUCCESS, stopped=False, properties=None):
    step.logs = logs
    step.results = results
    step.stopped = stopped
    return step


def mock_load_contributors(*args, **kwargs):
    return {
        'reviewer@apple.com': {'name': 'WebKit Reviewer', 'status': 'reviewer', 'email': 'reviewer@apple.com'},
        'webkit-reviewer': {'name': 'WebKit Reviewer', 'status': 'reviewer', 'email': 'reviewer@apple.com'},
        'WebKit Reviewer': {'status': 'reviewer'},
        'committer@webkit.org': {'name': 'WebKit Committer', 'status': 'committer', 'email': 'committer@webkit.org'},
        'webkit-commit-queue': {'name': 'WebKit Committer', 'status': 'committer', 'email': 'committer@webkit.org'},
        'WebKit Committer': {'status': 'committer'},
        'Myles C. Maxfield': {'status': 'reviewer'},
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
    def setUpBuildStep(self):
        self.patch(reactor, 'spawnProcess', lambda *args, **kwargs: self._checkSpawnProcess(*args, **kwargs))
        self.patch(send_email, 'send_email', self._send_email)
        self.patch(send_email, 'get_email_ids', lambda c: ['test@webkit.org'])
        self.patch(BugzillaMixin, 'get_bugzilla_api_key', lambda f: 'TEST-API-KEY')
        self._emails_list = []
        self._expected_local_commands = []
        self.setUpTestReactor()

        self._temp_directory = tempfile.mkdtemp()
        os.chdir(self._temp_directory)
        self._expected_uploaded_files = []

        super(BuildStepMixinAdditions, self).setUpBuildStep()

    def tearDownBuildStep(self):
        shutil.rmtree(self._temp_directory)
        super(BuildStepMixinAdditions, self).tearDownBuildStep()

    def fakeBuildFinished(self, text, results):
        self.build.text = text
        self.build.results = results

    def setupStep(self, step, *args, **kwargs):
        self.previous_steps = kwargs.get('previous_steps') or []
        if self.previous_steps:
            del kwargs['previous_steps']

        super(BuildStepMixinAdditions, self).setupStep(step, *args, **kwargs)
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
        self.properties.setProperty(name, value, source)

    def getProperty(self, name):
        return self.properties.getProperty(name)

    def expectAddedURLs(self, added_urls):
        self._expected_added_urls = added_urls

    def expectUploadedFile(self, path):
        self._expected_uploaded_files.append(path)

    def expectLocalCommands(self, *expected_commands):
        self._expected_local_commands.extend(expected_commands)

    def expectRemoteCommands(self, *expected_commands):
        self.expectCommands(*expected_commands)

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

    def runStep(self):
        def check(result):
            self.assertEqual(self._expected_local_commands, [], 'assert all expected local commands were run')
            self.expectAddedURLs(self._expected_added_urls)
            self.assertEqual(self._added_files(), self._expected_uploaded_files)
            if self._expected_sources is not None:
                # Convert to dictionaries because assertEqual() only knows how to diff Python built-in types.
                actual_sources = sorted([source.asDict() for source in self.build.sources], key=operator.itemgetter('codebase'))
                expected_sources = sorted([source.asDict() for source in self._expected_sources], key=operator.itemgetter('codebase'))
                self.assertEqual(actual_sources, expected_sources)
        deferred_result = super(BuildStepMixinAdditions, self).runStep()
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

    def test_no_reviewers(self):
        logs = dict(stdio=[])
        mixin = GitHubMixin()
        mixin.fetch_data_from_url_with_authentication_github = lambda url: self.Response.fromJson([])
        mixin._addToLog = lambda logName, message, logs=logs: logs[logName].append(message)
        self.assertEqual(mixin.get_reviewers(1234), [])
        self.assertEqual(logs, dict(stdio=[]))

    def test_single_review(self):
        logs = dict(stdio=[])
        mixin = GitHubMixin()
        mixin.fetch_data_from_url_with_authentication_github = lambda url: self.Response.fromJson([
            dict(id=1, state='APPROVED', user=dict(login='webkit-reviewer')),
        ], url=url)
        mixin._addToLog = lambda logName, message, logs=logs: logs[logName].append(message)
        self.assertEqual(mixin.get_reviewers(1234), ['webkit-reviewer'])
        self.assertEqual(logs, dict(stdio=[]))

    def test_multipe_reviews(self):
        logs = dict(stdio=[])
        mixin = GitHubMixin()
        mixin.fetch_data_from_url_with_authentication_github = lambda url: self.Response.fromJson([
            dict(id=1, state='APPROVED', user=dict(login='webkit-reviewer')),
            dict(id=2, state='COMMENTED', user=dict(login='webkit-committer')),
            dict(id=3, state='APPROVED', user=dict(login='webkit-committer')),
        ], url=url)
        mixin._addToLog = lambda logName, message, logs=logs: logs[logName].append(message)
        self.assertEqual(mixin.get_reviewers(1234), ['webkit-committer', 'webkit-reviewer'])
        self.assertEqual(logs, dict(stdio=[]))

    def test_retracted_review(self):
        logs = dict(stdio=[])
        mixin = GitHubMixin()
        mixin.fetch_data_from_url_with_authentication_github = lambda url: self.Response.fromJson([
            dict(id=1, state='APPROVED', user=dict(login='webkit-reviewer')),
            dict(id=2, state='CHANGES_REQUESTED', user=dict(login='webkit-reviewer')),
        ], url=url)
        mixin._addToLog = lambda logName, message, logs=logs: logs[logName].append(message)
        self.assertEqual(mixin.get_reviewers(1234), [])
        self.assertEqual(logs, dict(stdio=[]))

    def test_pagination(self):
        logs = dict(stdio=[])
        mixin = GitHubMixin()
        mixin.fetch_data_from_url_with_authentication_github = lambda url: self.Response.fromJson([
            dict(id=101, state='APPROVED', user=dict(login='webkit-committer')),
        ], url=url) if 'page=2' in url else self.Response.fromJson([
            dict(id=1, state='APPROVED', user=dict(login='webkit-reviewer')),
        ] + [
            dict(id=i, state='COMMENTED', user=dict(login='webkit-reviewer')) for i in range(1, 100)
        ], url=url)
        mixin._addToLog = lambda logName, message, logs=logs: logs[logName].append(message)
        self.assertEqual(mixin.get_reviewers(1234), ['webkit-committer', 'webkit-reviewer'])
        self.assertEqual(logs, dict(stdio=[]))

    def test_reviewers_invalid_response(self):
        logs = dict(stdio=[])
        mixin = GitHubMixin()
        mixin.fetch_data_from_url_with_authentication_github = lambda url: self.Response.fromJson({}, url=url)
        mixin._addToLog = lambda logName, message, logs=logs: logs[logName].append(message)
        self.assertEqual(mixin.get_reviewers(1234), [])
        self.assertEqual(logs, dict(stdio=[]))

    def test_reviewers_error(self):
        logs = dict(stdio=[])
        mixin = GitHubMixin()
        mixin.fetch_data_from_url_with_authentication_github = lambda url: None
        mixin._addToLog = lambda logName, message, logs=logs: logs[logName].append(message)
        self.assertEqual(mixin.get_reviewers(1234), [])
        self.assertEqual(logs, dict(stdio=[]))


class TestStepNameShouldBeValidIdentifier(BuildStepMixinAdditions, unittest.TestCase):
    def test_step_names_are_valid(self):
        import steps
        build_step_classes = inspect.getmembers(steps, inspect.isclass)
        for build_step in build_step_classes:
            if 'name' in vars(build_step[1]):
                name = build_step[1].name
                self.assertFalse(' ' in name, f'step name "{name}" contain space.')
                self.assertTrue(buildbot_identifiers.ident_re.match(name), f'step name "{name}" is not a valid buildbot identifier.')


class TestCheckStyle(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success_internal(self):
        self.setupStep(CheckStyle())
        self.setProperty('try-codebase', 'internal')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/check-webkit-style'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='check-webkit-style')
        return self.runStep()

    def test_failure_unknown_try_codebase(self):
        self.setupStep(CheckStyle())
        self.setProperty('try-codebase', 'foo')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/check-webkit-style'],
                        )
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='check-webkit-style (failure)')
        return self.runStep()

    def test_failures_with_style_issues(self):
        self.setupStep(CheckStyle())
        self.setProperty('try-codebase', 'internal')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/check-webkit-style'],
                        )
            + ExpectShell.log('stdio', stdout='''ERROR: Source/WebCore/layout/FloatingContext.cpp:36:  Code inside a namespace should not be indented.  [whitespace/indent] [4]
ERROR: Source/WebCore/layout/FormattingContext.h:94:  Weird number of spaces at line-start.  Are you using a 4-space indent?  [whitespace/indent] [3]
ERROR: Source/WebCore/layout/LayoutContext.cpp:52:  Place brace on its own line for function definitions.  [whitespace/braces] [4]
ERROR: Source/WebCore/layout/LayoutContext.cpp:55:  Extra space before last semicolon. If this should be an empty statement, use { } instead.  [whitespace/semicolon] [5]
ERROR: Source/WebCore/layout/LayoutContext.cpp:60:  Tab found; better to use spaces  [whitespace/tab] [1]
ERROR: Source/WebCore/layout/Verification.cpp:88:  Missing space before ( in while(  [whitespace/parens] [5]
Total errors found: 8 in 48 files''')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='8 style errors')
        return self.runStep()

    def test_failures_no_style_issues(self):
        self.setupStep(CheckStyle())
        self.setProperty('try-codebase', 'internal')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/check-webkit-style'],
                        )
            + ExpectShell.log('stdio', stdout='Total errors found: 0 in 6 files')
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='check-webkit-style')
        return self.runStep()

    def test_failures_no_changes(self):
        self.setupStep(CheckStyle())
        self.setProperty('try-codebase', 'internal')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/check-webkit-style'],
                        )
            + ExpectShell.log('stdio', stdout='Total errors found: 0 in 0 files')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='check-webkit-style (failure)')
        return self.runStep()


class TestApplyWatchList(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(ApplyWatchList())
        self.setProperty('bug_id', '1234')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=120,
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/webkit-patch', 'apply-watchlist-local', '1234'])
            + ExpectShell.log('stdio', stdout='Result of watchlist: cc "" messages ""')
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Applied WatchList')
        return self.runStep()

    def test_failure(self):
        self.setupStep(ApplyWatchList())
        self.setProperty('bug_id', '1234')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=120,
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/webkit-patch', 'apply-watchlist-local', '1234'])
            + ExpectShell.log('stdio', stdout='Unexpected failure')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to apply watchlist')
        return self.runStep()


class TestRunBindingsTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'bindings_test_results.json'
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunBindingsTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-bindings-tests', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed bindings tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunBindingsTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-bindings-tests', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='FAIL: (JS) JSTestInterface.cpp')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='bindings-tests (failure)')
        return self.runStep()


class TestRunWebKitPerlTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(RunWebKitPerlTests())

    def test_success(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/test-webkitperl'],
                        timeout=120,
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed webkitperl tests')
        return self.runStep()

    def test_failure(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/test-webkitperl'],
                        timeout=120,
                        )
            + ExpectShell.log('stdio', stdout='''Failed tests:  1-3, 5-7, 9, 11-13
Files=40, Tests=630,  4 wallclock secs ( 0.16 usr  0.09 sys +  2.78 cusr  0.64 csys =  3.67 CPU)
Result: FAIL
Failed 1/40 test programs. 10/630 subtests failed.''')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed webkitperl tests')
        return self.runStep()


class TestWebKitPyPython2Tests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'webkitpy_test_python2_results.json'
        self.json_with_failure = '''{"failures": [{"name": "webkitpy.port.wpe_unittest.WPEPortTest.test_diff_image"}]}\n'''
        self.json_with_errros = '''{"failures": [],
"errors": [{"name": "webkitpy.style.checkers.cpp_unittest.WebKitStyleTest.test_os_version_checks"}, {"name": "webkitpy.port.win_unittest.WinPortTest.test_diff_image__missing_actual"}]}\n'''
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunWebKitPyPython2Tests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed webkitpy python2 tests')
        return self.runStep()

    def test_unexpected_failure(self):
        self.setupStep(RunWebKitPyPython2Tests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        )
            + ExpectShell.log('stdio', stdout='''Ran 1744 tests in 5.913s
FAILED (failures=1, errors=0)''')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='webkitpy-tests (failure)')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunWebKitPyPython2Tests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        ) +
            ExpectShell.log('json', stdout=self.json_with_failure) +
            2,
        )
        self.expectOutcome(result=FAILURE, state_string='Found 1 webkitpy python2 test failure: webkitpy.port.wpe_unittest.WPEPortTest.test_diff_image')
        return self.runStep()

    def test_errors(self):
        self.setupStep(RunWebKitPyPython2Tests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        ) +
            ExpectShell.log('json', stdout=self.json_with_errros) +
            2,
        )
        self.expectOutcome(result=FAILURE, state_string='Found 2 webkitpy python2 test failures: webkitpy.style.checkers.cpp_unittest.WebKitStyleTest.test_os_version_checks, webkitpy.port.win_unittest.WinPortTest.test_diff_image__missing_actual')
        return self.runStep()

    def test_lot_of_failures(self):
        self.setupStep(RunWebKitPyPython2Tests())
        json_with_failures = json.dumps({"failures": [{"name": f'test{i}'} for i in range(1, 31)]})

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        ) +
            ExpectShell.log('json', stdout=json_with_failures) +
            2,
        )
        self.expectOutcome(result=FAILURE, state_string='Found 30 webkitpy python2 test failures: test1, test2, test3, test4, test5, test6, test7, test8, test9, test10 ...')
        return self.runStep()


class TestWebKitPyPython3Tests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'webkitpy_test_python3_results.json'
        self.json_with_failure = '''{"failures": [{"name": "webkitpy.port.wpe_unittest.WPEPortTest.test_diff_image"}]}\n'''
        self.json_with_errros = '''{"failures": [],
"errors": [{"name": "webkitpy.style.checkers.cpp_unittest.WebKitStyleTest.test_os_version_checks"}, {"name": "webkitpy.port.win_unittest.WinPortTest.test_diff_image__missing_actual"}]}\n'''
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunWebKitPyPython3Tests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed webkitpy python3 tests')
        return self.runStep()

    def test_unexpected_failure(self):
        self.setupStep(RunWebKitPyPython3Tests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        )
            + ExpectShell.log('stdio', stdout='''Ran 1744 tests in 5.913s
FAILED (failures=1, errors=0)''')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='webkitpy-tests (failure)')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunWebKitPyPython3Tests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        ) +
            ExpectShell.log('json', stdout=self.json_with_failure) +
            2,
        )
        self.expectOutcome(result=FAILURE, state_string='Found 1 webkitpy python3 test failure: webkitpy.port.wpe_unittest.WPEPortTest.test_diff_image')
        return self.runStep()

    def test_errors(self):
        self.setupStep(RunWebKitPyPython3Tests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        ) +
            ExpectShell.log('json', stdout=self.json_with_errros) +
            2,
        )
        self.expectOutcome(result=FAILURE, state_string='Found 2 webkitpy python3 test failures: webkitpy.style.checkers.cpp_unittest.WebKitStyleTest.test_os_version_checks, webkitpy.port.win_unittest.WinPortTest.test_diff_image__missing_actual')
        return self.runStep()

    def test_lot_of_failures(self):
        self.setupStep(RunWebKitPyPython3Tests())
        json_with_failures = json.dumps({'failures': [{f'name': f'test{i}'} for i in range(1, 31)]})

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        ) +
            ExpectShell.log('json', stdout=json_with_failures) +
            2,
        )
        self.expectOutcome(result=FAILURE, state_string='Found 30 webkitpy python3 test failures: test1, test2, test3, test4, test5, test6, test7, test8, test9, test10 ...')
        return self.runStep()


class TestRunBuildbotCheckConfigForEWS(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunBuildbotCheckConfigForEWS())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport/ews-build',
                        timeout=120,
                        logEnviron=False,
                        command=['buildbot', 'checkconfig'],
                        env={'LC_CTYPE': 'en_US.UTF-8'}
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed buildbot checkconfig')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunBuildbotCheckConfigForEWS())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport/ews-build',
                        timeout=120,
                        logEnviron=False,
                        command=['buildbot', 'checkconfig'],
                        env={'LC_CTYPE': 'en_US.UTF-8'}
                        )
            + ExpectShell.log('stdio', stdout='Configuration Errors:  builder(s) iOS-14-Debug-Build-EWS have no schedulers to drive them')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed buildbot checkconfig')
        return self.runStep()


class TestRunBuildbotCheckConfigForBuildWebKit(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunBuildbotCheckConfigForBuildWebKit())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport/build-webkit-org',
                        timeout=120,
                        logEnviron=False,
                        command=['buildbot', 'checkconfig'],
                        env={'LC_CTYPE': 'en_US.UTF-8'}
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed buildbot checkconfig')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunBuildbotCheckConfigForBuildWebKit())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport/build-webkit-org',
                        timeout=120,
                        logEnviron=False,
                        command=['buildbot', 'checkconfig'],
                        env={'LC_CTYPE': 'en_US.UTF-8'}
                        )
            + ExpectShell.log('stdio', stdout='Configuration Errors:  builder(s) Apple-iOS-14-Release-Build have no schedulers to drive them')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed buildbot checkconfig')
        return self.runStep()


class TestRunEWSUnitTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunEWSUnitTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport',
                        timeout=120,
                        logEnviron=False,
                        command=['python3', 'runUnittests.py', 'ews-build'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed EWS unit tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunEWSUnitTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport',
                        timeout=120,
                        logEnviron=False,
                        command=['python3', 'runUnittests.py', 'ews-build'],
                        )
            + ExpectShell.log('stdio', stdout='Unhandled Error. Traceback (most recent call last): Keys in cmd missing from expectation: [logfiles.json]')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed EWS unit tests')
        return self.runStep()


class TestRunResultsdbpyTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunResultsdbpyTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=120,
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/libraries/resultsdbpy/resultsdbpy/run-tests', '--verbose', '--no-selenium', '--fast-tests'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed resultsdbpy unit tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunResultsdbpyTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=120,
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/libraries/resultsdbpy/resultsdbpy/run-tests', '--verbose', '--no-selenium', '--fast-tests'],
                        )
            + ExpectShell.log('stdio', stdout='FAILED (errors=5, skipped=224)')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed resultsdbpy unit tests')
        return self.runStep()


class TestRunBuildWebKitOrgUnitTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunBuildWebKitOrgUnitTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport',
                        timeout=120,
                        logEnviron=False,
                        command=['python3', 'runUnittests.py', 'build-webkit-org'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed build.webkit.org unit tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunBuildWebKitOrgUnitTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/CISupport',
                        timeout=120,
                        logEnviron=False,
                        command=['python3', 'runUnittests.py', 'build-webkit-org'],
                        )
            + ExpectShell.log('stdio', stdout='Unhandled Error. Traceback (most recent call last): Keys in cmd missing from expectation: [logfiles.json]')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed build.webkit.org unit tests')
        return self.runStep()


class TestKillOldProcesses(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(KillOldProcesses())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/kill-old-processes', 'buildbot'],
                        logEnviron=False,
                        timeout=120,
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Killed old processes')
        return self.runStep()

    def test_failure(self):
        self.setupStep(KillOldProcesses())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/kill-old-processes', 'buildbot'],
                        logEnviron=False,
                        timeout=120,
                        )
            + ExpectShell.log('stdio', stdout='Unexpected error.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to kill old processes')
        return self.runStep()


class TestCleanBuild(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(CleanBuild())
        self.setProperty('fullPlatform', 'ios-11')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/clean-build', '--platform=ios-11', '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Deleted WebKitBuild directory')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CleanBuild())
        self.setProperty('fullPlatform', 'ios-simulator-11')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/clean-build', '--platform=ios-simulator-11', '--debug'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected error.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Deleted WebKitBuild directory (failure)')
        return self.runStep()


class TestCleanUpGitIndexLock(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(CleanUpGitIndexLock())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=120,
                        logEnviron=False,
                        command=['rm', '-f', '.git/index.lock'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Deleted .git/index.lock')
        return self.runStep()

    def test_success_windows(self):
        self.setupStep(CleanUpGitIndexLock())
        self.setProperty('platform', 'win')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=120,
                        logEnviron=False,
                        command=['rm', '-f', '.git/index.lock'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Deleted .git/index.lock')
        return self.runStep()

    def test_success_wincairo(self):
        self.setupStep(CleanUpGitIndexLock())
        self.setProperty('platform', 'wincairo')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=120,
                        logEnviron=False,
                        command=['del', r'.git\index.lock'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Deleted .git/index.lock')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CleanUpGitIndexLock())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=120,
                        logEnviron=False,
                        command=['rm', '-f', '.git/index.lock'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected error.')
            + 1,
        )
        self.expectOutcome(result=FAILURE, state_string='Deleted .git/index.lock (failure)')
        return self.runStep()


class TestInstallGtkDependencies(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(InstallGtkDependencies())
        self.setProperty('configuration', 'release')
        self.assertEqual(InstallGtkDependencies.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/update-webkitgtk-libs', '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Updated gtk dependencies')
        return self.runStep()

    def test_failure(self):
        self.setupStep(InstallGtkDependencies())
        self.setProperty('configuration', 'release')
        self.assertEqual(InstallGtkDependencies.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/update-webkitgtk-libs', '--release'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected error.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Updated gtk dependencies (failure)')
        return self.runStep()


class TestInstallWpeDependencies(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(InstallWpeDependencies())
        self.setProperty('configuration', 'release')
        self.assertEqual(InstallWpeDependencies.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/update-webkitwpe-libs', '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Updated wpe dependencies')
        return self.runStep()

    def test_failure(self):
        self.setupStep(InstallWpeDependencies())
        self.setProperty('configuration', 'release')
        self.assertEqual(InstallWpeDependencies.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/update-webkitwpe-libs', '--release'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected error.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Updated wpe dependencies (failure)')
        return self.runStep()


class TestCompileWebKit(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(CompileWebKit())
        self.setProperty('fullPlatform', 'ios-simulator-11')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=1800,
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-webkit', '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Compiled WebKit')
        return self.runStep()

    def test_success_gtk(self):
        self.setupStep(CompileWebKit())
        self.setProperty('platform', 'gtk')
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=1800,
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-webkit', '--release', '--prefix=/app/webkit/WebKitBuild/release/install', '--gtk'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Compiled WebKit')
        return self.runStep()

    def test_success_wpe(self):
        self.setupStep(CompileWebKit())
        self.setProperty('platform', 'wpe')
        self.setProperty('fullPlatform', 'wpe')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=1800,
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-webkit', '--release', '--wpe'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Compiled WebKit')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CompileWebKit())
        self.setProperty('fullPlatform', 'mac-bigsur')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=3600,  # only Big Sur uses an 3600 timeout
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-webkit', '--debug'],
                        )
            + ExpectShell.log('stdio', stdout='1 error generated.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to compile WebKit')
        return self.runStep()

    def test_skip_for_revert_patches_on_commit_queue(self):
        self.setupStep(CompileWebKit())
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('configuration', 'debug')
        self.setProperty('fast_commit_queue', True)
        self.expectOutcome(result=SKIPPED, state_string='Skipped compiling WebKit in fast-cq mode')
        return self.runStep()


class TestCompileWebKitWithoutChange(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(CompileWebKitWithoutChange())
        self.setProperty('fullPlatform', 'ios-simulator-11')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=1800,
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-webkit', '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Compiled WebKit')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CompileWebKitWithoutChange())
        self.setProperty('fullPlatform', 'mac-bigsur')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=3600,  # only Big Sur uses an 3600 timeout
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-webkit', '--debug'],
                        )
            + ExpectShell.log('stdio', stdout='1 error generated.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to compile WebKit')
        return self.runStep()


class TestAnalyzeCompileWebKitResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_patch_with_build_failure(self):
        previous_steps = [
            mock_step(CompileWebKit(), results=FAILURE),
            mock_step(CompileWebKitWithoutChange(), results=SUCCESS),
        ]
        self.setupStep(AnalyzeCompileWebKitResults(), previous_steps=previous_steps)
        self.setProperty('patch_id', '1234')
        self.expectOutcome(result=FAILURE, state_string='Patch 1234 does not build (failure)')
        rc = self.runStep()
        self.assertEqual(self.getProperty('comment_text'), None)
        self.assertEqual(self.getProperty('build_finish_summary'), 'Patch 1234 does not build')
        return rc

    def test_pull_request_with_build_failure(self):
        previous_steps = [
            mock_step(CompileWebKit(), results=FAILURE),
            mock_step(CompileWebKitWithoutChange(), results=SUCCESS),
        ]
        self.setupStep(AnalyzeCompileWebKitResults(), previous_steps=previous_steps)
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expectOutcome(result=FAILURE, state_string='Hash 7496f8ec for PR 1234 does not build (failure)')
        rc = self.runStep()
        self.assertEqual(self.getProperty('build_finish_summary'), 'Hash 7496f8ec for PR 1234 does not build')
        return rc

    def test_patch_with_build_failure_on_commit_queue(self):
        previous_steps = [
            mock_step(CompileWebKit(), results=FAILURE),
            mock_step(CompileWebKitWithoutChange(), results=SUCCESS),
        ]
        self.setupStep(AnalyzeCompileWebKitResults(), previous_steps=previous_steps)
        self.setProperty('patch_id', '1234')
        self.setProperty('buildername', 'commit-queue')
        self.expectOutcome(result=FAILURE, state_string='Patch 1234 does not build (failure)')
        rc = self.runStep()
        self.assertEqual(self.getProperty('comment_text'), 'Patch 1234 does not build')
        self.assertEqual(self.getProperty('build_finish_summary'), 'Patch 1234 does not build')
        return rc

    def test_patch_with_trunk_failure(self):
        previous_steps = [
            mock_step(CompileWebKit(), results=FAILURE),
            mock_step(CompileWebKitWithoutChange(), results=FAILURE),
        ]
        self.setupStep(AnalyzeCompileWebKitResults(), previous_steps=previous_steps)
        self.expectOutcome(result=FAILURE, state_string='Unable to build WebKit without patch, retrying build (failure)')
        return self.runStep()

    def test_pr_with_main_failure(self):
        previous_steps = [
            mock_step(CompileWebKit(), results=FAILURE),
            mock_step(CompileWebKitWithoutChange(), results=FAILURE),
        ]
        self.setupStep(AnalyzeCompileWebKitResults(), previous_steps=previous_steps)
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.expectOutcome(result=FAILURE, state_string='Unable to build WebKit without PR, retrying build (failure)')
        return self.runStep()

    def test_pr_with_branch_failure(self):
        previous_steps = [
            mock_step(CompileWebKit(), results=FAILURE),
            mock_step(CompileWebKitWithoutChange(), results=FAILURE),
        ]
        self.setupStep(AnalyzeCompileWebKitResults(), previous_steps=previous_steps)
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'safari-7614-branch')
        self.expectOutcome(result=FAILURE, state_string='Unable to build WebKit without PR, please check manually (failure)')
        return self.runStep()

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
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(CompileJSC())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=1800,
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-jsc', '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Compiled JSC')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CompileJSC())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=1800,
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-jsc', '--debug'],
                        )
            + ExpectShell.log('stdio', stdout='1 error generated.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to compile JSC')
        return self.runStep()


class TestCompileJSCWithoutChange(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(CompileJSCWithoutChange())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=1800,
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-jsc', '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Compiled JSC')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CompileJSCWithoutChange())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=1800,
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-jsc', '--debug'],
                        )
            + ExpectShell.log('stdio', stdout='1 error generated.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to compile JSC')
        return self.runStep()


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
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self, platform=None, fullPlatform=None, configuration=None):
        self.setupStep(RunJavaScriptCoreTests())
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
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', f'--json-output={self.jsonFileName}', '--release'] + self.command_extra,
                        logfiles={'json': self.jsonFileName},
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed JSC tests')
        return self.runStep()

    def test_remote_success(self):
        self.configureStep(platform='jsc-only', fullPlatform='jsc-only', configuration='release')
        self.setProperty('remotes', 'remote-machines.json')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', f'--json-output={self.jsonFileName}', '--release', '--remote-config-file=remote-machines.json', '--no-testmasm', '--no-testair', '--no-testb3', '--no-testdfg', '--no-testapi', '--memory-limited', '--verbose', '--jsc-only'] + self.command_extra,
                        logfiles={'json': self.jsonFileName},
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed JSC tests')
        return self.runStep()

    def test_failure(self):
        self.configureStep(platform='mac', fullPlatform='mac-highsierra', configuration='debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', f'--json-output={self.jsonFileName}', '--debug'] + self.command_extra,
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='9 failures found.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='jscore-tests (failure)')
        return self.runStep()

    def test_single_stress_test_failure(self):
        self.configureStep(platform='mac', fullPlatform='mac-highsierra', configuration='debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        logfiles={'json': self.jsonFileName},
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', f'--json-output={self.jsonFileName}', '--debug'] + self.command_extra,
                        )
            + 2
            + ExpectShell.log('json', stdout=self.jsc_single_stress_test_failure),
        )
        self.expectOutcome(result=FAILURE, state_string='Found 1 jsc stress test failure: stress/switch-on-char-llint-rope.js.dfg-eager')
        rc = self.runStep()
        self.assertEqual(self.getProperty(self.prefix + 'stress_test_failures'), ['stress/switch-on-char-llint-rope.js.dfg-eager'])
        self.assertEqual(self.getProperty(self.prefix + 'binary_failures'), None)
        return rc

    def test_lot_of_stress_test_failure(self):
        self.configureStep(platform='mac', fullPlatform='mac-highsierra', configuration='debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        logfiles={'json': self.jsonFileName},
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', f'--json-output={self.jsonFileName}', '--debug'] + self.command_extra,
                        )
            + 2
            + ExpectShell.log('json', stdout=self.jsc_multiple_stress_test_failures),
        )
        self.expectOutcome(result=FAILURE, state_string='Found 14 jsc stress test failures: stress/switch-on-char-llint-rope.js.dfg-eager, stress/switch-on-char-llint-rope.js.dfg-eager-no-cjit-validate, stress/switch-on-char-llint-rope.js.eager-jettison-no-cjit, stress/switch-on-char-llint-rope.js.ftl-eager, stress/switch-on-char-llint-rope.js.ftl-eager-no-cjit ...')
        rc = self.runStep()
        self.assertEqual(self.getProperty(self.prefix + 'stress_test_failures'), ["stress/switch-on-char-llint-rope.js.dfg-eager", "stress/switch-on-char-llint-rope.js.dfg-eager-no-cjit-validate", "stress/switch-on-char-llint-rope.js.eager-jettison-no-cjit", "stress/switch-on-char-llint-rope.js.ftl-eager", "stress/switch-on-char-llint-rope.js.ftl-eager-no-cjit", "stress/switch-on-char-llint-rope.js.ftl-eager-no-cjit-b3o1", "stress/switch-on-char-llint-rope.js.ftl-no-cjit-b3o0", "stress/switch-on-char-llint-rope.js.ftl-no-cjit-no-inline-validate", "stress/switch-on-char-llint-rope.js.ftl-no-cjit-no-put-stack-validate", "stress/switch-on-char-llint-rope.js.ftl-no-cjit-small-pool", "stress/switch-on-char-llint-rope.js.ftl-no-cjit-validate-sampling-profiler", "stress/switch-on-char-llint-rope.js.no-cjit-collect-continuously", "stress/switch-on-char-llint-rope.js.no-cjit-validate-phases", "stress/switch-on-char-llint-rope.js.no-ftl"])
        self.assertEqual(self.getProperty(self.prefix + 'binary_failures'), None)
        return rc

    def test_masm_failure(self):
        self.configureStep(platform='mac', fullPlatform='mac-highsierra', configuration='debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        logfiles={'json': self.jsonFileName},
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', f'--json-output={self.jsonFileName}', '--debug'] + self.command_extra,
                        )
            + 2
            + ExpectShell.log('json', stdout=self.jsc_masm_failure),
        )
        self.expectOutcome(result=FAILURE, state_string='JSC test binary failure: testmasm')
        rc = self.runStep()
        self.assertEqual(self.getProperty(self.prefix + 'stress_test_failures'), None)
        self.assertEqual(self.getProperty(self.prefix + 'binary_failures'), ['testmasm'])
        return rc

    def test_b3_and_stress_test_failure(self):
        self.configureStep(platform='mac', fullPlatform='mac-highsierra', configuration='release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        logfiles={'json': self.jsonFileName},
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', f'--json-output={self.jsonFileName}', '--release'] + self.command_extra,
                        )
            + 2
            + ExpectShell.log('json', stdout=self.jsc_b3_and_stress_test_failure),
        )
        self.expectOutcome(result=FAILURE, state_string='Found 1 jsc stress test failure: stress/weakset-gc.js, JSC test binary failure: testb3')
        rc = self.runStep()
        self.assertEqual(self.getProperty(self.prefix + 'stress_test_failures'), ['stress/weakset-gc.js'])
        self.assertEqual(self.getProperty(self.prefix + 'binary_failures'), ['testb3'])
        return rc

    def test_dfg_air_and_stress_test_failure(self):
        self.configureStep(platform='jsc-only', fullPlatform='jsc-only', configuration='release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        logfiles={'json': self.jsonFileName},
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', f'--json-output={self.jsonFileName}', '--release', '--memory-limited', '--verbose', '--jsc-only'] + self.command_extra,
                        )
            + 2
            + ExpectShell.log('json', stdout=self.jsc_dfg_air_and_stress_test_failure),
        )
        self.expectOutcome(result=FAILURE, state_string='Found 1 jsc stress test failure: stress/weakset-gc.js, JSC test binary failures: testair, testdfg')
        rc = self.runStep()
        self.assertEqual(self.getProperty(self.prefix + 'stress_test_failures'), ['stress/weakset-gc.js'])
        self.assertEqual(self.getProperty(self.prefix + 'binary_failures'), ['testair', 'testdfg'])
        return rc

    def test_success_with_flaky(self):
        self.configureStep(platform='jsc-only', fullPlatform='jsc-only', configuration='release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        logfiles={'json': self.jsonFileName},
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', f'--json-output={self.jsonFileName}', '--release', '--memory-limited', '--verbose', '--jsc-only'] + self.command_extra,
                        )
            + 0
            + ExpectShell.log('json', stdout=self.jsc_passed_with_flaky),
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed JSC tests (1 flaky)')
        rc = self.runStep()
        self.assertEqual(self.getProperty(self.prefix + 'flaky_and_passed'), {'stress/switch-on-char-llint-rope.js.default': {'P': '7', 'T': '10'}})
        self.assertEqual(self.getProperty(self.prefix + 'stress_test_failures'), None)
        self.assertEqual(self.getProperty(self.prefix + 'binary_test_failures'), None)
        return rc


class TestRunJSCTestsWithoutChange(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'jsc_results.json'
        self.command_extra = RunJSCTestsWithoutChange.command_extra
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunJSCTestsWithoutChange())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', f'--json-output={self.jsonFileName}', '--release'] + self.command_extra,
                        logfiles={'json': self.jsonFileName},
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='jscore-tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunJSCTestsWithoutChange())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', f'--json-output={self.jsonFileName}', '--debug'] + self.command_extra,
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='9 failures found.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='jscore-tests (failure)')
        return self.runStep()


class TestAnalyzeJSCTestsResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(AnalyzeJSCTestsResults())
        self.setProperty('jsc_stress_test_failures', [])
        self.setProperty('jsc_binary_failures', [])
        self.setProperty('jsc_flaky_and_passed', {})
        self.setProperty('jsc_clean_tree_stress_test_failures', [])
        self.setProperty('jsc_clean_tree_binary_failures', [])
        self.setProperty('jsc_clean_tree_flaky_and_passed', {})

    def test_single_new_stress_failure(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', ['stress/force-error.js.bytecode-cache'])
        self.expectOutcome(result=FAILURE, state_string='Found 1 new JSC stress test failure: stress/force-error.js.bytecode-cache (failure)')
        return self.runStep()

    def test_single_new_binary_failure(self):
        self.configureStep()
        self.setProperty('jsc_binary_failures', ['testmasm'])
        self.expectOutcome(result=FAILURE, state_string='Found 1 new JSC binary failure: testmasm (failure)')
        return self.runStep()

    def test_multiple_new_stress_failure(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', [f'test{i}' for i in range(0, 30)])
        self.expectOutcome(result=FAILURE, state_string='Found 30 new JSC stress test failures: test0, test1, test10, test11, test12, test13, test14, test15, test16, test17 ... (failure)')
        return self.runStep()

    def test_multiple_new_binary_failure(self):
        self.configureStep()
        self.setProperty('jsc_binary_failures', ['testmasm', 'testair', 'testb3', 'testdfg', 'testapi'])
        self.expectOutcome(result=FAILURE, state_string='Found 5 new JSC binary failures: testair, testapi, testb3, testdfg, testmasm (failure)')
        return self.runStep()

    def test_new_stress_and_binary_failure(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', ['es6.yaml/es6/Set_iterator_closing.js.default'])
        self.setProperty('jsc_binary_failures', ['testmasm'])
        self.expectOutcome(result=FAILURE, state_string='Found 1 new JSC binary failure: testmasm, Found 1 new JSC stress test failure: es6.yaml/es6/Set_iterator_closing.js.default (failure)')
        return self.runStep()

    def test_stress_failure_on_clean_tree(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', ['stress/force-error.js.default'])
        self.setProperty('jsc_clean_tree_stress_test_failures', ['stress/force-error.js.default'])
        self.expectOutcome(result=SUCCESS, state_string='Passed JSC tests')
        return self.runStep()

    def test_binary_failure_on_clean_tree(self):
        self.configureStep()
        self.setProperty('jsc_binary_failures', ['testdfg'])
        self.setProperty('jsc_rerun_binary_failures', ['testdfg'])
        self.setProperty('jsc_clean_tree_binary_failures', ['testdfg'])
        self.expectOutcome(result=SUCCESS, state_string='Passed JSC tests')
        return self.runStep()

    def test_stress_and_binary_failure_on_clean_tree(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', ['es6.yaml/es6/Set_iterator_closing.js.default'])
        self.setProperty('jsc_binary_failures', ['testair'])
        self.setProperty('jsc_clean_tree_stress_test_failures', ['es6.yaml/es6/Set_iterator_closing.js.default'])
        self.setProperty('jsc_clean_tree_binary_failures', ['testair'])
        self.expectOutcome(result=SUCCESS, state_string='Passed JSC tests')
        return self.runStep()

    def test_flaky_and_consistent_stress_failures(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', ['test1'])
        self.setProperty('jsc_flaky_and_passed', {'test2': {'P': '7', 'T': '10'}})
        self.expectOutcome(result=FAILURE, state_string='Found 1 new JSC stress test failure: test1 (failure)')
        return self.runStep()

    def test_flaky_and_consistent_failures_with_clean_tree_failures(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', ['test1'])
        self.setProperty('jsc_flaky_and_passed', {'test2': {'P': 7, 'T': 10}})
        self.setProperty('jsc_clean_tree_stress_test_failures', ['test1'])
        self.expectOutcome(result=SUCCESS, state_string='Passed JSC tests')
        return self.runStep()

    def test_unexpected_infra_issue(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', [])
        self.setProperty('jsc_clean_tree_stress_test_failures', [])
        self.setProperty('clean_tree_run_status', FAILURE)
        self.expectOutcome(result=RETRY, state_string='Unexpected infrastructure issue, retrying build (retry)')
        return self.runStep()

    def test_change_breaking_jsc_test_suite(self):
        self.configureStep()
        self.setProperty('jsc_stress_test_failures', [])
        self.setProperty('jsc_flaky_and_passed', {})
        self.setProperty('jsc_clean_tree_stress_test_failures', [])
        self.setProperty('jsc_clean_tree_flaky_and_passed', {})
        self.setProperty('clean_tree_run_status', SUCCESS)
        self.expectOutcome(result=FAILURE, state_string='Found unexpected failure with change (failure)')
        return self.runStep()


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

        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(RunWebKitTests())
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
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging', '--exit-after-n-failures', '60', '--skip-failing-tests'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        return self.runStep()

    def test_warnings(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging', '--exit-after-n-failures', '60', '--skip-failing-tests'],
                        )
            + 0
            + ExpectShell.log('stdio', stdout='''Unexpected flakiness: timeouts (2)
                              imported/blink/storage/indexeddb/blob-valid-before-commit.html [ Timeout Pass ]
                              storage/indexeddb/modern/deleteindex-2.html [ Timeout Pass ]'''),
        )
        self.expectOutcome(result=WARNINGS, state_string='2 flakes')
        return self.runStep()

    def test_skip_for_revert_patches_on_commit_queue(self):
        self.configureStep()
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('fullPlatform', 'mac')
        self.setProperty('configuration', 'debug')
        self.setProperty('fast_commit_queue', True)
        self.expectOutcome(result=SKIPPED, state_string='Skipped layout-tests in fast-cq mode')
        return self.runStep()

    def test_skip_for_mac_wk2_passed_change_on_commit_queue(self):
        self.configureStep()
        self.setProperty('patch_id', '1234')
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('fullPlatform', 'mac')
        self.setProperty('configuration', 'debug')
        self.setProperty('passed_mac_wk2', True)
        self.expectOutcome(result=SKIPPED, state_string='Skipped layout-tests')
        return self.runStep()

    def test_skip_for_mac_wk2_passed_change_on_merge_queue(self):
        self.configureStep()
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88a')
        self.setProperty('buildername', 'Merge-Queue')
        self.setProperty('fullPlatform', 'mac')
        self.setProperty('configuration', 'debug')
        self.setProperty('passed_mac_wk2', True)
        self.expectOutcome(result=SKIPPED, state_string='Skipped layout-tests')
        return self.runStep()

    def test_parse_results_json_regression(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging', '--exit-after-n-failures', '60', '--skip-failing-tests'],
                        )
            + 2
            + ExpectShell.log('json', stdout=self.results_json_regressions),
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.runStep()
        self.assertEqual(self.getProperty(self.property_exceed_failure_limit), True)
        self.assertEqual(self.getProperty(self.property_failures),
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
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging', '--exit-after-n-failures', '60', '--skip-failing-tests'],
                        )
            + 0
            + ExpectShell.log('json', stdout=self.results_json_flakes),
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.runStep()
        self.assertEqual(self.getProperty(self.property_exceed_failure_limit), False)
        self.assertEqual(self.getProperty(self.property_failures), [])
        return rc

    def test_parse_results_json_flakes_and_regressions(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging', '--exit-after-n-failures', '60', '--skip-failing-tests'],
                        )
            + 2
            + ExpectShell.log('json', stdout=self.results_json_mix_flakes_and_regression),
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.runStep()
        self.assertEqual(self.getProperty(self.property_exceed_failure_limit), False)
        self.assertEqual(self.getProperty(self.property_failures), ['fast/scrolling/ios/reconcile-layer-position-recursive.html'])
        return rc

    def test_parse_results_json_with_newlines(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging', '--exit-after-n-failures', '60', '--skip-failing-tests'],
                        )
            + 2
            + ExpectShell.log('json', stdout=self.results_json_with_newlines),
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.runStep()
        self.assertEqual(self.getProperty(self.property_exceed_failure_limit), False)
        self.assertEqual(self.getProperty(self.property_failures), ['fast/scrolling/ios/reconcile-layer-position-recursive.html'])
        return rc

    def test_parse_results_json_with_missing_results(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging', '--exit-after-n-failures', '60', '--skip-failing-tests'],
                        )
            + 2
            + ExpectShell.log('json', stdout=self.results_with_missing_results),
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.runStep()
        self.assertEqual(self.getProperty(self.property_exceed_failure_limit), False)
        self.assertEqual(self.getProperty(self.property_failures),
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
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--debug', '--results-directory', 'layout-test-results', '--debug-rwt-logging',  '--exit-after-n-failures', '60', '--skip-failing-tests'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected error.')
            + 254,
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.runStep()

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging',  '--exit-after-n-failures', '60', '--skip-failing-tests'],
                        )
            + ExpectShell.log('stdio', stdout='9 failures found.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.runStep()

    def test_success_wpt_import_bot(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('patch_author', 'webkit-wpt-import-bot@igalia.com')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging', 'imported/w3c/web-platform-tests'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        return self.runStep()


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

        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(ReRunWebKitTests())
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
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging', '--exit-after-n-failures', '60', '--skip-failing-tests'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.runStep()
        self.assertEqual(self.getProperty('build_summary'), 'Found flaky tests: test1, test2')
        return rc

    def test_first_run_failed_unexpectedly(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('first_run_failures', [])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging', '--exit-after-n-failures', '60', '--skip-failing-tests'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.runStep()
        self.assertEqual(self.getProperty('build_summary'), 'Passed layout tests')
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
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging', '--exit-after-n-failures', '60', '--skip-failing-tests'],
                        )
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.runStep()


class TestRunWebKitTestsInStressMode(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(RunWebKitTestsInStressMode())
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
                        logEnviron=False,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results',
                                 '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging',
                                 '--exit-after-n-failures', '10', '--skip-failing-tests',
                                 '--iterations', 100, 'test1', 'test2'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        return self.runStep()

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('modified_tests', ['test'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results',
                                 '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging',
                                 '--exit-after-n-failures', '10', '--skip-failing-tests',
                                 '--iterations', 100, 'test'],
                        )
            + ExpectShell.log('stdio', stdout='9 failures found.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.runStep()
        self.assertEqual(self.getProperty('build_summary'), 'Found test failures')
        return rc


class TestRunWebKitTestsInStressGuardmallocMode(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(RunWebKitTestsInStressGuardmallocMode())
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
                        logEnviron=False,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results',
                                 '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging',
                                 '--exit-after-n-failures', '10', '--skip-failing-tests', '--guard-malloc',
                                 '--iterations', 100, 'test1', 'test2'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        return self.runStep()

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('modified_tests', ['test'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results',
                                 '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging',
                                 '--exit-after-n-failures', '10', '--skip-failing-tests', '--guard-malloc',
                                 '--iterations', 100, 'test'],
                        )
            + ExpectShell.log('stdio', stdout='9 failures found.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.runStep()
        self.assertEqual(self.getProperty('build_summary'), 'Found test failures')
        return rc


class TestRunWebKitTestsWithoutChange(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        os.environ['RESULTS_SERVER_API_KEY'] = 'test-api-key'
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(RunWebKitTestsWithoutChange())
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
                        logEnviron=False,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build',
                                 '--no-show-results',
                                 '--no-new-test-results',
                                 '--clobber-old-results',
                                 '--release',
                                 '--results-directory', 'layout-test-results',
                                 '--debug-rwt-logging',
                                 '--exit-after-n-failures', '60',
                                 '--skip-failing-tests',
                                 '--builder-name', 'iOS-13-Simulator-WK2-Tests-EWS',
                                 '--build-number', '123',
                                 '--buildbot-worker', 'ews126',
                                 '--buildbot-master', EWS_BUILD_HOSTNAME,
                                 '--report', 'https://results.webkit.org/'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'},
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='layout-tests')
        with current_hostname(EWS_BUILD_HOSTNAME):
            return self.runStep()

    def test_run_subtest_tests_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('first_run_failures', ['test1.html', 'test2.html', 'test3.html'])
        self.setProperty('second_run_failures', ['test3.html', 'test4.html', 'test5.html'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build',
                                 '--no-show-results',
                                 '--no-new-test-results',
                                 '--clobber-old-results',
                                 '--release',
                                 '--results-directory', 'layout-test-results',
                                 '--debug-rwt-logging',
                                 '--exit-after-n-failures', '60',
                                 '--skip-failing-tests',
                                 '--builder-name', 'iOS-13-Simulator-WK2-Tests-EWS',
                                 '--build-number', '123',
                                 '--buildbot-worker', 'ews126',
                                 '--buildbot-master', EWS_BUILD_HOSTNAME,
                                 '--report', 'https://results.webkit.org/',
                                 '--skipped=always',
                                 'test1.html', 'test2.html', 'test3.html', 'test4.html', 'test5.html'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'},
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='layout-tests')
        with current_hostname(EWS_BUILD_HOSTNAME):
            return self.runStep()

    def test_run_subtest_tests_removes_skipped_that_fails(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('first_run_failures', ['test-was-skipped-patch-removed-expectation-but-still-fails.html'])
        self.setProperty('second_run_failures', ['test-was-skipped-patch-removed-expectation-but-still-fails.html'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build',
                                 '--no-show-results',
                                 '--no-new-test-results',
                                 '--clobber-old-results',
                                 '--release',
                                 '--results-directory', 'layout-test-results',
                                 '--debug-rwt-logging',
                                 '--exit-after-n-failures', '60',
                                 '--skip-failing-tests',
                                 '--builder-name', 'iOS-13-Simulator-WK2-Tests-EWS',
                                 '--build-number', '123',
                                 '--buildbot-worker', 'ews126',
                                 '--buildbot-master', EWS_BUILD_HOSTNAME,
                                 '--report', 'https://results.webkit.org/',
                                 '--skipped=always',
                                 'test-was-skipped-patch-removed-expectation-but-still-fails.html'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'},
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='layout-tests')
        with current_hostname(EWS_BUILD_HOSTNAME):
            return self.runStep()

    def test_run_subtest_tests_fail(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.setProperty('first_run_failures', ['test-fails-withpatch1.html', 'test-pre-existent-failure1.html'])
        self.setProperty('second_run_failures', ['test-fails-withpatch2.html', 'test-pre-existent-failure2.html'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build',
                                 '--no-show-results',
                                 '--no-new-test-results',
                                 '--clobber-old-results',
                                 '--release',
                                 '--results-directory', 'layout-test-results',
                                 '--debug-rwt-logging',
                                 '--exit-after-n-failures', '60',
                                 '--skip-failing-tests',
                                 '--builder-name', 'iOS-13-Simulator-WK2-Tests-EWS',
                                 '--build-number', '123',
                                 '--buildbot-worker', 'ews126',
                                 '--buildbot-master', EWS_BUILD_HOSTNAME,
                                 '--report', 'https://results.webkit.org/',
                                 '--skipped=always',
                                 'test-fails-withpatch1.html', 'test-fails-withpatch2.html', 'test-pre-existent-failure1.html', 'test-pre-existent-failure2.html'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'},
                        )
            + ExpectShell.log('stdio', stdout='2 failures found.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        with current_hostname(EWS_BUILD_HOSTNAME):
            return self.runStep()

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
                        logEnviron=False,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build',
                                 '--no-show-results',
                                 '--no-new-test-results',
                                 '--clobber-old-results',
                                 '--release',
                                 '--results-directory', 'layout-test-results',
                                 '--debug-rwt-logging',
                                 '--exit-after-n-failures', '60',
                                 '--skip-failing-tests',
                                 '--builder-name', 'iOS-13-Simulator-WK2-Tests-EWS',
                                 '--build-number', '123',
                                 '--buildbot-worker', 'ews126',
                                 '--buildbot-master', EWS_BUILD_HOSTNAME,
                                 '--report', 'https://results.webkit.org/'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'},
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='layout-tests')
        with current_hostname(EWS_BUILD_HOSTNAME):
            return self.runStep()

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build',
                                 '--no-show-results',
                                 '--no-new-test-results',
                                 '--clobber-old-results',
                                 '--release',
                                 '--results-directory', 'layout-test-results',
                                 '--debug-rwt-logging',
                                 '--exit-after-n-failures', '60',
                                 '--skip-failing-tests',
                                 '--builder-name', 'iOS-13-Simulator-WK2-Tests-EWS',
                                 '--build-number', '123',
                                 '--buildbot-worker', 'ews126',
                                 '--buildbot-master', EWS_BUILD_HOSTNAME,
                                 '--report', 'https://results.webkit.org/'],
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
                        )
            + ExpectShell.log('stdio', stdout='9 failures found.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        with current_hostname(EWS_BUILD_HOSTNAME):
            return self.runStep()


class TestRunWebKit1Tests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunWebKit1Tests())
        self.setProperty('fullPlatform', 'ios-11')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--debug', '--dump-render-tree', '--results-directory', 'layout-test-results', '--debug-rwt-logging', '--exit-after-n-failures', '60', '--skip-failing-tests'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunWebKit1Tests())
        self.setProperty('fullPlatform', 'ios-11')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results', '--release', '--dump-render-tree', '--results-directory', 'layout-test-results', '--debug-rwt-logging', '--exit-after-n-failures', '60', '--skip-failing-tests'],
                        )
            + ExpectShell.log('stdio', stdout='9 failures found.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.runStep()

    def test_skip_for_revert_patches_on_commit_queue(self):
        self.setupStep(RunWebKit1Tests())
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('fullPlatform', 'mac')
        self.setProperty('configuration', 'debug')
        self.setProperty('fast_commit_queue', True)
        self.expectOutcome(result=SKIPPED, state_string='Skipped layout-tests in fast-cq mode')
        return self.runStep()


class TestAnalyzeLayoutTestsResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(AnalyzeLayoutTestsResults())
        self.setProperty('first_results_exceed_failure_limit', False)
        self.setProperty('second_results_exceed_failure_limit', False)
        self.setProperty('clean_tree_results_exceed_failure_limit', False)
        self.setProperty('clean_tree_run_failures', [])

    def test_failure_introduced_by_change(self):
        self.configureStep()
        self.setProperty('first_run_failures', ["jquery/offset.html"])
        self.setProperty('second_run_failures', ["jquery/offset.html"])
        self.expectOutcome(result=FAILURE, state_string='Found 1 new test failure: jquery/offset.html (failure)')
        return self.runStep()

    def test_failure_on_clean_tree(self):
        self.configureStep()
        self.setProperty('first_run_failures', ["jquery/offset.html"])
        self.setProperty('second_run_failures', ["jquery/offset.html"])
        self.setProperty('clean_tree_run_failures', ["jquery/offset.html"])
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.runStep()
        self.assertEqual(self.getProperty('build_summary'), 'Found 1 pre-existing test failure: jquery/offset.html')
        return rc

    def test_flaky_and_consistent_failures_without_clean_tree_failures(self):
        self.configureStep()
        self.setProperty('buildername', 'iOS-13-Simulator-WK2-Tests-EWS')
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.setProperty('second_run_failures', ['test1'])
        self.expectOutcome(result=FAILURE, state_string='Found 1 new test failure: test1 (failure)')
        rc = self.runStep()
        self.assertEqual(self.getProperty('comment_text'), None)
        self.assertEqual(self.getProperty('build_finish_summary'), 'Found 1 new test failure: test1')
        return rc

    def test_consistent_failure_without_clean_tree_failures_commit_queue(self):
        self.configureStep()
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('first_run_failures', ['test1'])
        self.setProperty('second_run_failures', ['test1'])
        self.expectOutcome(result=FAILURE, state_string='Found 1 new test failure: test1 (failure)')
        rc = self.runStep()
        self.assertEqual(self.getProperty('comment_text'), 'Found 1 new test failure: test1')
        self.assertEqual(self.getProperty('build_finish_summary'), 'Found 1 new test failure: test1')
        return rc

    def test_flaky_and_inconsistent_failures_without_clean_tree_failures(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.setProperty('second_run_failures', ['test3'])
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        return self.runStep()

    def test_flaky_failures_in_first_run(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.setProperty('second_run_failures', [])
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.runStep()
        self.assertEqual(self.getProperty('build_summary'), ' Found flaky tests: test1, test2')
        return rc

    def test_flaky_and_inconsistent_failures_with_clean_tree_failures(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.setProperty('second_run_failures', ['test3'])
        self.setProperty('clean_tree_run_failures', ['test1', 'test2', 'test3'])
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.runStep()
        self.assertEqual(self.getProperty('build_summary'), 'Found 3 pre-existing test failures: test1, test2, test3 Found flaky tests: test1, test2, test3')
        return rc

    def test_flaky_and_consistent_failures_with_clean_tree_failures(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.setProperty('second_run_failures', ['test1'])
        self.setProperty('clean_tree_run_failures', ['test1', 'test2'])
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        return self.runStep()

    def test_mildly_flaky_change_with_some_tree_redness_and_flakiness(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1', 'test2', 'test3'])
        self.setProperty('second_run_failures', ['test1', 'test2'])
        self.setProperty('clean_tree_run_failures', ['test1', 'test2', 'test4'])
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.runStep()
        self.assertEqual(self.getProperty('build_summary'), 'Found 3 pre-existing test failures: test1, test2, test4 Found flaky test: test3')
        return rc

    def test_first_run_exceed_failure_limit(self):
        self.configureStep()
        self.setProperty('first_results_exceed_failure_limit', True)
        self.setProperty('first_run_failures',  [f'test{i}' for i in range(0, 30)])
        self.setProperty('second_run_failures', [])
        self.expectOutcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by change, retrying build (retry)')
        return self.runStep()

    def test_second_run_exceed_failure_limit(self):
        self.configureStep()
        self.setProperty('first_run_failures', [])
        self.setProperty('second_results_exceed_failure_limit', True)
        self.setProperty('second_run_failures',  [f'test{i}' for i in range(0, 30)])
        self.expectOutcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by change, retrying build (retry)')
        return self.runStep()

    def test_clean_tree_exceed_failure_limit(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1'])
        self.setProperty('second_run_failures', ['test1'])
        self.setProperty('clean_tree_results_exceed_failure_limit', True)
        self.setProperty('clean_tree_run_failures',  [f'test{i}' for i in range(0, 30)])
        self.expectOutcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by change, retrying build (retry)')
        return self.runStep()

    def test_clean_tree_exceed_failure_limit_with_triggered_by(self):
        self.configureStep()
        self.setProperty('buildername', 'iOS-13-Simulator-WK2-Tests-EWS')
        self.setProperty('triggered_by', 'ios-13-sim-build-ews')
        self.setProperty('first_run_failures', ['test1'])
        self.setProperty('second_run_failures', ['test1'])
        self.setProperty('clean_tree_results_exceed_failure_limit', True)
        self.setProperty('clean_tree_run_failures',  [f'test{i}' for i in range(0, 30)])
        message = 'Unable to confirm if test failures are introduced by change, retrying build'
        self.expectOutcome(result=SUCCESS, state_string=message)
        rc = self.runStep()
        self.assertEqual(self.getProperty('build_summary'), message)
        return rc

    def test_clean_tree_has_lot_of_failures(self):
        self.configureStep()
        self.setProperty('first_results_exceed_failure_limit', True)
        self.setProperty('first_run_failures', [f'test{i}' for i in range(0, 30)])
        self.setProperty('second_results_exceed_failure_limit', True)
        self.setProperty('second_run_failures', [f'test{i}' for i in range(0, 30)])
        self.setProperty('clean_tree_run_failures', [f'test{i}' for i in range(0, 27)])
        self.expectOutcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by change, retrying build (retry)')
        return self.runStep()

    def test_clean_tree_has_some_failures(self):
        self.configureStep()
        self.setProperty('first_results_exceed_failure_limit', True)
        self.setProperty('first_run_failures', [f'test{i}' for i in range(0, 30)])
        self.setProperty('second_results_exceed_failure_limit', True)
        self.setProperty('second_run_failures', [f'test{i}' for i in range(0, 30)])
        self.setProperty('clean_tree_run_failures', [f'test{i}' for i in range(0, 10)])
        self.expectOutcome(result=FAILURE, state_string='Found 30 new test failures: test0, test1, test10, test11, test12, test13, test14, test15, test16, test17 ... (failure)')
        return self.runStep()

    def test_clean_tree_has_lot_of_failures_and_no_new_failure(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1'])
        self.setProperty('second_run_failures', ['test1'])
        self.setProperty('clean_tree_run_failures', [f'test{i}' for i in range(0, 20)])
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        rc = self.runStep()
        self.assertEqual(self.getProperty('build_summary'), 'Found 20 pre-existing test failures: test0, test1, test10, test11, test12, test13, test14, test15, test16, test17 ...')
        return rc

    def test_change_introduces_lot_of_failures(self):
        self.configureStep()
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('first_results_exceed_failure_limit', True)
        self.setProperty('first_run_failures', [f'test{i}' for i in range(0, 300)])
        self.setProperty('second_results_exceed_failure_limit', True)
        self.setProperty('second_run_failures', [f'test{i}' for i in range(0, 300)])
        failure_message = 'Found 300 new test failures: test0, test1, test10, test100, test101, test102, test103, test104, test105, test106 ...'
        self.expectOutcome(result=FAILURE, state_string=failure_message + ' (failure)')
        rc = self.runStep()
        self.assertEqual(self.getProperty('comment_text'), failure_message)
        self.assertEqual(self.getProperty('build_finish_summary'), failure_message)
        return rc

    def test_change_introduces_lot_of_flakiness(self):
        self.configureStep()
        self.setProperty('first_results_exceed_failure_limit', False)
        self.setProperty('first_run_failures', [f'test{i}' for i in range(0, 5)])
        self.setProperty('second_results_exceed_failure_limit', False)
        self.setProperty('second_run_failures', [f'test{i}' for i in range(5, 12)])
        failure_message = 'Too many flaky failures: test0, test1, test10, test11, test2, test3, test4, test5, test6, test7, test8, test9 (failure)'
        self.expectOutcome(result=FAILURE, state_string=failure_message)
        return self.runStep()

    def test_unexpected_infra_issue(self):
        self.configureStep()
        self.setProperty('first_run_failures', [])
        self.setProperty('second_run_failures', [])
        self.setProperty('clean_tree_run_status', FAILURE)
        self.expectOutcome(result=RETRY, state_string='Unexpected infrastructure issue, retrying build (retry)')
        return self.runStep()

    def test_change_breaks_layout_tests(self):
        self.configureStep()
        self.setProperty('first_run_failures', [])
        self.setProperty('second_run_failures', [])
        self.setProperty('clean_tree_run_failures', [])
        self.setProperty('clean_tree_run_status', SUCCESS)
        self.expectOutcome(result=FAILURE, state_string='Found unexpected failure with change (failure)')
        return self.runStep()

    def test_change_removes_skipped_test_that_fails(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test-was-skipped-change-removed-expectation-but-still-fails.html'])
        self.setProperty('second_run_failures', ['test-was-skipped-change-removed-expectation-but-still-fails.html'])
        self.setProperty('clean_tree_run_failures', [])
        self.setProperty('clean_tree_run_status', SUCCESS)
        self.expectOutcome(result=FAILURE, state_string='Found 1 new test failure: test-was-skipped-change-removed-expectation-but-still-fails.html (failure)')
        return self.runStep()


class TestRunWebKitTestsRedTree(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(RunWebKitTestsRedTree())

    def test_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results',
                                 '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging',
                                 '--exit-after-n-failures', '500', '--skip-failing-tests']
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        return self.runStep()


class TestRunWebKitTestsRepeatFailuresRedTree(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(RunWebKitTestsRepeatFailuresRedTree())

    def test_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        first_run_failures = ['fast/css/test1.html', 'imported/test/test2.html', 'fast/svg/test3.svg']
        first_run_flakies = ['fast/css/flaky1.html', 'imported/test/flaky2.html', 'fast/svg/flaky3.svg']
        self.setProperty('first_run_failures', first_run_failures)
        self.setProperty('first_run_flakies', first_run_flakies)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        maxTime=18000,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results',
                                 '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging',
                                 '--skip-failing-tests', '--fully-parallel', '--repeat-each=10'] + sorted(first_run_failures)
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='layout-tests')
        return self.runStep()


class TestRunWebKitTestsRepeatFailuresWithoutChangeRedTree(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(RunWebKitTestsRepeatFailuresWithoutChangeRedTree())

    def test_success(self):
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
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        logEnviron=False,
                        maxTime=10800,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results',
                                 '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging',
                                 '--skip-failing-tests', '--fully-parallel', '--repeat-each=10', '--skipped=always'] + sorted(with_change_repeat_failures_results_nonflaky_failures)
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='layout-tests')
        return self.runStep()

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
                        logEnviron=False,
                        maxTime=10800,
                        command=['python3',
                                 'Tools/Scripts/run-webkit-tests',
                                 '--no-build', '--no-show-results', '--no-new-test-results', '--clobber-old-results',
                                 '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging',
                                 '--skip-failing-tests', '--fully-parallel', '--repeat-each=10', '--skipped=always'] + sorted(first_run_failures)
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='layout-tests')
        return self.runStep()


class TestAnalyzeLayoutTestsResultsRedTree(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(AnalyzeLayoutTestsResultsRedTree())

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
        self.expectOutcome(result=FAILURE, state_string='Found 1 new test failure: test/failure1.html (failure)')
        step_result = self.runStep()
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
        self.expectOutcome(result=FAILURE, state_string='Found 1 new test failure: test/failure1.html (failure)')
        step_result = self.runStep()
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
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        step_result = self.runStep()
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
        self.expectOutcome(result=SUCCESS, state_string='Passed layout tests')
        step_result = self.runStep()
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
        self.expectOutcome(result=FAILURE, state_string='Found unexpected failure with change (failure)')
        step_result = self.runStep()
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
        self.expectOutcome(
            result=WARNINGS,
            state_string=f'{expected_infrastructure_error}\nReached the maximum number of retries (3). Unable to determine if change is bad or there is a pre-existent infrastructure issue. (warnings)')
        step_result = self.runStep()
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
        self.expectOutcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 2 of 3] (retry)')
        step_result = self.runStep()
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
        self.expectOutcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 0 of 3] (retry)')
        step_result = self.runStep()
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
        self.expectOutcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 0 of 3] (retry)')
        step_result = self.runStep()
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
        self.expectOutcome(result=FAILURE, state_string='Found 2 new test failures: test/failure1.html, test/failure2.html (failure)')
        step_result = self.runStep()
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
        self.expectOutcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 0 of 3] (retry)')
        step_result = self.runStep()
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
        self.expectOutcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 0 of 3] (retry)')
        step_result = self.runStep()
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
        self.expectOutcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 0 of 3] (retry)')
        step_result = self.runStep()
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
        self.expectOutcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 0 of 3] (retry)')
        step_result = self.runStep()
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
        self.expectOutcome(result=RETRY, state_string=f'Unexpected infrastructure issue: {expected_infrastructure_error}\nRetrying build [retry count is 2 of 3] (retry)')
        step_result = self.runStep()
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
        self.expectOutcome(result=WARNINGS, state_string=f'{expected_infrastructure_error}\nReached the maximum number of retries (3). Unable to determine if change is bad or there is a pre-existent infrastructure issue. (warnings)')
        step_result = self.runStep()
        self.assertEqual(len(self._emails_list), 1)
        self.assertTrue(expected_infrastructure_error in self._emails_list[0])
        return step_result


class TestCheckOutSpecificRevision(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(CheckOutSpecificRevision())
        self.setProperty('ews_revision', '1a3425cb92dbcbca12a10aa9514f1b77c76dc26')
        self.expectHidden(False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=1200,
                        logEnviron=False,
                        command=['git', 'checkout', '1a3425cb92dbcbca12a10aa9514f1b77c76dc26'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Checked out required revision')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CheckOutSpecificRevision())
        self.setProperty('ews_revision', '1a3425cb92dbcbca12a10aa9514f1b77c76dc26')
        self.expectHidden(False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=1200,
                        logEnviron=False,
                        command=['git', 'checkout', '1a3425cb92dbcbca12a10aa9514f1b77c76dc26'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected failure')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Checked out required revision (failure)')
        return self.runStep()

    def test_skip(self):
        self.setupStep(CheckOutSpecificRevision())
        self.expectHidden(True)
        self.expectOutcome(result=SKIPPED, state_string='Checked out required revision (skipped)')
        return self.runStep()


class TestCleanWorkingDirectory(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(CleanWorkingDirectory())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/clean-webkit'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Cleaned working directory')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CleanWorkingDirectory())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/clean-webkit'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected failure.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Cleaned working directory (failure)')
        return self.runStep()


class TestUpdateWorkingDirectory(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(UpdateWorkingDirectory())
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                command=['git', 'checkout', 'remotes/origin/main', '-f'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                command=['/bin/sh', '-c', 'git branch -D main || true'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                command=['git', 'checkout', '-b', 'main'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Updated working directory')
        return self.runStep()

    def test_success_branch(self):
        self.setupStep(UpdateWorkingDirectory())
        self.setProperty('github.base.ref', 'safari-xxx-branch')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                command=['git', 'checkout', 'remotes/origin/safari-xxx-branch', '-f'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                command=['/bin/sh', '-c', 'git branch -D safari-xxx-branch || true'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                command=['git', 'checkout', '-b', 'safari-xxx-branch'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                command=['/bin/sh', '-c', 'git branch -D main || true'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                command=['git', 'branch', '--track', 'main', 'remotes/origin/main'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Updated working directory')
        return self.runStep()

    def test_success_remote(self):
        self.setupStep(UpdateWorkingDirectory())
        self.setProperty('remote', 'security')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                command=['git', 'checkout', 'remotes/security/main', '-f'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                command=['/bin/sh', '-c', 'git branch -D main || true'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                command=['git', 'checkout', '-b', 'main'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Updated working directory')
        return self.runStep()

    def test_success_remote_branch(self):
        self.setupStep(UpdateWorkingDirectory())
        self.setProperty('remote', 'security')
        self.setProperty('github.base.ref', 'safari-xxx-branch')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                command=['git', 'checkout', 'remotes/security/safari-xxx-branch', '-f'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                command=['/bin/sh', '-c', 'git branch -D safari-xxx-branch || true'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                command=['git', 'checkout', '-b', 'safari-xxx-branch'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                command=['/bin/sh', '-c', 'git branch -D main || true'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                command=['git', 'branch', '--track', 'main', 'remotes/origin/main'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Updated working directory')
        return self.runStep()

    def test_failure(self):
        self.setupStep(UpdateWorkingDirectory())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['git', 'checkout', 'remotes/origin/main', '-f'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected failure.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to updated working directory')
        return self.runStep()


class TestApplyPatch(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True

        def mock_start(cls, *args, **kwargs):
            from buildbot.steps import shell
            return shell.ShellCommand.start(cls)
        ApplyPatch.start = mock_start
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(ApplyPatch())
        self.setProperty('patch_id', '1234')
        self.assertEqual(ApplyPatch.flunkOnFailure, True)
        self.assertEqual(ApplyPatch.haltOnFailure, False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=600,
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/svn-apply', '--force', '.buildbot-diff'],
                        ) +
            0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Applied patch')
        return self.runStep()

    def test_failure(self):
        self.setupStep(ApplyPatch())
        self.setProperty('patch_id', '1234')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=600,
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/svn-apply', '--force', '.buildbot-diff'],
                        ) +
            ExpectShell.log('stdio', stdout='Unexpected failure.') +
            2,
        )
        self.expectOutcome(result=FAILURE, state_string='svn-apply failed to apply patch to trunk')
        rc = self.runStep()
        self.assertEqual(self.getProperty('comment_text'), None)
        self.assertEqual(self.getProperty('build_finish_summary'), None)
        return rc

    def test_skipped(self):
        self.setupStep(ApplyPatch())
        self.expectHidden(True)
        self.expectOutcome(result=SKIPPED, state_string="Skipping applying patch since patch_id isn't provided")
        return self.runStep()

    def test_failure_on_commit_queue(self):
        self.setupStep(ApplyPatch())
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('patch_id', '1234')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=600,
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/svn-apply', '--force', '.buildbot-diff'],
                        ) +
            ExpectShell.log('stdio', stdout='Unexpected failure.') +
            2,
        )
        self.expectOutcome(result=FAILURE, state_string='svn-apply failed to apply patch to trunk')
        rc = self.runStep()
        self.assertEqual(self.getProperty('comment_text'), 'Tools/Scripts/svn-apply failed to apply attachment 1234 to trunk.\nPlease resolve the conflicts and upload a new patch.')
        self.assertEqual(self.getProperty('build_finish_summary'), 'Tools/Scripts/svn-apply failed to apply patch 1234 to trunk')
        return rc


class TestCheckOutPullRequest(BuildStepMixinAdditions, unittest.TestCase):
    ENV = dict(
        GIT_COMMITTER_NAME='EWS',
        GIT_COMMITTER_EMAIL='ews@webkit.org',
        GIT_USER=None,
        GIT_PASSWORD=None,
    )

    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(CheckOutPullRequest())
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
                logEnviron=False,
                env=self.ENV,
                command=['git', 'config', 'credential.helper', '!echo_credentials() { sleep 1; echo "username=${GIT_USER}"; echo "password=${GIT_PASSWORD}"; }; echo_credentials'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=600,
                logEnviron=False,
                env=self.ENV,
                command=['/bin/sh', '-c', 'git remote add Contributor https://github.com/Contributor/WebKit.git || true'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=600,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'remote', 'set-url', 'Contributor', 'https://github.com/Contributor/WebKit.git'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=600,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'fetch', 'Contributor', '--prune'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=600,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'checkout', '-b', 'eng/pull-request-branch'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=600,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'cherry-pick', 'HEAD..remotes/Contributor/eng/pull-request-branch'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Checked out pull request')
        return self.runStep()

    def test_success_wincairo(self):
        self.setupStep(CheckOutPullRequest())
        self.setProperty('platform', 'wincairo')
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
                logEnviron=False,
                env=self.ENV,
                command=['git', 'config', 'credential.helper', '!echo_credentials() { sleep 1; echo "username=${GIT_USER}"; echo "password=${GIT_PASSWORD}"; }; echo_credentials'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=600,
                logEnviron=False,
                env=self.ENV,
                command=['sh', '-c', 'git remote add Contributor https://github.com/Contributor/WebKit.git || exit 0'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=600,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'remote', 'set-url', 'Contributor', 'https://github.com/Contributor/WebKit.git'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=600,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'fetch', 'Contributor', '--prune'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=600,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'checkout', '-b', 'eng/pull-request-branch'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=600,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'cherry-pick', 'HEAD..remotes/Contributor/eng/pull-request-branch'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Checked out pull request')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CheckOutPullRequest())
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
                logEnviron=False,
                env=self.ENV,
                command=['git', 'config', 'credential.helper', '!echo_credentials() { sleep 1; echo "username=${GIT_USER}"; echo "password=${GIT_PASSWORD}"; }; echo_credentials'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=600,
                logEnviron=False,
                env=self.ENV,
                command=['/bin/sh', '-c', 'git remote add Contributor https://github.com/Contributor/WebKit.git || true'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=600,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'remote', 'set-url', 'Contributor', 'https://github.com/Contributor/WebKit.git'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=600,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'fetch', 'Contributor', '--prune'],
            ) + 1,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to checkout and rebase branch from PR 1234')
        return self.runStep()

    def test_skipped(self):
        self.setupStep(CheckOutPullRequest())
        self.expectHidden(True)
        self.expectOutcome(result=SKIPPED, state_string='No pull request to checkout')
        return self.runStep()


class TestUnApplyPatch(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(UnApplyPatch())
        self.setProperty('patch_id', 1234)
        self.expectHidden(False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/clean-webkit'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Unapplied patch')
        return self.runStep()

    def test_failure(self):
        self.setupStep(UnApplyPatch())
        self.setProperty('patch_id', 1234)
        self.expectHidden(False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/clean-webkit'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected failure.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Unapplied patch (failure)')
        return self.runStep()

    def test_skip(self):
        self.setupStep(UnApplyPatch())
        self.expectHidden(True)
        self.expectOutcome(result=SKIPPED, state_string='Unapplied patch (skipped)')
        return self.runStep()


class TestRevertPullRequestChanges(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RevertPullRequestChanges())
        self.setProperty('got_revision', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c')
        self.setProperty('github.number', 1234)
        self.expectHidden(False)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                timeout=5 * 60,
                command=['git', 'clean', '-f', '-d'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                timeout=5 * 60,
                command=['git', 'checkout', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Reverted pull request changes')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RevertPullRequestChanges())
        self.setProperty('ews_revision', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c')
        self.setProperty('github.number', 1234)
        self.expectHidden(False)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                timeout=5 * 60,
                command=['git', 'clean', '-f', '-d'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                timeout=5 * 60,
                command=['git', 'checkout', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c'],
            ) + ExpectShell.log('stdio', stdout='Unexpected failure.') + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Reverted pull request changes (failure)')
        return self.runStep()

    def test_skip(self):
        self.setupStep(RevertPullRequestChanges())
        self.expectHidden(True)
        self.expectOutcome(result=SKIPPED, state_string='Reverted pull request changes (skipped)')
        return self.runStep()

    def test_glib_cleanup(self):
        self.setupStep(RevertPullRequestChanges())
        self.setProperty('got_revision', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c')
        self.setProperty('github.number', 1234)
        self.setProperty('platform', 'gtk')
        self.setProperty('configuration', 'release')
        self.expectHidden(False)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                timeout=5 * 60,
                command=['git', 'clean', '-f', '-d'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                timeout=5 * 60,
                command=['git', 'checkout', 'b2db8d1da7b74b5ddf075e301370e64d914eef7c'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                timeout=5 * 60,
                command=['rm', '-f', 'WebKitBuild/GTK/Release/build-webkit-options.txt'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Reverted pull request changes')
        return self.runStep()


class TestCheckChangeRelevance(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_relevant_jsc_patch(self):
        file_names = ['JSTests/', 'Source/JavaScriptCore/', 'Source/WTF/', 'Source/bmalloc/', 'Makefile', 'Makefile.shared',
                      'Source/Makefile', 'Source/Makefile.shared', 'Tools/Scripts/build-webkit', 'Tools/Scripts/build-jsc',
                      'Tools/Scripts/jsc-stress-test-helpers/', 'Tools/Scripts/run-jsc', 'Tools/Scripts/run-jsc-benchmarks',
                      'Tools/Scripts/run-jsc-stress-tests', 'Tools/Scripts/run-javascriptcore-tests', 'Tools/Scripts/run-layout-jsc',
                      'Tools/Scripts/update-javascriptcore-test-results', 'Tools/Scripts/webkitdirs.pm',
                      'Source/cmake/OptionsJSCOnly.cmake']

        self.setupStep(CheckChangeRelevance())
        self.setProperty('buildername', 'JSC-Tests-EWS')
        self.assertEqual(CheckChangeRelevance.haltOnFailure, True)
        self.assertEqual(CheckChangeRelevance.flunkOnFailure, True)
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: f'Sample patch; file: {file_name}'
            self.expectOutcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.runStep()
        return rc

    def test_relevant_jsc_arm64_patch(self):
        file_names = ['JSTests/', 'Source/JavaScriptCore/', 'Source/WTF/', 'Source/bmalloc/', 'Makefile', 'Makefile.shared',
                      'Source/Makefile', 'Source/Makefile.shared', 'Tools/Scripts/build-webkit', 'Tools/Scripts/build-jsc',
                      'Tools/Scripts/jsc-stress-test-helpers/', 'Tools/Scripts/run-jsc', 'Tools/Scripts/run-jsc-benchmarks',
                      'Tools/Scripts/run-jsc-stress-tests', 'Tools/Scripts/run-javascriptcore-tests', 'Tools/Scripts/run-layout-jsc',
                      'Tools/Scripts/update-javascriptcore-test-results', 'Tools/Scripts/webkitdirs.pm',
                      'Source/cmake/OptionsJSCOnly.cmake']

        self.setupStep(CheckChangeRelevance())
        self.setProperty('buildername', 'JSC-Tests-arm64-EWS')
        self.assertEqual(CheckChangeRelevance.haltOnFailure, True)
        self.assertEqual(CheckChangeRelevance.flunkOnFailure, True)
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: f'Sample patch; file: {file_name}'
            self.expectOutcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.runStep()
        return rc

    def test_relevant_wk1_patch(self):
        file_names = ['Source/WebKitLegacy', 'Source/WebCore', 'Source/WebInspectorUI', 'Source/WebDriver', 'Source/WTF',
                      'Source/bmalloc', 'Source/JavaScriptCore', 'Source/ThirdParty', 'LayoutTests', 'Tools']

        self.setupStep(CheckChangeRelevance())
        self.setProperty('buildername', 'macOS-Catalina-Release-WK1-Tests-EWS')
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: f'Sample patch; file: {file_name}'
            self.expectOutcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.runStep()
        return rc

    def test_relevant_bigsur_builder_patch(self):
        file_names = ['Source/xyz', 'Tools/abc']
        self.setupStep(CheckChangeRelevance())
        self.setProperty('buildername', 'macOS-BigSur-Release-Build-EWS')
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: f'Sample patch; file: {file_name}'
            self.expectOutcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.runStep()
        return rc

    def test_relevant_windows_wk1_patch(self):
        CheckChangeRelevance._get_patch = lambda x: b'Sample patch; file: Source/WebKitLegacy'
        self.setupStep(CheckChangeRelevance())
        self.setProperty('buildername', 'Windows-EWS')
        self.expectOutcome(result=SUCCESS, state_string='Patch contains relevant changes')
        return self.runStep()

    def test_relevant_webkitpy_patch(self):
        file_names = ['Tools/Scripts/webkitpy', 'Tools/Scripts/libraries', 'Tools/Scripts/commit-log-editor']
        self.setupStep(CheckChangeRelevance())
        self.setProperty('buildername', 'WebKitPy-Tests-EWS')
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: f'Sample patch; file: {file_name}'
            self.expectOutcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.runStep()
        return rc

    def test_relevant_services_patch(self):
        file_names = ['Tools/CISupport/build-webkit-org', 'Tools/CISupport/ews-build', 'Tools/CISupport/Shared',
                      'Tools/Scripts/libraries/resultsdbpy', 'Tools/Scripts/libraries/webkitcorepy', 'Tools/Scripts/libraries/webkitscmpy']
        self.setupStep(CheckChangeRelevance())
        self.setProperty('buildername', 'Services-EWS')
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: f'Sample patch; file: {file_name}'
            self.expectOutcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.runStep()
        return rc

    def test_relevant_services_pull_request(self):
        file_names = ['Tools/CISupport/build-webkit-org', 'Tools/CISupport/ews-build', 'Tools/CISupport/Shared',
                      'Tools/Scripts/libraries/resultsdbpy', 'Tools/Scripts/libraries/webkitcorepy', 'Tools/Scripts/libraries/webkitscmpy']
        self.setupStep(CheckChangeRelevance())
        self.setProperty('buildername', 'Services-EWS')
        self.setProperty('github.number', 1234)
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: file_name
            self.expectOutcome(result=SUCCESS, state_string='Pull request contains relevant changes')
            rc = self.runStep()
        return rc

    def test_relevant_bindings_tests_patch(self):
        file_names = ['Source/WebCore', 'Tools']
        self.setupStep(CheckChangeRelevance())
        self.setProperty('buildername', 'Bindings-Tests-EWS')
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: f'Sample patch; file: {file_name}'
            self.expectOutcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.runStep()
        return rc

    def test_relevant_bindings_tests_pull_request(self):
        file_names = ['Source/WebCore', 'Tools']
        self.setupStep(CheckChangeRelevance())
        self.setProperty('buildername', 'Bindings-Tests-EWS')
        self.setProperty('github.number', 1234)
        for file_name in file_names:
            CheckChangeRelevance._get_patch = lambda x: file_name
            self.expectOutcome(result=SUCCESS, state_string='Pull request contains relevant changes')
            rc = self.runStep()
        return rc

    def test_queues_without_relevance_info(self):
        CheckChangeRelevance._get_patch = lambda x: 'Sample patch'
        queues = ['Commit-Queue', 'Style-EWS', 'Apply-WatchList-EWS', 'GTK-Build-EWS', 'GTK-WK2-Tests-EWS',
                  'iOS-13-Build-EWS', 'iOS-13-Simulator-Build-EWS', 'iOS-13-Simulator-WK2-Tests-EWS',
                  'macOS-Catalina-Release-Build-EWS', 'macOS-Catalina-Release-WK2-Tests-EWS', 'macOS-Catalina-Debug-Build-EWS',
                  'WinCairo-EWS', 'WPE-EWS', 'WebKitPerl-Tests-EWS']
        for queue in queues:
            self.setupStep(CheckChangeRelevance())
            self.setProperty('buildername', queue)
            self.expectOutcome(result=SUCCESS, state_string='Patch contains relevant changes')
            rc = self.runStep()
        return rc

    def test_non_relevant_patch_on_various_queues(self):
        CheckChangeRelevance._get_patch = lambda x: 'Sample patch'
        queues = ['Bindings-Tests-EWS', 'JSC-Tests-EWS', 'macOS-BigSur-Release-Build-EWS',
                  'macOS-Catalina-Debug-WK1-Tests-EWS', 'Services-EWS', 'WebKitPy-Tests-EWS']
        for queue in queues:
            self.setupStep(CheckChangeRelevance())
            self.setProperty('buildername', queue)
            self.expectOutcome(result=FAILURE, state_string='Patch doesn\'t have relevant changes')
            rc = self.runStep()
        return rc

    def test_non_relevant_pull_request_on_various_queues(self):
        CheckChangeRelevance._get_patch = lambda x: '\n'
        queues = ['Bindings-Tests-EWS', 'JSC-Tests-EWS', 'macOS-BigSur-Release-Build-EWS',
                  'macOS-Catalina-Debug-WK1-Tests-EWS', 'Services-EWS', 'WebKitPy-Tests-EWS']
        for queue in queues:
            self.setupStep(CheckChangeRelevance())
            self.setProperty('buildername', queue)
            self.setProperty('github.number', 1234)
            self.expectOutcome(result=FAILURE, state_string='Pull request doesn\'t have relevant changes')
            rc = self.runStep()
        return rc


class TestFindModifiedLayoutTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_relevant_patch(self):
        self.setupStep(FindModifiedLayoutTests())
        self.assertEqual(FindModifiedLayoutTests.haltOnFailure, True)
        self.assertEqual(FindModifiedLayoutTests.flunkOnFailure, True)
        FindModifiedLayoutTests._get_patch = lambda x: b'+++ LayoutTests/http/tests/events/device-orientation-motion-insecure-context.html'
        self.expectOutcome(result=SUCCESS, state_string='Patch contains relevant changes')
        rc = self.runStep()
        self.assertEqual(self.getProperty('modified_tests'), ['LayoutTests/http/tests/events/device-orientation-motion-insecure-context.html'])
        return rc

    def test_ignore_certain_directories(self):
        self.setupStep(FindModifiedLayoutTests())
        dir_names = ['reference', 'reftest', 'resources', 'support', 'script-tests', 'tools']
        for dir_name in dir_names:
            FindModifiedLayoutTests._get_patch = lambda x: f'+++ LayoutTests/{dir_name}/test-name.html'.encode('utf-8')
            self.expectOutcome(result=SKIPPED, state_string='Patch doesn\'t have relevant changes')
            rc = self.runStep()
            self.assertEqual(self.getProperty('modified_tests'), None)
        return rc

    def test_ignore_certain_suffixes(self):
        self.setupStep(FindModifiedLayoutTests())
        suffixes = ['-expected', '-expected-mismatch', '-ref', '-notref']
        for suffix in suffixes:
            FindModifiedLayoutTests._get_patch = lambda x: f'+++ LayoutTests/http/tests/events/device-motion-{suffix}.html'.encode('utf-8')
            self.expectOutcome(result=SKIPPED, state_string='Patch doesn\'t have relevant changes')
            rc = self.runStep()
            self.assertEqual(self.getProperty('modified_tests'), None)
        return rc

    def test_ignore_non_layout_test_in_html_directory(self):
        self.setupStep(FindModifiedLayoutTests())
        FindModifiedLayoutTests._get_patch = lambda x: '+++ LayoutTests/html/test.txt'.encode('utf-8')
        self.expectOutcome(result=SKIPPED, state_string='Patch doesn\'t have relevant changes')
        rc = self.runStep()
        self.assertEqual(self.getProperty('modified_tests'), None)
        return rc

    def test_non_relevant_patch(self):
        self.setupStep(FindModifiedLayoutTests())
        FindModifiedLayoutTests._get_patch = lambda x: b'Sample patch which does not modify any layout test'
        self.expectOutcome(result=SKIPPED, state_string='Patch doesn\'t have relevant changes')
        rc = self.runStep()
        self.assertEqual(self.getProperty('modified_tests'), None)
        return rc


class TestArchiveBuiltProduct(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(ArchiveBuiltProduct())
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/CISupport/built-product-archive', '--platform=ios-simulator',  '--release', 'archive'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Archived built product')
        return self.runStep()

    def test_failure(self):
        self.setupStep(ArchiveBuiltProduct())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/CISupport/built-product-archive', '--platform=mac-sierra',  '--debug', 'archive'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected failure.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Archived built product (failure)')
        return self.runStep()


class TestUploadBuiltProduct(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(UploadBuiltProduct())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '1234')
        self.expectHidden(False)
        self.expectRemoteCommands(
            Expect('uploadFile', dict(workersrc='WebKitBuild/release.zip', workdir='wkdir',
                                      blocksize=1024 * 256, maxsize=None, keepstamp=False,
                                      writer=ExpectRemoteRef(remotetransfer.FileWriter)))
            + Expect.behavior(uploadFileWithContentsOfString('Dummy zip file content.'))
            + 0,
        )
        self.expectUploadedFile('public_html/archives/mac-sierra-x86_64-release/1234.zip')

        self.expectOutcome(result=SUCCESS, state_string='Uploaded built product')
        return self.runStep()

    def test_failure(self):
        self.setupStep(UploadBuiltProduct())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '1234')
        self.expectHidden(False)
        self.expectRemoteCommands(
            Expect('uploadFile', dict(workersrc='WebKitBuild/release.zip', workdir='wkdir',
                                      blocksize=1024 * 256, maxsize=None, keepstamp=False,
                                      writer=ExpectRemoteRef(remotetransfer.FileWriter)))
            + Expect.behavior(uploadFileWithContentsOfString('Dummy zip file content.'))
            + 1,
        )
        self.expectUploadedFile('public_html/archives/mac-sierra-x86_64-release/1234.zip')

        self.expectOutcome(result=FAILURE, state_string='Failed to upload built product')
        return self.runStep()


class TestDownloadBuiltProduct(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(DownloadBuiltProduct())
        self.setProperty('fullPlatform', 'ios-simulator-12')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '1234')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/CISupport/download-built-product', '--release', 'https://s3-us-west-2.amazonaws.com/ews-archives.webkit.org/ios-simulator-12-x86_64-release/1234.zip'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Downloaded built product')
        with current_hostname(EWS_BUILD_HOSTNAME):
            return self.runStep()

    def test_failure(self):
        self.setupStep(DownloadBuiltProduct())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '123456')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/CISupport/download-built-product', '--debug', 'https://s3-us-west-2.amazonaws.com/ews-archives.webkit.org/mac-sierra-x86_64-debug/123456.zip'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected failure.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to download built product from S3')
        with current_hostname(EWS_BUILD_HOSTNAME):
            return self.runStep()

    def test_deployment_skipped(self):
        self.setupStep(DownloadBuiltProduct())
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '123456')
        self.expectOutcome(result=SKIPPED)
        with current_hostname('test-ews-deployment.igalia.com'):
            return self.runStep()


class TestDownloadBuiltProductFromMaster(BuildStepMixinAdditions, unittest.TestCase):
    READ_LIMIT = 1000

    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    @staticmethod
    def downloadFileRecordingContents(limit, recorder):
        def behavior(command):
            reader = command.args['reader']
            data = reader.remote_read(limit)
            recorder(data)
            reader.remote_close()
        return behavior

    @defer.inlineCallbacks
    def test_success(self):
        self.setupStep(DownloadBuiltProductFromMaster(mastersrc=__file__))
        self.setProperty('fullPlatform', 'ios-simulator-12')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '1234')
        self.expectHidden(False)
        buf = []
        self.expectRemoteCommands(
            Expect('downloadFile', dict(
                workerdest='WebKitBuild/release.zip', workdir='wkdir',
                blocksize=1024 * 256, maxsize=None, mode=0o0644,
                reader=ExpectRemoteRef(remotetransfer.FileReader),
            ))
            + Expect.behavior(self.downloadFileRecordingContents(self.READ_LIMIT, buf.append))
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='downloading to release.zip')

        yield self.runStep()

        buf = b''.join(buf)
        self.assertEqual(len(buf), self.READ_LIMIT)
        with open(__file__, 'rb') as masterFile:
            data = masterFile.read(self.READ_LIMIT)
            if data != buf:
                self.assertEqual(buf, data)

    @defer.inlineCallbacks
    def test_failure(self):
        self.setupStep(DownloadBuiltProductFromMaster(mastersrc=__file__))
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
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to download built product from build master')
        yield self.runStep()


class TestExtractBuiltProduct(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(ExtractBuiltProduct())
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/CISupport/built-product-archive', '--platform=ios-simulator',  '--release', 'extract'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Extracted built product')
        return self.runStep()

    def test_failure(self):
        self.setupStep(ExtractBuiltProduct())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/CISupport/built-product-archive', '--platform=mac-sierra',  '--debug', 'extract'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected failure.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Extracted built product (failure)')
        return self.runStep()


class current_hostname(object):
    def __init__(self, hostname):
        self.hostname = hostname
        self.saved_hostname = None

    def __enter__(self):
        import steps
        self.saved_hostname = steps.CURRENT_HOSTNAME
        steps.CURRENT_HOSTNAME = self.hostname

    def __exit__(self, type, value, tb):
        import steps
        steps.CURRENT_HOSTNAME = self.saved_hostname


class TestTransferToS3(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(TransferToS3())
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
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Transferred archive to S3')
        with current_hostname(EWS_BUILD_HOSTNAME):
            return self.runStep()

    def test_failure(self):
        self.setupStep(TransferToS3())
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
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to transfer archive to S3')
        with current_hostname(EWS_BUILD_HOSTNAME):
            return self.runStep()

    def test_skipped(self):
        self.setupStep(TransferToS3())
        self.setProperty('fullPlatform', 'mac-highsierra')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '1234')
        self.expectOutcome(result=SKIPPED, state_string='Transferred archive to S3 (skipped)')
        with current_hostname('something-other-than-steps.EWS_BUILD_HOSTNAME'):
            return self.runStep()


class TestRunAPITests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'api_test_results.json'
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success_mac(self):
        self.setupStep(RunAPITests())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'release')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-api-tests', '--no-build', '--release', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='''...
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
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='run-api-tests')
        return self.runStep()

    def test_success_ios_simulator(self):
        self.setupStep(RunAPITests())
        self.setProperty('fullPlatform', 'ios-simulator-11')
        self.setProperty('platform', 'ios')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-api-tests', '--no-build', '--debug', '--verbose', f'--json-output={self.jsonFileName}', '--ios-simulator'],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='''...
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
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='run-api-tests')
        return self.runStep()

    def test_success_gtk(self):
        self.setupStep(RunAPITests())
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('platform', 'gtk')
        self.setProperty('configuration', 'release')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-gtk-tests', '--release', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='''...
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
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='run-api-tests')
        return self.runStep()

    def test_one_failure(self):
        self.setupStep(RunAPITests())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-api-tests', '--no-build', '--debug', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='''
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
            + 1,
        )
        self.expectOutcome(result=FAILURE, state_string='1 api test failed or timed out')
        return self.runStep()

    def test_multiple_failures_and_timeouts(self):
        self.setupStep(RunAPITests())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-api-tests', '--no-build', '--debug', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='''...
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
            + 4,
        )
        self.expectOutcome(result=FAILURE, state_string='4 api tests failed or timed out')
        return self.runStep()

    def test_unexpected_failure(self):
        self.setupStep(RunAPITests())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-api-tests', '--no-build', '--debug', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='Unexpected failure. Failed to run api tests.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='run-api-tests (failure)')
        return self.runStep()

    def test_no_failures_or_timeouts_with_disabled(self):
        self.setupStep(RunAPITests())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/run-api-tests', '--no-build', '--debug', '--verbose', f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='''...
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
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='run-api-tests')
        return self.runStep()


class TestRunAPITestsWithoutChange(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'api_test_results.json'
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success_mac(self):
        self.setupStep(RunAPITestsWithoutChange())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'release')
        self.setProperty('buildername', 'API-Tests-macOS-EWS')
        self.setProperty('buildnumber', '11525')
        self.setProperty('workername', 'ews155')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3',
                                 'Tools/Scripts/run-api-tests',
                                 '--no-build',
                                 '--release',
                                 '--verbose',
                                 f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='''...
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
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='run-api-tests-without-change')
        return self.runStep()

    def test_one_failure(self):
        self.setupStep(RunAPITestsWithoutChange())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'ios-simulator')
        self.setProperty('configuration', 'debug')
        self.setProperty('buildername', 'API-Tests-iOS-EWS')
        self.setProperty('buildnumber', '123')
        self.setProperty('workername', 'ews156')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3',
                                 'Tools/Scripts/run-api-tests',
                                 '--no-build',
                                 '--debug',
                                 '--verbose',
                                 f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='''
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
            + 1,
        )
        self.expectOutcome(result=FAILURE, state_string='1 api test failed or timed out')
        return self.runStep()

    def test_multiple_failures_gtk(self):
        self.setupStep(RunAPITestsWithoutChange())
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('platform', 'gtk')
        self.setProperty('configuration', 'debug')
        self.setProperty('buildername', 'API-Tests-GTK-EWS')
        self.setProperty('buildnumber', '13529')
        self.setProperty('workername', 'igalia4-gtk-wk2-ews')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3',
                                 'Tools/Scripts/run-gtk-tests',
                                 '--debug',
                                 f'--json-output={self.jsonFileName}'],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='''
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
            + 3,
        )
        self.expectOutcome(result=FAILURE, state_string='3 api tests failed or timed out')
        return self.runStep()


class TestArchiveTestResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(ArchiveTestResults())
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('platform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/CISupport/test-result-archive', '--platform=ios-simulator',  '--release', 'archive'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Archived test results')
        return self.runStep()

    def test_failure(self):
        self.setupStep(ArchiveTestResults())
        self.setProperty('fullPlatform', 'mac-catalina')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python3', 'Tools/CISupport/test-result-archive', '--platform=mac',  '--debug', 'archive'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected failure.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Archived test results (failure)')
        return self.runStep()


class TestUploadTestResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(UploadTestResults())
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '1234')
        self.setProperty('buildername', 'macOS-Sierra-Release-WK2-Tests-EWS')
        self.setProperty('buildnumber', '12')
        self.expectHidden(False)
        self.expectRemoteCommands(
            Expect('uploadFile', dict(workersrc='layout-test-results.zip', workdir='wkdir',
                                      blocksize=1024 * 256, maxsize=None, keepstamp=False,
                                      writer=ExpectRemoteRef(remotetransfer.FileWriter)))
            + Expect.behavior(uploadFileWithContentsOfString('Dummy zip file content.'))
            + 0,
        )
        self.expectUploadedFile('public_html/results/macOS-Sierra-Release-WK2-Tests-EWS/1234-12.zip')

        self.expectOutcome(result=SUCCESS, state_string='Uploaded test results')
        return self.runStep()

    def test_success_hash(self):
        self.setupStep(UploadTestResults())
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '8f75a5fa')
        self.setProperty('buildername', 'macOS-Sierra-Release-WK2-Tests-EWS')
        self.setProperty('buildnumber', '12')
        self.expectHidden(False)
        self.expectRemoteCommands(
            Expect('uploadFile', dict(workersrc='layout-test-results.zip', workdir='wkdir',
                                      blocksize=1024 * 256, maxsize=None, keepstamp=False,
                                      writer=ExpectRemoteRef(remotetransfer.FileWriter)))
            + Expect.behavior(uploadFileWithContentsOfString('Dummy zip file content.'))
            + 0,
        )
        self.expectUploadedFile('public_html/results/macOS-Sierra-Release-WK2-Tests-EWS/8f75a5fa-12.zip')

        self.expectOutcome(result=SUCCESS, state_string='Uploaded test results')
        return self.runStep()

    def test_success_with_identifier(self):
        self.setupStep(UploadTestResults(identifier='clean-tree'))
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('change_id', '37be32c5')
        self.setProperty('buildername', 'iOS-12-Simulator-WK2-Tests-EWS')
        self.setProperty('buildnumber', '120')
        self.expectHidden(False)
        self.expectRemoteCommands(
            Expect('uploadFile', dict(workersrc='layout-test-results.zip', workdir='wkdir',
                                      blocksize=1024 * 256, maxsize=None, keepstamp=False,
                                      writer=ExpectRemoteRef(remotetransfer.FileWriter)))
            + Expect.behavior(uploadFileWithContentsOfString('Dummy zip file content.'))
            + 0,
        )
        self.expectUploadedFile('public_html/results/iOS-12-Simulator-WK2-Tests-EWS/37be32c5-120-clean-tree.zip')

        self.expectOutcome(result=SUCCESS, state_string='Uploaded test results')
        return self.runStep()


class TestExtractTestResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(ExtractTestResults())
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
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Extracted test results')
        self.expectAddedURLs([call('view layout test results', 'https://ews-build.s3-us-west-2.amazonaws.com/macOS-Sierra-Release-WK2-Tests-EWS/r2468-12/results.html')])
        return self.runStep()

    def test_success_with_identifier(self):
        self.setupStep(ExtractTestResults(identifier='rerun'))
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
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Extracted test results')
        self.expectAddedURLs([call('view layout test results', 'https://ews-build.s3-us-west-2.amazonaws.com/iOS-12-Simulator-WK2-Tests-EWS/1234-12/results.html')])
        return self.runStep()

    def test_failure(self):
        self.setupStep(ExtractTestResults())
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
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='failed (2) (failure)')
        self.expectAddedURLs([call('view layout test results', 'https://ews-build.s3-us-west-2.amazonaws.com/macOS-Sierra-Release-WK2-Tests-EWS/1234-12/results.html')])
        return self.runStep()


class TestPrintConfiguration(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

        def test_success_mac(self):
            self.setupStep(PrintConfiguration())
            self.setProperty('buildername', 'macOS-Monterey-Release-WK2-Tests-EWS')
            self.setProperty('platform', 'mac-monterey')

            self.expectRemoteCommands(
                ExpectShell(command=['hostname'], workdir='wkdir', timeout=60, logEnviron=False) + 0
                + ExpectShell.log('stdio', stdout='ews150.apple.com'),
                ExpectShell(command=['df', '-hl'], workdir='wkdir', timeout=60, logEnviron=False) + 0
                + ExpectShell.log('stdio', stdout='''Filesystem     Size   Used  Avail Capacity iused  ifree %iused  Mounted on
    /dev/disk1s1  119Gi   95Gi   23Gi    81%  937959 9223372036853837848    0%   /
    /dev/disk1s4  119Gi   20Ki   23Gi     1%       0 9223372036854775807    0%   /private/var/vm
    /dev/disk0s3  119Gi   22Gi   97Gi    19%  337595          4294629684    0%   /Volumes/Data'''),
                ExpectShell(command=['date'], workdir='wkdir', timeout=60, logEnviron=False) + 0
                + ExpectShell.log('stdio', stdout='Tue Apr  9 15:30:52 PDT 2019'),
                ExpectShell(command=['sw_vers'], workdir='wkdir', timeout=60, logEnviron=False) + 0
                + ExpectShell.log('stdio', stdout='''ProductName:	macOS
    ProductVersion:	12.0.1
    BuildVersion:	21A558'''),
                ExpectShell(command=['system_profiler', 'SPSoftwareDataType', 'SPHardwareDataType'], workdir='wkdir', timeout=60, logEnviron=False) + 0
                + ExpectShell.log('stdio', stdout='Configuration version: Software: System Software Overview: System Version: macOS 11.4 (20F71) Kernel Version: Darwin 20.5.0 Boot Volume: Macintosh HD Boot Mode: Normal Computer Name: bot1020 User Name: WebKit Build Worker (buildbot) Secure Virtual Memory: Enabled System Integrity Protection: Enabled Time since boot: 27 seconds Hardware: Hardware Overview: Model Name: Mac mini Model Identifier: Macmini8,1 Processor Name: 6-Core Intel Core i7 Processor Speed: 3.2 GHz Number of Processors: 1 Total Number of Cores: 6 L2 Cache (per Core): 256 KB L3 Cache: 12 MB Hyper-Threading Technology: Enabled Memory: 32 GB System Firmware Version: 1554.120.19.0.0 (iBridge: 18.16.14663.0.0,0) Serial Number (system): C07DXXXXXXXX Hardware UUID: F724DE6E-706A-5A54-8D16-000000000000 Provisioning UDID: E724DE6E-006A-5A54-8D16-000000000000 Activation Lock Status: Disabled Xcode 12.5 Build version 12E262'),
                ExpectShell(command=['xcodebuild', '-sdk', '-version'], workdir='wkdir', timeout=60, logEnviron=False)
                + ExpectShell.log('stdio', stdout='''MacOSX12.0.sdk - macOS 12.0 (macosx12.0)
    SDKVersion: 12.0
    Path: /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.0.sdk
    PlatformVersion: 12.0
    PlatformPath: /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform
    ProductBuildVersion: 21A344
    ProductCopyright: 1983-2021 Apple Inc.
    ProductName: macOS
    ProductUserVisibleVersion: 12.0
    ProductVersion: 12.0
    iOSSupportVersion: 15.0

    Xcode 13.1
    Build version 13A1030d''')
                + 0,
            )
            self.expectOutcome(result=SUCCESS, state_string='OS: Monterey (12.0.1), Xcode: 13.1')
            return self.runStep()

        def test_success_ios_simulator(self):
            self.setupStep(PrintConfiguration())
            self.setProperty('buildername', 'Apple-iOS-15-Simulator-Release-WK2-Tests')
            self.setProperty('platform', 'ios-simulator-15')

            self.expectRemoteCommands(
                ExpectShell(command=['hostname'], workdir='wkdir', timeout=60, logEnviron=False) + 0
                + ExpectShell.log('stdio', stdout='ews152.apple.com'),
                ExpectShell(command=['df', '-hl'], workdir='wkdir', timeout=60, logEnviron=False) + 0
                + ExpectShell.log('stdio', stdout='''Filesystem     Size   Used  Avail Capacity iused  ifree %iused  Mounted on
    /dev/disk1s1  119Gi   95Gi   23Gi    81%  937959 9223372036853837848    0%   /
    /dev/disk1s4  119Gi   20Ki   23Gi     1%       0 9223372036854775807    0%   /private/var/vm
    /dev/disk0s3  119Gi   22Gi   97Gi    19%  337595          4294629684    0%   /Volumes/Data'''),
                ExpectShell(command=['date'], workdir='wkdir', timeout=60, logEnviron=False) + 0
                + ExpectShell.log('stdio', stdout='Tue Apr  9 15:30:52 PDT 2019'),
                ExpectShell(command=['sw_vers'], workdir='wkdir', timeout=60, logEnviron=False) + 0
                + ExpectShell.log('stdio', stdout='''ProductName:	macOS
    ProductVersion:	11.6
    BuildVersion:	20G165'''),
                ExpectShell(command=['system_profiler', 'SPSoftwareDataType', 'SPHardwareDataType'], workdir='wkdir', timeout=60, logEnviron=False) + 0
                + ExpectShell.log('stdio', stdout='Sample system information'),
                ExpectShell(command=['xcodebuild', '-sdk', '-version'], workdir='wkdir', timeout=60, logEnviron=False)
                + ExpectShell.log('stdio', stdout='''iPhoneSimulator15.0.sdk - Simulator - iOS 15.0 (iphonesimulator15.0)
    SDKVersion: 15.0
    Path: /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator15.0.sdk
    PlatformVersion: 15.0
    PlatformPath: /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform
    BuildID: 84856584-0587-11EC-B99C-6807972BB3D4
    ProductBuildVersion: 19A339
    ProductCopyright: 1983-2021 Apple Inc.
    ProductName: iPhone OS
    ProductVersion: 15.0

    Xcode 13.0
    Build version 13A233''')
                + 0,
            )
            self.expectOutcome(result=SUCCESS, state_string='OS: Big Sur (11.6), Xcode: 13.0')
            return self.runStep()

        def test_success_webkitpy(self):
            self.setupStep(PrintConfiguration())
            self.setProperty('platform', '*')

            self.expectRemoteCommands(
                ExpectShell(command=['hostname'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
                ExpectShell(command=['df', '-hl'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
                ExpectShell(command=['date'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
                ExpectShell(command=['sw_vers'], workdir='wkdir', timeout=60, logEnviron=False) + 0
                + ExpectShell.log('stdio', stdout='''ProductName:	macOS
    ProductVersion:	11.6
    BuildVersion:	20G165'''),
                ExpectShell(command=['system_profiler', 'SPSoftwareDataType', 'SPHardwareDataType'], workdir='wkdir', timeout=60, logEnviron=False) + 0
                + ExpectShell.log('stdio', stdout='Sample system information'),
                ExpectShell(command=['xcodebuild', '-sdk', '-version'], workdir='wkdir', timeout=60,
                            logEnviron=False) + 0
                + ExpectShell.log('stdio', stdout='''Xcode 13.0\nBuild version 13A233'''),
            )
            self.expectOutcome(result=SUCCESS, state_string='OS: Big Sur (11.6), Xcode: 13.0')
            return self.runStep()

    def test_success_linux_wpe(self):
        self.setupStep(PrintConfiguration())
        self.setProperty('platform', 'wpe')

        self.expectRemoteCommands(
            ExpectShell(command=['hostname'], workdir='wkdir', timeout=60, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='ews190'),
            ExpectShell(command=['df', '-hl'], workdir='wkdir', timeout=60, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='''Filesystem     Size   Used  Avail Capacity iused  ifree %iused  Mounted on
/dev/disk0s3  119Gi   22Gi   97Gi    19%  337595          4294629684    0%   /'''),
            ExpectShell(command=['date'], workdir='wkdir', timeout=60, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='Tue Apr  9 15:30:52 PDT 2019'),
            ExpectShell(command=['uname', '-a'], workdir='wkdir', timeout=60, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='''Linux kodama-ews 5.0.4-arch1-1-ARCH #1 SMP PREEMPT Sat Mar 23 21:00:33 UTC 2019 x86_64 GNU/Linux'''),
            ExpectShell(command=['uptime'], workdir='wkdir', timeout=60, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=' 6:31  up 22 seconds, 12:05, 2 users, load averages: 3.17 7.23 5.45'),
        )
        self.expectOutcome(result=SUCCESS, state_string='Printed configuration')
        return self.runStep()

    def test_success_linux_gtk(self):
        self.setupStep(PrintConfiguration())
        self.setProperty('platform', 'gtk')

        self.expectRemoteCommands(
            ExpectShell(command=['hostname'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
            ExpectShell(command=['df', '-hl'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
            ExpectShell(command=['date'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
            ExpectShell(command=['uname', '-a'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
            ExpectShell(command=['uptime'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Printed configuration')
        return self.runStep()

    def test_success_win(self):
        self.setupStep(PrintConfiguration())
        self.setProperty('platform', 'win')

        self.expectRemoteCommands(
            ExpectShell(command=['hostname'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
            ExpectShell(command=['df', '-hl'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Printed configuration')
        return self.runStep()

    def test_failure(self):
        self.setupStep(PrintConfiguration())
        self.setProperty('platform', 'ios-12')
        self.expectRemoteCommands(
            ExpectShell(command=['hostname'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
            ExpectShell(command=['df', '-hl'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
            ExpectShell(command=['date'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
            ExpectShell(command=['sw_vers'], workdir='wkdir', timeout=60, logEnviron=False) + 1
            + ExpectShell.log('stdio', stdout='''Upon execvpe sw_vers ['sw_vers'] in environment id 7696545650400
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
            ExpectShell(command=['system_profiler', 'SPSoftwareDataType', 'SPHardwareDataType'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
            ExpectShell(command=['xcodebuild', '-sdk', '-version'], workdir='wkdir', timeout=60, logEnviron=False)
            + ExpectShell.log('stdio', stdout='''Upon execvpe xcodebuild ['xcodebuild', '-sdk', '-version'] in environment id 7696545612416
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
            + 1,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to print configuration')
        return self.runStep()


class TestCleanGitRepo(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(CleanGitRepo())
        self.setProperty('buildername', 'Style-EWS')

        self.expectRemoteCommands(
            ExpectShell(command=['/bin/sh', '-c', 'rm -f .git/gc.log || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', 'git rebase --abort || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', 'git am --abort || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', 'git cherry-pick --abort || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'clean', '-f', '-d'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'checkout', 'origin/main', '-f'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='You are in detached HEAD state.'),
            ExpectShell(command=['git', 'branch', '-D', 'main'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='Deleted branch main (was 57015967fef9).'),
            ExpectShell(command=['git', 'branch', 'main'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout="Switched to a new branch 'main'"),
            ExpectShell(command=['/bin/sh', '-c', "git branch | grep -v ' main$' | xargs git branch -D || true"], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', "git remote | grep -v 'origin$' | xargs -L 1 git remote rm || true"], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'prune'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
        )
        self.expectOutcome(result=SUCCESS, state_string='Cleaned up git repository')
        return self.runStep()

    def test_success_wincairo(self):
        self.setupStep(CleanGitRepo())
        self.setProperty('buildername', 'WinCairo-EWS')
        self.setProperty('platform', 'wincairo')

        self.expectRemoteCommands(
            ExpectShell(command=['sh', '-c', r'del .git\gc.log || exit 0'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['sh', '-c', 'git rebase --abort || exit 0'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['sh', '-c', 'git am --abort || exit 0'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['sh', '-c', 'git cherry-pick --abort || exit 0'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'clean', '-f', '-d'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'checkout', 'origin/main', '-f'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='You are in detached HEAD state.'),
            ExpectShell(command=['git', 'branch', '-D', 'main'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='Deleted branch main (was 57015967fef9).'),
            ExpectShell(command=['git', 'branch', 'main'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout="Switched to a new branch 'main'"),
            ExpectShell(command=['sh', '-c', "git branch | grep -v ' main$' | xargs git branch -D || exit 0"], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['sh', '-c', "git remote | grep -v 'origin$' | xargs -L 1 git remote rm || exit 0"], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'prune'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
        )
        self.expectOutcome(result=SUCCESS, state_string='Cleaned up git repository')
        return self.runStep()

    def test_success_master(self):
        self.setupStep(CleanGitRepo(default_branch='master'))
        self.setProperty('buildername', 'Commit-Queue')

        self.expectRemoteCommands(
            ExpectShell(command=['/bin/sh', '-c', 'rm -f .git/gc.log || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', 'git rebase --abort || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', 'git am --abort || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', 'git cherry-pick --abort || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'clean', '-f', '-d'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'checkout', 'origin/master', '-f'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='You are in detached HEAD state.'),
            ExpectShell(command=['git', 'branch', '-D', 'master'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='Deleted branch master (was 57015967fef9).'),
            ExpectShell(command=['git', 'branch', 'master'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout="Switched to a new branch 'master'"),
            ExpectShell(command=['/bin/sh', '-c', "git branch | grep -v ' master$' | xargs git branch -D || true"], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', "git remote | grep -v 'origin$' | xargs -L 1 git remote rm || true"], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'prune'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
        )
        self.expectOutcome(result=SUCCESS, state_string='Cleaned up git repository')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CleanGitRepo())
        self.setProperty('buildername', 'Commit-Queue')

        self.expectRemoteCommands(
            ExpectShell(command=['/bin/sh', '-c', 'rm -f .git/gc.log || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', 'git rebase --abort || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', 'git am --abort || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', 'git cherry-pick --abort || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'clean', '-f', '-d'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'checkout', 'origin/main', '-f'], workdir='wkdir', timeout=300, logEnviron=False) + 128
            + ExpectShell.log('stdio', stdout='You are in detached HEAD state.'),
            ExpectShell(command=['git', 'branch', '-D', 'main'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='Deleted branch main (was 57015967fef9).'),
            ExpectShell(command=['git', 'branch', 'main'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout="Switched to a new branch 'main'"),
            ExpectShell(command=['/bin/sh', '-c', "git branch | grep -v ' main$' | xargs git branch -D || true"], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', "git remote | grep -v 'origin$' | xargs -L 1 git remote rm || true"], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'prune'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
        )
        self.expectOutcome(result=FAILURE, state_string='Encountered some issues during cleanup')
        return self.runStep()

    def test_branch(self):
        self.setupStep(CleanGitRepo())
        self.setProperty('buildername', 'Commit-Queue')
        self.setProperty('basename', 'safari-612-branch')

        self.expectRemoteCommands(
            ExpectShell(command=['/bin/sh', '-c', 'rm -f .git/gc.log || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', 'git rebase --abort || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', 'git am --abort || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', 'git cherry-pick --abort || true'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'clean', '-f', '-d'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'checkout', 'origin/main', '-f'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='You are in detached HEAD state.'),
            ExpectShell(command=['git', 'branch', '-D', 'main'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='Deleted branch main (was 57015967fef9).'),
            ExpectShell(command=['git', 'branch', 'main'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout="Switched to a new branch 'main'"),
            ExpectShell(command=['/bin/sh', '-c', "git branch | grep -v ' main$' | xargs git branch -D || true"], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['/bin/sh', '-c', "git remote | grep -v 'origin$' | xargs -L 1 git remote rm || true"], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
            ExpectShell(command=['git', 'prune'], workdir='wkdir', timeout=300, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=''),
        )
        self.expectOutcome(result=SUCCESS, state_string='Cleaned up git repository')
        return self.runStep()


class TestValidateChange(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

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
        self.setupStep(ValidateChange())
        self.setProperty('patch_id', '1234')
        self.setProperty('bug_id', '5678')
        self.setProperty('skip_validation', True)
        self.expectOutcome(result=SKIPPED, state_string='Validated change (skipped)')
        return self.runStep()

    def test_skipped_pr(self):
        self.setupStep(ValidateChange())
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('skip_validation', True)
        self.expectOutcome(result=SKIPPED, state_string='Validated change (skipped)')
        return self.runStep()

    def test_success_patch(self):
        self.setupStep(ValidateChange(verifyBugClosed=False))
        ValidateChange.get_patch_json = lambda x, patch_id: self.get_patch()
        self.setProperty('patch_id', '425806')
        self.expectOutcome(result=SUCCESS, state_string='Validated change')
        rc = self.runStep()
        self.assertEqual(self.getProperty('fast_commit_queue'), None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_success_pr(self):
        self.setupStep(ValidateChange(verifyBugClosed=False))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request)
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expectOutcome(result=SUCCESS, state_string='Validated change')
        rc = self.runStep()
        self.assertEqual(self.getProperty('fast_commit_queue'), None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_success_pr_blocked(self):
        self.setupStep(ValidateChange(verifyBugClosed=False))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request, labels=['merging-blocked'])
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expectOutcome(result=SUCCESS, state_string='Validated change')
        rc = self.runStep()
        self.assertEqual(self.getProperty('fast_commit_queue'), None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_obsolete_patch(self):
        self.setupStep(ValidateChange(verifyBugClosed=False))
        ValidateChange.get_patch_json = lambda x, patch_id: self.get_patch(obsolete=1)
        self.setProperty('patch_id', '425806')
        self.expectOutcome(result=FAILURE, state_string='Patch 425806 is obsolete')
        rc = self.runStep()
        self.assertEqual(self.getProperty('fast_commit_queue'), None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_obsolete_pr(self):
        self.setupStep(ValidateChange(verifyBugClosed=False))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request)
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '1ad60d45a112301f7b9f93dac06134524dae8480')
        self.expectOutcome(result=FAILURE, state_string='Hash 1ad60d45 on PR 1234 is outdated')
        rc = self.runStep()
        self.assertEqual(self.getProperty('fast_commit_queue'), None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_deleted_pr(self):
        self.setupStep(ValidateChange(verifyBugClosed=True))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: False
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '1ad60d45a112301f7b9f93dac06134524dae8480')
        self.expectOutcome(result=FAILURE, state_string='Pull request 1234 is already closed')
        rc = self.runStep()
        self.assertEqual(self.getProperty('fast_commit_queue'), None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_fast_cq_patches_trigger_fast_cq_mode(self):
        fast_cq_patch_titles = ('REVERT OF r1234', 'revert of r1234', 'REVERT of 123456@main', '[fast-cq]Patch', '[FAST-cq] patch', 'fast-cq-patch', 'FAST-CQ Patch')
        for fast_cq_patch_title in fast_cq_patch_titles:
            self.setupStep(ValidateChange(verifyBugClosed=False))
            ValidateChange.get_patch_json = lambda x, patch_id: self.get_patch(title=fast_cq_patch_title)
            self.setProperty('patch_id', '425806')
            self.expectOutcome(result=SUCCESS, state_string='Validated change')
            rc = self.runStep()
            self.assertEqual(self.getProperty('fast_commit_queue'), True, f'fast_commit_queue is not set, patch title: {fast_cq_patch_title}')
        return rc

    def test_merge_queue(self):
        self.setupStep(ValidateChange(verifyMergeQueue=True))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request, labels=['merge-queue'])
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expectOutcome(result=SUCCESS, state_string='Validated change')
        rc = self.runStep()
        self.assertEqual(self.getProperty('fast_commit_queue'), None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_merge_queue_blocked(self):
        self.setupStep(ValidateChange(verifyMergeQueue=True))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request, labels=['merge-queue', 'merging-blocked'])
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expectOutcome(result=FAILURE, state_string="PR 1234 has been marked as 'merging-blocked'")
        rc = self.runStep()
        self.assertEqual(self.getProperty('fast_commit_queue'), None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_no_merge_queue(self):
        self.setupStep(ValidateChange(verifyMergeQueue=True))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request)
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expectOutcome(result=FAILURE, state_string='PR 1234 does not have a merge queue label')
        rc = self.runStep()
        self.assertEqual(self.getProperty('fast_commit_queue'), None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_draft(self):
        self.setupStep(ValidateChange(verifyNoDraftForMergeQueue=True))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request, draft=True)
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expectOutcome(result=FAILURE, state_string='PR 1234 is a draft pull request')
        rc = self.runStep()
        self.assertEqual(self.getProperty('fast_commit_queue'), None, 'fast_commit_queue is unexpectedly set')
        return rc

    def test_no_draft(self):
        self.setupStep(ValidateChange(verifyNoDraftForMergeQueue=True))
        ValidateChange.get_pr_json = lambda x, pull_request, repository_url=None, retry=None: self.get_pr(pr_number=pull_request, draft=False)
        self.setProperty('github.number', '1234')
        self.setProperty('repository', 'https://github.com/WebKit/WebKit')
        self.setProperty('github.head.sha', '7496f8ecc4cc8011f19c8cc1bc7b18fe4a88ad5c')
        self.expectOutcome(result=SUCCESS, state_string='Validated change')
        rc = self.runStep()
        self.assertEqual(self.getProperty('fast_commit_queue'), None, 'fast_commit_queue is unexpectedly set')
        return rc


class TestValidateCommitterAndReviewer(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        Contributors.load = mock_load_contributors
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success_patch(self):
        self.setupStep(ValidateCommitterAndReviewer())
        self.setProperty('patch_id', '1234')
        self.setProperty('patch_committer', 'committer@webkit.org')
        self.setProperty('reviewer', 'reviewer@apple.com')
        self.expectHidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expectOutcome(result=SUCCESS, state_string='Validated commiter and reviewer')
        rc = self.runStep()
        self.assertEqual(self.getProperty('reviewers_full_names'), ['WebKit Reviewer'])
        return rc

    def test_success_pr(self):
        self.setupStep(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-reviewer']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.expectHidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expectOutcome(result=SUCCESS, state_string='Validated commiter and reviewer')
        rc = self.runStep()
        self.assertEqual(self.getProperty('reviewers_full_names'), ['WebKit Reviewer'])
        return rc

    def test_success_pr_duplicate(self):
        self.setupStep(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-reviewer', 'webkit-reviewer']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.expectHidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expectOutcome(result=SUCCESS, state_string='Validated commiter and reviewer')
        rc = self.runStep()
        self.assertEqual(self.getProperty('reviewers_full_names'), ['WebKit Reviewer'])
        return rc

    def test_success_no_reviewer_patch(self):
        self.setupStep(ValidateCommitterAndReviewer())
        self.setProperty('patch_id', '1234')
        self.setProperty('patch_committer', 'reviewer@apple.com')
        self.expectHidden(False)
        self.expectOutcome(result=SUCCESS, state_string='Validated committer, reviewer not found')
        return self.runStep()

    def test_success_no_reviewer_pr(self):
        self.setupStep(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: []
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-reviewer'])
        self.expectHidden(False)
        self.expectOutcome(result=SUCCESS, state_string='Validated committer, reviewer not found')
        return self.runStep()

    def test_failure_load_contributors_patch(self):
        self.setupStep(ValidateCommitterAndReviewer())
        self.setProperty('patch_id', '1234')
        self.setProperty('patch_committer', 'abc@webkit.org')
        Contributors.load = lambda *args, **kwargs: ({}, [])
        self.expectHidden(False)
        self.expectOutcome(result=FAILURE, state_string='Failed to get contributors information')
        return self.runStep()

    def test_failure_load_contributors_pr(self):
        self.setupStep(ValidateCommitterAndReviewer())
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['abc'])
        Contributors.load = lambda *args, **kwargs: ({}, [])
        self.expectHidden(False)
        self.expectOutcome(result=FAILURE, state_string='Failed to get contributors information')
        return self.runStep()

    def test_failure_invalid_committer_patch(self):
        self.setupStep(ValidateCommitterAndReviewer())
        self.setProperty('patch_id', '1234')
        self.setProperty('patch_committer', 'abc@webkit.org')
        self.expectHidden(False)
        self.expectOutcome(result=FAILURE, state_string='abc@webkit.org does not have committer permissions')
        return self.runStep()

    def test_failure_invalid_committer_pr(self):
        self.setupStep(ValidateCommitterAndReviewer())
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['abc'])
        self.expectHidden(False)
        self.expectOutcome(result=FAILURE, state_string='abc does not have committer permissions')
        return self.runStep()

    def test_failure_invalid_reviewer_patch(self):
        self.setupStep(ValidateCommitterAndReviewer())
        self.setProperty('patch_id', '1234')
        self.setProperty('patch_committer', 'reviewer@apple.com')
        self.setProperty('reviewer', 'committer@webkit.org')
        self.expectHidden(False)
        self.expectOutcome(result=FAILURE, state_string='committer@webkit.org does not have reviewer permissions')
        return self.runStep()

    def test_failure_invalid_reviewer_pr(self):
        self.setupStep(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-commit-queue']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-reviewer'])
        self.expectHidden(False)
        self.expectOutcome(result=FAILURE, state_string='webkit-commit-queue does not have reviewer permissions')
        return self.runStep()

    def test_load_contributors_from_disk(self):
        contributors = filter(lambda element: element.get('name') == 'Aakash Jain', Contributors().load_from_disk()[0])
        self.assertEqual(list(contributors)[0]['emails'][0], 'aakash_jain@apple.com')

    def test_success_pr_validators(self):
        self.setupStep(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-reviewer', 'geoffreygaren']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'apple')
        self.expectHidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expectOutcome(result=SUCCESS, state_string='Validated commiter and reviewer')
        rc = self.runStep()
        self.assertEqual(self.getProperty('reviewers_full_names'), ['WebKit Reviewer'])
        return rc

    def test_success_pr_validators_case(self):
        self.setupStep(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-reviewer', 'jonwbedard']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'apple')
        self.expectHidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expectOutcome(result=SUCCESS, state_string='Validated commiter and reviewer')
        rc = self.runStep()
        self.assertEqual(self.getProperty('reviewers_full_names'), ['WebKit Reviewer'])
        return rc

    def test_success_no_pr_validators(self):
        self.setupStep(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-reviewer']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'security')
        self.expectHidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expectOutcome(result=SUCCESS, state_string='Validated commiter and reviewer')
        rc = self.runStep()
        self.assertEqual(self.getProperty('reviewers_full_names'), ['WebKit Reviewer'])
        return rc

    def test_failure_pr_validators(self):
        self.setupStep(ValidateCommitterAndReviewer())
        ValidateCommitterAndReviewer.get_reviewers = lambda x, pull_request, repository_url=None: ['webkit-reviewer']
        self.setProperty('github.number', '1234')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'apple')
        self.expectHidden(False)
        self.assertEqual(ValidateCommitterAndReviewer.haltOnFailure, False)
        self.expectOutcome(result=FAILURE, state_string="Landing changes on 'apple' remote requires validation from @geoffreygaren, @markcgee, @rjepstein, @JonWBedard or @ryanhaddad")
        return self.runStep()


class TestCheckStatusOnEWSQueues(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        CheckStatusOnEWSQueues.get_change_status = lambda cls, change_id, queue: SUCCESS
        self.setupStep(CheckStatusOnEWSQueues())
        self.setProperty('patch_id', '1234')
        self.expectOutcome(result=SUCCESS, state_string='Checked change status on other queues')
        rc = self.runStep()
        self.assertEqual(self.getProperty('passed_mac_wk2'), True)
        return rc

    def test_success_hash(self):
        CheckStatusOnEWSQueues.get_change_status = lambda cls, change_id, queue: SUCCESS
        self.setupStep(CheckStatusOnEWSQueues())
        self.setProperty('github.head.sha', '0e5b5facb6445ca7a1feb46cee6322189df5282c')
        self.expectOutcome(result=SUCCESS, state_string='Checked change status on other queues')
        rc = self.runStep()
        self.assertEqual(self.getProperty('passed_mac_wk2'), True)
        return rc

    def test_failure(self):
        self.setupStep(CheckStatusOnEWSQueues())
        self.setProperty('patch_id', '1234')
        CheckStatusOnEWSQueues.get_change_status = lambda cls, change_id, queue: FAILURE
        self.expectOutcome(result=SUCCESS, state_string='Checked change status on other queues')
        rc = self.runStep()
        self.assertEqual(self.getProperty('passed_mac_wk2'), None)
        return rc


class TestPushCommitToWebKitRepo(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        GitHub.credentials = lambda user=None: ('webkit-commit-queue', 'password')
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(PushCommitToWebKitRepo())
        self.setProperty('patch_id', '1234')
        self.setProperty('remote', 'origin')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        env=dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password'),
                        command=['git', 'push', 'origin', 'HEAD:main']) +
            ExpectShell.log('stdio', stdout=' 4c3bac1de151...b94dc426b331 ') +
            0,
        )
        self.expectOutcome(result=SUCCESS, state_string='')
        with current_hostname(EWS_BUILD_HOSTNAME):
            rc = self.runStep()
        self.assertEqual(self.getProperty('landed_hash'), 'b94dc426b331')
        return rc

    def test_failure_retry(self):
        self.setupStep(PushCommitToWebKitRepo())
        self.setProperty('patch_id', '2345')
        self.setProperty('remote', 'origin')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        env=dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password'),
                        command=['git', 'push', 'origin', 'HEAD:main']) +
            ExpectShell.log('stdio', stdout='Unexpected failure') +
            2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to push commit to Webkit repository')
        with current_hostname(EWS_BUILD_HOSTNAME):
            rc = self.runStep()
        self.assertEqual(self.getProperty('retry_count'), 1)
        self.assertEqual(self.getProperty('landed_hash'), None)
        return rc

    def test_failure_patch(self):
        self.setupStep(PushCommitToWebKitRepo())
        self.setProperty('remote', 'origin')
        self.setProperty('patch_id', '2345')
        self.setProperty('retry_count', PushCommitToWebKitRepo.MAX_RETRY)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        env=dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password'),
                        command=['git', 'push', 'origin', 'HEAD:main']) +
            ExpectShell.log('stdio', stdout='Unexpected failure') +
            2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to push commit to Webkit repository')
        with current_hostname(EWS_BUILD_HOSTNAME):
            rc = self.runStep()
        self.assertEqual(self.getProperty('build_finish_summary'), 'Failed to commit to WebKit repository')
        self.assertEqual(self.getProperty('comment_text'), 'commit-queue failed to commit attachment 2345 to WebKit repository. To retry, please set cq+ flag again.')
        return rc

    def test_failure_pr(self):
        self.setupStep(PushCommitToWebKitRepo())
        self.setProperty('github.number', '1234')
        self.setProperty('remote', 'origin')
        self.setProperty('retry_count', PushCommitToWebKitRepo.MAX_RETRY)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        env=dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password'),
                        command=['git', 'push', 'origin', 'HEAD:main']) +
            ExpectShell.log('stdio', stdout='Unexpected failure') +
            2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to push commit to Webkit repository')
        with current_hostname(EWS_BUILD_HOSTNAME):
            rc = self.runStep()
        self.assertEqual(self.getProperty('build_finish_summary'), 'Failed to commit to WebKit repository')
        self.assertEqual(self.getProperty('comment_text'), 'merge-queue failed to commit PR to repository. To retry, remove any blocking labels and re-apply merge-queue label')
        return rc


class TestDetermineLandedIdentifier(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def mock_commits_webkit_org(self, identifier=None):
        return patch('steps.TwistedAdditions.request', lambda *args, **kwargs: TwistedAdditions.Response(
            status_code=200,
            content=json.dumps(dict(identifier=identifier) if identifier else dict(status='Not Found')).encode('utf-8'),
        ))

    @classmethod
    def mock_sleep(cls):
        return patch('twisted.internet.task.deferLater', lambda *_, **__: None)

    @defer.inlineCallbacks
    def test_success_pr(self):
        with self.mock_commits_webkit_org(), self.mock_sleep():
            self.setupStep(DetermineLandedIdentifier())
            self.setProperty('landed_hash', '14dbf1155cf5')
            self.setProperty('github.number', '1234')
            self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        command=['git', 'log', '-1', '--no-decorate']) +
                ExpectShell.log('stdio', stdout=''''commit 220797@main (14dbf1155cf56a1dd4d86a847e61af3c3e5d2ca5, r256729)
Author: Aakash Jain <aakash_jain@apple.com>
Date:   Mon Feb 17 15:09:42 2020 +0000

    [ews] add SetBuildSummary step for Windows EWS
    https://bugs.webkit.org/show_bug.cgi?id=207556
    
    Reviewed by Jonathan Bedard.
    
    * BuildSlaveSupport/ews-build/factories.py:
    (WindowsFactory.__init__):
    (GTKBuildAndTestFactory.__init__):
    * BuildSlaveSupport/ews-build/factories_unittest.py:
    (TestBuildAndTestsFactory.test_windows_factory): Added unit-test.
    
    
    Canonical link: https://commits.webkit.org/220797@main
    git-svn-id: https://svn.webkit.org/repository/webkit/trunk@256729 268f45cc-cd09-0410-ab3c-d52691b4dbfc''') +
                0,
            )
            self.expectOutcome(result=SUCCESS, state_string='Identifier: 220797@main')
            with current_hostname(EWS_BUILD_HOSTNAME):
                yield self.runStep()

        self.assertEqual(self.getProperty('comment_text'), 'Committed 220797@main (14dbf1155cf5): <https://commits.webkit.org/220797@main>\n\nReviewed commits have been landed. Closing PR #1234 and removing active labels.')
        self.assertEqual(self.getProperty('build_summary'), 'Committed 220797@main')

    @defer.inlineCallbacks
    def test_success_gardening_pr(self):
        with self.mock_commits_webkit_org(), self.mock_sleep():
            self.setupStep(DetermineLandedIdentifier())
            self.setProperty('landed_hash', '5dc27962b4c5')
            self.setProperty('github.number', '1234')
            self.setProperty('is_test_gardening', True)
            self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        command=['git', 'log', '-1', '--no-decorate']) +
                ExpectShell.log('stdio', stdout=''''commit 5dc27962b4c5bdfd17d17faa785f70abbb0550ed
Author: Matteo Flores <matteo_flores@apple.com>
Date:   Fri Apr 22 21:24:12 2022 +0000

    REBASLINE: [ Monterey ] fast/text/khmer-lao-font.html is a constant text failure
    
    https://bugs.webkit.org/show_bug.cgi?id=238917
    
    Unreviewed test gardening.
    
    * platform/mac-bigsur/fast/text/khmer-lao-font-expected.txt: Copied from LayoutTests/platform/mac/fast/text/khmer-lao-font-expected.txt.
    * platform/mac/fast/text/khmer-lao-font-expected.txt:
    
    Canonical link: https://commits.webkit.org/249903@main
    git-svn-id: https://svn.webkit.org/repository/webkit/trunk@293254 268f45cc-cd09-0410-ab3c-d52691b4dbfc''') +
                0,
            )
            self.expectOutcome(result=SUCCESS, state_string='Identifier: 249903@main')
            with current_hostname(EWS_BUILD_HOSTNAME):
                yield self.runStep()

        self.assertEqual(self.getProperty('comment_text'), 'Test gardening commit 249903@main (5dc27962b4c5): <https://commits.webkit.org/249903@main>\n\nReviewed commits have been landed. Closing PR #1234 and removing active labels.')
        self.assertEqual(self.getProperty('build_summary'), 'Committed 249903@main')

    @defer.inlineCallbacks
    def test_success_pr_fallback(self):
        with self.mock_commits_webkit_org(identifier='220797@main'), self.mock_sleep():
            self.setupStep(DetermineLandedIdentifier())
            self.setProperty('landed_hash', '5dc27962b4c5')
            self.setProperty('github.number', '1234')
            self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        command=['git', 'log', '-1', '--no-decorate']) +
                ExpectShell.log('stdio', stdout='') +
                0,
            )
            self.expectOutcome(result=SUCCESS, state_string='Identifier: 220797@main')
            with current_hostname(EWS_BUILD_HOSTNAME):
                yield self.runStep()

        self.assertEqual(self.getProperty('comment_text'), 'Committed 220797@main (5dc27962b4c5): <https://commits.webkit.org/220797@main>\n\nReviewed commits have been landed. Closing PR #1234 and removing active labels.')
        self.assertEqual(self.getProperty('build_summary'), 'Committed 220797@main')

    @defer.inlineCallbacks
    def test_pr_no_identifier(self):
        with self.mock_commits_webkit_org(), self.mock_sleep():
            self.setupStep(DetermineLandedIdentifier())
            self.setProperty('landed_hash', '5dc27962b4c5')
            self.setProperty('github.number', '1234')
            self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        command=['git', 'log', '-1', '--no-decorate']) +
                ExpectShell.log('stdio', stdout='') +
                0,
            )
            self.expectOutcome(result=FAILURE, state_string='Failed to determine identifier')
            with current_hostname(EWS_BUILD_HOSTNAME):
                yield self.runStep()

        self.assertEqual(self.getProperty('comment_text'), 'Committed ? (5dc27962b4c5): <https://commits.webkit.org/5dc27962b4c5>\n\nReviewed commits have been landed. Closing PR #1234 and removing active labels.')
        self.assertEqual(self.getProperty('build_summary'), 'Committed 5dc27962b4c5')

    @defer.inlineCallbacks
    def test_success_patch(self):
        with self.mock_commits_webkit_org(identifier='220797@main'), self.mock_sleep():
            self.setupStep(DetermineLandedIdentifier())
            self.setProperty('landed_hash', '5dc27962b4c5')
            self.setProperty('patch_id', '1234')
            self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        command=['git', 'log', '-1', '--no-decorate']) +
                ExpectShell.log('stdio', stdout='') +
                0,
            )
            self.expectOutcome(result=SUCCESS, state_string='Identifier: 220797@main')
            with current_hostname(EWS_BUILD_HOSTNAME):
                yield self.runStep()

        self.assertEqual(self.getProperty('comment_text'), 'Committed 220797@main (5dc27962b4c5): <https://commits.webkit.org/220797@main>\n\nAll reviewed patches have been landed. Closing bug and clearing flags on attachment 1234.')
        self.assertEqual(self.getProperty('build_summary'), 'Committed 220797@main')

    @defer.inlineCallbacks
    def test_patch_no_identifier(self):
        with self.mock_commits_webkit_org(), self.mock_sleep():
            self.setupStep(DetermineLandedIdentifier())
            self.setProperty('landed_hash', '5dc27962b4c5')
            self.setProperty('patch_id', '1234')
            self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        command=['git', 'log', '-1', '--no-decorate']) +
                ExpectShell.log('stdio', stdout='') +
                0,
            )
            self.expectOutcome(result=FAILURE, state_string='Failed to determine identifier')
            with current_hostname(EWS_BUILD_HOSTNAME):
                yield self.runStep()

        self.assertEqual(self.getProperty('comment_text'), 'Committed ? (5dc27962b4c5): <https://commits.webkit.org/5dc27962b4c5>\n\nAll reviewed patches have been landed. Closing bug and clearing flags on attachment 1234.')
        self.assertEqual(self.getProperty('build_summary'), 'Committed 5dc27962b4c5')


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
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        previous_steps = {
            CheckOutSpecificRevision.name: self.MockPreviousStep(),
            CheckOutSource.name: self.MockPreviousStep(),
        }
        ShowIdentifier.getLastBuildStepByName = lambda _, name: previous_steps.get(name, self.MockPreviousStep())
        self.setupStep(ShowIdentifier())
        self.setProperty('ews_revision', '51a6aec9f664')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/git-webkit', 'find', '51a6aec9f664']) +
            ExpectShell.log('stdio', stdout='Identifier: 233175@main') +
            0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Identifier: 233175@main')
        rc = self.runStep()
        self.assertEqual(self.getProperty('identifier'), '233175@main')
        self.assertEqual(previous_steps[CheckOutSpecificRevision.name].text, 'Updated to 233175@main')
        self.assertEqual(previous_steps[CheckOutSpecificRevision.name].url, 'https://commits.webkit.org/233175@main')
        self.assertEqual(previous_steps[CheckOutSource.name].text, None)
        self.assertEqual(previous_steps[CheckOutSource.name].url, None)
        return rc

    def test_success_pull_request(self):
        previous_steps = {
            CheckOutSpecificRevision.name: self.MockPreviousStep(),
            CheckOutSource.name: self.MockPreviousStep(),
        }
        ShowIdentifier.getLastBuildStepByName = lambda _, name: previous_steps.get(name, self.MockPreviousStep())
        self.setupStep(ShowIdentifier())
        self.setProperty('got_revision', '51a6aec9f664')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/git-webkit', 'find', '51a6aec9f664']) +
            ExpectShell.log('stdio', stdout='Identifier: 233175@main') +
            0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Identifier: 233175@main')
        rc = self.runStep()
        self.assertEqual(self.getProperty('identifier'), '233175@main')
        self.assertEqual(previous_steps[CheckOutSpecificRevision.name].text, None)
        self.assertEqual(previous_steps[CheckOutSpecificRevision.name].url, None)
        self.assertEqual(previous_steps[CheckOutSource.name].text, 'Updated to 233175@main')
        self.assertEqual(previous_steps[CheckOutSource.name].url, 'https://commits.webkit.org/233175@main')
        return rc

    def test_prioritized(self):
        self.setupStep(ShowIdentifier())
        self.setProperty('ews_revision', '51a6aec9f664')
        self.setProperty('got_revision', '9f66451a6aec')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/git-webkit', 'find', '51a6aec9f664']) +
            ExpectShell.log('stdio', stdout='Identifier: 233175@main') +
            0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Identifier: 233175@main')
        rc = self.runStep()
        self.assertEqual(self.getProperty('identifier'), '233175@main')
        return rc

    def test_failure(self):
        self.setupStep(ShowIdentifier())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        command=['python3', 'Tools/Scripts/git-webkit', 'find', 'HEAD']) +
            ExpectShell.log('stdio', stdout='Unexpected failure') +
            2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to find identifier')
        return self.runStep()


class TestFetchBranches(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(FetchBranches())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        command=['git', 'fetch', 'origin', '--prune']) +
            ExpectShell.log('stdio', stdout='   fb192c1de607..afb17ed1708b  main       -> origin/main\n') +
            0,
        )
        self.expectOutcome(result=SUCCESS)
        return self.runStep()

    def test_success_remote(self):
        GitHub.credentials = lambda user=None: ('webkit-commit-queue', 'password')
        self.setupStep(FetchBranches())
        self.setProperty('project', 'WebKit/WebKit-security')
        self.setProperty('remote', 'security')
        env = dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password')

        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=env,
                command=['git', 'fetch', 'origin', '--prune'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=env,
                command=['git', 'config', 'credential.helper', '!echo_credentials() { sleep 1; echo "username=${GIT_USER}"; echo "password=${GIT_PASSWORD}"; }; echo_credentials'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=env,
                command=['/bin/sh', '-c', 'git remote add security https://github.com/WebKit/WebKit-security.git || true'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=env,
                command=['git', 'remote', 'set-url', 'security', 'https://github.com/WebKit/WebKit-security.git'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=env,
                command=['git', 'fetch', 'security', '--prune'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS)
        return self.runStep()

    def test_failure(self):
        self.setupStep(FetchBranches())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=300,
                        logEnviron=False,
                        command=['git', 'fetch',  'origin', '--prune']) +
            ExpectShell.log('stdio', stdout="fatal: unable to access 'https://github.com/WebKit/WebKit/': Could not resolve host: github.com\n") +
            2,
        )
        self.expectOutcome(result=FAILURE)
        return self.runStep()


class TestInstallBuiltProduct(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(InstallBuiltProduct())
        self.setProperty('fullPlatform', 'ios-14')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/Scripts/install-built-product', '--platform=ios-14', '--release'],
                        logEnviron=True,
                        timeout=1200,
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Installed Built Product')
        return self.runStep()

    def test_failure(self):
        self.setupStep(InstallBuiltProduct())
        self.setProperty('fullPlatform', 'ios-14')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/Scripts/install-built-product', '--platform=ios-14', '--debug'],
                        logEnviron=True,
                        timeout=1200,
                        )
            + ExpectShell.log('stdio', stdout='Unexpected error.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Installed Built Product (failure)')
        return self.runStep()


class TestValidateRemote(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_patch(self):
        self.setupStep(ValidateRemote())
        self.setProperty('patch_id', '1234')
        self.expectOutcome(result=SKIPPED, state_string='finished (skipped)')
        return self.runStep()

    def test_origin(self):
        self.setupStep(ValidateRemote())
        self.setProperty('remote', 'origin')
        self.expectOutcome(result=SKIPPED, state_string='finished (skipped)')
        return self.runStep()

    def test_success(self):
        self.setupStep(ValidateRemote())
        self.setProperty('remote', 'security')
        self.setProperty('github.base.ref', 'safari-000-branch')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                command=['git', 'merge-base', '--is-ancestor', 'remotes/security/safari-000-branch', 'remotes/origin/safari-000-branch'],
            ) + 1,
        )
        self.expectOutcome(result=SUCCESS, state_string="Verified 'WebKit/WebKit' does not own 'safari-000-branch'")
        return self.runStep()

    def test_failure(self):
        self.setupStep(ValidateRemote())
        self.setProperty('remote', 'security')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                command=['git', 'merge-base', '--is-ancestor', 'remotes/security/main', 'remotes/origin/main'],
            ) + 0,
        )
        self.expectOutcome(result=FAILURE, state_string="Cannot land on 'main', it is owned by 'WebKit/WebKit'")
        rc = self.runStep()
        self.assertEqual(self.getProperty('comment_text'), "Cannot land on 'main', it is owned by 'WebKit/WebKit', blocking PR #1234.\nMake a pull request against 'WebKit/WebKit' to land this change.")
        self.assertEqual(self.getProperty('build_finish_summary'), "Cannot land on 'main', it is owned by 'WebKit/WebKit'")
        return rc


class TestMapBranchAlias(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_patch(self):
        self.setupStep(MapBranchAlias())
        self.setProperty('patch_id', '1234')
        self.expectOutcome(result=SKIPPED, state_string='finished (skipped)')
        return self.runStep()

    def test_main(self):
        self.setupStep(MapBranchAlias())
        self.setProperty('remote', 'origin')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.number', '1234')
        self.expectOutcome(result=SKIPPED, state_string='finished (skipped)')
        return self.runStep()

    def test_prod_branch(self):
        self.setupStep(MapBranchAlias())
        self.setProperty('remote', 'origin')
        self.setProperty('github.base.ref', 'safari-000-branch')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                timeout=60,
                command=['git', 'branch', '-a', '--contains', 'remotes/origin/safari-000-branch'],
            ) + 0
            + ExpectShell.log('stdio', stdout='  safari-000-branch\n  remotes/origin/safari-000-branch\n  remotes/origin/safari-alias\n  remotes/origin/eng/pr-branch\n'),
        )
        self.expectOutcome(result=SUCCESS, state_string="'safari-000-branch' is the prevailing alias")
        rc = self.runStep()
        self.assertEqual(self.getProperty('github.base.ref'), 'safari-000-branch')
        return rc

    def test_main_override(self):
        self.setupStep(MapBranchAlias())
        self.setProperty('remote', 'origin')
        self.setProperty('github.base.ref', 'safari-000-branch')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                timeout=60,
                command=['git', 'branch', '-a', '--contains', 'remotes/origin/safari-000-branch'],
            ) + 0
            + ExpectShell.log('stdio', stdout='  safari-000-branch\n  remotes/origin/safari-000-branch\n  remotes/origin/safari-alias\n  remotes/origin/eng/pr-branch\n  remotes/origin/main\n'),
        )
        self.expectOutcome(result=SUCCESS, state_string="'main' is the prevailing alias")
        rc = self.runStep()
        self.assertEqual(self.getProperty('github.base.ref'), 'main')
        return rc

    def test_alias_branch(self):
        self.setupStep(MapBranchAlias())
        self.setProperty('remote', 'origin')
        self.setProperty('github.base.ref', 'safari-alias')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                timeout=60,
                command=['git', 'branch', '-a', '--contains', 'remotes/origin/safari-alias'],
            ) + 0
            + ExpectShell.log('stdio', stdout='  safari-alias\n  remotes/origin/safari-000-branch\n  remotes/origin/safari-alias\n  remotes/origin/eng/pr-branch\n'),
        )
        self.expectOutcome(result=SUCCESS, state_string="'safari-000-branch' is the prevailing alias")
        rc = self.runStep()
        self.assertEqual(self.getProperty('github.base.ref'), 'safari-000-branch')
        return rc

    def test_prod_branch_alternate_remote(self):
        self.setupStep(MapBranchAlias())
        self.setProperty('remote', 'security')
        self.setProperty('github.base.ref', 'safari-000-branch')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                timeout=60,
                command=['git', 'branch', '-a', '--contains', 'remotes/security/safari-000-branch'],
            ) + 0
            + ExpectShell.log('stdio', stdout='  safari-000-branch\n  remotes/security/safari-000-branch\n  remotes/security/safari-alias\n  remotes/security/eng/pr-branch\n  remotes/origin/main\n'),
        )
        self.expectOutcome(result=SUCCESS, state_string="'safari-000-branch' is the prevailing alias")
        rc = self.runStep()
        self.assertEqual(self.getProperty('github.base.ref'), 'safari-000-branch')
        return rc

    def test_alias_branch_alternate_remote(self):
        self.setupStep(MapBranchAlias())
        self.setProperty('remote', 'security')
        self.setProperty('github.base.ref', 'safari-alias')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                timeout=60,
                command=['git', 'branch', '-a', '--contains', 'remotes/security/safari-alias'],
            ) + 0
            + ExpectShell.log('stdio', stdout='  safari-alias\n  remotes/security/safari-000-branch\n  remotes/security/safari-alias\n  remotes/security/eng/pr-branch\n  remotes/origin/main\n'),
        )
        self.expectOutcome(result=SUCCESS, state_string="'safari-000-branch' is the prevailing alias")
        rc = self.runStep()
        self.assertEqual(self.getProperty('github.base.ref'), 'safari-000-branch')
        return rc

    def test_failure(self):
        self.setupStep(MapBranchAlias())
        self.setProperty('remote', 'origin')
        self.setProperty('github.base.ref', 'safari-000-branch')
        self.setProperty('github.number', '1234')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=False,
                timeout=60,
                command=['git', 'branch', '-a', '--contains', 'remotes/origin/safari-000-branch'],
            ) + 129
            + ExpectShell.log('stdio', stdout='error: malformed object name remotes/origin/safari-000-branch\n'),
        )
        self.expectOutcome(result=FAILURE, state_string="Failed to query checkout for aliases of 'safari-000-branch'")
        return self.runStep()


class TestValidateSquashed(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_patch(self):
        self.setupStep(ValidateSquashed())
        self.setProperty('patch_id', '1234')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['git', 'log', '--oneline', 'HEAD', '^origin/main', '--max-count=2'],
                        )
            + 0
            + ExpectShell.log('stdio', stdout='e1eb24603493 (HEAD -> eng/pull-request-branch) First line of commit\n'),
        )
        self.expectOutcome(result=SUCCESS, state_string='Verified commit is squashed')
        return self.runStep()

    def test_failure_patch(self):
        self.setupStep(ValidateSquashed())
        self.setProperty('patch_id', '1234')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['git', 'log', '--oneline', 'HEAD', '^origin/main', '--max-count=2'],
                        )
            + 0
            + ExpectShell.log('stdio', stdout='''e1eb24603493 (HEAD -> eng/pull-request-branch) Commit Series (3)
08abb9ddcbb5 Commit Series (2)
45cf3efe4dfb Commit Series (1)
'''),
        )
        self.expectOutcome(result=FAILURE, state_string='Can only land squashed commits')
        rc = self.runStep()
        self.assertEqual(self.getProperty('comment_text'), 'This change contains multiple commits which are not squashed together, rejecting attachment 1234 from commit queue. Please squash the commits to land.')
        self.assertEqual(self.getProperty('build_finish_summary'), 'Can only land squashed commits')
        return rc

    def test_success(self):
        self.setupStep(ValidateSquashed())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['git', 'log', '--oneline', 'eng/pull-request-branch', '^main', '--max-count=2'],
                        )
            + 0
            + ExpectShell.log('stdio', stdout='e1eb24603493 (HEAD -> eng/pull-request-branch) First line of commit\n'),
        )
        self.expectOutcome(result=SUCCESS, state_string='Verified commit is squashed')
        return self.runStep()

    def test_failure_multiple_commits(self):
        self.setupStep(ValidateSquashed())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['git', 'log', '--oneline', 'eng/pull-request-branch', '^main', '--max-count=2'],
                        )
            + 0
            + ExpectShell.log('stdio', stdout='''e1eb24603493 (HEAD -> eng/pull-request-branch) Commit Series (3)
08abb9ddcbb5 Commit Series (2)
45cf3efe4dfb Commit Series (1)
'''),
        )
        self.expectOutcome(result=FAILURE, state_string='Can only land squashed commits')
        rc = self.runStep()
        self.assertEqual(self.getProperty('comment_text'), 'This change contains multiple commits which are not squashed together, blocking PR #1234. Please squash the commits to land.')
        self.assertEqual(self.getProperty('build_finish_summary'), 'Can only land squashed commits')
        return rc

    def test_failure_merged(self):
        self.setupStep(ValidateSquashed())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['git', 'log', '--oneline', 'eng/pull-request-branch', '^main', '--max-count=2'],
                        )
            + 0
            + ExpectShell.log('stdio', stdout=''),
        )
        self.expectOutcome(result=FAILURE, state_string='Can only land squashed commits')
        rc = self.runStep()
        self.assertEqual(self.getProperty('comment_text'), 'This change contains multiple commits which are not squashed together, blocking PR #1234. Please squash the commits to land.')
        self.assertEqual(self.getProperty('build_finish_summary'), 'Can only land squashed commits')
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
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_skipped_patch(self):
        self.setupStep(AddReviewerToCommitMessage())
        self.setProperty('patch_id', '1234')
        self.expectOutcome(result=SKIPPED, state_string='finished (skipped)')
        return self.runStep()

    def test_success(self):
        gmtoffset = int(time.localtime().tm_gmtoff * 100 / (60 * 60))
        fixed_time = int(time.time())
        date = f'{int(time.time())} {gmtoffset}'
        time.time = lambda: fixed_time

        self.setupStep(AddReviewerToCommitMessage())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('reviewers_full_names', ['WebKit Reviewer', 'Other Reviewer'])
        self.setProperty('owners', ['webkit-commit-queue'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        env=self.ENV,
                        timeout=60,
                        command=[
                            'git', 'filter-branch', '-f',
                            '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}'".format(date=date),
                            '--msg-filter', 'sed "s/NOBODY (OO*PP*S!*)/WebKit Reviewer and Other Reviewer/g"',
                            'eng/pull-request-branch...main',
                        ])
            + 0
            + ExpectShell.log('stdio', stdout="Ref 'refs/heads/eng/pull-request-branch' was rewritten\n"),
        )
        self.expectOutcome(result=SUCCESS, state_string='Reviewed by WebKit Reviewer and Other Reviewer')
        return self.runStep()

    def test_failure(self):
        gmtoffset = int(time.localtime().tm_gmtoff * 100 / (60 * 60))
        fixed_time = int(time.time())
        date = f'{int(time.time())} {gmtoffset}'
        time.time = lambda: fixed_time

        self.setupStep(AddReviewerToCommitMessage())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('reviewers_full_names', ['WebKit Reviewer', 'Other Reviewer'])
        self.setProperty('owners', ['webkit-commit-queue'])
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        env=self.ENV,
                        timeout=60,
                        command=[
                            'git', 'filter-branch', '-f',
                            '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}'".format(date=date),
                            '--msg-filter', 'sed "s/NOBODY (OO*PP*S!*)/WebKit Reviewer and Other Reviewer/g"',
                            'eng/pull-request-branch...main',
                        ])
            + 2
            + ExpectShell.log('stdio', stdout="Failed to rewrite 'refs/heads/eng/pull-request-branch'\n"),
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to apply reviewers')
        return self.runStep()

    def test_no_reviewers(self):
        self.setupStep(AddReviewerToCommitMessage())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('reviewers_full_names', [])
        self.expectOutcome(result=SKIPPED, state_string='finished (skipped)')
        return self.runStep()


class TestValidateCommitMessage(BuildStepMixinAdditions, unittest.TestCase):
    def expectCommonRemoteCommandsWithOutput(self, expected_remote_command_output):
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        timeout=60,
                        command=['/bin/sh', '-c',
                                 "git log eng/pull-request-branch ^main | grep -q 'OO*PP*S!' && echo 'Commit message contains (OOPS!) and no reviewer found' || test $? -eq 1"])
            + 0, ExpectShell(workdir='wkdir',
                             logEnviron=False,
                             timeout=60,
                             command=['/bin/sh', '-c',
                                      "git log eng/pull-request-branch ^main | grep -q '\\(Reviewed by\\|Rubber-stamped by\\|Rubber stamped by\\|Unreviewed\\|Versioning.\\)' || echo 'No reviewer information in commit message'"])
            + 0, ExpectShell(workdir='wkdir',
                             logEnviron=False,
                             timeout=60,
                             command=['/bin/sh', '-c',
                                      "git log eng/pull-request-branch ^main | grep '\\(Reviewed by\\|Rubber-stamped by\\|Rubber stamped by\\)' || true"])
            + 0
            + ExpectShell.log('stdio', stdout=expected_remote_command_output),
        )

    def setUp(self):
        self.longMessage = True
        Contributors.load = mock_load_contributors
        return self.setUpBuildStep()

    def setUpCommonProperties(self):
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_patch(self):
        self.setupStep(ValidateCommitMessage())
        self.setProperty('patch_id', '1234')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        timeout=60,
                        command=['/bin/sh', '-c', "git log HEAD ^origin/main | grep -q 'OO*PP*S!' && echo 'Commit message contains (OOPS!) and no reviewer found' || test $? -eq 1"])
            + 0, ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        timeout=60,
                        command=['/bin/sh', '-c', "git log HEAD ^origin/main | grep -q '\\(Reviewed by\\|Rubber-stamped by\\|Rubber stamped by\\|Unreviewed\\|Versioning.\\)' || echo 'No reviewer information in commit message'"])
            + 0, ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        timeout=60,
                        command=['/bin/sh', '-c', "git log HEAD ^origin/main | grep '\\(Reviewed by\\|Rubber-stamped by\\|Rubber stamped by\\)' || true"])
            + 0
            + ExpectShell.log('stdio', stdout='    Reviewed by WebKit Reviewer.\n'),
        )
        self.expectOutcome(result=SUCCESS, state_string='Validated commit message')
        return self.runStep()

    def test_success(self):
        self.setupStep(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        expected_remote_command_output = '    Reviewed by WebKit Reviewer.\n'
        self.expectCommonRemoteCommandsWithOutput(expected_remote_command_output)
        self.expectOutcome(result=SUCCESS, state_string='Validated commit message')
        return self.runStep()

    def test_success_period(self):
        self.setupStep(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        expected_remote_command_output = '    Reviewed by Myles C. Maxfield.\n'
        self.expectCommonRemoteCommandsWithOutput(expected_remote_command_output)
        self.expectOutcome(result=SUCCESS, state_string='Validated commit message')
        return self.runStep()

    def test_failure_oops(self):
        self.setupStep(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        timeout=60,
                        command=['/bin/sh', '-c', "git log eng/pull-request-branch ^main | grep -q 'OO*PP*S!' && echo 'Commit message contains (OOPS!) and no reviewer found' || test $? -eq 1"])
            + 1
            + ExpectShell.log('stdio', stdout='Commit message contains (OOPS!) and no reviewer found\n'),
        )
        self.expectOutcome(result=FAILURE, state_string='Commit message contains (OOPS!) and no reviewer found')
        rc = self.runStep()
        self.assertEqual(self.getProperty('comment_text'), 'Commit message contains (OOPS!) and no reviewer found, blocking PR #1234')
        return rc

    def test_failure_no_reviewer(self):
        self.setupStep(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        self.expectCommonRemoteCommandsWithOutput('No reviewer information in commit message\n')
        self.expectOutcome(result=FAILURE, state_string='No reviewer information in commit message')
        rc = self.runStep()
        self.assertEqual(self.getProperty('comment_text'), 'No reviewer information in commit message, blocking PR #1234')
        return rc

    def test_failure_no_changelog(self):
        self.setupStep(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/ChangeLog', '+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        expected_remote_command_output = '    Reviewed by WebKit Reviewer.\n'
        self.expectCommonRemoteCommandsWithOutput(expected_remote_command_output)
        self.expectOutcome(result=FAILURE, state_string='ChangeLog modified, WebKit only allows commit messages')
        return self.runStep()

    def test_success_with_changelog_tools(self):
        self.setupStep(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: [
            '+++ Tools/Scripts/prepare-ChangeLog',
            '+++ Tools/Scripts/webkitperl/prepare-ChangeLog_unittest/resources/swift_unittests-expected.txt',
            '+++ Tools/Scripts/webkitperl/prepare-ChangeLog_unittest/resources/swift_unittests.swift']
        self.setUpCommonProperties()
        expected_remote_command_output = '    Reviewed by WebKit Reviewer.\n'
        self.expectCommonRemoteCommandsWithOutput(expected_remote_command_output)
        self.expectOutcome(result=SUCCESS, state_string='Validated commit message')
        return self.runStep()

    def test_invalid_reviewer(self):
        self.setupStep(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        expected_remote_command_output = '    Reviewed by WebKit Contributor.\n'
        self.expectCommonRemoteCommandsWithOutput(expected_remote_command_output)
        self.expectOutcome(result=SUCCESS, state_string="'WebKit Contributor' is not a reviewer, still continuing")
        return self.runStep()

    def test_self_reviewer(self):
        self.setupStep(ValidateCommitMessage())
        ValidateCommitMessage._files = lambda x: ['+++ Tools/CISupport/ews-build/steps.py']
        self.setUpCommonProperties()
        self.setProperty('author', 'WebKit Reviewer <reviewer@apple.com>')
        expected_remote_command_output = '    Reviewed by WebKit Reviewer.\n'
        self.expectCommonRemoteCommandsWithOutput(expected_remote_command_output)
        self.expectOutcome(result=FAILURE, state_string="'WebKit Reviewer <reviewer@apple.com>' cannot review their own change")
        return self.runStep()


class TestCanonicalize(BuildStepMixinAdditions, unittest.TestCase):
    ENV = dict(FILTER_BRANCH_SQUELCH_WARNING='1')

    def setUp(self):
        self.longMessage = True
        Contributors.load = mock_load_contributors
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_patch(self):
        self.setupStep(Canonicalize())
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
                logEnviron=False,
                env=self.ENV,
                command=['/bin/sh', '-c', 'rm .git/identifiers.json || true'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'pull', 'origin', 'main', '--rebase'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'checkout', 'main'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['python3', 'Tools/Scripts/git-webkit', 'canonicalize', '-n', '1'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                env=self.ENV,
                timeout=300,
                command=[
                    'git', 'filter-branch', '-f',
                    '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}';GIT_COMMITTER_NAME='WebKit Committer';GIT_COMMITTER_EMAIL='committer@webkit.org'".format(date=date),
                    'HEAD...HEAD~1',
                ],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Canonicalized commit')
        return self.runStep()

    def test_success(self):
        self.setupStep(Canonicalize())
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
                logEnviron=False,
                env=self.ENV,
                command=['/bin/sh', '-c', 'rm .git/identifiers.json || true'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'pull', 'origin', 'main', '--rebase'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'branch', '-f', 'main', 'eng/pull-request-branch'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'checkout', 'main'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['python3', 'Tools/Scripts/git-webkit', 'canonicalize', '-n', '1'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                env=self.ENV,
                timeout=300,
                command=[
                    'git', 'filter-branch', '-f',
                    '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}';GIT_COMMITTER_NAME='WebKit Committer';GIT_COMMITTER_EMAIL='committer@webkit.org'".format(date=date),
                    'HEAD...HEAD~1',
                ],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Canonicalized commit')
        return self.runStep()

    def test_success_branch(self):
        self.setupStep(Canonicalize())
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
                logEnviron=False,
                env=self.ENV,
                command=['/bin/sh', '-c', 'rm .git/identifiers.json || true'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'pull', 'security', 'safari-000-branch', '--rebase'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'branch', '-f', 'safari-000-branch', 'eng/pull-request-branch'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'checkout', 'safari-000-branch'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['python3', 'Tools/Scripts/git-webkit', 'canonicalize', '-n', '1'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                env=self.ENV,
                timeout=300,
                command=[
                    'git', 'filter-branch', '-f',
                    '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}';GIT_COMMITTER_NAME='WebKit Committer';GIT_COMMITTER_EMAIL='committer@webkit.org'".format(date=date),
                    'HEAD...HEAD~1',
                ],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Canonicalized commit')
        return self.runStep()

    def test_success_no_rebase(self):
        self.setupStep(Canonicalize(rebase_enabled=False))
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
                logEnviron=False,
                env=self.ENV,
                command=['/bin/sh', '-c', 'rm .git/identifiers.json || true'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['python3', 'Tools/Scripts/git-webkit', 'canonicalize', '-n', '3'],
            ) + 0, ExpectShell(workdir='wkdir',
                logEnviron=False,
                env=self.ENV,
                timeout=300,
                command=[
                    'git', 'filter-branch', '-f',
                    '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}';GIT_COMMITTER_NAME='WebKit Committer';GIT_COMMITTER_EMAIL='committer@webkit.org'".format(date=date),
                    'HEAD...HEAD~1',
                ],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Canonicalized commits')
        return self.runStep()

    def test_failure(self):
        self.setupStep(Canonicalize())
        self.setProperty('github.number', '1234')
        self.setProperty('github.base.ref', 'main')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('owners', ['webkit-commit-queue'])
        self.setProperty('remote', 'origin')

        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['/bin/sh', '-c', 'rm .git/identifiers.json || true'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'pull', 'origin', 'main', '--rebase'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'branch', '-f', 'main', 'eng/pull-request-branch'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['git', 'checkout', 'main'],
            ) + 0, ExpectShell(
                workdir='wkdir',
                timeout=300,
                logEnviron=False,
                env=self.ENV,
                command=['python3', 'Tools/Scripts/git-webkit', 'canonicalize', '-n', '1'],
            ) + 1,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to canonicalize commit')
        return self.runStep()


class TestPushPullRequestBranch(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_skipped_patch(self):
        self.setupStep(PushPullRequestBranch())
        self.setProperty('patch_id', '1234')
        self.expectOutcome(result=SKIPPED, state_string='finished (skipped)')
        return self.runStep()

    def test_success(self):
        GitHub.credentials = lambda user=None: ('webkit-commit-queue', 'password')
        self.setupStep(PushPullRequestBranch())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.repo.full_name', 'Contributor/WebKit')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        timeout=300,
                        env=dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password'),
                        command=['git', 'push', '-f', 'Contributor', 'HEAD:eng/pull-request-branch'])
            + 0
            + ExpectShell.log('stdio', stdout='To https://github.com/Contributor/WebKit.git\n37b7da95723b...9e2cb83b07b6 eng/pull-request-branch -> eng/pull-request-branch (forced update)\n'),
        )
        self.expectOutcome(result=SUCCESS, state_string='Pushed to pull request branch')
        with current_hostname(EWS_BUILD_HOSTNAME):
            return self.runStep()

    def test_failure(self):
        GitHub.credentials = lambda user=None: ('webkit-commit-queue', 'password')
        self.setupStep(PushPullRequestBranch())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.repo.full_name', 'Contributor/WebKit')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        timeout=300,
                        env=dict(GIT_USER='webkit-commit-queue', GIT_PASSWORD='password'),
                        command=['git', 'push', '-f', 'Contributor', 'HEAD:eng/pull-request-branch'])
            + 1
            + ExpectShell.log('stdio', stdout="fatal: could not read Username for 'https://github.com': Device not configured\n"),
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to push to pull request branch')
        with current_hostname(EWS_BUILD_HOSTNAME):
            return self.runStep()


class TestUpdatePullRequest(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_skipped_patch(self):
        self.setupStep(UpdatePullRequest())
        self.setProperty('patch_id', '1234')
        self.expectOutcome(result=SKIPPED, state_string="'git log ...' (skipped)")
        return self.runStep()

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
&lt;rdar://problem/91044821&gt;

Reviewed by NOBODY (OOPS!).

* Tools/CISupport/ews-build/steps.py:
(CheckOutPullRequest.run): Add credential helper that pulls http credentials
from environment variables.
* Tools/CISupport/ews-build/steps_unittest.py:

Canonical link: <a href="https://commits.webkit.org/249006@main">https://commits.webkit.org/249006@main</a>
</pre>
''',
            )

            return True

        UpdatePullRequest.update_pr = update_pr
        self.setupStep(UpdatePullRequest())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.user.login', 'JonWBedard')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('github.base.ref', 'main')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        timeout=300,
                        command=['git', 'log', '-1', '--no-decorate'])
            + 0
            + ExpectShell.log('stdio', stdout='''commit 44a3b7100bd5dba51c57d874d3e89f89081e7886
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
        self.expectOutcome(result=SUCCESS, state_string='Updated pull request')
        with current_hostname(EWS_BUILD_HOSTNAME):
            rc = self.runStep()
            self.assertEqual(self.getProperty('bug_id'), '238553')
            self.assertEqual(self.getProperty('is_test_gardening'), False)
            return rc

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

            return True

        UpdatePullRequest.update_pr = update_pr
        self.setupStep(UpdatePullRequest())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.user.login', 'karlrackler')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('github.base.ref', 'main')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        timeout=300,
                        command=['git', 'log', '-1', '--no-decorate'])
            + 0
            + ExpectShell.log('stdio', stdout='''commit 6a50b47fd71d922f753c06f46917086c839520b
Author: Karl Rackler <rackler@apple.com>
Date:   Thu Apr 21 00:25:03 2022 +0000

    [ macOS wk2 ] some/test/path.html a flaky failure
    https://bugs.webkit.org/show_bug.cgi?id=239577

    Unreviewed test gardening.

    * LayoutTests/platform/mac-wk2/TestExpectations:

    Canonical link: https://commits.webkit.org/249833@main
'''),
        )
        self.expectOutcome(result=SUCCESS, state_string='Updated pull request')
        with current_hostname(EWS_BUILD_HOSTNAME):
            rc = self.runStep()
            self.assertEqual(self.getProperty('bug_id'), '239577')
            self.assertEqual(self.getProperty('is_test_gardening'), True)
            return rc

    def test_failure(self):
        def update_pr(x, pr_number, title, description, base=None, head=None, repository_url=None):
            return False

        UpdatePullRequest.update_pr = update_pr
        self.setupStep(UpdatePullRequest())
        self.setProperty('github.number', '1234')
        self.setProperty('github.head.user.login', 'JonWBedard')
        self.setProperty('github.head.ref', 'eng/pull-request-branch')
        self.setProperty('github.base.ref', 'main')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        timeout=300,
                        command=['git', 'log', '-1', '--no-decorate'])
            + 0
            + ExpectShell.log('stdio', stdout='''commit 44a3b7100bd5dba51c57d874d3e89f89081e7886
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
        self.expectOutcome(result=FAILURE, state_string='Failed to update pull request')
        with current_hostname(EWS_BUILD_HOSTNAME):
            rc = self.runStep()
            self.assertEqual(self.getProperty('bug_id'), '238553')
            self.assertEqual(self.getProperty('is_test_gardening'), False)
            return rc


class TestDeleteStaleBuildFiles(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(DeleteStaleBuildFiles())
        self.setProperty('fullPlatform', 'win')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/delete-stale-build-files', '--platform=win'],
                        logEnviron=False,
                        timeout=600,
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Deleted stale build files')
        return self.runStep()

    def test_failure(self):
        self.setupStep(DeleteStaleBuildFiles())
        self.setProperty('fullPlatform', 'win')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/delete-stale-build-files', '--platform=win'],
                        logEnviron=False,
                        timeout=600,
                        )
            + ExpectShell.log('stdio', stdout='Unexpected error.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Deleted stale build files (failure)')
        return self.runStep()


if __name__ == '__main__':
    unittest.main()
