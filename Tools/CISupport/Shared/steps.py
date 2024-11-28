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

from buildbot.plugins import steps, util
from buildbot.process import buildstep, logobserver, properties
from buildbot.process.results import Results, SUCCESS, FAILURE, CANCELLED, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.steps import master, shell, transfer, trigger
from buildbot.steps.source import git
from twisted.internet import defer
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
LLVM_REVISION = 'c6d7a57066d78b337d5f0d352918bdba8c3e9592'


class ShellMixin(object):
    WINDOWS_SHELL_PLATFORMS = ['win']

    def has_windows_shell(self):
        return self.getProperty('platform', '*') in self.WINDOWS_SHELL_PLATFORMS

    def shell_command(self, command):
        if self.has_windows_shell():
            return ['sh', '-c', command]
        return ['/bin/sh', '-c', command]

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


class InstallCMake(shell.ShellCommandNewStyle):
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


class InstallNinja(shell.ShellCommandNewStyle, ShellMixin):
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
        rc = yield super().run_vc(self.branch, LLVM_REVISION, None)
        return rc

    @defer.inlineCallbacks
    def parseGotRevision(self, _=None):
        stdout = yield self._dovccmd(['rev-parse', 'HEAD'], collectStdout=True)
        revision = stdout.strip()
        if len(revision) != self.GIT_HASH_LENGTH:
            raise buildstep.BuildStepFailed()
        return SUCCESS

    def doStepIf(self, step):
        return self.build.getProperty('llvm_revision', '') != LLVM_REVISION

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

        rc = yield super().runShellSequence(self.commands)
        if rc != SUCCESS:
            if self.getProperty('llvm_revision', ''):
                self.summary = 'Failed to update clang, using previous build'
                return WARNINGS
            self.summary = 'Failed to update clang'
            self.build.buildFinished(['Failed to set up analyzer, retrying build'], RETRY)
            return defer.returnValue(rc)

        self.summary = 'Successfully updated clang'
        self.build.addStepsAfterCurrentStep([PrintClangVersionAfterUpdate()])
        defer.returnValue(rc)

    def doStepIf(self, step):
        return self.build.getProperty('llvm_revision', '') != LLVM_REVISION

    def getResultSummary(self):
        if self.results == SKIPPED:
            return {'step': 'Clang is already up to date'}
        return {'step': self.summary}


class PrintClangVersion(shell.ShellCommandNewStyle):
    name = 'print-clang-version'
    haltOnFailure = False
    flunkOnFailure = False
    warnOnFailure = False

    @defer.inlineCallbacks
    def run(self):
        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('stdio', self.log_observer)
        self.command = ['../llvm-project/build/bin/clang', '--version']
        rc = yield super().run()
        return defer.returnValue(rc)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to print clang version'}
        log_text = self.log_observer.getStdout()
        match = re.search('(.*clang version.+) (\\(.+?\\))', log_text)
        if match:
            return {'step': match.group(0)}


class PrintClangVersion(shell.ShellCommandNewStyle):
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
            self.build.setProperty('llvm_revision', match.group(2).split()[1])
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
