# Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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

from buildbot.plugins import steps, util
from buildbot.process import buildstep, logobserver, properties
from buildbot.process.results import Results, SUCCESS, FAILURE, CANCELLED, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.steps import master, shell, transfer, trigger
from buildbot.steps.source import git
from twisted.internet import defer, reactor
from shlex import quote

import json
import os
import re
import socket
import sys

if sys.version_info < (3, 9):  # noqa: UP036
    print('ERROR: Minimum supported Python version for this code is Python 3.9')
    sys.exit(1)

CURRENT_HOSTNAME = socket.gethostname().strip()

GITHUB_URL = 'https://github.com/'
SCAN_BUILD_OUTPUT_DIR = 'scan-build-output'
LLVM_DIR = 'llvm-project'
SWIFT_DIR = 'swift-project/swift'


class ShellMixin(object):
    WINDOWS_SHELL_PLATFORMS = ['win', 'playstation']

    def has_windows_shell(self):
        return self.getProperty('platform', '*') in self.WINDOWS_SHELL_PLATFORMS

    def shell_command(self, command, pipefail=True):
        if pipefail:
            # -o pipefail is new in POSIX 2024, and on systems using `dash` to provide
            # `sh` (e.g., Debian and Ubuntu) this is unsupported, as it is currently
            # only supported in pre-release versions of `dash`. For now, we use `bash`
            # in its POSIX mode (which is also its default when it is invoked as `sh`)
            # to try and reduce the risk of bashisms slipping in.
            if self.has_windows_shell():
                shell = 'bash'
            else:
                shell = '/bin/bash'
            return [shell, '--posix', '-o', 'pipefail', '-c', command]
        else:
            if self.has_windows_shell():
                shell = 'sh'
            else:
                shell = '/bin/sh'
            return [shell, '-c', command]

    def shell_exit_0(self):
        if self.has_windows_shell():
            return 'exit 0'
        return 'true'


class AddToLogMixin(object):
    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)


class SetBuildSummary(buildstep.BuildStep, AddToLogMixin):
    name = 'set-build-summary'
    descriptionDone = ['Set build summary']
    alwaysRun = True
    haltOnFailure = False
    flunkOnFailure = False
    FAILURE_MSG_IN_STRESS_MODE = 'Found test failures in stress mode'

    def doStepIf(self, step):
        return self.getProperty('build_summary', False)

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)

    @defer.inlineCallbacks
    def run(self):
        build_summary = self.getProperty('build_summary', 'build successful')
        yield self._addToLog('stdio', f'Setting build summary as: {build_summary}')
        previous_build_summary = self.getProperty('build_summary', '')
        if self.FAILURE_MSG_IN_STRESS_MODE in previous_build_summary:
            self.build.results = FAILURE
        elif any(s in previous_build_summary for s in ('Committed ', '@', 'Passed', 'Ignored pre-existing failure')):
            self.build.results = SUCCESS
        self.build.buildFinished([build_summary], self.build.results)
        return defer.returnValue(SUCCESS)


class InstallCMake(shell.ShellCommand):
    name = 'install-cmake'
    haltOnFailure = True
    summary = 'Successfully installed CMake'

    def __init__(self, **kwargs):
        super().__init__(logEnviron=True, **kwargs)

    @defer.inlineCallbacks
    def run(self):
        self.env['PATH'] = f'/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/Applications/CMake.app/Contents/bin/'
        self.command = ['python3', 'Tools/CISupport/Shared/download-and-install-build-tools', 'cmake']

        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('stdio', self.log_observer)

        rc = yield super().run()
        if rc != SUCCESS:
            return defer.returnValue(rc)

        log_text = self.log_observer.getStdout()
        index_skipped = log_text.rfind('skipping download and installation')
        if index_skipped != -1:
            self.summary = 'CMake is already installed'
        return defer.returnValue(rc)

    def evaluateCommand(self, cmd):
        if cmd.rc != 0:
            self.commandFailed = True
            return FAILURE
        return SUCCESS

    def getResultSummary(self):
        if self.results != SUCCESS:
            self.summary = f'Failed to install CMake'
        return {u'step': self.summary}


class InstallNinja(shell.ShellCommand, ShellMixin):
    name = 'install-ninja'
    haltOnFailure = True
    summary = 'Successfully installed Ninja'

    def __init__(self, **kwargs):
        super().__init__(logEnviron=True, **kwargs)

    @defer.inlineCallbacks
    def run(self):
        self.env['PATH'] = f"/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:{self.getProperty('builddir')}"
        self.command = self.shell_command('cd ../; python3 build/Tools/CISupport/Shared/download-and-install-build-tools ninja')

        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('stdio', self.log_observer)

        rc = yield super().run()
        if rc != SUCCESS:
            return defer.returnValue(rc)

        log_text = self.log_observer.getStdout()
        index_skipped = log_text.rfind('skipping download and installation')
        if index_skipped != -1:
            self.summary = 'Ninja is already installed'
        return defer.returnValue(rc)

    def evaluateCommand(self, cmd):
        if cmd.rc != 0:
            self.commandFailed = True
            return FAILURE
        return SUCCESS

    def getResultSummary(self):
        if self.results != SUCCESS:
            self.summary = f'Failed to install Ninja'
        return {u'step': self.summary}


class GetLLVMVersion(shell.ShellCommand, ShellMixin):
    name = 'get-llvm-version'
    summary = 'Found LLVM version'

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, timeout=60, **kwargs)

    @defer.inlineCallbacks
    def run(self):
        self.command = self.shell_command('cat Tools/CISupport/safer-cpp-llvm-version')

        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('stdio', self.log_observer)

        rc = yield super().run()
        if rc != SUCCESS:
            return defer.returnValue(rc)

        log_text = self.log_observer.getStdout().strip()
        if log_text:
            self.setProperty('canonical_llvm_revision', log_text)
            self.summary = f"Canonical LLVM version: {self.getProperty('canonical_llvm_revision')}"
            return defer.returnValue(SUCCESS)

        return defer.returnValue(FAILURE)

    def getResultSummary(self):
        if self.results != SUCCESS:
            self.summary = f'Failed to find canonical LLVM version'
        return {u'step': self.summary}


class PrintSwiftVersion(steps.ShellSequence, ShellMixin):
    name = 'print-swift-version'
    haltOnFailure = False
    flunkOnFailure = False
    warnOnFailure = False
    summary = ''

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, workdir=SWIFT_DIR, **kwargs)
        self.commands = []
        self.summary = ''

    @defer.inlineCallbacks
    def run(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

        self.commands = [
            util.ShellArg(command=self.shell_command('git describe --tags'), logname='stdio', haltOnFailure=True),
            util.ShellArg(command=self.shell_command(f'../build/Ninja-ReleaseAssert/swift-macosx-arm64/bin/swift --version'), logname='stdio')
        ]

        rc = yield super().runShellSequence(self.commands)

        # First, check for the swift checkout
        log_text = self.log_observer.getStdout() + self.log_observer.getStderr()
        if 'not a git repository' in log_text:
            self.summary = 'Swift repository does not exist'
            rc = SUCCESS
        else:
            git_tag = log_text.split()[0]
            self.setProperty('current_swift_tag', git_tag)
            self.summary = f'Current Swift tag: {git_tag}'

        # Then, check if the executable exists - if not, we want to build
        if 'No such file or directory' in log_text:
            self.summary = 'Swift executable does not exist'
            rc = SUCCESS
        else:
            self.setProperty('has_swift_executable', True)
        return defer.returnValue(rc)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to print Swift version'}
        return {'step': self.summary}


class GetSwiftTagName(shell.ShellCommand, ShellMixin):
    name = 'get-swift-tag-name'
    summary = 'Found Swift tag'

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, timeout=60, **kwargs)

    @defer.inlineCallbacks
    def run(self):
        self.command = self.shell_command('cat Tools/CISupport/safer-cpp-swift-version')

        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('stdio', self.log_observer)

        rc = yield super().run()
        if rc != SUCCESS:
            return defer.returnValue(rc)

        log_text = self.log_observer.getStdout().strip()
        if log_text:
            self.setProperty('canonical_swift_tag', log_text)
            self.summary = f"Canonical Swift tag name: {self.getProperty('canonical_swift_tag')}"
            return defer.returnValue(SUCCESS)
        return defer.returnValue(FAILURE)

    def getResultSummary(self):
        if self.results != SUCCESS:
            self.summary = f'Failed to find canonical Swift tag'
        return {u'step': self.summary}


class CheckOutSwiftProject(git.Git, AddToLogMixin):
    name = 'checkout-swift-project'
    directory = 'swift-project/swift'
    branch = 'main'
    CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR = (0, 2)
    GIT_HASH_LENGTH = 40
    haltOnFailure = False

    def __init__(self, **kwargs):
        repourl = f'{GITHUB_URL}swiftlang/swift.git'
        super().__init__(
            repourl=repourl,
            workdir=self.directory,
            retry=self.CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR,
            timeout=5 * 60,
            branch=self.branch,
            alwaysUseLatest=True,
            logEnviron=False,
            progress=True,
            **kwargs
        )

    @defer.inlineCallbacks
    def parseGotRevision(self, _=None):
        stdout = yield self._dovccmd(['rev-parse', 'HEAD'], collectStdout=True)
        revision = stdout.strip()
        if len(revision) != self.GIT_HASH_LENGTH:
            raise buildstep.BuildStepFailed()
        return SUCCESS

    def doStepIf(self, step):
        return self.getProperty('canonical_swift_tag') and self.getProperty('current_swift_tag', '') != self.getProperty('canonical_swift_tag')

    def getResultSummary(self):
        if self.results == SKIPPED:
            return {'step': 'swift-project is already up to date'}
        elif self.results != SUCCESS:
            return {'step': 'Failed to update swift-project directory'}
        else:
            return {'step': 'Cleaned and updated swift-project directory'}


class UpdateSwiftCheckouts(steps.ShellSequence, ShellMixin):
    name = 'update-swift-checkouts'
    description = 'updating swift checkouts'
    descriptionDone = 'Successfully updated swift checkouts'
    flunkOnFailure = False
    warnOnFailure = False

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, workdir=SWIFT_DIR, **kwargs)
        self.commands = []
        self.summary = ''

    @defer.inlineCallbacks
    def run(self):
        if not self.getProperty('canonical_swift_tag'):
            self.summary = 'Could not find canonical Swift version, using previous checkout'
            return WARNINGS

        swift_tag = self.getProperty('canonical_swift_tag')
        self.commands = [
            util.ShellArg(command=self.shell_command('utils/update-checkout --clone'), logname='stdio', haltOnFailure=True),
            util.ShellArg(command=self.shell_command(f'utils/update-checkout --tag {swift_tag}'), logname='stdio')
        ]

        rc = yield super().runShellSequence(self.commands)
        if rc != SUCCESS:
            if self.getProperty('current_swift_tag', ''):
                self.summary = 'Failed to update swift, using previous checkout'
                return WARNINGS
            self.summary = 'Failed to update swift checkout'
            self.build.buildFinished(['Failed to set up swift, retrying update'], RETRY)
            return defer.returnValue(rc)

        self.summary = 'Successfully updated swift checkout'
        defer.returnValue(rc)

    def doStepIf(self, step):
        return self.getProperty('canonical_swift_tag') and self.getProperty('current_swift_tag', '') != self.getProperty('canonical_swift_tag')

    def getResultSummary(self):
        if self.results == SKIPPED:
            return {'step': 'Swift checkout is already up to date'}
        return {'step': self.summary}


class CheckOutLLVMProject(git.Git, AddToLogMixin):
    name = 'checkout-llvm-project'
    directory = 'llvm-project'
    branch = 'webkit'
    CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR = (0, 2)
    GIT_HASH_LENGTH = 40
    haltOnFailure = False

    def __init__(self, **kwargs):
        repourl = f'{GITHUB_URL}rniwa/llvm-project.git'
        super().__init__(
            repourl=repourl,
            workdir=self.directory,
            retry=self.CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR,
            timeout=5 * 60,
            branch=self.branch,
            alwaysUseLatest=False,
            logEnviron=False,
            progress=True,
            **kwargs
        )

    @defer.inlineCallbacks
    def run_vc(self, branch, revision, patch):
        rc = yield super().run_vc(self.branch, self.getProperty('canonical_llvm_revision'), None)
        return rc

    @defer.inlineCallbacks
    def parseGotRevision(self, _=None):
        stdout = yield self._dovccmd(['rev-parse', 'HEAD'], collectStdout=True)
        revision = stdout.strip()
        if len(revision) != self.GIT_HASH_LENGTH:
            raise buildstep.BuildStepFailed()
        return SUCCESS

    def doStepIf(self, step):
        return self.getProperty('canonical_llvm_revision') and self.getProperty('current_llvm_revision', '') != self.getProperty('canonical_llvm_revision')

    def getResultSummary(self):
        if self.results == SKIPPED:
            return {'step': 'llvm-project is already up to date'}
        elif self.results != SUCCESS:
            return {'step': 'Failed to update llvm-project directory'}
        else:
            return {'step': 'Cleaned and updated llvm-project directory'}


class UpdateClang(steps.ShellSequence, ShellMixin):
    name = 'update-clang'
    description = 'updating clang'
    descriptionDone = 'Successfully updated clang'
    flunkOnFailure = False
    warnOnFailure = False

    def __init__(self, **kwargs):
        super().__init__(logEnviron=True, workdir=LLVM_DIR, **kwargs)
        self.commands = []
        self.summary = ''

    @defer.inlineCallbacks
    def run(self):
        self.env['PATH'] = f"/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/Applications/CMake.app/Contents/bin/:{self.getProperty('builddir')}"
        self.commands = [
            util.ShellArg(command=self.shell_command('rm -r build-new; mkdir build-new'), logname='stdio', flunkOnFailure=False),
            util.ShellArg(command=self.shell_command('cd build-new; xcrun cmake -DLLVM_ENABLE_PROJECTS=clang -DCMAKE_BUILD_TYPE=Release -G Ninja ../llvm -DCMAKE_MAKE_PROGRAM=$(xcrun --sdk macosx --find ninja)'), logname='stdio', haltOnFailure=True),
            util.ShellArg(command=self.shell_command('cd build-new; ninja clang'), logname='stdio', haltOnFailure=True),
            util.ShellArg(command=['rm', '-r', '../build/WebKitBuild'], logname='stdio', flunkOnFailure=False),  # Need a clean build after complier update
        ]

        if not self.getProperty('canonical_llvm_revision'):
            self.summary = 'Could not find canonical revision, using previous build'
            return WARNINGS

        rc = yield super().runShellSequence(self.commands)
        if rc != SUCCESS:
            if self.getProperty('current_llvm_revision', ''):
                self.summary = 'Failed to update clang, using previous build'
                return WARNINGS
            self.summary = 'Failed to update clang'
            self.build.buildFinished(['Failed to set up analyzer, retrying build'], RETRY)
            return defer.returnValue(rc)

        self.summary = 'Successfully updated clang'
        self.build.addStepsAfterCurrentStep([PrintClangVersionAfterUpdate()])
        defer.returnValue(rc)

    def doStepIf(self, step):
        return self.getProperty('current_llvm_revision', '') != self.getProperty('canonical_llvm_revision')

    def getResultSummary(self):
        if self.results == SKIPPED:
            return {'step': 'Clang is already up to date'}
        return {'step': self.summary}


class PrintClangVersion(shell.ShellCommand):
    name = 'print-clang-version'
    haltOnFailure = False
    flunkOnFailure = False
    warnOnFailure = False
    CLANG_VERSION_RE = '(.*clang version.+) \\((.+?)\\)'
    summary = ''

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, workdir=LLVM_DIR, timeout=60, **kwargs)
        self.command = ['./build/bin/clang', '--version']

    @defer.inlineCallbacks
    def run(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)
        rc = yield super().run()
        log_text = self.log_observer.getStdout() + self.log_observer.getStderr()
        match = re.search(self.CLANG_VERSION_RE, log_text)
        if match:
            self.setProperty('current_llvm_revision', match.group(2).split()[1])
            self.summary = match.group(0)
        elif 'No such file or directory' in log_text:
            self.summary = 'Clang executable does not exist'
            rc = SUCCESS
        return defer.returnValue(rc)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to print clang version'}
        return {'step': self.summary}


class PrintClangVersionAfterUpdate(PrintClangVersion, ShellMixin):
    name = 'print-clang-version-after-update'
    haltOnFailure = True

    @defer.inlineCallbacks
    def run(self):
        command = './build-new/bin/clang --version; rm -r build; mv build-new build'
        self.command = self.shell_command(command)
        rc = yield super().run()
        return defer.returnValue(rc)

    def getResultSummary(self):
        if self.results != SUCCESS:
            self.build.buildFinished(['Failed to set up analyzer, retrying build'], RETRY)
        return super().getResultSummary()


class PruneCoreSymbolicationdCacheIfTooLarge(shell.ShellCommand):
    name = "prune-coresymbolicationd-cache-if-too-large"
    description = ["pruning coresymbolicationd cache to < 10GB"]
    descriptionDone = ["pruned coresymbolicationd cache"]
    flunkOnFailure = False
    haltOnFailure = False
    command = ["sudo", "python3", "Tools/Scripts/delete-if-too-large",
               "/System/Library/Caches/com.apple.coresymbolicationd"]


class SetO3OptimizationLevel(shell.ShellCommand):
    command = ["Tools/Scripts/set-webkit-configuration", "--force-optimization-level=O3"]
    name = "set-o3-optimization-level"
    description = ["set O3 optimization level"]
    descriptionDone = ["set O3 optimization level"]


class WaitForDuration(buildstep.BuildStep):
    name = 'wait'
    DEFAULT_WAIT_DURATION = 60

    def __init__(self, duration=DEFAULT_WAIT_DURATION, **kwargs):
        self.duration = duration
        self.delayedCall = None
        self.deferred = None
        super().__init__(**kwargs)

    @defer.inlineCallbacks
    def run(self):
        self.description = [f'Waiting for {self.duration}s']
        self.descriptionDone = [f'Waited for {self.duration}s']

        self.deferred = defer.Deferred()
        self.delayedCall = reactor.callLater(self.duration, self.deferred.callback, None)
        try:
            yield self.deferred
            return SUCCESS
        except defer.CancelledError:
            return CANCELLED

    def interrupt(self, reason):
        if self.delayedCall and self.delayedCall.active():
            self.delayedCall.cancel()
        if self.deferred and not self.deferred.called:
            self.deferred.cancel()
        return super().interrupt(reason)
