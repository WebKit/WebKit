# Copyright (C) 2018 Apple Inc. All rights reserved.
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

from buildbot.process.results import Results, SUCCESS, FAILURE, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.test.fake.remotecommand import ExpectShell
from buildbot.test.util.steps import BuildStepMixin
from twisted.internet import error, reactor
from twisted.python import failure, log
from twisted.trial import unittest

from steps import *


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
                        command=['Tools/Scripts/check-webkit-style'],
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
                        command=['Tools/Scripts/check-webkit-style'],
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
                        command=['Tools/Scripts/check-webkit-style'],
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
        self.expectOutcome(result=FAILURE, state_string='check-webkit-style (failure)')
        return self.runStep()

    def test_failures_no_style_issues(self):
        self.setupStep(CheckStyle())
        self.setProperty('try-codebase', 'internal')
        self.setProperty('platform', 'mac')
        self.setProperty('configuration', 'debug')

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['Tools/Scripts/check-webkit-style'],
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
                        command=['Tools/Scripts/check-webkit-style'],
                        )
            + ExpectShell.log('stdio', stdout='Total errors found: 0 in 0 files')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='check-webkit-style (failure)')
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
                        command=['Tools/Scripts/run-bindings-tests', '--json-output={0}'.format(self.jsonFileName)],
                        logfiles={'json': self.jsonFileName},
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='bindings-tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunBindingsTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['Tools/Scripts/run-bindings-tests', '--json-output={0}'.format(self.jsonFileName)],
                        logfiles={'json': self.jsonFileName},
                        )
            + ExpectShell.log('stdio', stdout='FAIL: (JS) JSTestInterface.cpp')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='bindings-tests (failure)')
        return self.runStep()


class TestunWebKitPerlTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunWebKitPerlTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['Tools/Scripts/test-webkitperl'],
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
                        command=['Tools/Scripts/test-webkitperl'],
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
                        command=['Tools/Scripts/test-webkitpy', '--json-output={0}'.format(self.jsonFileName)],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='webkitpy-tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunWebKitPyTests())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['Tools/Scripts/test-webkitpy', '--json-output={0}'.format(self.jsonFileName)],
                        logfiles={'json': self.jsonFileName},
                        timeout=120,
                        )
            + ExpectShell.log('stdio', stdout='''Ran 1744 tests in 5.913s
FAILED (failures=1, errors=0)''')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='webkitpy-tests (failure)')
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
                        timeout=60,
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='killed old processes')
        return self.runStep()

    def test_failure(self):
        self.setupStep(KillOldProcesses())
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python', 'Tools/BuildSlaveSupport/kill-old-processes', 'buildbot'],
                        timeout=60,
                        )
            + ExpectShell.log('stdio', stdout='Unexpected error.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='killed old processes (failure)')
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
        self.expectOutcome(result=SUCCESS, state_string='deleted WebKitBuild directory')
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
        self.expectOutcome(result=FAILURE, state_string='deleted WebKitBuild directory (failure)')
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
                        command=["perl", "Tools/Scripts/build-webkit", '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='compiled')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CompileWebKit())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=["perl", "Tools/Scripts/build-webkit", '--debug'],
                        )
            + ExpectShell.log('stdio', stdout='1 error generated.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='compiled (failure)')
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
                        command=['perl', 'Tools/Scripts/build-webkit', '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='compiled')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CompileWebKitToT())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.setProperty('patchFailedToBuild', True)
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['perl', 'Tools/Scripts/build-webkit', '--debug'],
                        )
            + ExpectShell.log('stdio', stdout='1 error generated.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='compiled (failure)')
        return self.runStep()

    def test_skip(self):
        self.setupStep(CompileWebKitToT())
        self.setProperty('fullPlatform', 'ios-simulator-11')
        self.setProperty('configuration', 'release')
        self.expectHidden(True)
        self.expectOutcome(result=SKIPPED, state_string='compiled (skipped)')
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
                        command=["perl", "Tools/Scripts/build-jsc", '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='compiled')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CompileJSCOnly())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=["perl", "Tools/Scripts/build-jsc", '--debug'],
                        )
            + ExpectShell.log('stdio', stdout='1 error generated.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='compiled (failure)')
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
                        command=['perl', 'Tools/Scripts/build-jsc', '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='compiled')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CompileJSCOnlyToT())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.setProperty('patchFailedToBuild', 'True')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['perl', 'Tools/Scripts/build-jsc', '--debug'],
                        )
            + ExpectShell.log('stdio', stdout='1 error generated.')
            + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='compiled (failure)')
        return self.runStep()

    def test_skip(self):
        self.setupStep(CompileJSCOnlyToT())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.expectHidden(True)
        self.expectOutcome(result=SKIPPED, state_string='compiled (skipped)')
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
        self.setProperty('patchFailedJSCTests', 'True')
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
        self.setProperty('patchFailedJSCTests', 'True')
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
        self.setProperty('patchFailedJSCTests', 'True')
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
        self.setProperty('patchFailedJSCTests', 'True')
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


if __name__ == '__main__':
    unittest.main()
