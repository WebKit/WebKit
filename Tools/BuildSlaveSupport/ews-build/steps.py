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

from buildbot.process import buildstep, properties
from buildbot.process.results import Results, SUCCESS, FAILURE, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.steps import shell
from buildbot.steps.source import svn
from twisted.internet import defer

WithProperties = properties.WithProperties

class ConfigureBuild(buildstep.BuildStep):
    name = "configure-build"
    description = ["configuring build"]
    descriptionDone = ["configured build"]

    def __init__(self, platform, configuration, architectures, buildOnly, additionalArguments):
        super(ConfigureBuild, self).__init__()
        self.platform = platform
        if platform != 'jsc-only':
            self.platform = platform.split('-', 1)[0]
        self.fullPlatform = platform
        self.configuration = configuration
        self.architecture = " ".join(architectures) if architectures else None
        self.buildOnly = buildOnly
        self.additionalArguments = additionalArguments

    def start(self):
        self.setProperty("platform", self.platform)
        self.setProperty("fullPlatform", self.fullPlatform)
        self.setProperty("configuration", self.configuration)
        self.setProperty("architecture", self.architecture)
        self.setProperty("buildOnly", self.buildOnly)
        self.setProperty("additionalArguments", self.additionalArguments)
        self.finished(SUCCESS)
        return defer.succeed(None)


class CheckOutSource(svn.SVN):
    CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR = (0, 2)

    def __init__(self, **kwargs):
        self.repourl = 'https://svn.webkit.org/repository/webkit/trunk'
        super(CheckOutSource, self).__init__(repourl=self.repourl,
                                                retry=self.CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR,
                                                preferLastChangedRev=True,
                                                **kwargs)


class CheckStyle(shell.ShellCommand):
    name = 'check-webkit-style'
    description = ['check-webkit-style running']
    descriptionDone = ['check-webkit-style']
    flunkOnFailure = True
    command = ['Tools/Scripts/check-webkit-style']


class RunBindingsTests(shell.ShellCommand):
    name = 'bindings-tests'
    description = ['bindings-tests running']
    descriptionDone = ['bindings-tests']
    flunkOnFailure = True
    command = ['Tools/Scripts/run-bindings-tests']


class RunWebKitPerlTests(shell.ShellCommand):
    name = 'webkitperl-tests'
    description = ['webkitperl-tests running']
    descriptionDone = ['webkitperl-tests']
    flunkOnFailure = True
    command = ['Tools/Scripts/test-webkitperl']

    def __init__(self, **kwargs):
        super(RunWebKitPerlTests, self).__init__(timeout=2 * 60, **kwargs)


class RunWebKitPyTests(shell.ShellCommand):
    name = 'webkitpy-tests'
    description = ['webkitpy-tests running']
    descriptionDone = ['webkitpy-tests']
    flunkOnFailure = True
    command = ['Tools/Scripts/test-webkitpy']

    def __init__(self, **kwargs):
        super(RunWebKitPyTests, self).__init__(timeout=2 * 60, **kwargs)


def appendCustomBuildFlags(step, platform, fullPlatform):
    # FIXME: Make a common 'supported platforms' list.
    if platform not in ('gtk', 'wincairo', 'ios', 'jsc-only', 'wpe'):
        return
    if fullPlatform.startswith('ios-simulator'):
        platform = 'ios-simulator'
    elif platform == 'ios':
        platform = 'device'
    step.setCommand(step.command + ['--' + platform])


class CompileWebKit(shell.Compile):
    name = "compile-webkit"
    description = ["compiling"]
    descriptionDone = ["compiled"]
    env = {'MFLAGS': ''}
    warningPattern = ".*arning: .*"
    command = ["perl", "Tools/Scripts/build-webkit", WithProperties("--%(configuration)s")]

    def start(self):
        platform = self.getProperty('platform')
        buildOnly = self.getProperty('buildOnly')
        architecture = self.getProperty('architecture')
        additionalArguments = self.getProperty('additionalArguments')

        if additionalArguments:
            self.setCommand(self.command + additionalArguments)
        if platform in ('mac', 'ios') and architecture:
            self.setCommand(self.command + ['ARCHS=' + architecture])
            if platform == 'ios':
                self.setCommand(self.command + ['ONLY_ACTIVE_ARCH=NO'])
        if platform in ('mac', 'ios') and buildOnly:
            # For build-only bots, the expectation is that tests will be run on separate machines,
            # so we need to package debug info as dSYMs. Only generating line tables makes
            # this much faster than full debug info, and crash logs still have line numbers.
            self.setCommand(self.command + ['DEBUG_INFORMATION_FORMAT=dwarf-with-dsym'])
            self.setCommand(self.command + ['CLANG_DEBUG_INFORMATION_LEVEL=line-tables-only'])

        appendCustomBuildFlags(self, platform, self.getProperty('fullPlatform'))

        return shell.Compile.start(self)


class CompileJSCOnly(CompileWebKit):
    name = "build-jsc"
    command = ["perl", "Tools/Scripts/build-jsc", WithProperties("--%(configuration)s")]


class CleanBuild(shell.Compile):
    name = "delete-WebKitBuild-directory"
    description = ["deleting WebKitBuild directory"]
    descriptionDone = ["deleted WebKitBuild directory"]
    command = ["python", "Tools/BuildSlaveSupport/clean-build", WithProperties("--platform=%(fullPlatform)s"), WithProperties("--%(configuration)s")]


class KillOldProcesses(shell.Compile):
    name = "kill-old-processes"
    description = ["killing old processes"]
    descriptionDone = ["killed old processes"]
    command = ["python", "Tools/BuildSlaveSupport/kill-old-processes", "buildbot"]

    def __init__(self, **kwargs):
        super(KillOldProcesses, self).__init__(timeout=60, **kwargs)
