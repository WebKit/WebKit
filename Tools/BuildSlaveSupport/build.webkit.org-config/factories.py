# Copyright (C) 2017 Apple Inc. All rights reserved.
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

from buildbot.process import factory
from buildbot.steps import trigger

from steps import *


class Factory(factory.BuildFactory):
    def __init__(self, platform, configuration, architectures, buildOnly, additionalArguments, SVNMirror):
        factory.BuildFactory.__init__(self)
        self.addStep(ConfigureBuild(platform=platform, configuration=configuration, architecture=" ".join(architectures), buildOnly=buildOnly, additionalArguments=additionalArguments, SVNMirror=SVNMirror))
        if SVNMirror:
            self.addStep(WaitForSVNServer())
        self.addStep(CheckOutSource(SVNMirror=SVNMirror))
        if not (platform == "jsc-only"):
            self.addStep(KillOldProcesses())
        self.addStep(CleanBuildIfScheduled())
        self.addStep(DeleteStaleBuildFiles())
        if platform == "win":
            self.addStep(InstallWin32Dependencies())
        if platform == "gtk" and additionalArguments != ["--no-experimental-features"]:
            self.addStep(InstallGtkDependencies())
        if platform == "wpe":
            self.addStep(InstallWpeDependencies())


class BuildFactory(Factory):
    ShouldRunJSCBundleStep = False

    def __init__(self, platform, configuration, architectures, triggers=None, additionalArguments=None, SVNMirror=None):
        Factory.__init__(self, platform, configuration, architectures, True, additionalArguments, SVNMirror)

        if platform == "win":
            self.addStep(CompileWebKit(timeout=2 * 60 * 60))
        else:
            self.addStep(CompileWebKit())

        if triggers:
            self.addStep(ArchiveBuiltProduct())
            self.addStep(UploadBuiltProduct())
            if platform.startswith('mac') or platform.startswith('ios-simulator'):
                self.addStep(ArchiveMinifiedBuiltProduct())
                self.addStep(UploadMinifiedBuiltProduct())
            if self.ShouldRunJSCBundleStep:
                self.addStep(GenerateJSCBundle())
            self.addStep(TransferToS3())
            self.addStep(trigger.Trigger(schedulerNames=triggers))


class TestFactory(Factory):
    JSCTestClass = RunJavaScriptCoreTests
    LayoutTestClass = RunWebKitTests
    ShouldRunJSCBundleStep = False

    def getProduct(self):
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())

    def __init__(self, platform, configuration, architectures, additionalArguments=None, SVNMirror=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, SVNMirror, **kwargs)
        self.getProduct()

        if platform == 'wincairo':
            self.addStep(InstallWinCairoDependencies())

        if self.JSCTestClass:
            self.addStep(self.JSCTestClass())
        if self.LayoutTestClass:
            self.addStep(self.LayoutTestClass())

        if platform.startswith('win') or platform.startswith('mac') or platform.startswith('ios-simulator'):
            self.addStep(RunAPITests())
        self.addStep(RunPythonTests())
        self.addStep(RunPerlTests())
        self.addStep(RunBindingsTests())
        self.addStep(RunBuiltinsTests())
        if not platform.startswith('win'):
            self.addStep(RunDashboardTests())
        if self.LayoutTestClass:
            self.addStep(ArchiveTestResults())
            self.addStep(UploadTestResults())
            self.addStep(ExtractTestResults())
        if self.ShouldRunJSCBundleStep:
            self.addStep(GenerateJSCBundle())
        if platform == "gtk":
            self.addStep(RunGtkAPITests())
            self.addStep(RunWebDriverTests())
        if platform == "wpe":
            self.addStep(RunWPEAPITests())


class BuildAndTestFactory(TestFactory):
    def getProduct(self):
        self.addStep(CompileWebKit())

    def __init__(self, platform, configuration, architectures, triggers=None, additionalArguments=None, SVNMirror=None, **kwargs):
        TestFactory.__init__(self, platform, configuration, architectures, additionalArguments, SVNMirror, **kwargs)
        if triggers:
            self.addStep(ArchiveBuiltProduct())
            self.addStep(UploadBuiltProduct())
            self.addStep(trigger.Trigger(schedulerNames=triggers))


class BuildAndTestLLINTCLoopFactory(Factory):
    def __init__(self, platform, configuration, architectures, triggers=None, additionalArguments=None, SVNMirror=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, SVNMirror, **kwargs)
        self.addStep(CompileLLINTCLoop())
        self.addStep(RunLLINTCLoopTests())


class BuildAndTest32bitJSCFactory(Factory):
    def __init__(self, platform, configuration, architectures, triggers=None, additionalArguments=None, SVNMirror=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, SVNMirror, **kwargs)
        self.addStep(Compile32bitJSC())
        self.addStep(Run32bitJSCTests())


class BuildAndNonLayoutTestFactory(BuildAndTestFactory):
    LayoutTestClass = None


class BuildAndRemoteJSCTestsFactory(Factory):
    def __init__(self, platform, configuration, architectures, triggers=None, additionalArguments=None, SVNMirror=None):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, SVNMirror)
        self.addStep(CompileJSCOnly(timeout=60 * 60))
        self.addStep(RunRemoteJavaScriptCoreTests(timeout=60 * 60))


class TestWebKit1LeaksFactory(Factory):
    def __init__(self, platform, configuration, architectures, additionalArguments=None, SVNMirror=None):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, SVNMirror)
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())
        self.addStep(RunWebKit1LeakTests())
        self.addStep(ArchiveTestResults())
        self.addStep(UploadTestResults())
        self.addStep(ExtractTestResultsAndLeaks())


class TestAllButJSCFactory(TestFactory):
    JSCTestClass = None


class BuildAndGenerateJSCBundleFactory(BuildFactory):
    ShouldRunJSCBundleStep = True


class BuildAndNonLayoutTestAndGenerateJSCBundleFactory(BuildAndNonLayoutTestFactory):
    ShouldRunJSCBundleStep = True


class TestJSCFactory(Factory):
    def __init__(self, platform, configuration, architectures, additionalArguments=None, SVNMirror=None):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, SVNMirror)
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())
        self.addStep(RunJavaScriptCoreTests())


class Test262Factory(Factory):
    def __init__(self, platform, configuration, architectures, additionalArguments=None, SVNMirror=None):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, SVNMirror)
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())
        self.addStep(RunTest262Tests())


class TestWebKit1Factory(TestFactory):
    LayoutTestClass = RunWebKit1Tests


class TestWebKit1AllButJSCFactory(TestWebKit1Factory):
    JSCTestClass = None


class BuildAndPerfTestFactory(Factory):
    def __init__(self, platform, configuration, architectures, additionalArguments=None, SVNMirror=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, SVNMirror, **kwargs)
        self.addStep(CompileWebKit())
        self.addStep(RunAndUploadPerfTests())
        if platform == "gtk":
            self.addStep(RunBenchmarkTests(timeout=2000))


class DownloadAndPerfTestFactory(Factory):
    def __init__(self, platform, configuration, architectures, additionalArguments=None, SVNMirror=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, SVNMirror, **kwargs)
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())
        self.addStep(RunAndUploadPerfTests())
        if platform == "gtk":
            self.addStep(RunBenchmarkTests(timeout=2000))
