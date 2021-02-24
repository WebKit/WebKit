# Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
import operator
import os
import shutil
import tempfile

from buildbot.process.results import Results, SUCCESS, FAILURE, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.test.fake.remotecommand import Expect, ExpectRemoteRef, ExpectShell
from buildbot.test.util.misc import TestReactorMixin
from buildbot.test.util.steps import BuildStepMixin
from buildbot.util import identifiers as buildbot_identifiers
from mock import call
from twisted.internet import defer, error, reactor
from twisted.python import failure, log
from twisted.trial import unittest

from steps import *

CURRENT_HOSTNAME = socket.gethostname().strip()


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


class BuildStepMixinAdditions(BuildStepMixin, TestReactorMixin):
    def setUpBuildStep(self):
        self.patch(reactor, 'spawnProcess', lambda *args, **kwargs: self._checkSpawnProcess(*args, **kwargs))
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


class TestStepNameShouldBeValidIdentifier(BuildStepMixinAdditions, unittest.TestCase):
    def test_step_names_are_valid(self):
        import steps
        build_step_classes = inspect.getmembers(steps, inspect.isclass)
        for build_step in build_step_classes:
            if 'name' in vars(build_step[1]):
                name = build_step[1].name
                self.assertFalse(' ' in name, 'step name "{}" contain space.'.format(name))
                self.assertTrue(buildbot_identifiers.ident_re.match(name), 'step name "{}" is not a valid buildbot identifier.'.format(name))


class TestRunBindingsTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunBindingsTests())
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['python', './Tools/Scripts/run-bindings-tests'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='bindings-tests')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunBindingsTests())
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['python', './Tools/Scripts/run-bindings-tests'],
            ) + 2
            + ExpectShell.log('stdio', stdout='FAIL: (JS) JSTestInterface.cpp'),
        )
        self.expectOutcome(result=FAILURE, state_string='bindings-tests (failure)')
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
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['python', './Tools/CISupport/kill-old-processes', 'buildbot'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='killed old processes')
        return self.runStep()

    def test_failure(self):
        self.setupStep(KillOldProcesses())
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['python', './Tools/CISupport/kill-old-processes', 'buildbot'],
            ) + 2
            + ExpectShell.log('stdio', stdout='Unexpected error.'),
        )
        self.expectOutcome(result=FAILURE, state_string='killed old processes (failure)')
        return self.runStep()


class TestCleanBuildIfScheduled(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(CleanBuildIfScheduled())
        self.setProperty('fullPlatform', 'ios-14')
        self.setProperty('configuration', 'release')
        self.setProperty('is_clean', 'True')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['python', './Tools/CISupport/clean-build', '--platform=ios-14', '--release'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='deleted WebKitBuild directory')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CleanBuildIfScheduled())
        self.setProperty('fullPlatform', 'ios-simulator-14')
        self.setProperty('configuration', 'debug')
        self.setProperty('is_clean', 'True')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['python', './Tools/CISupport/clean-build', '--platform=ios-simulator-14', '--debug'],
            ) + 2
            + ExpectShell.log('stdio', stdout='Unexpected error.'),
        )
        self.expectOutcome(result=FAILURE, state_string='deleted WebKitBuild directory (failure)')
        return self.runStep()

    def test_skip(self):
        self.setupStep(CleanBuildIfScheduled())
        self.setProperty('fullPlatform', 'ios-simulator-14')
        self.setProperty('configuration', 'debug')
        self.expectOutcome(result=SKIPPED, state_string='deleted WebKitBuild directory (skipped)')
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
                        timeout=1200,
                        logEnviron=True,
                        command=['perl', './Tools/Scripts/update-webkitgtk-libs', '--release'],
                        )
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='updated gtk dependencies')
        return self.runStep()

    def test_failure(self):
        self.setupStep(InstallGtkDependencies())
        self.setProperty('configuration', 'debug')
        self.assertEqual(InstallGtkDependencies.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['perl', './Tools/Scripts/update-webkitgtk-libs', '--debug'],
            ) + 2
            + ExpectShell.log('stdio', stdout='Unexpected error.'),
        )
        self.expectOutcome(result=FAILURE, state_string='updated gtk dependencies (failure)')
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
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['perl', './Tools/Scripts/update-webkitwpe-libs', '--release'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='updated wpe dependencies')
        return self.runStep()

    def test_failure(self):
        self.setupStep(InstallWpeDependencies())
        self.setProperty('configuration', 'release')
        self.assertEqual(InstallWpeDependencies.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['perl', './Tools/Scripts/update-webkitwpe-libs', '--release'],
            ) + 2
            + ExpectShell.log('stdio', stdout='Unexpected error.'),
        )
        self.expectOutcome(result=FAILURE, state_string='updated wpe dependencies (failure)')
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
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['perl', './Tools/Scripts/build-webkit', '--release'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='compiled')
        return self.runStep()

    def test_success_gtk(self):
        self.setupStep(CompileWebKit())
        self.setProperty('platform', 'gtk')
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['perl', './Tools/Scripts/build-webkit', '--release', '--gtk'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='compiled')
        return self.runStep()

    def test_success_wpe(self):
        self.setupStep(CompileWebKit())
        self.setProperty('platform', 'wpe')
        self.setProperty('fullPlatform', 'wpe')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['perl', './Tools/Scripts/build-webkit', '--release', '--wpe'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='compiled')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CompileWebKit())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['perl', './Tools/Scripts/build-webkit', '--debug'],
            ) + 2
            + ExpectShell.log('stdio', stdout='1 error generated.'),
        )
        self.expectOutcome(result=FAILURE, state_string='compiled (failure)')
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
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['perl', './Tools/Scripts/build-jsc', '--release'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='compiled')
        return self.runStep()

    def test_failure(self):
        self.setupStep(CompileJSCOnly())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                logEnviron=True,
                command=['perl', './Tools/Scripts/build-jsc', '--debug'],
            ) + 2
            + ExpectShell.log('stdio', stdout='1 error generated.'),
        )
        self.expectOutcome(result=FAILURE, state_string='compiled (failure)')
        return self.runStep()


class TestShowIdentifier(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(ShowIdentifier())
        self.setProperty('got_revision', '272692')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=600,
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/git-webkit', 'find', 'r272692']) +
            ExpectShell.log('stdio', stdout='Identifier: 233939@main') +
            0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Identifier: 233939@main')
        rc = self.runStep()
        self.assertEqual(self.getProperty('identifier'), '233939@main')
        return rc

    def test_failure(self):
        self.setupStep(ShowIdentifier())
        self.setProperty('got_revision', '272692')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=600,
                        logEnviron=False,
                        command=['python', 'Tools/Scripts/git-webkit', 'find', 'r272692']) +
            ExpectShell.log('stdio', stdout='Unexpected failure') +
            2,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to find identifier')
        return self.runStep()


class TestRunWebKitPerlTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunPerlTests())
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['perl', './Tools/Scripts/test-webkitperl'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='webkitperl-test')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunPerlTests())
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['perl', './Tools/Scripts/test-webkitperl'],
            ) + 2
            + ExpectShell.log('stdio', stdout='''Failed tests:  1-3, 5-7, 9, 11-13
Files=40, Tests=630,  4 wallclock secs ( 0.16 usr  0.09 sys +  2.78 cusr  0.64 csys =  3.67 CPU)
Result: FAIL
Failed 1/40 test programs. 10/630 subtests failed.'''),
        )
        self.expectOutcome(result=FAILURE, state_string='10 perl tests failed')
        return self.runStep()


class TestRunWebKitPyTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        os.environ['RESULTS_SERVER_API_KEY'] = 'test-api-key'
        return self.setUpBuildStep()

    def tearDown(self):
        del os.environ['RESULTS_SERVER_API_KEY']
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(RunWebKitPyTests())
        self.setProperty('buildername', 'WebKitPy-Tests-EWS')
        self.setProperty('buildnumber', '101')
        self.setProperty('workername', 'ews100')

    def test_success(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=False,
                command=['python3', './Tools/Scripts/test-webkitpy', '--verbose',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--builder-name', 'WebKitPy-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--report', RESULTS_WEBKIT_URL],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='webkitpy-test')
        return self.runStep()

    def test_unexpected_failure(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=False,
                command=['python3', './Tools/Scripts/test-webkitpy', '--verbose',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--builder-name', 'WebKitPy-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--report', RESULTS_WEBKIT_URL],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ) + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='webkitpy-test (failure)')
        return self.runStep()

    def test_failure(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=False,
                command=['python3', './Tools/Scripts/test-webkitpy', '--verbose',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--builder-name', 'WebKitPy-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--report', RESULTS_WEBKIT_URL],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ) + 2
            + ExpectShell.log('stdio', stdout='FAILED (failures=2, errors=0)'),
        )
        self.expectOutcome(result=FAILURE, state_string='2 python tests failed')
        return self.runStep()

    def test_errors(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=False,
                command=['python3', './Tools/Scripts/test-webkitpy', '--verbose',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--builder-name', 'WebKitPy-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--report', RESULTS_WEBKIT_URL],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ) + 2
            + ExpectShell.log('stdio', stdout='FAILED (failures=0, errors=2)'),
        )
        self.expectOutcome(result=FAILURE, state_string='2 python tests failed')
        return self.runStep()

    def test_lot_of_failures(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=False,
                command=['python3', './Tools/Scripts/test-webkitpy', '--verbose',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--builder-name', 'WebKitPy-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--report', RESULTS_WEBKIT_URL],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ) + 2
            + ExpectShell.log('stdio', stdout='FAILED (failures=30, errors=2)'),
        )
        self.expectOutcome(result=FAILURE, state_string='32 python tests failed')
        return self.runStep()


class TestRunLLDBWebKitTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def test_success(self):
        self.setupStep(RunLLDBWebKitTests())
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['python', './Tools/Scripts/test-lldb-webkit', '--verbose', '--no-build', '--release'],
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='lldb-webkit-test')
        return self.runStep()

    def test_unexpected_failure(self):
        self.setupStep(RunLLDBWebKitTests())
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['python', './Tools/Scripts/test-lldb-webkit', '--verbose', '--no-build', '--release'],
            ) + 2,
        )
        self.expectOutcome(result=FAILURE, state_string='lldb-webkit-test (failure)')
        return self.runStep()

    def test_failure(self):
        self.setupStep(RunLLDBWebKitTests())
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['python', './Tools/Scripts/test-lldb-webkit', '--verbose', '--no-build', '--release'],
            ) + 2
            + ExpectShell.log('stdio', stdout='FAILED (failures=2, errors=0)'),
        )
        self.expectOutcome(result=FAILURE, state_string='2 lldb tests failed')
        return self.runStep()

    def test_errors(self):
        self.setupStep(RunLLDBWebKitTests())
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['python', './Tools/Scripts/test-lldb-webkit', '--verbose', '--no-build', '--release'],
            ) + 2
            + ExpectShell.log('stdio', stdout='FAILED (failures=0, errors=2)'),
        )
        self.expectOutcome(result=FAILURE, state_string='2 lldb tests failed')
        return self.runStep()

    def test_lot_of_failures(self):
        self.setupStep(RunLLDBWebKitTests())
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=True,
                command=['python', './Tools/Scripts/test-lldb-webkit', '--verbose', '--no-build', '--release'],
            ) + 2
            + ExpectShell.log('stdio', stdout='FAILED (failures=30, errors=2)'),
        )
        self.expectOutcome(result=FAILURE, state_string='32 lldb tests failed')
        return self.runStep()


class TestRunWebKitTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        os.environ['RESULTS_SERVER_API_KEY'] = 'test-api-key'
        return self.setUpBuildStep()

    def tearDown(self):
        del os.environ['RESULTS_SERVER_API_KEY']
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(RunWebKitTests())
        self.setProperty('buildername', 'iOS-14-Simulator-WK2-Tests-EWS')
        self.setProperty('buildnumber', '101')
        self.setProperty('workername', 'ews100')

    def test_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=False,
                command=['python', './Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results',
                         '--no-new-test-results', '--clobber-old-results',
                         '--builder-name', 'iOS-14-Simulator-WK2-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--master-name', 'webkit.org',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--report', RESULTS_WEBKIT_URL,
                         '--exit-after-n-crashes-or-timeouts', '50',
                         '--exit-after-n-failures', '500',
                         '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='layout-tests')
        return self.runStep()

    def test_warnings(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=False,
                command=['python', './Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results',
                         '--no-new-test-results', '--clobber-old-results',
                         '--builder-name', 'iOS-14-Simulator-WK2-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--master-name', 'webkit.org',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--report', RESULTS_WEBKIT_URL,
                         '--exit-after-n-crashes-or-timeouts', '50',
                         '--exit-after-n-failures', '500',
                         '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ) + 0
            + ExpectShell.log('stdio', stdout='''Unexpected flakiness: timeouts (2)
                              imported/blink/storage/indexeddb/blob-valid-before-commit.html [ Timeout Pass ]
                              storage/indexeddb/modern/deleteindex-2.html [ Timeout Pass ]'''),
        )
        self.expectOutcome(result=WARNINGS, state_string='2 flakes')
        return self.runStep()

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=False,
                command=['python', './Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results',
                         '--no-new-test-results', '--clobber-old-results',
                         '--builder-name', 'iOS-14-Simulator-WK2-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--master-name', 'webkit.org',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--report', RESULTS_WEBKIT_URL,
                         '--exit-after-n-crashes-or-timeouts', '50',
                         '--exit-after-n-failures', '500',
                         '--release', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ) + 2
            + ExpectShell.log('stdio', stdout='9 failures found.'),
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.runStep()

    def test_unexpected_error(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'mac-highsierra')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=False,
                command=['python', './Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results',
                         '--no-new-test-results', '--clobber-old-results',
                         '--builder-name', 'iOS-14-Simulator-WK2-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--master-name', 'webkit.org',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--report', RESULTS_WEBKIT_URL,
                         '--exit-after-n-crashes-or-timeouts', '50',
                         '--exit-after-n-failures', '500',
                         '--debug', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ) + 2
            + ExpectShell.log('stdio', stdout='Unexpected error.'),
        )
        self.expectOutcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.runStep()

    def test_exception(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'mac-highsierra')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                logEnviron=False,
                command=['python', './Tools/Scripts/run-webkit-tests', '--no-build', '--no-show-results',
                         '--no-new-test-results', '--clobber-old-results',
                         '--builder-name', 'iOS-14-Simulator-WK2-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--master-name', 'webkit.org',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--report', RESULTS_WEBKIT_URL,
                         '--exit-after-n-crashes-or-timeouts', '50',
                         '--exit-after-n-failures', '500',
                         '--debug', '--results-directory', 'layout-test-results', '--debug-rwt-logging'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ) + 254
            + ExpectShell.log('stdio', stdout='Unexpected error.'),
        )
        self.expectOutcome(result=EXCEPTION, state_string='layout-tests (exception)')
        return self.runStep()
