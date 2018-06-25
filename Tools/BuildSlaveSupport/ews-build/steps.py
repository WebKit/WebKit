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

from buildbot.process import buildstep
from buildbot.process.results import Results, SUCCESS, FAILURE, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.steps import shell
from buildbot.steps.source import svn
from twisted.internet import defer


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
