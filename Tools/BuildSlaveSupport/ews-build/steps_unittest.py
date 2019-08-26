# Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

import operator
import os
import shutil
import tempfile

from buildbot.process import remotetransfer
from buildbot.process.results import Results, SUCCESS, FAILURE, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.test.fake.remotecommand import Expect, ExpectRemoteRef, ExpectShell
from buildbot.test.util.steps import BuildStepMixin
from mock import call
from twisted.internet import error, reactor
from twisted.python import failure, log
from twisted.trial import unittest

from steps import (AnalyzeAPITestsResults, AnalyzeCompileWebKitResults, AnalyzeLayoutTestsResults, ApplyPatch, ApplyWatchList, ArchiveBuiltProduct, ArchiveTestResults,
                   CheckOutSource, CheckOutSpecificRevision, CheckPatchRelevance, CheckStyle, CleanBuild, CleanUpGitIndexLock, CleanWorkingDirectory,
                   CompileJSCOnly, CompileJSCOnlyToT, CompileWebKit, CompileWebKitToT, ConfigureBuild,
                   DownloadBuiltProduct, ExtractBuiltProduct, ExtractTestResults, InstallGtkDependencies, InstallWpeDependencies, KillOldProcesses,
                   PrintConfiguration, ReRunAPITests, ReRunJavaScriptCoreTests, ReRunWebKitTests, RunAPITests, RunAPITestsWithoutPatch,
                   RunBindingsTests, RunEWSBuildbotCheckConfig, RunEWSUnitTests, RunJavaScriptCoreTests, RunJavaScriptCoreTestsToT, RunWebKit1Tests,
                   RunWebKitPerlTests, RunWebKitPyTests, RunWebKitTests, TestWithFailureCount, Trigger, TransferToS3, UnApplyPatchIfRequired,
                   UpdateWorkingDirectory, UploadBuiltProduct, UploadTestResults, ValidatePatch)

# Workaround for https://github.com/buildbot/buildbot/issues/4669
from buildbot.test.fake.fakebuild import FakeBuild
FakeBuild.addStepsAfterCurrentStep = lambda FakeBuild, step_factories: None


def mock_step(step, logs='', results=SUCCESS, stopped=False, properties=None):
    step.logs = logs
    step.results = results
    step.stopped = stopped
    return step


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
        return 'ExpectMasterShellCommand({0})'.format(repr(self.args))


class BuildStepMixinAdditions(BuildStepMixin):
    def setUpBuildStep(self):
        self.patch(reactor, 'spawnProcess', lambda *args, **kwargs: self._checkSpawnProcess(*args, **kwargs))
        self._expected_local_commands = []

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
        return filter(lambda step: not step.stopped, self.previous_steps)

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
            self.fail('got local command {0} when no further commands were expected'.format(got))
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
                        command=['python', 'Tools/Scripts/check-webkit-style'],
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
                        command=['python', 'Tools/Scripts/check-webkit-style'],
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
                        command=['python', 'Tools/Scripts/check-webkit-style'],
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
                        command=['python', 'Tools/Scripts/check-webkit-style'],
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
                        command=['python', 'Tools/Scripts/check-webkit-style'],
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
                        command=['python', 'Tools/Scripts/webkit-patch', 'apply-watchlist-local', '1234'])
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
                        command=['python', 'Tools/Scripts/webkit-patch', 'apply-watchlist-local', '1234'])
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
                        command=['python', 'Tools/Scripts/run-bindings-tests', '--json-output={0}'.format(self.jsonFileName)],
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
                        command=['python', 'Tools/Scripts/run-bindings-tests', '--json-output={0}'.format(self.jsonFileName)],
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

    def test_success(self):
        self.setupStep(RunWebKitPerlTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/test-webkitperl'],
                        timeout=120,
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='webkitperl-tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunWebKitPerlTests())
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
        self.expectOutcome(result=FAILURE, state_string='webkitperl-tests (failure)')
        return self.runStep()


class TestWebKitPyTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'webkitpy_test_results.json'
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunWebKitPyTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/test-webkitpy', '--json-output={0}'.format(self.jsonFileName)],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed webkitpy tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunWebKitPyTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/test-webkitpy', '--json-output={0}'.format(self.jsonFileName)],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        )
            + ExpectShell.log('stdio', stdout='''Ran 1744 tests in 5.913s
FAILED (failures=1, errors=0)''')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='webkitpy-tests (failure)')
        return self.runStep()


class TestRunEWSBuildbotCheckConfig(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunEWSBuildbotCheckConfig())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/BuildSlaveSupport/ews-build',
                        timeout=120,
                        logEnviron=False,
                        command=['buildbot', 'checkconfig'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed buildbot checkconfig')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunEWSBuildbotCheckConfig())
        self.expectRemoteCommands(
            ExpectShell(workdir='build/Tools/BuildSlaveSupport/ews-build',
                        timeout=120,
                        logEnviron=False,
                        command=['buildbot', 'checkconfig'],
                        )
            + ExpectShell.log('stdio', stdout='Configuration Errors:  builder(s) iOS-12-Debug-Build-EWS have no schedulers to drive them')
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
            ExpectShell(workdir='wkdir',
                        timeout=120,
                        logEnviron=False,
                        command=['python', 'Tools/BuildSlaveSupport/ews-build/runUnittests.py'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Passed EWS unit tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunEWSUnitTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=120,
                        logEnviron=False,
                        command=['python', 'Tools/BuildSlaveSupport/ews-build/runUnittests.py'],
                        )
            + ExpectShell.log('stdio', stdout='Unhandled Error. Traceback (most recent call last): Keys in cmd missing from expectation: [logfiles.json]')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed EWS unit tests')
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
                        command=['python', 'Tools/BuildSlaveSupport/kill-old-processes', 'buildbot'],
                        logEnviron=False,
                        timeout=60,
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Killed old processes')
        return self.runStep()

    def test_failure(self):
        self.setupStep(KillOldProcesses())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python', 'Tools/BuildSlaveSupport/kill-old-processes', 'buildbot'],
                        logEnviron=False,
                        timeout=60,
                        )
            + ExpectShell.log('stdio', stdout='Unexpected error.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Killed old processes (failure)')
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
                        command=['python', 'Tools/BuildSlaveSupport/clean-build', '--platform=ios-11', '--release'],
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
                        command=['python', 'Tools/BuildSlaveSupport/clean-build', '--platform=ios-simulator-11', '--debug'],
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
        self.assertEqual(InstallGtkDependencies.haltOnFailure, False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/update-webkitgtk-libs'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Updated gtk dependencies')
        return self.runStep()

    def test_failure(self):
        self.setupStep(InstallGtkDependencies())
        self.assertEqual(InstallGtkDependencies.haltOnFailure, False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/update-webkitgtk-libs'],
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
        self.assertEqual(InstallWpeDependencies.haltOnFailure, False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/update-webkitwpe-libs'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Updated wpe dependencies')
        return self.runStep()

    def test_failure(self):
        self.setupStep(InstallWpeDependencies())
        self.assertEqual(InstallWpeDependencies.haltOnFailure, False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/update-webkitwpe-libs'],
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
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-webkit', '--release', '--gtk'],
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
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-webkit', '--release', '--wpe'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Compiled WebKit')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CompileWebKit())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-webkit', '--debug'],
                        )
            + ExpectShell.log('stdio', stdout='1 error generated.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Compiled WebKit (failure)')
        return self.runStep()


class TestCompileWebKitToT(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(CompileWebKitToT())
        self.setProperty('fullPlatform', 'ios-simulator-11')
        self.setProperty('configuration', 'release')
        self.setProperty('patchFailedToBuild', True)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-webkit', '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Compiled WebKit')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CompileWebKitToT())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.setProperty('patchFailedTests', True)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-webkit', '--debug'],
                        )
            + ExpectShell.log('stdio', stdout='1 error generated.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Compiled WebKit (failure)')
        return self.runStep()

    def test_skip(self):
        self.setupStep(CompileWebKitToT())
        self.setProperty('fullPlatform', 'ios-simulator-11')
        self.setProperty('configuration', 'release')
        self.expectHidden(True)
        self.expectOutcome(result=SKIPPED, state_string='Compiled WebKit (skipped)')
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
            mock_step(CompileWebKitToT(), results=SUCCESS),
        ]
        self.setupStep(AnalyzeCompileWebKitResults(), previous_steps=previous_steps)
        self.expectOutcome(result=FAILURE, state_string='Patch does not build (failure)')
        return self.runStep()

    def test_patch_with_ToT_failure(self):
        previous_steps = [
            mock_step(CompileWebKit(), results=FAILURE),
            mock_step(CompileWebKitToT(), results=FAILURE),
        ]
        self.setupStep(AnalyzeCompileWebKitResults(), previous_steps=previous_steps)
        self.expectOutcome(result=FAILURE, state_string='Unable to build WebKit without patch, retrying build (failure)')
        return self.runStep()


class TestCompileJSCOnly(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(CompileJSCOnly())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-jsc', '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Compiled JSC')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CompileJSCOnly())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-jsc', '--debug'],
                        )
            + ExpectShell.log('stdio', stdout='1 error generated.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Compiled JSC (failure)')
        return self.runStep()


class TestCompileJSCOnlyToT(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(CompileJSCOnlyToT())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'release')
        self.setProperty('patchFailedToBuild', 'True')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-jsc', '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Compiled JSC')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CompileJSCOnlyToT())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.setProperty('patchFailedToBuild', 'True')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/build-jsc', '--debug'],
                        )
            + ExpectShell.log('stdio', stdout='1 error generated.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Compiled JSC (failure)')
        return self.runStep()

    def test_skip(self):
        self.setupStep(CompileJSCOnlyToT())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.expectHidden(True)
        self.expectOutcome(result=SKIPPED, state_string='Compiled JSC (skipped)')
        return self.runStep()


class TestRunJavaScriptCoreTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'jsc_results.json'
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunJavaScriptCoreTests())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', '--json-output={0}'.format(self.jsonFileName), '--release'],
                        logfiles={'json': self.jsonFileName},
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='jscore-tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunJavaScriptCoreTests())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', '--json-output={0}'.format(self.jsonFileName), '--debug'],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='9 failures found.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='jscore-tests (failure)')
        return self.runStep()


class TestReRunJavaScriptCoreTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'jsc_results.json'
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(ReRunJavaScriptCoreTests())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'release')
        self.setProperty('patchFailedTests', 'True')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', '--json-output={0}'.format(self.jsonFileName), '--release'],
                        logfiles={'json': self.jsonFileName},
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='jscore-tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(ReRunJavaScriptCoreTests())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.setProperty('patchFailedTests', 'True')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', '--json-output={0}'.format(self.jsonFileName), '--debug'],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='9 failures found.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='jscore-tests (failure)')
        return self.runStep()

    def test_skip(self):
        self.setupStep(ReRunJavaScriptCoreTests())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.expectHidden(True)
        self.expectOutcome(result=SKIPPED, state_string='jscore-tests (skipped)')
        return self.runStep()


class TestRunJavaScriptCoreTestsToT(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'jsc_results.json'
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunJavaScriptCoreTestsToT())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'release')
        self.setProperty('patchFailedTests', 'True')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-fail-fast', '--json-output={0}'.format(self.jsonFileName), '--release'],
                        logfiles={'json': self.jsonFileName},
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='jscore-tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunJavaScriptCoreTestsToT())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.setProperty('patchFailedTests', 'True')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-fail-fast', '--json-output={0}'.format(self.jsonFileName), '--debug'],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='9 failures found.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='jscore-tests (failure)')
        return self.runStep()

    def test_skip(self):
        self.setupStep(RunJavaScriptCoreTestsToT())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.expectHidden(True)
        self.expectOutcome(result=SKIPPED, state_string='jscore-tests (skipped)')
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

        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(RunWebKitTests())
        self.property_exceed_failure_limit = 'first_results_exceed_failure_limit'
        self.property_failures = 'first_run_failures'

    def test_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        command=['python', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-new-test-results', '--no-show-results', '--exit-after-n-failures', '30', '--skip-failing-tests', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
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
                        command=['python', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-new-test-results', '--no-show-results', '--exit-after-n-failures', '30', '--skip-failing-tests', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
                        )
            + 0
            + ExpectShell.log('stdio', stdout='''Unexpected flakiness: timeouts (2)
                              imported/blink/storage/indexeddb/blob-valid-before-commit.html [ Timeout Pass ]
                              storage/indexeddb/modern/deleteindex-2.html [ Timeout Pass ]'''),
        )
        self.expectOutcome(result=WARNINGS, state_string='2 flakes')
        return self.runStep()

    def test_parse_results_json_regression(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        command=['python', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-new-test-results', '--no-show-results', '--exit-after-n-failures', '30', '--skip-failing-tests', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
                        )
            + 2
            + ExpectShell.log('json', stdout=self.results_json_regressions),
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.runStep()
        self.assertEqual(self.getProperty(self.property_exceed_failure_limit), True)
        self.assertEqual(self.getProperty(self.property_failures),
                            ["imported/w3c/web-platform-tests/IndexedDB/interleaved-cursors-large.html",
                             "imported/w3c/web-platform-tests/wasm/jsapi/interface.any.html",
                             "imported/w3c/web-platform-tests/wasm/jsapi/instance/constructor-bad-imports.any.html",
                             "imported/w3c/web-platform-tests/wasm/jsapi/global/constructor.any.html",
                             "imported/w3c/web-platform-tests/wasm/jsapi/global/constructor.any.worker.html",
                             "imported/w3c/web-platform-tests/wasm/jsapi/global/toString.any.html",
                             "imported/w3c/web-platform-tests/wasm/jsapi/interface.any.worker.html",
                             "imported/w3c/web-platform-tests/wasm/jsapi/constructor/instantiate-bad-imports.any.html",
                             "imported/w3c/web-platform-tests/wasm/jsapi/constructor/instantiate-bad-imports.any.worker.html",
                             "imported/blink/storage/indexeddb/blob-valid-before-commit.html"])
        return rc

    def test_parse_results_json_flakes(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        command=['python', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-new-test-results', '--no-show-results', '--exit-after-n-failures', '30', '--skip-failing-tests', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
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
                        command=['python', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-new-test-results', '--no-show-results', '--exit-after-n-failures', '30', '--skip-failing-tests', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
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
                        command=['python', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-new-test-results', '--no-show-results', '--exit-after-n-failures', '30', '--skip-failing-tests', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
                        )
            + 2
            + ExpectShell.log('json', stdout=self.results_json_with_newlines),
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        rc = self.runStep()
        self.assertEqual(self.getProperty(self.property_exceed_failure_limit), False)
        self.assertEqual(self.getProperty(self.property_failures), ['fast/scrolling/ios/reconcile-layer-position-recursive.html'])
        return rc

    def test_unexpected_error(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'mac-highsierra')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        command=['python', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-new-test-results', '--no-show-results', '--exit-after-n-failures', '30', '--skip-failing-tests', '--debug', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected error.')
            + 254,
        )
        self.expectOutcome(result=RETRY, state_string='layout-tests (retry)')
        return self.runStep()

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logfiles={'json': self.jsonFileName},
                        command=['python', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-new-test-results', '--no-show-results', '--exit-after-n-failures', '30', '--skip-failing-tests', '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
                        )
            + ExpectShell.log('stdio', stdout='9 failures found.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.runStep()


class TestReRunWebKitTests(TestRunWebKitTests):
    def configureStep(self):
        self.setupStep(ReRunWebKitTests())
        self.property_exceed_failure_limit = 'second_results_exceed_failure_limit'
        self.property_failures = 'second_run_failures'


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
                        command=['python', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-new-test-results', '--no-show-results', '--exit-after-n-failures', '30', '--skip-failing-tests', '--debug', '--dump-render-tree', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
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
                        command=['python', 'Tools/Scripts/run-webkit-tests', '--no-build', '--no-new-test-results', '--no-show-results', '--exit-after-n-failures', '30', '--skip-failing-tests', '--release', '--dump-render-tree', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
                        )
            + ExpectShell.log('stdio', stdout='9 failures found.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
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

    def test_failure_introduced_by_patch(self):
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
        return self.runStep()

    def test_flaky_and_consistent_failures_without_clean_tree_failures(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.setProperty('second_run_failures', ['test1'])
        self.expectOutcome(result=FAILURE, state_string='Found 1 new test failure: test1 (failure)')
        return self.runStep()

    def test_flaky_and_inconsistent_failures_without_clean_tree_failures(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.setProperty('second_run_failures', ['test3'])
        self.expectOutcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by patch, retrying build (retry)')
        return self.runStep()

    def test_flaky_and_inconsistent_failures_with_clean_tree_failures(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.setProperty('second_run_failures', ['test3'])
        self.setProperty('clean_tree_run_failures', ['test1', 'test2', 'test3'])
        self.expectOutcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by patch, retrying build (retry)')
        return self.runStep()

    def test_flaky_and_consistent_failures_with_clean_tree_failures(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1', 'test2'])
        self.setProperty('second_run_failures', ['test1'])
        self.setProperty('clean_tree_run_failures', ['test1', 'test2'])
        self.expectOutcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by patch, retrying build (retry)')
        return self.runStep()

    def test_first_run_exceed_failure_limit(self):
        self.configureStep()
        self.setProperty('first_results_exceed_failure_limit', True)
        self.setProperty('first_run_failures',  ['test{}'.format(i) for i in range(0, 30)])
        self.setProperty('second_run_failures', [])
        self.expectOutcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by patch, retrying build (retry)')
        return self.runStep()

    def test_second_run_exceed_failure_limit(self):
        self.configureStep()
        self.setProperty('first_run_failures', [])
        self.setProperty('second_results_exceed_failure_limit', True)
        self.setProperty('second_run_failures',  ['test{}'.format(i) for i in range(0, 30)])
        self.expectOutcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by patch, retrying build (retry)')
        return self.runStep()

    def test_clean_tree_exceed_failure_limit(self):
        self.configureStep()
        self.setProperty('first_run_failures', ['test1'])
        self.setProperty('second_run_failures', ['test1'])
        self.setProperty('clean_tree_results_exceed_failure_limit', True)
        self.setProperty('clean_tree_run_failures',  ['test{}'.format(i) for i in range(0, 30)])
        self.expectOutcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by patch, retrying build (retry)')
        return self.runStep()

    def test_clean_tree_has_lot_of_failures(self):
        self.configureStep()
        self.setProperty('first_results_exceed_failure_limit', True)
        self.setProperty('first_run_failures', ['test{}'.format(i) for i in range(0, 30)])
        self.setProperty('second_results_exceed_failure_limit', True)
        self.setProperty('second_run_failures', ['test{}'.format(i) for i in range(0, 30)])
        self.setProperty('clean_tree_run_failures', ['test{}'.format(i) for i in range(0, 27)])
        self.expectOutcome(result=RETRY, state_string='Unable to confirm if test failures are introduced by patch, retrying build (retry)')
        return self.runStep()

    def test_clean_tree_has_some_failures(self):
        self.configureStep()
        self.setProperty('first_results_exceed_failure_limit', True)
        self.setProperty('first_run_failures', ['test{}'.format(i) for i in range(0, 30)])
        self.setProperty('second_results_exceed_failure_limit', True)
        self.setProperty('second_run_failures', ['test{}'.format(i) for i in range(0, 30)])
        self.setProperty('clean_tree_run_failures', ['test{}'.format(i) for i in range(0, 10)])
        self.expectOutcome(result=FAILURE, state_string='Found 30 new test failures: test1, test0, test3, test2, test5, test4, test7, test6, test9, test8, test24, test25, test26, test27, test20, test21, test22, test23, test28, test29, test19, test18, test11, test10, test13, test12, test15, test14, test17, test16 (failure)')
        return self.runStep()

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
                        command=['python', 'Tools/Scripts/clean-webkit'],
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
                        command=['python', 'Tools/Scripts/clean-webkit'],
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
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/update-webkit'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Updated working directory')
        return self.runStep()

    def test_failure(self):
        self.setupStep(UpdateWorkingDirectory())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['perl', 'Tools/Scripts/update-webkit'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected failure.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Updated working directory (failure)')
        return self.runStep()


class TestUnApplyPatchIfRequired(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(UnApplyPatchIfRequired())
        self.setProperty('patchFailedToBuild', True)
        self.expectHidden(False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/clean-webkit'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Unapplied patch')
        return self.runStep()

    def test_failure(self):
        self.setupStep(UnApplyPatchIfRequired())
        self.setProperty('patchFailedTests', True)
        self.expectHidden(False)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/clean-webkit'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected failure.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Unapplied patch (failure)')
        return self.runStep()

    def test_skip(self):
        self.setupStep(UnApplyPatchIfRequired())
        self.expectHidden(True)
        self.expectOutcome(result=SKIPPED, state_string='Unapplied patch (skipped)')
        return self.runStep()


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
                        command=['python', 'Tools/BuildSlaveSupport/built-product-archive', '--platform=ios-simulator',  '--release', 'archive'],
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
                        command=['python', 'Tools/BuildSlaveSupport/built-product-archive', '--platform=mac-sierra',  '--debug', 'archive'],
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
        self.setProperty('patch_id', '1234')
        self.expectHidden(False)
        self.expectRemoteCommands(
            Expect('uploadFile', dict(
                                        workersrc='WebKitBuild/release.zip', workdir='wkdir',
                                        blocksize=1024 * 256, maxsize=None, keepstamp=False,
                                        writer=ExpectRemoteRef(remotetransfer.FileWriter),
                                     ))
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
        self.setProperty('patch_id', '1234')
        self.expectHidden(False)
        self.expectRemoteCommands(
            Expect('uploadFile', dict(
                                        workersrc='WebKitBuild/release.zip', workdir='wkdir',
                                        blocksize=1024 * 256, maxsize=None, keepstamp=False,
                                        writer=ExpectRemoteRef(remotetransfer.FileWriter),
                                     ))
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
        self.setProperty('patch_id', '1234')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/BuildSlaveSupport/download-built-product', '--release', 'https://s3-us-west-2.amazonaws.com/ews-archives.webkit.org/ios-simulator-12-x86_64-release/1234.zip'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Downloaded built product')
        return self.runStep()

    def test_failure(self):
        self.setupStep(DownloadBuiltProduct())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('patch_id', '123456')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/BuildSlaveSupport/download-built-product', '--debug', 'https://s3-us-west-2.amazonaws.com/ews-archives.webkit.org/mac-sierra-x86_64-debug/123456.zip'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected failure.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to download built product from S3')
        return self.runStep()


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
                        command=['python', 'Tools/BuildSlaveSupport/built-product-archive', '--platform=ios-simulator',  '--release', 'extract'],
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
                        command=['python', 'Tools/BuildSlaveSupport/built-product-archive', '--platform=mac-sierra',  '--debug', 'extract'],
                        )
            + ExpectShell.log('stdio', stdout='Unexpected failure.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Extracted built product (failure)')
        return self.runStep()


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
        self.setProperty('patch_id', '1234')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['python',
                                              '../Shared/transfer-archive-to-s3',
                                              '--patch_id', '1234',
                                              '--identifier', 'mac-highsierra-x86_64-release',
                                              '--archive', 'public_html/archives/mac-highsierra-x86_64-release/1234.zip',
                                             ])
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Transferred archive to S3')
        return self.runStep()

    def test_failure(self):
        self.setupStep(TransferToS3())
        self.setProperty('fullPlatform', 'ios-simulator-12')
        self.setProperty('configuration', 'debug')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('patch_id', '1234')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['python',
                                              '../Shared/transfer-archive-to-s3',
                                              '--patch_id', '1234',
                                              '--identifier', 'ios-simulator-12-x86_64-debug',
                                              '--archive', 'public_html/archives/ios-simulator-12-x86_64-debug/1234.zip',
                                             ])
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to transfer archive to S3')
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
        self.setProperty('fullPlatform', 'mac-mojave')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'release')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/run-api-tests', '--no-build', '--release', '--verbose', '--json-output={0}'.format(self.jsonFileName)],
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
                        command=['python', 'Tools/Scripts/run-api-tests', '--no-build', '--debug', '--verbose', '--json-output={0}'.format(self.jsonFileName), '--ios-simulator'],
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

    def test_one_failure(self):
        self.setupStep(RunAPITests())
        self.setProperty('fullPlatform', 'mac-mojave')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/run-api-tests', '--no-build', '--debug', '--verbose', '--json-output={0}'.format(self.jsonFileName)],
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
        self.setProperty('fullPlatform', 'mac-mojave')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/run-api-tests', '--no-build', '--debug', '--verbose', '--json-output={0}'.format(self.jsonFileName)],
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

        Tools\TestWebKitAPI\Tests\WTF\Expected.cpp:96
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
        self.setProperty('fullPlatform', 'mac-mojave')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/run-api-tests', '--no-build', '--debug', '--verbose', '--json-output={0}'.format(self.jsonFileName)],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='Unexpected failure. Failed to run api tests.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='run-api-tests (failure)')
        return self.runStep()

    def test_no_failures_or_timeouts_with_disabled(self):
        self.setupStep(RunAPITests())
        self.setProperty('fullPlatform', 'mac-mojave')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/run-api-tests', '--no-build', '--debug', '--verbose', '--json-output={0}'.format(self.jsonFileName)],
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
                        command=['python', 'Tools/BuildSlaveSupport/test-result-archive', '--platform=ios-simulator',  '--release', 'archive'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Archived test results')
        return self.runStep()

    def test_failure(self):
        self.setupStep(ArchiveTestResults())
        self.setProperty('fullPlatform', 'mac-mojave')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=False,
                        command=['python', 'Tools/BuildSlaveSupport/test-result-archive', '--platform=mac',  '--debug', 'archive'],
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
        self.setProperty('patch_id', '1234')
        self.setProperty('buildername', 'macOS-Sierra-Release-WK2-Tests-EWS')
        self.setProperty('buildnumber', '12')
        self.expectHidden(False)
        self.expectRemoteCommands(
            Expect('uploadFile', dict(
                                        workersrc='layout-test-results.zip', workdir='wkdir',
                                        blocksize=1024 * 256, maxsize=None, keepstamp=False,
                                        writer=ExpectRemoteRef(remotetransfer.FileWriter),
                                     ))
            + Expect.behavior(uploadFileWithContentsOfString('Dummy zip file content.'))
            + 0,
        )
        self.expectUploadedFile('public_html/results/macOS-Sierra-Release-WK2-Tests-EWS/r1234-12.zip')

        self.expectOutcome(result=SUCCESS, state_string='Uploaded test results')
        return self.runStep()

    def test_success_with_identifier(self):
        self.setupStep(UploadTestResults(identifier='clean-tree'))
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64')
        self.setProperty('patch_id', '271211')
        self.setProperty('buildername', 'iOS-12-Simulator-WK2-Tests-EWS')
        self.setProperty('buildnumber', '120')
        self.expectHidden(False)
        self.expectRemoteCommands(
            Expect('uploadFile', dict(
                                        workersrc='layout-test-results.zip', workdir='wkdir',
                                        blocksize=1024 * 256, maxsize=None, keepstamp=False,
                                        writer=ExpectRemoteRef(remotetransfer.FileWriter),
                                     ))
            + Expect.behavior(uploadFileWithContentsOfString('Dummy zip file content.'))
            + 0,
        )
        self.expectUploadedFile('public_html/results/iOS-12-Simulator-WK2-Tests-EWS/r271211-120-clean-tree.zip')

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
        self.setProperty('patch_id', '1234')
        self.setProperty('buildername', 'macOS-Sierra-Release-WK2-Tests-EWS')
        self.setProperty('buildnumber', '12')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['unzip',
                                              'public_html/results/macOS-Sierra-Release-WK2-Tests-EWS/r1234-12.zip',
                                              '-d',
                                              'public_html/results/macOS-Sierra-Release-WK2-Tests-EWS/r1234-12',
                                             ])
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Extracted test results')
        self.expectAddedURLs([call('view layout test results', '/results/test/r2468_ab1a28b4feee0d42973c7c05335b35bca927e974 (1)/results.html')])
        return self.runStep()

    def test_success_with_identifier(self):
        self.setupStep(ExtractTestResults(identifier='rerun'))
        self.setProperty('configuration', 'release')
        self.setProperty('patch_id', '1234')
        self.setProperty('buildername', 'iOS-12-Simulator-WK2-Tests-EWS')
        self.setProperty('buildnumber', '12')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['unzip',
                                              'public_html/results/iOS-12-Simulator-WK2-Tests-EWS/r1234-12-rerun.zip',
                                              '-d',
                                              'public_html/results/iOS-12-Simulator-WK2-Tests-EWS/r1234-12-rerun',
                                             ])
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Extracted test results')
        self.expectAddedURLs([call('view layout test results', '/results/test/r2468_ab1a28b4feee0d42973c7c05335b35bca927e974 (1)/results.html')])
        return self.runStep()

    def test_failure(self):
        self.setupStep(ExtractTestResults())
        self.setProperty('configuration', 'debug')
        self.setProperty('patch_id', '1234')
        self.setProperty('buildername', 'macOS-Sierra-Release-WK2-Tests-EWS')
        self.setProperty('buildnumber', '12')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['unzip',
                                              'public_html/results/macOS-Sierra-Release-WK2-Tests-EWS/r1234-12.zip',
                                              '-d',
                                              'public_html/results/macOS-Sierra-Release-WK2-Tests-EWS/r1234-12',
                                             ])
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='failed (2) (failure)')
        self.expectAddedURLs([call('view layout test results', '/results/test/r2468_ab1a28b4feee0d42973c7c05335b35bca927e974 (1)/results.html')])
        return self.runStep()


class TestPrintConfiguration(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success_mac(self):
        self.setupStep(PrintConfiguration())
        self.setProperty('buildername', 'macOS-High-Sierra-Release-WK2-Tests-EWS')
        self.setProperty('platform', 'mac-highsierra')

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
            + ExpectShell.log('stdio', stdout='''ProductName:	Mac OS X
ProductVersion:	10.13.4
BuildVersion:	17E199'''),
            ExpectShell(command=['xcodebuild', '-sdk', '-version'], workdir='wkdir', timeout=60, logEnviron=False)
            + ExpectShell.log('stdio', stdout='''MacOSX10.13.sdk - macOS 10.13 (macosx10.13)
SDKVersion: 10.13
Path: /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.13.sdk
PlatformVersion: 1.1
PlatformPath: /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform
ProductBuildVersion: 17E189
ProductCopyright: 1983-2018 Apple Inc.
ProductName: Mac OS X
ProductUserVisibleVersion: 10.13.4
ProductVersion: 10.13.4

Xcode 9.4.1
Build version 9F2000''')
            + 0,
            ExpectShell(command=['uptime'], workdir='wkdir', timeout=60, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=' 6:31  up 1 day, 19:05, 24 users, load averages: 4.17 7.23 5.45'),
        )
        self.expectOutcome(result=SUCCESS, state_string='OS: High Sierra (10.13.4), Xcode: 9.4.1')
        return self.runStep()

    def test_success_ios_simulator(self):
        self.setupStep(PrintConfiguration())
        self.setProperty('buildername', 'macOS-Sierra-Release-WK2-Tests-EWS')
        self.setProperty('platform', 'ios-simulator-12')

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
            + ExpectShell.log('stdio', stdout='''ProductName:	Mac OS X
ProductVersion:	10.14.5
BuildVersion:	18F132'''),
            ExpectShell(command=['xcodebuild', '-sdk', '-version'], workdir='wkdir', timeout=60, logEnviron=False)
            + ExpectShell.log('stdio', stdout='''iPhoneSimulator12.2.sdk - Simulator - iOS 12.2 (iphonesimulator12.2)
SDKVersion: 12.2
Path: /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator12.2.sdk
PlatformVersion: 12.2
PlatformPath: /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform
BuildID: 15C4BAF8-4632-11E9-86EB-BA47F1FFAC3C
ProductBuildVersion: 16E226
ProductCopyright: 1983-2019 Apple Inc.
ProductName: iPhone OS
ProductVersion: 12.2

Xcode 10.2
Build version 10E125''')
            + 0,
            ExpectShell(command=['uptime'], workdir='wkdir', timeout=60, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=' 6:31  up 1 day, 19:05, 24 users, load averages: 4.17 7.23 5.45'),
        )
        self.expectOutcome(result=SUCCESS, state_string='OS: Mojave (10.14.5), Xcode: 10.2')
        return self.runStep()

    def test_success_webkitpy(self):
        self.setupStep(PrintConfiguration())
        self.setProperty('platform', '*')

        self.expectRemoteCommands(
            ExpectShell(command=['hostname'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
            ExpectShell(command=['df', '-hl'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
            ExpectShell(command=['date'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
            ExpectShell(command=['sw_vers'], workdir='wkdir', timeout=60, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='''ProductName:	Mac OS X
ProductVersion:	10.13.6
BuildVersion:	17G7024'''),
            ExpectShell(command=['xcodebuild', '-sdk', '-version'], workdir='wkdir', timeout=60, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout='''Xcode 10.2\nBuild version 10E125'''),
            ExpectShell(command=['uptime'], workdir='wkdir', timeout=60, logEnviron=False) + 0
            + ExpectShell.log('stdio', stdout=' 6:31  up 22 seconds, 12:05, 2 users, load averages: 3.17 7.23 5.45'),
        )
        self.expectOutcome(result=SUCCESS, state_string='OS: High Sierra (10.13.6), Xcode: 10.2')
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
            ExpectShell(command=['uptime'], workdir='wkdir', timeout=60, logEnviron=False) + 0,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to print configuration')
        return self.runStep()


if __name__ == '__main__':
    unittest.main()
