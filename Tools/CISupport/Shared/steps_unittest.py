# Copyright (C) 2024 Apple Inc. All rights reserved.
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
from buildbot.test.fake.fakebuild import FakeBuild
from buildbot.test.fake.remotecommand import Expect, ExpectRemoteRef, ExpectShell
from buildbot.test.util.misc import TestReactorMixin
from buildbot.test.util.steps import BuildStepMixin
from buildbot.util import identifiers as buildbot_identifiers
from mock import call
from twisted.internet import defer, error, reactor
from twisted.python import failure, log
from twisted.trial import unittest

from .steps import *

CURRENT_HOSTNAME = socket.gethostname().strip()
LLVM_DIR = 'llvm-project'

# Workaround for https://github.com/buildbot/buildbot/issues/4669
FakeBuild.addStepsAfterLastStep = lambda FakeBuild, step_factories: None
FakeBuild._builderid = 1


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


class TestPrintClangVersion(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(PrintClangVersion())

    def test_success(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir=LLVM_DIR,
                        logEnviron=False,
                        timeout=60,
                        command=['./build/bin/clang', '--version'])
            + ExpectShell.log('stdio', stdout='clang version 17.0.6 (https://github.com/rniwa/llvm-project.git 34715c1b2049d8aa738ade79f003ed4b82259a89) Target: arm64-apple-darwin23.5.0\nThread model: posix\nInstalledDir: /Volumes/Data/worker/macOS-Sonoma-Safer-CPP-Checks-EWS/llvm-project/./build/bin')
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='clang version 17.0.6 (https://github.com/rniwa/llvm-project.git 34715c1b2049d8aa738ade79f003ed4b82259a89)')
        rc = self.runStep()
        self.assertEqual(self.getProperty('llvm_revision'), '34715c1b2049d8aa738ade79f003ed4b82259a89')
        return rc

    def test_failure(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir=LLVM_DIR,
                        logEnviron=False,
                        timeout=60,
                        command=['./build/bin/clang', '--version'])
            + ExpectShell.log('stdio', stdout='No such file or directory\n')
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Clang executable does not exist')
        rc = self.runStep()
        self.assertEqual(self.getProperty('llvm_revision'), None)
        return rc


class TestCheckoutLLVMProject(BuildStepMixinAdditions, unittest.TestCase):
    LLVM_REVISION = '123456'

    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(CheckOutLLVMProject())

        def doStepIf(self, step):
            return self.build.getProperty('llvm_revision', '') != TestCheckoutLLVMProject.LLVM_REVISION
        CheckOutLLVMProject.doStepIf = doStepIf

    def test_skipped(self):
        self.configureStep()
        self.setProperty('llvm_revision', '123456')
        self.expectOutcome(result=SKIPPED, state_string='llvm-project is already up to date')
        return self.runStep()


class TestUpdateClang(BuildStepMixinAdditions, unittest.TestCase):
    ENV = {'PATH': '/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/Applications/CMake.app/Contents/bin/:BuildDir'}
    LLVM_REVISION = '123456'

    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(UpdateClang())
        self.setProperty('builddir', 'BuildDir')

        def doStepIf(self, step):
            return self.build.getProperty('llvm_revision', '') != TestUpdateClang.LLVM_REVISION
        UpdateClang.doStepIf = doStepIf

    def test_success(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir=LLVM_DIR,
                        logEnviron=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['/bin/sh', '-c', 'rm -r build-new; mkdir build-new']) + 0,
            ExpectShell(workdir=LLVM_DIR,
                        logEnviron=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['/bin/sh', '-c', 'cd build-new; xcrun cmake -DLLVM_ENABLE_PROJECTS=clang -DCMAKE_BUILD_TYPE=Release -G Ninja ../llvm -DCMAKE_MAKE_PROGRAM=$(xcrun --sdk macosx --find ninja)']) + 0,
            ExpectShell(workdir=LLVM_DIR,
                        logEnviron=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['/bin/sh', '-c', 'cd build-new; ninja clang']) + 0,
            ExpectShell(workdir=LLVM_DIR,
                        logEnviron=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['rm', '-r', '../build/WebKitBuild']) + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Successfully updated clang')
        return self.runStep()

    def test_skipped(self):
        self.configureStep()
        self.setProperty('llvm_revision', '123456')
        self.expectOutcome(result=SKIPPED, state_string='Clang is already up to date')
        self.runStep()


class TestInstallCMake(BuildStepMixinAdditions, unittest.TestCase):
    ENV = {'PATH': '/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/Applications/CMake.app/Contents/bin/'}

    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(InstallCMake())

    def test_success_update(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['python3', 'Tools/CISupport/Shared/download-and-install-build-tools', 'cmake'])
            + ExpectShell.log('stdio', stdout='cmake version 3.30.4\n')
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Successfully installed CMake')
        return self.runStep()

    def test_success_update_skipped(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['python3', 'Tools/CISupport/Shared/download-and-install-build-tools', 'cmake'])
            + ExpectShell.log('stdio', stdout='cmake is already up to date... skipping download and installation.\n')
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='CMake is already installed')
        return self.runStep()

    def test_failure(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['python3', 'Tools/CISupport/Shared/download-and-install-build-tools', 'cmake'])
            + ExpectShell.log('stdio', stdout='zsh: command not found: cmake\n')
            + 1,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to install CMake')
        return self.runStep()


class TestInstallNinja(BuildStepMixinAdditions, unittest.TestCase):
    ENV = {'PATH': "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:BuildDir"}

    def setUp(self):
        self.longMessage = True
        return self.setUpBuildStep()

    def tearDown(self):
        return self.tearDownBuildStep()

    def configureStep(self):
        self.setupStep(InstallNinja())
        self.setProperty('builddir', 'BuildDir')

    def test_success_update(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['/bin/sh', '-c', 'cd ../; python3 build/Tools/CISupport/Shared/download-and-install-build-tools ninja'])
            + ExpectShell.log('stdio', stdout='1.12.1\n')
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Successfully installed Ninja')
        return self.runStep()

    def test_success_update_skipped(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['/bin/sh', '-c', 'cd ../; python3 build/Tools/CISupport/Shared/download-and-install-build-tools ninja'])
            + ExpectShell.log('stdio', stdout='ninja is already up to date... skipping download and installation.\n')
            + 0,
        )
        self.expectOutcome(result=SUCCESS, state_string='Ninja is already installed')
        return self.runStep()

    def test_failure(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        logEnviron=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['/bin/sh', '-c', 'cd ../; python3 build/Tools/CISupport/Shared/download-and-install-build-tools ninja'])
            + ExpectShell.log('stdio', stdout='zsh: command not found: ninja')
            + 1,
        )
        self.expectOutcome(result=FAILURE, state_string='Failed to install Ninja')
        return self.runStep()


if __name__ == '__main__':
    unittest.main()
