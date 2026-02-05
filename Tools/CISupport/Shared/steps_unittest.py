# Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

from buildbot.process.results import SUCCESS, FAILURE, WARNINGS, SKIPPED
from buildbot.test.fake.fakebuild import FakeBuild
from buildbot.test.reactor import TestReactorMixin
from buildbot.test.steps import ExpectShell
from buildbot.test.steps import TestBuildStepMixin as BuildStepMixin
from twisted.internet import error, reactor
from twisted.python import failure, log
from twisted.trial import unittest

from .steps import *

CURRENT_HOSTNAME = socket.gethostname().strip()
LLVM_DIR = 'llvm-project'
SWIFT_DIR = 'swift-project/swift'

# Workaround for https://github.com/buildbot/buildbot/issues/4669
FakeBuild.addStepsAfterCurrentStep = lambda FakeBuild, step_factories: None
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


class TestPrintClangVersion(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(PrintClangVersion())

    def test_success(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir=LLVM_DIR,
                        log_environ=False,
                        timeout=60,
                        command=['./build/bin/clang', '--version'])
            .log('stdio', stdout='clang version 17.0.6 (https://github.com/rniwa/llvm-project.git 34715c1b2049d8aa738ade79f003ed4b82259a89) Target: arm64-apple-darwin23.5.0\nThread model: posix\nInstalledDir: /Volumes/Data/worker/macOS-Sonoma-Safer-CPP-Checks-EWS/llvm-project/./build/bin')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='clang version 17.0.6 (https://github.com/rniwa/llvm-project.git 34715c1b2049d8aa738ade79f003ed4b82259a89)')
        rc = self.run_step()
        self.expect_property('current_llvm_revision', '34715c1b2049d8aa738ade79f003ed4b82259a89')
        return rc

    @expectedFailure
    def test_failure(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir=LLVM_DIR,
                        log_environ=False,
                        timeout=60,
                        command=['./build/bin/clang', '--version'])
            .log('stdio', stdout='No such file or directory\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Clang executable does not exist')
        rc = self.run_step()
        self.expect_property('current_llvm_revision', None)
        return rc


class TestGetLLVMVersion(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(GetLLVMVersion())

    def test_success(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'cat Tools/CISupport/safer-cpp-llvm-version'])
            .log('stdio', stdout='34715c1b2049d8aa738ade79f003ed4b82259a89\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Canonical LLVM version: 34715c1b2049d8aa738ade79f003ed4b82259a89')
        rc = self.run_step()
        self.expect_property('canonical_llvm_revision', '34715c1b2049d8aa738ade79f003ed4b82259a89')
        return rc


class TestCheckoutLLVMProject(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(CheckOutLLVMProject())
        self.setProperty('canonical_llvm_revision', '123456')

    def test_skipped(self):
        self.configureStep()
        self.setProperty('current_llvm_revision', '123456')
        self.expect_outcome(result=SKIPPED, state_string='llvm-project is already up to date')
        return self.run_step()


class TestUpdateClang(BuildStepMixinAdditions, unittest.TestCase):
    ENV = {'PATH': '/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/Applications/CMake.app/Contents/bin/:BuildDir'}

    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(UpdateClang())
        self.setProperty('builddir', 'BuildDir')
        self.setProperty('canonical_llvm_revision', '123456')

    def test_success(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir=LLVM_DIR,
                        log_environ=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'rm -r build-new; mkdir build-new']).exit(0),
            ExpectShell(workdir=LLVM_DIR,
                        log_environ=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'cd build-new; xcrun cmake -DLLVM_ENABLE_PROJECTS=clang -DCMAKE_BUILD_TYPE=Release -G Ninja ../llvm -DCMAKE_MAKE_PROGRAM=$(xcrun --sdk macosx --find ninja)']).exit(0),
            ExpectShell(workdir=LLVM_DIR,
                        log_environ=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'cd build-new; ninja clang']).exit(0),
            ExpectShell(workdir=LLVM_DIR,
                        log_environ=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['rm', '-r', '../build/WebKitBuild']).exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Successfully updated clang')
        return self.run_step()

    def test_skipped(self):
        self.configureStep()
        self.setProperty('current_llvm_revision', '123456')
        self.expect_outcome(result=SKIPPED, state_string='Clang is already up to date')
        self.run_step()

    def test_use_previous_build(self):
        self.configureStep()
        self.setProperty('canonical_llvm_revision', '')
        self.setProperty('current_llvm_revision', '123456')
        self.expect_outcome(result=WARNINGS, state_string='Could not find canonical revision, using previous build')
        self.run_step()


class TestInstallCMake(BuildStepMixinAdditions, unittest.TestCase):
    ENV = {'PATH': '/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/Applications/CMake.app/Contents/bin/'}

    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(InstallCMake())

    def test_success_update(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['python3', 'Tools/CISupport/Shared/download-and-install-build-tools', 'cmake'])
            .log('stdio', stdout='cmake version 3.30.4\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Successfully installed CMake')
        return self.run_step()

    def test_success_update_skipped(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['python3', 'Tools/CISupport/Shared/download-and-install-build-tools', 'cmake'])
            .log('stdio', stdout='cmake is already up to date... skipping download and installation.\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='CMake is already installed')
        return self.run_step()

    def test_failure(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['python3', 'Tools/CISupport/Shared/download-and-install-build-tools', 'cmake'])
            .log('stdio', stdout='zsh: command not found: cmake\n')
            .exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to install CMake')
        return self.run_step()


class TestInstallNinja(BuildStepMixinAdditions, unittest.TestCase):
    ENV = {'PATH': "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:BuildDir"}

    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(InstallNinja())
        self.setProperty('builddir', 'BuildDir')

    def test_success_update(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'cd ../; python3 build/Tools/CISupport/Shared/download-and-install-build-tools ninja'])
            .log('stdio', stdout='1.12.1\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Successfully installed Ninja')
        return self.run_step()

    def test_success_update_skipped(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'cd ../; python3 build/Tools/CISupport/Shared/download-and-install-build-tools ninja'])
            .log('stdio', stdout='ninja is already up to date... skipping download and installation.\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Ninja is already installed')
        return self.run_step()

    def test_failure(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=True,
                        timeout=1200,
                        env=self.ENV,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'cd ../; python3 build/Tools/CISupport/Shared/download-and-install-build-tools ninja'])
            .log('stdio', stdout='zsh: command not found: ninja')
            .exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to install Ninja')
        return self.run_step()


class TestPrintSwiftVersion(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(PrintSwiftVersion())

    def test_success_with_tag_and_executable(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir=SWIFT_DIR,
                        log_environ=False,
                        timeout=1200,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git describe --tags'])
            .log('stdio', stdout='swift-6.3-DEVELOPMENT-SNAPSHOT\n')
            .exit(0),
            ExpectShell(workdir=SWIFT_DIR,
                        log_environ=False,
                        timeout=1200,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', '../build/Ninja-ReleaseAssert/swift-macosx-arm64/bin/swift --version'])
            .log('stdio', stdout='Swift version 6.0.3 (swift-6.0.3-RELEASE)\nTarget: arm64-apple-macosx26.0\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Current Swift tag: swift-6.3-DEVELOPMENT-SNAPSHOT')
        rc = self.run_step()
        self.expect_property('current_swift_tag', 'swift-6.3-DEVELOPMENT-SNAPSHOT')
        self.expect_property('has_swift_executable', True)
        return rc

    def test_no_git_repository(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir=SWIFT_DIR,
                        log_environ=False,
                        timeout=1200,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git describe --tags'])
            .log('stdio', stdout='fatal: not a git repository\n')
            .exit(1),
        )
        self.expect_outcome(result=SUCCESS, state_string='Swift repository does not exist')
        rc = self.run_step()
        return rc

    def test_no_swift_executable(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir=SWIFT_DIR,
                        log_environ=False,
                        timeout=1200,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'git describe --tags'])
            .log('stdio', stdout='swift-6.3-DEVELOPMENT-SNAPSHOT\n')
            .exit(0),
            ExpectShell(workdir=SWIFT_DIR,
                        log_environ=False,
                        timeout=1200,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', '../build/Ninja-ReleaseAssert/swift-macosx-arm64/bin/swift --version'])
            .log('stdio', stdout='No such file or directory\n')
            .exit(1),
        )
        self.expect_outcome(result=SUCCESS, state_string='Swift executable does not exist')
        rc = self.run_step()
        self.expect_property('current_swift_tag', 'swift-6.3-DEVELOPMENT-SNAPSHOT')
        return rc


class TestGetSwiftTagName(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(GetSwiftTagName())

    def test_success(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'cat Tools/CISupport/safer-cpp-swift-version'])
            .log('stdio', stdout='swift-6.3-DEVELOPMENT-SNAPSHOT\n')
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Canonical Swift tag name: swift-6.3-DEVELOPMENT-SNAPSHOT')
        rc = self.run_step()
        self.expect_property('canonical_swift_tag', 'swift-6.3-DEVELOPMENT-SNAPSHOT')
        return rc

    def test_failure_empty_file(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir='wkdir',
                        log_environ=False,
                        timeout=60,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'cat Tools/CISupport/safer-cpp-swift-version'])
            .log('stdio', stdout='')
            .exit(0),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to find canonical Swift tag')
        return self.run_step()


class TestCheckOutSwiftProject(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(CheckOutSwiftProject())
        self.setProperty('canonical_swift_tag', 'swift-6.3-DEVELOPMENT-SNAPSHOT')

    def test_skipped_already_up_to_date(self):
        self.configureStep()
        self.setProperty('current_swift_tag', 'swift-6.3-DEVELOPMENT-SNAPSHOT')
        self.expect_outcome(result=SKIPPED, state_string='swift-project is already up to date')
        return self.run_step()


class TestUpdateSwiftCheckouts(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def configureStep(self):
        self.setup_step(UpdateSwiftCheckouts())
        self.setProperty('canonical_swift_tag', 'swift-6.3-DEVELOPMENT-SNAPSHOT')

    def test_success(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir=SWIFT_DIR,
                        log_environ=False,
                        timeout=1200,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'utils/update-checkout --clone'])
            .exit(0),
            ExpectShell(workdir=SWIFT_DIR,
                        log_environ=False,
                        timeout=1200,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'utils/update-checkout --tag swift-6.3-DEVELOPMENT-SNAPSHOT'])
            .exit(0),
        )
        self.expect_outcome(result=SUCCESS, state_string='Successfully updated swift checkout')
        return self.run_step()

    def test_skipped_already_up_to_date(self):
        self.configureStep()
        self.setProperty('current_swift_tag', 'swift-6.3-DEVELOPMENT-SNAPSHOT')
        self.expect_outcome(result=SKIPPED, state_string='Swift checkout is already up to date')
        return self.run_step()

    def test_failure_with_previous_checkout(self):
        self.configureStep()
        self.setProperty('current_swift_tag', 'swift-6.4-DEVELOPMENT-SNAPSHOT')
        self.expectRemoteCommands(
            ExpectShell(workdir=SWIFT_DIR,
                        log_environ=False,
                        timeout=1200,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'utils/update-checkout --clone'])
            .exit(1),
        )
        self.expect_outcome(result=WARNINGS, state_string='Failed to update swift, using previous checkout')
        return self.run_step()

    def test_failure_without_previous_checkout(self):
        self.configureStep()
        self.expectRemoteCommands(
            ExpectShell(workdir=SWIFT_DIR,
                        log_environ=False,
                        timeout=1200,
                        command=['/bin/bash', '--posix', '-o', 'pipefail', '-c', 'utils/update-checkout --clone'])
            .exit(1),
        )
        self.expect_outcome(result=FAILURE, state_string='Failed to update swift checkout')
        return self.run_step()


class TestWaitForDuration(BuildStepMixinAdditions, unittest.TestCase):
    def setUp(self):
        self.longMessage = True
        return self.setup_test_build_step()

    def tearDown(self):
        return self.tear_down_test_build_step()

    def test_default_duration(self):
        step = WaitForDuration()
        self.assertEqual(step.duration, WaitForDuration.DEFAULT_WAIT_DURATION)

    def test_custom_duration(self):
        step = WaitForDuration(duration=300)
        self.assertEqual(step.duration, 300)

    def test_success(self):
        self.setup_step(WaitForDuration(duration=1))
        self.expect_outcome(result=SUCCESS, state_string='Waited for 1s')
        return self.run_step()


if __name__ == '__main__':
    unittest.main()
