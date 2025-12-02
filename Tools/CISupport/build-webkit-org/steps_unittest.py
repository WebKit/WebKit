# Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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
from unittest import skip as skipTest

from buildbot.process.results import SUCCESS, FAILURE, WARNINGS, SKIPPED, EXCEPTION
from buildbot.test.fake.fakebuild import FakeBuild
from buildbot.test.reactor import TestReactorMixin
from buildbot.test.steps import Expect, ExpectShell
from buildbot.test.steps import TestBuildStepMixin as BuildStepMixin
from buildbot.util import identifiers as buildbot_identifiers
from twisted.internet import defer, error, reactor
from twisted.python import failure, log
from twisted.trial import unittest

from .steps import *

CURRENT_HOSTNAME = socket.gethostname().strip()
# Workaround for https://github.com/buildbot/buildbot/issues/4669
FakeBuild.addStepsAfterLastStep = lambda FakeBuild, step_factories: None
FakeBuild._builderid = 1


def expectedFailure(f):
    """A unittest.expectedFailure-like decorator for twisted.trial.unittest"""
    f.todo = 'expectedFailure'
    return f


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
        self._expected_local_commands = []
        self.setup_test_reactor()

        self._temp_directory = tempfile.mkdtemp()
        os.chdir(self._temp_directory)
        self._expected_uploaded_files = []

        super(BuildStepMixinAdditions, self).setup_test_build_step()

    def tear_down_test_build_step(self):
        shutil.rmtree(self._temp_directory)

    def fakeBuildFinished(self, text, results):
        self.build.text = text
        self.build.results = results

    def setup_step(self, step, *args, **kwargs):
        self.previous_steps = kwargs.get('previous_steps') or []
        if self.previous_steps:
            del kwargs['previous_steps']

        super(BuildStepMixinAdditions, self).setup_step(step, *args, **kwargs)
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
        deferred_result = super(BuildStepMixinAdditions, self).run_step()
        deferred_result.addCallback(check)
        return deferred_result


class TestStepNameShouldBeValidIdentifier(BuildStepMixinAdditions, unittest.TestCase):
    def test_step_names_are_valid(self):
        from . import steps
        build_step_classes = inspect.getmembers(steps, inspect.isclass)
        for build_step in build_step_classes:
            if 'name' in vars(build_step[1]):
                name = build_step[1].name
                self.assertFalse(' ' in name, f'step name "{name}" contain space.')
                self.assertTrue(buildbot_identifiers.ident_re.match(name), f'step name "{name}" is not a valid buildbot identifier.')


class TestRunBindingsTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RunBindingsTests())
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['python3', 'Tools/Scripts/run-bindings-tests'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='bindings-tests')
        return self.run_step()

    def test_failure(self):
        self.setup_step(RunBindingsTests())
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['python3', 'Tools/Scripts/run-bindings-tests'],
            ).exit(2)
            .log('stdio', stdout='FAIL: (JS) JSTestInterface.cpp'),
        )
        self.expect_outcome(result=FAILURE, state_string='bindings-tests (failure)')
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
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['python3', 'Tools/CISupport/kill-old-processes', 'buildbot'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='killed old processes')
        return self.run_step()

    def test_failure(self):
        self.setup_step(KillOldProcesses())
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['python3', 'Tools/CISupport/kill-old-processes', 'buildbot'],
            ).exit(2)
            .log('stdio', stdout='Unexpected error.'),
        )
        self.expect_outcome(result=FAILURE, state_string='killed old processes (failure)')
        return self.run_step()


class TestCleanBuildIfScheduled(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CleanBuildIfScheduled())
        self.setProperty('fullPlatform', 'ios-14')
        self.setProperty('configuration', 'release')
        self.setProperty('is_clean', 'True')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['python3', 'Tools/CISupport/clean-build', '--platform=ios-14', '--release'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='deleted WebKitBuild directory')
        return self.run_step()

    def test_failure(self):
        self.setup_step(CleanBuildIfScheduled())
        self.setProperty('fullPlatform', 'ios-simulator-14')
        self.setProperty('configuration', 'debug')
        self.setProperty('is_clean', 'True')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['python3', 'Tools/CISupport/clean-build', '--platform=ios-simulator-14', '--debug'],
            ).exit(2)
            .log('stdio', stdout='Unexpected error.'),
        )
        self.expect_outcome(result=FAILURE, state_string='deleted WebKitBuild directory (failure)')
        return self.run_step()

    def test_skip(self):
        self.setup_step(CleanBuildIfScheduled())
        self.setProperty('fullPlatform', 'ios-simulator-14')
        self.setProperty('configuration', 'debug')
        self.expect_outcome(result=SKIPPED, state_string='deleted WebKitBuild directory (skipped)')
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
                        timeout=1200,
                        log_environ=True,
                        command=['perl', 'Tools/Scripts/update-webkitgtk-libs', '--release'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='updated gtk dependencies')
        return self.run_step()

    def test_failure(self):
        self.setup_step(InstallGtkDependencies())
        self.setProperty('configuration', 'debug')
        self.assertEqual(InstallGtkDependencies.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['perl', 'Tools/Scripts/update-webkitgtk-libs', '--debug'],
            ).exit(2)
            .log('stdio', stdout='Unexpected error.'),
        )
        self.expect_outcome(result=FAILURE, state_string='updated gtk dependencies (failure)')
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
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['perl', 'Tools/Scripts/update-webkitwpe-libs', '--release'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='updated wpe dependencies')
        return self.run_step()

    def test_failure(self):
        self.setup_step(InstallWpeDependencies())
        self.setProperty('configuration', 'release')
        self.assertEqual(InstallWpeDependencies.haltOnFailure, True)
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['perl', 'Tools/Scripts/update-webkitwpe-libs', '--release'],
            ).exit(2)
            .log('stdio', stdout='Unexpected error.'),
        )
        self.expect_outcome(result=FAILURE, state_string='updated wpe dependencies (failure)')
        return self.run_step()


class TestCompileWebKit(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CompileWebKit())
        self.setProperty('fullPlatform', 'ios-simulator-11')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=3600,
                log_environ=True,
                command=['perl', 'Tools/Scripts/build-webkit', '--no-fatal-warnings', '--release'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='compiled')
        return self.run_step()

    def test_custom_timeout_specified_in_factory(self):
        self.setup_step(CompileWebKit(timeout=2 * 60 * 60))
        self.setProperty('fullPlatform', 'ios-simulator-11')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=True,
                command=['perl', 'Tools/Scripts/build-webkit', '--no-fatal-warnings', '--release'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='compiled')
        return self.run_step()

    def test_success_architecture(self):
        self.setup_step(CompileWebKit())
        self.setProperty('platform', 'mac')
        self.setProperty('fullPlatform', 'mac-sonoma')
        self.setProperty('configuration', 'release')
        self.setProperty('architecture', 'x86_64 arm64')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=3600,
                log_environ=True,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'perl Tools/Scripts/build-webkit --no-fatal-warnings --release --architecture "x86_64 arm64" WK_VALIDATE_DEPENDENCIES=YES 2>&1 | perl Tools/Scripts/filter-build-webkit -logfile build-log.txt'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='compiled')
        return self.run_step()

    def test_bigsur_timeout(self):
        self.setup_step(CompileWebKit())
        self.setProperty('fullPlatform', 'mac-sonoma')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=3600,
                log_environ=True,
                command=['perl', 'Tools/Scripts/build-webkit', '--no-fatal-warnings', '--release'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='compiled')
        return self.run_step()

    def test_success_gtk(self):
        self.setup_step(CompileWebKit())
        self.setProperty('platform', 'gtk')
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=3600,
                log_environ=True,
                command=['perl', 'Tools/Scripts/build-webkit', '--no-fatal-warnings', '--release', '--gtk'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='compiled')
        return self.run_step()

    def test_success_wpe(self):
        self.setup_step(CompileWebKit())
        self.setProperty('platform', 'wpe')
        self.setProperty('fullPlatform', 'wpe')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=3600,
                log_environ=True,
                command=['perl', 'Tools/Scripts/build-webkit', '--no-fatal-warnings', '--release', '--wpe'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='compiled')
        return self.run_step()

    def test_failure(self):
        self.setup_step(CompileWebKit())
        self.setProperty('fullPlatform', 'mac-sierra')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=3600,
                log_environ=True,
                command=['perl', 'Tools/Scripts/build-webkit', '--no-fatal-warnings', '--debug'],
            ).exit(2)
            .log('stdio', stdout='1 error generated.'),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed compile-webkit')
        return self.run_step()


class TestCompileJSCOnly(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(CompileJSCOnly())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=3600,
                log_environ=True,
                command=['perl', 'Tools/Scripts/build-jsc', '--release'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='compiled')
        return self.run_step()

    def test_failure(self):
        self.setup_step(CompileJSCOnly())
        self.setProperty('fullPlatform', 'jsc-only')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=3600,
                log_environ=True,
                command=['perl', 'Tools/Scripts/build-jsc', '--debug'],
            ).exit(2)
            .log('stdio', stdout='1 error generated.'),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed compile-jsc')
        return self.run_step()


class TestShowIdentifier(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    @defer.inlineCallbacks
    def test_success(self):
        self.setup_step(ShowIdentifier())
        self.setProperty('got_revision', 'd3f2b739b65eda1eeb651991a3554dfaeebdfe0b')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=600,
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/git-webkit', 'find', 'd3f2b739b65eda1eeb651991a3554dfaeebdfe0b'])
            .log('stdio', stdout='Identifier: 233939@main\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Identifier: 233939@main')
        rc = yield self.run_step()
        self.expect_property('identifier', '233939@main')
        defer.returnValue(rc)

    def test_failure(self):
        self.setup_step(ShowIdentifier())
        self.setProperty('got_revision', 'd3f2b739b65eda1eeb651991a3554dfaeebdfe0b')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=600,
                        log_environ=False,
                        command=['python3', 'Tools/Scripts/git-webkit', 'find', 'd3f2b739b65eda1eeb651991a3554dfaeebdfe0b'])
            .log('stdio', stdout='Unexpected failure')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to find identifier')
        return self.run_step()


class TestRunWebKitPerlTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RunPerlTests())
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['perl', 'Tools/Scripts/test-webkitperl'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='webkitperl-test')
        return self.run_step()

    def test_failure(self):
        self.setup_step(RunPerlTests())
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['perl', 'Tools/Scripts/test-webkitperl'],
            ).exit(2)
            .log('stdio', stdout='''Failed tests:  1-3, 5-7, 9, 11-13
Files=40, Tests=630,  4 wallclock secs ( 0.16 usr  0.09 sys +  2.78 cusr  0.64 csys =  3.67 CPU)
Result: FAIL
Failed 1/40 test programs. 10/630 subtests failed.'''),
        )
        self.expect_outcome(result=FAILURE, state_string='10 perl tests failed')
        return self.run_step()


class TestRunWebKitPyTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        os.environ['RESULTS_SERVER_API_KEY'] = 'test-api-key'
        return self.setup_test_build_step()

    def tearDown(self):
        del os.environ['RESULTS_SERVER_API_KEY']
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(RunWebKitPyTests())
        self.setProperty('buildername', 'WebKitPy-Tests-EWS')
        self.setProperty('buildnumber', '101')
        self.setProperty('workername', 'ews100')

    def test_success(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=False,
                command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--builder-name', 'WebKitPy-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--report', RESULTS_WEBKIT_URL],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='webkitpy-test')
        return self.run_step()

    def test_unexpected_failure(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=False,
                command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--builder-name', 'WebKitPy-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--report', RESULTS_WEBKIT_URL],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='webkitpy-test (failure)')
        return self.run_step()

    def test_failure(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=False,
                command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--builder-name', 'WebKitPy-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--report', RESULTS_WEBKIT_URL],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(2)
            .log('stdio', stdout='FAILED (failures=2, errors=0)'),
        )
        self.expect_outcome(result=FAILURE, state_string='2 python tests failed')
        return self.run_step()

    def test_errors(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=False,
                command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--builder-name', 'WebKitPy-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--report', RESULTS_WEBKIT_URL],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(2)
            .log('stdio', stdout='FAILED (failures=0, errors=2)'),
        )
        self.expect_outcome(result=FAILURE, state_string='2 python tests failed')
        return self.run_step()

    def test_lot_of_failures(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=False,
                command=['python3', 'Tools/Scripts/test-webkitpy', '--verbose',
                         '--buildbot-master', CURRENT_HOSTNAME,
                         '--builder-name', 'WebKitPy-Tests-EWS',
                         '--build-number', '101', '--buildbot-worker', 'ews100',
                         '--report', RESULTS_WEBKIT_URL],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(2)
            .log('stdio', stdout='FAILED (failures=30, errors=2)'),
        )
        self.expect_outcome(result=FAILURE, state_string='32 python tests failed')
        return self.run_step()


class TestRunLLDBWebKitTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RunLLDBWebKitTests())
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['python3', 'Tools/Scripts/test-lldb-webkit', '--verbose', '--no-build', '--release'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='lldb-webkit-test')
        return self.run_step()

    def test_unexpected_failure(self):
        self.setup_step(RunLLDBWebKitTests())
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['python3', 'Tools/Scripts/test-lldb-webkit', '--verbose', '--no-build', '--release'],
            ).exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='lldb-webkit-test (failure)')
        return self.run_step()

    def test_failure(self):
        self.setup_step(RunLLDBWebKitTests())
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['python3', 'Tools/Scripts/test-lldb-webkit', '--verbose', '--no-build', '--release'],
            ).exit(2)
            .log('stdio', stdout='FAILED (failures=2, errors=0)'),
        )
        self.expect_outcome(result=FAILURE, state_string='2 lldb tests failed')
        return self.run_step()

    def test_errors(self):
        self.setup_step(RunLLDBWebKitTests())
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['python3', 'Tools/Scripts/test-lldb-webkit', '--verbose', '--no-build', '--release'],
            ).exit(2)
            .log('stdio', stdout='FAILED (failures=0, errors=2)'),
        )
        self.expect_outcome(result=FAILURE, state_string='2 lldb tests failed')
        return self.run_step()

    def test_lot_of_failures(self):
        self.setup_step(RunLLDBWebKitTests())
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=1200,
                log_environ=True,
                command=['python3', 'Tools/Scripts/test-lldb-webkit', '--verbose', '--no-build', '--release'],
            ).exit(2)
            .log('stdio', stdout='FAILED (failures=30, errors=2)'),
        )
        self.expect_outcome(result=FAILURE, state_string='32 lldb tests failed')
        return self.run_step()


class TestRunWebKitTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        os.environ['RESULTS_SERVER_API_KEY'] = 'test-api-key'
        return self.setup_test_build_step()

    def tearDown(self):
        del os.environ['RESULTS_SERVER_API_KEY']
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(RunWebKitTests())
        self.setProperty('buildername', 'iOS-14-Simulator-WK2-Tests-EWS')
        self.setProperty('buildnumber', '101')
        self.setProperty('workername', 'ews100')

    @expectedFailure
    def test_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=10800,
                log_environ=False,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                         f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --builder-name iOS-14-Simulator-WK2-Tests-EWS --build-number 101 --buildbot-worker ews100 --buildbot-master {CURRENT_HOSTNAME} --exit-after-n-crashes-or-timeouts 50 --exit-after-n-failures 500 --release --report {RESULTS_WEBKIT_URL} --results-directory layout-test-results --debug-rwt-logging 2>&1 | python3 Tools/Scripts/filter-test-logs layout'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='layout-tests')
        rc = self.run_step()
        self.assertEqual([GenerateS3URL('ios-simulator-None-release-layout-test',  additions='13', extension='txt', content_type='text/plain'), UploadFileToS3('logs.txt', links={'layout-test': 'Full logs'}, content_type='text/plain')], next_steps)
        return rc

    def test_warnings(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=10800,
                log_environ=False,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                         f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --builder-name iOS-14-Simulator-WK2-Tests-EWS --build-number 101 --buildbot-worker ews100 --buildbot-master {CURRENT_HOSTNAME} --exit-after-n-crashes-or-timeouts 50 --exit-after-n-failures 500 --release --report {RESULTS_WEBKIT_URL} --results-directory layout-test-results --debug-rwt-logging 2>&1 | python3 Tools/Scripts/filter-test-logs layout'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(0)
            .log('stdio', stdout='''Unexpected flakiness: timeouts (2)
                              imported/blink/storage/indexeddb/blob-valid-before-commit.html [ Timeout Pass ]
                              storage/indexeddb/modern/deleteindex-2.html [ Timeout Pass ]'''),
        )
        self.expect_outcome(result=WARNINGS, state_string='2 flakes')
        return self.run_step()

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=10800,
                log_environ=False,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                         f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --builder-name iOS-14-Simulator-WK2-Tests-EWS --build-number 101 --buildbot-worker ews100 --buildbot-master {CURRENT_HOSTNAME} --exit-after-n-crashes-or-timeouts 50 --exit-after-n-failures 500 --release --report {RESULTS_WEBKIT_URL} --results-directory layout-test-results --debug-rwt-logging 2>&1 | python3 Tools/Scripts/filter-test-logs layout'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(2)
            .log('stdio', stdout='9 failures found.'),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.run_step()

    def test_unexpected_error(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'mac-highsierra')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=10800,
                log_environ=False,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                         f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --builder-name iOS-14-Simulator-WK2-Tests-EWS --build-number 101 --buildbot-worker ews100 --buildbot-master {CURRENT_HOSTNAME} --exit-after-n-crashes-or-timeouts 50 --exit-after-n-failures 500 --debug --report {RESULTS_WEBKIT_URL} --results-directory layout-test-results --debug-rwt-logging 2>&1 | python3 Tools/Scripts/filter-test-logs layout'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(2)
            .log('stdio', stdout='Unexpected error.'),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.run_step()

    def test_exception(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'mac-highsierra')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=10800,
                log_environ=False,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                         f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --builder-name iOS-14-Simulator-WK2-Tests-EWS --build-number 101 --buildbot-worker ews100 --buildbot-master {CURRENT_HOSTNAME} --exit-after-n-crashes-or-timeouts 50 --exit-after-n-failures 500 --debug --report {RESULTS_WEBKIT_URL} --results-directory layout-test-results --debug-rwt-logging 2>&1 | python3 Tools/Scripts/filter-test-logs layout'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(254)
            .log('stdio', stdout='Unexpected error.'),
        )
        self.expect_outcome(result=EXCEPTION, state_string='layout-tests (exception)')
        return self.run_step()

    def test_gtk_parameters(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('platform', 'gtk')
        self.setProperty('configuration', 'release')
        self.setProperty('buildername', 'GTK-Linux-64-bit-Release-Tests')
        self.setProperty('buildnumber', '103')
        self.setProperty('workername', 'gtk103')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=10800,
                log_environ=False,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                         f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --builder-name GTK-Linux-64-bit-Release-Tests --build-number 103 --buildbot-worker gtk103 --buildbot-master {CURRENT_HOSTNAME} --exit-after-n-crashes-or-timeouts 50 --exit-after-n-failures 500 --release --gtk --report {RESULTS_WEBKIT_URL} --results-directory layout-test-results --debug-rwt-logging --enable-core-dumps-nolimit 2>&1 | python3 Tools/Scripts/filter-test-logs layout'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='layout-tests')
        return self.run_step()

    def test_wpe_parameters(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'wpe')
        self.setProperty('platform', 'wpe')
        self.setProperty('configuration', 'release')
        self.setProperty('buildername', 'WPE-Linux-64-bit-Release-Tests')
        self.setProperty('buildnumber', '103')
        self.setProperty('workername', 'wpe103')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=10800,
                log_environ=False,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                         f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --builder-name WPE-Linux-64-bit-Release-Tests --build-number 103 --buildbot-worker wpe103 --buildbot-master {CURRENT_HOSTNAME} --exit-after-n-crashes-or-timeouts 50 --exit-after-n-failures 500 --release --wpe --report {RESULTS_WEBKIT_URL} --results-directory layout-test-results --debug-rwt-logging --enable-core-dumps-nolimit 2>&1 | python3 Tools/Scripts/filter-test-logs layout'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='layout-tests')
        return self.run_step()

    def test_site_isolation_timeout(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'mac-highsierra')
        self.setProperty('configuration', 'debug')
        self.setProperty('additionalArguments', ['--site-isolation'])
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=36000,
                log_environ=False,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                         f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --builder-name iOS-14-Simulator-WK2-Tests-EWS --build-number 101 --buildbot-worker ews100 --buildbot-master {CURRENT_HOSTNAME} --exit-after-n-crashes-or-timeouts 50 --exit-after-n-failures 500 --debug --report {RESULTS_WEBKIT_URL} --results-directory layout-test-results --debug-rwt-logging --site-isolation 2>&1 | python3 Tools/Scripts/filter-test-logs layout'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(0)
        )
        self.expect_outcome(result=SUCCESS, state_string='layout-tests')
        return self.run_step()


class TestRunDashboardTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        os.environ['RESULTS_SERVER_API_KEY'] = 'test-api-key'
        return self.setup_test_build_step()

    def tearDown(self):
        del os.environ['RESULTS_SERVER_API_KEY']
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(RunDashboardTests())
        self.setProperty('buildername', 'Apple-Sequoia-Release-WK2-Tests')
        self.setProperty('buildnumber', '101')
        self.setProperty('workername', 'bot100')

    def test_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'mac-sequoia')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=10800,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                         f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --builder-name Apple-Sequoia-Release-WK2-Tests --build-number 101 --buildbot-worker bot100 --buildbot-master {CURRENT_HOSTNAME} --exit-after-n-crashes-or-timeouts 50 --exit-after-n-failures 500 --debug --no-http-servers --layout-tests-directory Tools/CISupport/build-webkit-org/public_html/dashboard/Scripts/tests --results-directory layout-test-results/dashboard-layout-test-results --debug-rwt-logging 2>&1 | python3 Tools/Scripts/filter-test-logs layout'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='dashboard-tests')
        return self.run_step()

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-14')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=10800,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                         f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --builder-name Apple-Sequoia-Release-WK2-Tests --build-number 101 --buildbot-worker bot100 --buildbot-master {CURRENT_HOSTNAME} --exit-after-n-crashes-or-timeouts 50 --exit-after-n-failures 500 --release --no-http-servers --layout-tests-directory Tools/CISupport/build-webkit-org/public_html/dashboard/Scripts/tests --results-directory layout-test-results/dashboard-layout-test-results --debug-rwt-logging 2>&1 | python3 Tools/Scripts/filter-test-logs layout'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ) .log('stdio', stdout='9 failures found.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='dashboard-tests (failure)')
        return self.run_step()


class TestRunWebKit1Tests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        os.environ['RESULTS_SERVER_API_KEY'] = 'test-api-key'
        return self.setup_test_build_step()

    def tearDown(self):
        del os.environ['RESULTS_SERVER_API_KEY']
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(RunWebKit1Tests())
        self.setProperty('buildername', 'Apple-iOS-14-Simulator-Debug-Build')
        self.setProperty('buildnumber', '101')
        self.setProperty('workername', 'bot100')

    @expectedFailure
    def test_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'debug')
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=10800,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                         f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --builder-name Apple-iOS-14-Simulator-Debug-Build --build-number 101 --buildbot-worker bot100 --buildbot-master {CURRENT_HOSTNAME} --exit-after-n-crashes-or-timeouts 50 --exit-after-n-failures 500 --debug --dump-render-tree --report {RESULTS_WEBKIT_URL} --results-directory layout-test-results --debug-rwt-logging 2>&1 | python3 Tools/Scripts/filter-test-logs layout'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='layout-tests')
        rc = self.run_step()
        self.assertEqual([GenerateS3URL('ios-simulator-None-debug-layout-test',  additions='13-wk1', extension='txt', content_type='text/plain'), UploadFileToS3('logs.txt', links={'layout-test': 'Full logs'}, content_type='text/plain')], next_steps)
        return rc

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-14')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=10800,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                         f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --builder-name Apple-iOS-14-Simulator-Debug-Build --build-number 101 --buildbot-worker bot100 --buildbot-master {CURRENT_HOSTNAME} --exit-after-n-crashes-or-timeouts 50 --exit-after-n-failures 500 --release --dump-render-tree --report {RESULTS_WEBKIT_URL} --results-directory layout-test-results --debug-rwt-logging 2>&1 | python3 Tools/Scripts/filter-test-logs layout'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ) .log('stdio', stdout='9 failures found.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='layout-tests (failure)')
        return self.run_step()


class TestRunWorldLeaksTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'layout-test-results/full_results.json'
        os.environ['RESULTS_SERVER_API_KEY'] = 'test-api-key'
        return self.setup_test_build_step()

    def tearDown(self):
        del os.environ['RESULTS_SERVER_API_KEY']
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(RunWorldLeaksTests())
        self.setProperty('buildername', 'Apple-iOS-14-Simulator-Debug-Build')
        self.setProperty('buildnumber', '101')
        self.setProperty('workername', 'bot100')

    def test_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-simulator')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=10800,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                         f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --exit-after-n-crashes-or-timeouts 50 --exit-after-n-failures 500 --debug --world-leaks --results-directory layout-test-results/world-leaks-layout-test-results --debug-rwt-logging 2>&1 | python3 Tools/Scripts/filter-test-logs layout'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='world-leaks-tests')
        return self.run_step()

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'ios-14')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=False,
                timeout=10800,
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c',
                         f'python3 Tools/Scripts/run-webkit-tests --no-build --no-show-results --no-new-test-results --clobber-old-results --exit-after-n-crashes-or-timeouts 50 --exit-after-n-failures 500 --release --world-leaks --results-directory layout-test-results/world-leaks-layout-test-results --debug-rwt-logging 2>&1 | python3 Tools/Scripts/filter-test-logs layout'],
                env={'RESULTS_SERVER_API_KEY': 'test-api-key'}
            ) .log('stdio', stdout='9 failures found.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='world-leaks-tests (failure)')
        return self.run_step()


class TestRunJavaScriptCoreTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        os.environ['RESULTS_SERVER_API_KEY'] = 'test-api-key'
        self.jsonFileName = 'jsc_results.json'
        return self.setup_test_build_step()

    def tearDown(self):
        del os.environ['RESULTS_SERVER_API_KEY']
        return self.tear_down_test_build_step()

    def configureStep(self, platform=None, fullPlatform=None, configuration=None):
        self.setup_step(RunJavaScriptCoreTests())
        self.commandExtra = RunJavaScriptCoreTests.commandExtra
        if platform:
            self.setProperty('platform', platform)
        if fullPlatform:
            self.setProperty('fullPlatform', fullPlatform)
        if configuration:
            self.setProperty('configuration', configuration)
        self.setProperty('buildername', 'JSC-Tests')
        self.setProperty('buildnumber', '101')
        self.setProperty('workername', 'bot100')

    def test_success(self):
        self.configureStep(platform='mac', fullPlatform='mac-highsierra', configuration='release')
        command = ['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', f'--json-output={self.jsonFileName}', '--release', '--builder-name', 'JSC-Tests', '--build-number', '101', '--buildbot-worker', 'bot100', '--buildbot-master', CURRENT_HOSTNAME, '--report', 'https://results.webkit.org'] + self.commandExtra
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', ' '.join(command) + ' 2>&1 | python3 Tools/Scripts/filter-test-logs jsc'],
                        logfiles={'json': self.jsonFileName},
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'},
                        timeout=72000,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='jscore-test')
        return self.run_step()

    def test_failure(self):
        self.configureStep(platform='mac', fullPlatform='mac-highsierra', configuration='debug')
        command = ['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', f'--json-output={self.jsonFileName}', '--debug', '--builder-name', 'JSC-Tests', '--build-number', '101', '--buildbot-worker', 'bot100', '--buildbot-master', CURRENT_HOSTNAME, '--report', 'https://results.webkit.org'] + self.commandExtra
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', ' '.join(command) + ' 2>&1 | python3 Tools/Scripts/filter-test-logs jsc'],
                        logfiles={'json': self.jsonFileName},
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'},
                        timeout=72000,
                        )
            .log('stdio', stdout='Results for JSC stress tests:\n 9 failures found.')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='9 JSC tests failed')
        return self.run_step()


class TestRunAPITests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        os.environ['RESULTS_SERVER_API_KEY'] = 'test-api-key'
        self.jsonFileName = 'api_test_results.json'
        return self.setup_test_build_step()

    def tearDown(self):
        del os.environ['RESULTS_SERVER_API_KEY']
        return self.tear_down_test_build_step()

    def configureStep(self, platform=None, fullPlatform=None, configuration=None):
        self.setup_step(RunAPITests())
        if platform:
            self.setProperty('platform', platform)
        if fullPlatform:
            self.setProperty('fullPlatform', fullPlatform)
        if configuration:
            self.setProperty('configuration', configuration)
        self.setProperty('buildername', 'API-Tests')
        self.setProperty('buildnumber', '101')
        self.setProperty('workername', 'bot100')

    def test_success(self):
        self.configureStep(platform='mac', fullPlatform='mac-highsierra', configuration='release')
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        command = f'python3 Tools/Scripts/run-api-tests --timestamps --no-build --json-output={self.jsonFileName} --release --verbose --buildbot-master {CURRENT_HOSTNAME} --builder-name API-Tests --build-number 101 --buildbot-worker bot100 --report https://results.webkit.org'
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', command + ' > logs.txt 2>&1 ; ret=$? ; grep "Ran " logs.txt ; exit $ret'],
                        logfiles={'json': self.jsonFileName},
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'},
                        timeout=10800,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='run-api-tests')
        rc = self.run_step()
        self.assertEqual([GenerateS3URL('mac-highsierra-None-release-run-api-tests', extension='txt', additions='13', content_type='text/plain'), UploadFileToS3('logs.txt', links={'run-api-tests': 'Full logs'}, content_type='text/plain')], next_steps)
        return rc


class TestSetPermissions(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    @expectedFailure
    def test_success(self):
        self.setup_step(SetPermissions())
        self.setProperty('result_directory', 'public_html/results/Apple-Sonoma-Release-WK2-Tests/r277034 (2346)')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['chmod', 'a+rx', 'public_html/results/Apple-Sonoma-Release-WK2-Tests/r277034 (2346)'])
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Ran')
        return self.run_step()

    @expectedFailure
    def test_failure(self):
        self.setup_step(SetPermissions())
        self.setProperty('result_directory', 'testdir')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['chmod', 'a+rx', 'testdir'])
            .exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='failed (1) (failure)')
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
            ExpectShell(
                workdir='wkdir',
                timeout=120,
                log_environ=False,
                command=['rm', '-f', '.git/index.lock'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Deleted .git/index.lock')
        return self.run_step()

    def test_failure(self):
        self.setup_step(CleanUpGitIndexLock())
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                timeout=120,
                log_environ=False,
                command=['rm', '-f', '.git/index.lock'],
            ).exit(2)
            .log('stdio', stdout='Unexpected error.'),
        )
        self.expect_outcome(result=FAILURE, state_string='Deleted .git/index.lock (failure)')
        return self.run_step()


class TestCheckOutSourceNextSteps(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        def fakeaddStepsAfterCurrentStep(self, step_factories):
            self.addedStepsAfterCurrentStep = step_factories

        FakeBuild.addedStepsAfterCurrentStep = []
        FakeBuild.addStepsAfterCurrentStep = fakeaddStepsAfterCurrentStep
        self.longMessage = True
        self.patch(git.Git, 'checkFeatureSupport', lambda *args, **kwargs: True)
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    @defer.inlineCallbacks
    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_sucess_checkout_source(self):
        self.setup_step(CheckOutSource(alwaysUseLatest=True))
        self.expectRemoteCommands(
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
                command=['git', 'clone', 'https://github.com/WebKit/WebKit.git', '.'],
            ).exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                command=['git', 'rev-parse', 'HEAD'],
            ) .log('stdio', stdout='3b84731a5f6a0a38b6f48a16ab927e5dbcb5c770\n').exit(0),
            ExpectShell(
                workdir='wkdir',
                timeout=7200,
                log_environ=False,
                command=['git', 'remote', 'set-url', '--push', 'origin', 'PUSH_DISABLED_BY_ADMIN'],
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Cleaned and updated working directory')
        rc = yield self.run_step()
        self.assertFalse(CleanUpGitIndexLock() in self.build.addedStepsAfterCurrentStep)
        defer.returnValue(rc)

    @defer.inlineCallbacks
    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_failure_checkout_source(self):
        self.setup_step(CheckOutSource(alwaysUseLatest=True))
        self.expectRemoteCommands(
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
                command=['git', 'clone', 'https://github.com/WebKit/WebKit.git', '.'],
            ).exit(1),
            Expect(
                'rmdir', dict(
                    dir='wkdir',
                    timeout=7200,
                    log_environ=False,
                ),
            )
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to update working directory')
        rc = yield self.run_step()
        self.assertTrue(CleanUpGitIndexLock() in self.build.addedStepsAfterCurrentStep)
        defer.returnValue(rc)

    @defer.inlineCallbacks
    @skipTest("This should be expectedFailure, except https://github.com/twisted/twisted/issues/10969")
    def test_failure_checkout_source_retry(self):
        self.setup_step(CheckOutSource(alwaysUseLatest=True))
        self.setProperty('cleanUpGitIndexLockAlreadyTried', True)
        self.expectRemoteCommands(
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
                command=['git', 'clone', 'https://github.com/WebKit/WebKit.git', '.'],
            ).exit(1),
            Expect(
                'rmdir', dict(
                    dir='wkdir',
                    timeout=7200,
                    log_environ=False,
                ),
            )
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to update working directory')
        rc = yield self.run_step()
        self.assertFalse(CleanUpGitIndexLock() in self.build.addedStepsAfterCurrentStep)
        defer.returnValue(rc)


class TestPrintConfiguration(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success_mac(self):
        self.setup_step(PrintConfiguration())
        self.setProperty('buildername', 'macOS-Sequoia-Release-WK2-Tests')
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
            ExpectShell(command=['xcodebuild', '-sdk', '-version'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
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
            ExpectShell(command=['system_profiler', 'SPSoftwareDataType', 'SPHardwareDataType'], workdir='wkdir', timeout=60, log_environ=False).exit(0)
            .log('stdio', stdout='Sample system information'),
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


class TestGenerateUploadBundleSteps(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        def fakeaddStepsAfterCurrentStep(self, step_factories):
            self.addedStepsAfterCurrentStep = step_factories

        FakeBuild.addedStepsAfterCurrentStep = []
        FakeBuild.addStepsAfterCurrentStep = fakeaddStepsAfterCurrentStep
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def setUpPropertiesForTest(self):
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        self.setProperty('buildername', 'GTK-Linux-64-bit-Release-Build')
        self.setProperty('archive_revision', '261281@main')

    @expectedFailure
    def test_success_generate_minibrowser_bundle(self):
        self.setup_step(GenerateMiniBrowserBundle())
        self.setUpPropertiesForTest()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['Tools/Scripts/generate-bundle', '--release', '--platform=gtk', '--bundle=MiniBrowser', '--syslibs=bundle-all', '--compression=tar.xz', '--compression-level=9', '--revision=261281@main', '--builder-name', 'GTK-Linux-64-bit-Release-Build'],
                        log_environ=True,
                        timeout=1200,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='generated minibrowser bundle')
        rc = self.run_step()
        self.assertTrue(TestMiniBrowserBundle() in self.build.addedStepsAfterCurrentStep)
        self.assertTrue(UploadMiniBrowserBundleViaSftp() not in self.build.addedStepsAfterCurrentStep)
        return rc

    def test_failure_generate_minibrowser_bundle(self):
        self.setup_step(GenerateMiniBrowserBundle())
        self.setUpPropertiesForTest()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['Tools/Scripts/generate-bundle', '--release', '--platform=gtk', '--bundle=MiniBrowser', '--syslibs=bundle-all', '--compression=tar.xz', '--compression-level=9', '--revision=261281@main', '--builder-name', 'GTK-Linux-64-bit-Release-Build'],
                        log_environ=True,
                        timeout=1200,
                        )
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='generated minibrowser bundle (failure)')
        rc = self.run_step()
        self.assertTrue(TestMiniBrowserBundle() not in self.build.addedStepsAfterCurrentStep)
        self.assertTrue(UploadMiniBrowserBundleViaSftp() not in self.build.addedStepsAfterCurrentStep)
        return rc

    @expectedFailure
    def test_success_test_minibrowser_bundle(self):
        self.setup_step(TestMiniBrowserBundle())
        self.setUpPropertiesForTest()
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'Tools/Scripts/test-bundle --platform=gtk --bundle-type=universal WebKitBuild/MiniBrowser_gtk_release.tar.xz 2>&1 | python3 Tools/Scripts/filter-test-logs minibrowser'],
                        log_environ=True,
                        timeout=10800,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='tested minibrowser bundle')
        rc = self.run_step()
        self.assertEqual([GenerateS3URL('gtk-None-release-test-minibrowser-bundle', extension='txt', content_type='text/plain', additions='13'), UploadFileToS3('logs.txt', links={'test-minibrowser-bundle': 'Full logs'}, content_type='text/plain'), UploadMiniBrowserBundleViaSftp()], next_steps)
        return rc

    @expectedFailure
    def test_failure_test_minibrowser_bundle(self):
        self.setup_step(TestMiniBrowserBundle())
        self.setUpPropertiesForTest()
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'Tools/Scripts/test-bundle --platform=gtk --bundle-type=universal WebKitBuild/MiniBrowser_gtk_release.tar.xz 2>&1 | python3 Tools/Scripts/filter-test-logs minibrowser'],
                        log_environ=True,
                        timeout=10800,
                        )
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='tested minibrowser bundle (failure)')
        rc = self.run_step()
        self.assertEqual([GenerateS3URL('gtk-None-release-test-minibrowser-bundle', extension='txt', content_type='text/plain', additions='13'), UploadFileToS3('logs.txt', links={'test-minibrowser-bundle': 'Full logs'}, content_type='text/plain')], next_steps)
        return rc

    @expectedFailure
    def test_success_generate_jsc_bundle(self):
        self.setup_step(GenerateJSCBundle())
        self.setUpPropertiesForTest()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['Tools/Scripts/generate-bundle', '--builder-name', 'GTK-Linux-64-bit-Release-Build', '--bundle=jsc', '--syslibs=bundle-all', '--platform=gtk', '--release', '--revision=261281@main'],
                        log_environ=True,
                        timeout=1200,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='generated jsc bundle')
        rc = self.run_step()
        self.assertTrue(UploadJSCBundleViaSftp() in self.build.addedStepsAfterCurrentStep)
        return rc

    def test_failure_generate_jsc_bundle(self):
        self.setup_step(GenerateJSCBundle())
        self.setUpPropertiesForTest()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['Tools/Scripts/generate-bundle', '--builder-name', 'GTK-Linux-64-bit-Release-Build', '--bundle=jsc', '--syslibs=bundle-all', '--platform=gtk', '--release', '--revision=261281@main'],
                        log_environ=True,
                        timeout=1200,
                        )
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='generated jsc bundle (failure)')
        rc = self.run_step()
        self.assertTrue(UploadJSCBundleViaSftp() not in self.build.addedStepsAfterCurrentStep)
        return rc

    def test_parameters_upload_minibrowser_bundle_sftp(self):
        self.setup_step(UploadMiniBrowserBundleViaSftp())
        self.setUpPropertiesForTest()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/Shared/transfer-archive-via-sftp', '--remote-config-file', '../../remote-minibrowser-bundle-upload-config.json', '--remote-file', 'MiniBrowser_gtk_261281@main.tar.xz', 'WebKitBuild/MiniBrowser_gtk_release.tar.xz'],
                        log_environ=True,
                        timeout=1200,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='uploaded minibrowser bundle via sftp')
        return self.run_step()

    def test_parameters_upload_jsc_bundle_sftp(self):
        self.setup_step(UploadJSCBundleViaSftp())
        self.setUpPropertiesForTest()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/CISupport/Shared/transfer-archive-via-sftp', '--remote-config-file', '../../remote-jsc-bundle-upload-config.json', '--remote-file', '261281@main.zip', 'WebKitBuild/jsc_gtk_release.zip'],
                        log_environ=True,
                        timeout=1200,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='uploaded jsc bundle via sftp')
        return self.run_step()


class TestCheckIfNeededUpdateCrossTargetImageSteps(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        def fakeaddStepsAfterCurrentStep(self, step_factories):
            self.addedStepsAfterCurrentStep = step_factories

        FakeBuild.addedStepsAfterCurrentStep = []
        FakeBuild.addStepsAfterCurrentStep = fakeaddStepsAfterCurrentStep
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def setUpPropertiesForTest(self):
        self.setProperty('fullPlatform', 'wpe')
        self.setProperty('configuration', 'release')
        self.setProperty('buildername', 'WPE-Linux-RPi4-64bits-Mesa-Release-Perf-Build')
        self.setProperty('archive_revision', '265300@main')
        self.setProperty('additionalArguments', ['--cross-target=rpi4-64bits-mesa'])

    def test_success_check_if_deployed_cross_target_image_is_updated(self):
        self.setup_step(CheckIfNeededUpdateDeployedCrossTargetImage())
        self.setUpPropertiesForTest()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/Scripts/cross-toolchain-helper', '--check-if-image-is-updated', 'deployed', '--cross-target=rpi4-64bits-mesa'],
                        log_environ=True,
                        timeout=1200,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='deployed cross target image is updated')
        rc = self.run_step()
        self.assertTrue(BuildAndDeployCrossTargetImage() not in self.build.addedStepsAfterCurrentStep)
        return rc

    @expectedFailure
    def test_failure_check_if_deployed_cross_target_image_is_updated(self):
        self.setup_step(CheckIfNeededUpdateDeployedCrossTargetImage())
        self.setUpPropertiesForTest()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/Scripts/cross-toolchain-helper', '--check-if-image-is-updated', 'deployed', '--cross-target=rpi4-64bits-mesa'],
                        log_environ=True,
                        timeout=1200,
                        )
            .exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='deployed cross target image is updated (failure)')
        self.assertTrue(BuildAndDeployCrossTargetImage() not in self.build.addedStepsAfterCurrentStep)
        rc = self.run_step()
        self.assertTrue(BuildAndDeployCrossTargetImage() in self.build.addedStepsAfterCurrentStep)
        return rc

    def test_success_check_if_running_cross_target_image_is_updated(self):
        self.setup_step(CheckIfNeededUpdateRunningCrossTargetImage())
        self.setUpPropertiesForTest()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/Scripts/cross-toolchain-helper', '--check-if-image-is-updated', 'running'],
                        log_environ=True,
                        timeout=1200,
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='running cross target image is updated')
        rc = self.run_step()
        self.assertTrue(RebootWithUpdatedCrossTargetImage() not in self.build.addedStepsAfterCurrentStep)
        return rc

    @expectedFailure
    def test_failure_check_if_running_cross_target_image_is_updated(self):
        self.setup_step(CheckIfNeededUpdateRunningCrossTargetImage())
        self.setUpPropertiesForTest()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['python3', 'Tools/Scripts/cross-toolchain-helper', '--check-if-image-is-updated', 'running'],
                        log_environ=True,
                        timeout=1200,
                        )
            .exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='running cross target image is updated (failure)')
        self.assertTrue(RebootWithUpdatedCrossTargetImage() not in self.build.addedStepsAfterCurrentStep)
        rc = self.run_step()
        self.assertTrue(RebootWithUpdatedCrossTargetImage() in self.build.addedStepsAfterCurrentStep)
        return rc


class TestRunWebDriverTests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        self.jsonFileName = 'webdriver_tests.json'
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(RunWebDriverTests())
        self.setProperty('buildername', 'GTK-Linux-64-bit-Release-WebDriver-Tests')
        self.setProperty('buildnumber', '101')
        self.setProperty('workername', 'gtk-linux-bot-14')

    def test_success(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=True,
                logfiles={'json': self.jsonFileName},
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webdriver-tests --verbose --json-output=webdriver_tests.json --release 2>&1 | python3 Tools/Scripts/filter-test-logs webdriver'],
                timeout=5400
            ).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='webdriver-tests')
        return self.run_step()

    def test_failure(self):
        self.configureStep()
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(
                workdir='wkdir',
                log_environ=True,
                logfiles={'json': self.jsonFileName},
                command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'python3 Tools/Scripts/run-webdriver-tests --verbose --json-output=webdriver_tests.json --release 2>&1 | python3 Tools/Scripts/filter-test-logs webdriver'],
                timeout=5400
            ).exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='webdriver-tests (failure)')
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

    def configureStep(self, identifier='mac-highsierra-x86_64-release', extension='zip', content_type=None, additions=None):
        self.setup_step(GenerateS3URL(identifier, extension=extension, content_type=content_type, additions=additions))
        self.setProperty('archive_revision', '1234')

    def disabled_test_success(self):
        # TODO: Figure out how to pass logs to unit-test for MasterShellCommand steps
        self.configureStep()
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['python3',
                                              '../Shared/generate-s3-url',
                                              '--revision', '1234',
                                              '--identifier', 'mac-highsierra-x86_64-release',
                                              '--extension', 'zip',
                                              ])
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Generated S3 URL')
        with current_hostname(BUILD_WEBKIT_HOSTNAMES[0]):
            return self.run_step()

    @expectedFailure
    def test_failure(self):
        self.configureStep('ios-simulator-16-x86_64-debug', additions='123')
        self.expectLocalCommands(
            ExpectMasterShellCommand(command=['python3',
                                              '../Shared/generate-s3-url',
                                              '--revision', '1234',
                                              '--identifier', 'ios-simulator-16-x86_64-debug',
                                              '--extension', 'zip',
                                              '--additions', '123'
                                              ])
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to generate S3 URL')

        try:
            with current_hostname(BUILD_WEBKIT_HOSTNAMES[0]), open(os.devnull, 'w') as null:
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
                                              '--revision', '1234',
                                              '--identifier', 'macos-arm64-release-compile-webkit',
                                              '--extension', 'txt',
                                              '--content-type', 'text/plain',
                                              ])
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to generate S3 URL')

        try:
            with current_hostname(BUILD_WEBKIT_HOSTNAMES[0]), open(os.devnull, 'w') as null:
                sys.stdout = null
                return self.run_step()
        finally:
            sys.stdout = sys.__stdout__

    def test_skipped(self):
        self.configureStep()
        self.expect_outcome(result=SKIPPED, state_string='Generated S3 URL (skipped)')
        with current_hostname('something-other-than-steps.BUILD_WEBKIT_HOSTNAMES'):
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
        with current_hostname(BUILD_WEBKIT_HOSTNAMES[0]):
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
        with current_hostname(BUILD_WEBKIT_HOSTNAMES[0]):
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
        with current_hostname(BUILD_WEBKIT_HOSTNAMES[0]):
            return self.run_step()

    def test_skipped(self):
        self.configureStep()
        self.expect_outcome(result=SKIPPED, state_string='Skipped upload to S3')
        with current_hostname('something-other-than-steps.BUILD_WEBKIT_HOSTNAMES'):
            return self.run_step()


class TestScanBuild(BuildStepMixinAdditions, unittest.TestCase):
    WORK_DIR = 'wkdir'
    EXPECTED_BUILD_COMMAND = ['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'Tools/Scripts/build-and-analyze --output-dir wkdir/build/{SCAN_BUILD_OUTPUT_DIR} --configuration release --only-smart-pointers --analyzer-path=wkdir/llvm-project/build/bin/clang --scan-build-path=../llvm-project/clang/tools/scan-build/bin/scan-build --sdkroot=macosx --preprocessor-additions=CLANG_WEBKIT_BRANCH=1 2>&1 | python3 Tools/Scripts/filter-test-logs scan-build --output build-log.txt']

    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(ScanBuild())

    def test_failure(self):
        self.configureStep()
        self.setProperty('builddir', self.WORK_DIR)
        self.setProperty('configuration', 'release')

        self.expectRemoteCommands(
            ExpectShell(workdir=self.WORK_DIR,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'/bin/rm -rf wkdir/build/{SCAN_BUILD_OUTPUT_DIR}'],
                        timeout=2 * 60 * 60).exit(0),
            ExpectShell(workdir=self.WORK_DIR,
                        command=self.EXPECTED_BUILD_COMMAND,
                        timeout=2 * 60 * 60)
            .log('stdio', stdout='scan-build-static-analyzer: No bugs found.\nTotal issue count: 123\n')
            .exit(0)
        )
        self.expect_outcome(result=FAILURE, state_string='ANALYZE FAILED: scan-build found 123 issues (failure)')
        return self.run_step()

    @expectedFailure
    def test_success(self):
        self.configureStep()
        self.setProperty('builddir', self.WORK_DIR)
        self.setProperty('configuration', 'release')
        self.setProperty('fullPlatform', 'mac-sonoma')
        self.setProperty('architecture', 'arm64')
        next_steps = []
        self.patch(self.build, 'addStepsAfterCurrentStep', lambda s: next_steps.extend(s))

        self.expectRemoteCommands(
            ExpectShell(workdir=self.WORK_DIR,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'/bin/rm -rf wkdir/build/{SCAN_BUILD_OUTPUT_DIR}'],
                        timeout=2 * 60 * 60)
            .exit(0),
            ExpectShell(workdir=self.WORK_DIR,
                        command=self.EXPECTED_BUILD_COMMAND,
                        timeout=2 * 60 * 60)
            .log('stdio', stdout='ANALYZE SUCCEEDED No issues found.\n')
            .exit(0)
        )
        self.expect_outcome(result=SUCCESS, state_string='scan-build found 0 issues')
        rc = self.run_step()
        self.assertEqual(
            [
                GenerateS3URL('mac-sonoma-arm64-release-scan-build', extension='txt', content_type='text/plain', additions='13'),
                UploadFileToS3('build-log.txt', links={'scan-build': 'Full build log'}, content_type='text/plain'),
                ParseStaticAnalyzerResults(),
                FindUnexpectedStaticAnalyzerResults(),
                ArchiveStaticAnalyzerResults(),
                UploadStaticAnalyzerResults(),
                ExtractStaticAnalyzerTestResults(),
                DisplaySaferCPPResults(),
                CleanSaferCPPArchive(),
                SetBuildSummary()
            ], next_steps)
        return rc

    def test_success_with_issues(self):
        self.configureStep()
        self.setProperty('builddir', self.WORK_DIR)
        self.setProperty('configuration', 'release')

        self.expectRemoteCommands(
            ExpectShell(workdir=self.WORK_DIR,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', f'/bin/rm -rf wkdir/build/{SCAN_BUILD_OUTPUT_DIR}'],
                        timeout=2 * 60 * 60)
            .exit(0),
            ExpectShell(workdir=self.WORK_DIR,
                        command=self.EXPECTED_BUILD_COMMAND,
                        timeout=2 * 60 * 60)
            .log('stdio', stdout='ANALYZE SUCCEEDED\n Total issue count: 300\n')
            .exit(0)
        )
        self.expect_outcome(result=SUCCESS, state_string='scan-build found 300 issues')
        return self.run_step()


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

        command = ['python3', 'Tools/Scripts/generate-dirty-files', f'wkdir/build/{SCAN_BUILD_OUTPUT_DIR}', '--output-dir', 'wkdir/smart-pointer-result-archive/1234', '--build-dir', 'wkdir/build']

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=command)
            .log('stdio', stdout='Total (24247) WebKit (327) WebCore (23920)\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string=' Issues by project: Total (24247) WebKit (327) WebCore (23920)\n')
        return self.run_step()


class TestFindUnexpectedStaticAnalyzerResults(BuildStepMixinAdditions, unittest.TestCase):
    command = ['python3', 'Tools/Scripts/compare-static-analysis-results', 'wkdir/smart-pointer-result-archive/1234', '--scan-build-path', '../llvm-project/clang/tools/scan-build/bin/scan-build', '--build-output', SCAN_BUILD_OUTPUT_DIR, '--check-expectations']
    upload_options = ['--builder-name', 'Safer-CPP-Checks', '--build-number', 1234, '--buildbot-worker', 'bot600', '--buildbot-master', CURRENT_HOSTNAME, '--report', 'https://results.webkit.org']
    configuration = ['--architecture', 'arm64', '--platform', 'mac', '--version', '14.6.1', '--version-name', 'Sonoma', '--style', 'release', '--sdk', '23G93']

    def setUp(self):
        os.environ['RESULTS_SERVER_API_KEY'] = 'test-api-key'
        return self.setup_test_build_step()

    def tearDown(self):
        del os.environ['RESULTS_SERVER_API_KEY']
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(FindUnexpectedStaticAnalyzerResults())
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
        self.setProperty('workername', 'bot600')

    def test_success_no_issues(self):
        self.configureStep()

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=self.command + self.upload_options + self.configuration,
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'})
            .log('stdio', stdout='').exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Found no unexpected results')
        return self.run_step()

    def test_new_issues(self):
        self.configureStep()

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=self.command + self.upload_options + self.configuration,
                        env={'RESULTS_SERVER_API_KEY': 'test-api-key'})
            .log('stdio', stdout='Total unexpected failing files: 123\nTotal unexpected passing files: 456\nTotal unexpected issues: 789\n').exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Unexpected failing files: 123 Unexpected passing files: 456 Unexpected issues: 789')
        return self.run_step()


class TestUpdateSaferCPPBaseline(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(UpdateSaferCPPBaseline())

    def test_success(self):
        self.configureStep()
        self.setProperty('builddir', 'wkdir')
        self.setProperty('buildnumber', 2)

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'rm -r wkdir/smart-pointer-result-archive/baseline'],)
            .log('stdio', stdout='')
            .exit(0),
            ExpectShell(workdir='wkdir',
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'cp -r wkdir/smart-pointer-result-archive/2 wkdir/smart-pointer-result-archive/baseline'],)
            .log('stdio', stdout='')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS)
        return self.run_step()


class TestCleanSaferCPPArchive(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(CleanSaferCPPArchive())

    def test_success(self):
        self.configureStep()
        self.setProperty('builddir', 'wkdir')
        self.setProperty('buildnumber', 2)

        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        command=['rm', '-rf', 'wkdir/smart-pointer-result-archive/2'],
                        log_environ=False,
                        timeout=1200,
                        env={})
            .log('stdio', stdout='')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='cleaned safer cpp archive')
        return self.run_step()


class TestDisplaySaferCPPResults(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(DisplaySaferCPPResults())
        self.setProperty('buildnumber', '123')

        def loadResultsData(self, path):
            return {
                "passes": {
                    "WebCore": {
                        "NoUncountedMemberChecker": ['File17.cpp'],
                        "RefCntblBaseVirtualDtor": ['File17.cpp'],
                        "UncountedCallArgsChecker": [],
                        "UncountedLocalVarsChecker": []
                    },
                    "WebKit": {
                        "NoUncountedMemberChecker": [],
                        "RefCntblBaseVirtualDtor": [],
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

    def test_success(self):
        self.configureStep()

        self.expect_outcome(result=SUCCESS, state_string='No unexpected results')
        return self.run_step()

    def test_warning(self):
        self.configureStep()
        self.setProperty('unexpected_passing_files', 1)

        self.expect_outcome(result=WARNINGS, state_string='Unexpected passing files: 1')
        return self.run_step()

    def test_failure(self):
        self.configureStep()
        self.setProperty('unexpected_new_issues', 10)
        self.setProperty('unexpected_passing_files', 1)
        self.setProperty('unexpected_failing_files', 1)

        self.expect_outcome(result=FAILURE, state_string='Unexpected failing files: 1 Unexpected passing files: 1')
        return self.run_step()


class TestRunTest262Tests(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_success(self):
        self.setup_step(RunTest262Tests())
        self.setProperty('fullPlatform', 'mac-sonoma')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=7200,
                        log_environ=True,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'perl Tools/Scripts/test262-runner --verbose --release 2>&1 | python3 Tools/Scripts/filter-test-logs test262'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='test262-test')
        return self.run_step()

    def test_failure(self):
        self.setup_step(RunTest262Tests())
        self.setProperty('fullPlatform', 'mac-sonoma')
        self.setProperty('configuration', 'debug')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=7200,
                        log_environ=True,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'perl Tools/Scripts/test262-runner --verbose --debug 2>&1 | python3 Tools/Scripts/filter-test-logs test262'],
                        )
            .log('stdio', stdout='''! NEW FAIL: test/built-ins/Array/prototype/at/index-non-numeric.js
! NEW FAIL: test/built-ins/Array/prototype/at/index-out-of-range.js
! NEW FAIL: test/built-ins/Array/prototype/at/index-string.js''')
            .exit(2),
        )
        self.expect_outcome(result=FAILURE, state_string='3 Test262 tests failed')
        return self.run_step()

    def test_success_platform_flag_gtk(self):
        self.setup_step(RunTest262Tests())
        self.setProperty('platform', 'gtk')
        self.setProperty('fullPlatform', 'gtk')
        self.setProperty('configuration', 'release')
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        timeout=7200,
                        log_environ=True,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'perl Tools/Scripts/test262-runner --verbose --release --gtk 2>&1 | python3 Tools/Scripts/filter-test-logs test262'],
                        )
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='test262-test')
        return self.run_step()
