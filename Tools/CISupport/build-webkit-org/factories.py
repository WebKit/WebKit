# Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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

from .steps import *
from Shared.steps import *


class Factory(factory.BuildFactory):
    shouldInstallDependencies = True
    shouldUseCrossTargetImage = False

    def __init__(self, platform, configuration, architectures, buildOnly, additionalArguments, device_model, triggers=None):
        factory.BuildFactory.__init__(self)
        self.addStep(ConfigureBuild(platform=platform, configuration=configuration, architecture=" ".join(architectures), buildOnly=buildOnly, additionalArguments=additionalArguments, device_model=device_model, triggers=triggers))
        self.addStep(PrintConfiguration())
        self.addStep(CheckOutSource())
        self.addStep(CheckOutSpecificRevision())
        self.addStep(ShowIdentifier())
        if not (platform == "jsc-only"):
            self.addStep(KillOldProcesses())
        self.addStep(CleanBuildIfScheduled())
        self.addStep(DeleteStaleBuildFiles())
        if platform.startswith('mac'):
            self.addStep(PruneCoreSymbolicationdCacheIfTooLarge())
        if self.shouldInstallDependencies:
            if platform.startswith("gtk"):
                self.addStep(InstallGtkDependencies())
            if platform == "wpe":
                self.addStep(InstallWpeDependencies())


class BuildFactory(Factory):
    shouldRunJSCBundleStep = False
    shouldRunMiniBrowserBundleStep = False

    def __init__(self, platform, configuration, architectures, triggers=None, additionalArguments=None, device_model=None):
        Factory.__init__(self, platform, configuration, architectures, True, additionalArguments, device_model, triggers=triggers)

        if platform.startswith("playstation"):
            self.addStep(CompileWebKit(timeout=2 * 60 * 60))
        else:
            self.addStep(CompileWebKit())

        if self.shouldUseCrossTargetImage:
            self.addStep(CheckIfNeededUpdateDeployedCrossTargetImage())
        if self.shouldRunJSCBundleStep:
            self.addStep(GenerateJSCBundle())
        if self.shouldRunMiniBrowserBundleStep:
            self.addStep(GenerateMiniBrowserBundle())

        if triggers:
            self.addStep(trigger.Trigger(schedulerNames=triggers))


class NoInstallDependenciesBuildFactory(BuildFactory):
    shouldInstallDependencies = False


class CrossTargetBuildFactory(NoInstallDependenciesBuildFactory):
    shouldUseCrossTargetImage = True


class TestFactory(Factory):
    JSCTestClass = RunJavaScriptCoreTests
    LayoutTestClass = RunWebKitTests

    def getProduct(self):
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())

    def __init__(self, platform, configuration, architectures, additionalArguments=None, device_model=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, device_model, **kwargs)
        self.getProduct()

        if platform == 'win':
            self.addStep(InstallWindowsDependencies())

        if platform.startswith(('mac', 'ios-simulator', 'visionos-simulator')):
            self.addStep(WaitForCrashCollection())

        if self.JSCTestClass:
            self.addStep(self.JSCTestClass())
        if self.LayoutTestClass:
            self.addStep(self.LayoutTestClass())
        if not platform.startswith('win'):
            self.addStep(RunDashboardTests())
        if self.LayoutTestClass:
            self.addStep(ArchiveTestResults())
            self.addStep(UploadTestResults())
            self.addStep(ExtractTestResults())
            self.addStep(SetPermissions())

        if platform.startswith(('win', 'mac', 'ios-simulator')):
            self.addStep(RunAPITests())

        # FIXME: Re-enable these tests for Monterey once webkit.org/b/239463 is resolved.
        if platform.startswith('mac') and (platform != 'mac-monterey'):
            self.addStep(RunLLDBWebKitTests())

        self.addStep(RunWebKitPyTests())
        self.addStep(RunPerlTests())
        self.addStep(RunBindingsTests())
        self.addStep(RunBuiltinsTests())

        if platform.startswith(('mac', 'ios-simulator', 'visionos-simulator')):
            self.addStep(TriggerCrashLogSubmission())

        if platform.startswith("gtk"):
            self.addStep(RunGtkAPITests())
            if additionalArguments and "--display-server=wayland" in additionalArguments:
                self.addStep(RunWebDriverTests())
        if platform == "wpe":
            self.addStep(RunWPEAPITests())


class BuildAndTestFactory(TestFactory):
    def getProduct(self):
        self.addStep(CompileWebKit())

    def __init__(self, platform, configuration, architectures, triggers=None, additionalArguments=None, device_model=None, **kwargs):
        TestFactory.__init__(self, platform, configuration, architectures, additionalArguments, device_model, **kwargs)
        if triggers:
            self.addStep(ArchiveBuiltProduct())
            self.addStep(UploadBuiltProduct())
            self.addStep(trigger.Trigger(schedulerNames=triggers))


class BuildAndTestLLINTCLoopFactory(Factory):
    def __init__(self, platform, configuration, architectures, triggers=None, additionalArguments=None, device_model=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, device_model, **kwargs)
        self.addStep(CompileLLINTCLoop())
        self.addStep(RunLLINTCLoopTests())


class BuildAndTest32bitJSCFactory(Factory):
    def __init__(self, platform, configuration, architectures, triggers=None, additionalArguments=None, device_model=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, device_model, **kwargs)
        self.addStep(Compile32bitJSC())
        self.addStep(Run32bitJSCTests())


class BuildAndNonLayoutTestFactory(BuildAndTestFactory):
    LayoutTestClass = None


class BuildAndJSCTestsFactory(Factory):
    def __init__(self, platform, configuration, architectures, triggers=None, additionalArguments=None, device_model=None):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, device_model)
        self.addStep(CompileJSCOnly(timeout=60 * 60))
        self.addStep(RunJavaScriptCoreTests(timeout=60 * 60))


class TestAllButJSCFactory(TestFactory):
    JSCTestClass = None


class BuildAndTestAndArchiveAllButJSCFactory(BuildAndTestFactory):
    JSCTestClass = None

    def __init__(self, platform, configuration, architectures, triggers=None, additionalArguments=None, device_model=None, **kwargs):
        BuildAndTestFactory.__init__(self, platform, configuration, architectures, triggers, additionalArguments, device_model, **kwargs)
        # The parent class will already archive if triggered
        if not triggers:
            self.addStep(ArchiveBuiltProduct())
            self.addStep(UploadBuiltProduct())
        if platform == "gtk-3":
            self.addStep(RunWebDriverTests())


class BuildAndGenerateJSCBundleFactory(BuildFactory):
    shouldRunJSCBundleStep = True


class BuildAndGenerateMiniBrowserBundleFactory(BuildFactory):
    shouldRunMiniBrowserBundleStep = True


class BuildAndGenerateMiniBrowserJSCBundleFactory(BuildFactory):
    shouldRunJSCBundleStep = True
    shouldRunMiniBrowserBundleStep = True


class BuildAndUploadBuiltProductviaSftpFactory(BuildFactory):
    def __init__(self, platform, configuration, architectures, triggers=None, additionalArguments=None, device_model=None):
        BuildFactory.__init__(self, platform, configuration, architectures, triggers, additionalArguments, device_model)
        self.addStep(InstallBuiltProduct())
        self.addStep(ArchiveBuiltProduct())
        self.addStep(UploadBuiltProductViaSftp())


class TestJSCFactory(Factory):
    def __init__(self, platform, configuration, architectures, additionalArguments=None, device_model=None):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, device_model)
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())
        if platform == 'win':
            self.addStep(InstallWindowsDependencies())
        self.addStep(RunJavaScriptCoreTests())


class Test262Factory(Factory):
    def __init__(self, platform, configuration, architectures, additionalArguments=None, device_model=None):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, device_model)
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())
        self.addStep(RunTest262Tests())


class TestJSFactory(Factory):
    def __init__(self, platform, configuration, architectures, additionalArguments=None, device_model=None):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, device_model)
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())
        self.addStep(RunJavaScriptCoreTests())
        self.addStep(RunTest262Tests())


class TestLayoutFactory(Factory):
    def __init__(self, platform, configuration, architectures, additionalArguments=None, device_model=None):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, device_model)
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())
        self.addStep(RunWebKitTests())
        if not platform.startswith('win'):
            self.addStep(RunDashboardTests())
        self.addStep(ArchiveTestResults())
        self.addStep(UploadTestResults())
        self.addStep(ExtractTestResults())
        self.addStep(SetPermissions())


class TestWebDriverFactory(Factory):
    def __init__(self, platform, configuration, architectures, additionalArguments=None, device_model=None):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, device_model)
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())
        self.addStep(RunWebDriverTests())


class TestWebKit1Factory(TestFactory):
    LayoutTestClass = RunWebKit1Tests


class TestWebKit1AllButJSCFactory(TestWebKit1Factory):
    JSCTestClass = None


class BuildAndPerfTestFactory(Factory):
    def __init__(self, platform, configuration, architectures, additionalArguments=None, device_model=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, device_model, **kwargs)
        self.addStep(CompileWebKit())
        self.addStep(RunAndUploadPerfTests())
        if platform.startswith("gtk"):
            self.addStep(RunBenchmarkTests(timeout=2000))


class DownloadAndPerfTestFactory(Factory):
    def __init__(self, platform, configuration, architectures, additionalArguments=None, device_model=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, device_model, **kwargs)
        if self.shouldUseCrossTargetImage:
            self.addStep(CheckIfNeededUpdateRunningCrossTargetImage())
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())
        if platform != "wpe":
            self.addStep(RunAndUploadPerfTests())
        if platform in ["gtk", "wpe"]:
            self.addStep(RunBenchmarkTests(timeout=2000))


class SaferCPPStaticAnalyzerFactory(Factory):
    def __init__(self, platform, configuration, architectures, additionalArguments=None, device_model=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, device_model, **kwargs)
        self.addStep(InstallCMake())
        self.addStep(InstallNinja())
        self.addStep(PrintClangVersion())
        self.addStep(CheckOutLLVMProject())
        self.addStep(UpdateClang())
        self.addStep(ScanBuild())


class CrossTargetDownloadAndPerfTestFactory(DownloadAndPerfTestFactory):
    shouldInstallDependencies = False
    shouldUseCrossTargetImage = True
