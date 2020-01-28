# Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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

from steps import (ApplyPatch, ApplyWatchList, CheckOutSource, CheckOutSpecificRevision, CheckPatchRelevance,
                   CheckStyle, CompileJSC, CompileWebKit, ConfigureBuild,
                   DownloadBuiltProduct, ExtractBuiltProduct, InstallGtkDependencies, InstallWpeDependencies, KillOldProcesses,
                   PrintConfiguration, RunAPITests, RunBindingsTests, RunBuildWebKitOrgUnitTests, RunEWSBuildbotCheckConfig, RunEWSUnitTests,
                   RunJavaScriptCoreTests, RunWebKit1Tests, RunWebKitPerlTests,
                   RunWebKitPyPython2Tests, RunWebKitPyPython3Tests, RunWebKitTests, UpdateWorkingDirectory, ValidatePatch)


class Factory(factory.BuildFactory):
    def __init__(self, platform, configuration=None, architectures=None, buildOnly=True, triggers=None, remotes=None, additionalArguments=None, checkRelevance=False, verifycqplus=False, **kwargs):
        factory.BuildFactory.__init__(self)
        self.addStep(ConfigureBuild(platform=platform, configuration=configuration, architectures=architectures, buildOnly=buildOnly, triggers=triggers, remotes=remotes, additionalArguments=additionalArguments))
        if checkRelevance:
            self.addStep(CheckPatchRelevance())
        self.addStep(ValidatePatch(verifycqplus=verifycqplus))
        self.addStep(PrintConfiguration())
        self.addStep(CheckOutSource())
        # CheckOutSource step pulls the latest revision, since we use alwaysUseLatest=True. Without alwaysUseLatest Buildbot will
        # automatically apply the patch to the repo, and that doesn't handle ChangeLogs well. See https://webkit.org/b/193138
        # Therefore we add CheckOutSpecificRevision step to checkout required revision.
        self.addStep(CheckOutSpecificRevision())
        self.addStep(ApplyPatch())


class StyleFactory(factory.BuildFactory):
    def __init__(self, platform, configuration=None, architectures=None, triggers=None, remotes=None, additionalArguments=None, **kwargs):
        factory.BuildFactory.__init__(self)
        self.addStep(ConfigureBuild(platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, triggers=triggers, remotes=remotes, additionalArguments=additionalArguments))
        self.addStep(ValidatePatch())
        self.addStep(PrintConfiguration())
        self.addStep(CheckOutSource())
        self.addStep(UpdateWorkingDirectory())
        self.addStep(ApplyPatch())
        self.addStep(CheckStyle())


class WatchListFactory(factory.BuildFactory):
    def __init__(self, platform, configuration=None, architectures=None, triggers=None, remotes=None, additionalArguments=None, **kwargs):
        factory.BuildFactory.__init__(self)
        self.addStep(ConfigureBuild(platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, triggers=triggers, remotes=remotes, additionalArguments=additionalArguments))
        self.addStep(ValidatePatch())
        self.addStep(PrintConfiguration())
        self.addStep(CheckOutSource())
        self.addStep(UpdateWorkingDirectory())
        self.addStep(ApplyPatch())
        self.addStep(ApplyWatchList())


class BindingsFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, additionalArguments=additionalArguments, checkRelevance=True)
        self.addStep(RunBindingsTests())


class WebKitPerlFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, additionalArguments=additionalArguments)
        self.addStep(RunWebKitPerlTests())


class WebKitPyFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, additionalArgument=additionalArguments, checkRelevance=True)
        self.addStep(RunWebKitPyPython2Tests())
        self.addStep(RunWebKitPyPython3Tests())


class BuildFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, triggers=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, triggers=triggers, additionalArguments=additionalArguments)
        self.addStep(KillOldProcesses())
        self.addStep(CompileWebKit())


class TestFactory(Factory):
    LayoutTestClass = None
    APITestClass = None

    def getProduct(self):
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())

    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, additionalArguments=additionalArguments)
        self.getProduct()
        self.addStep(KillOldProcesses())
        if self.LayoutTestClass:
            self.addStep(self.LayoutTestClass())
        if self.APITestClass:
            self.addStep(self.APITestClass())


class JSCTestsFactory(Factory):
    def __init__(self, platform, configuration='release', architectures=None, remotes=None, additionalArguments=None, runTests='true', **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, remotes=remotes, additionalArguments=additionalArguments, checkRelevance=True)
        self.addStep(KillOldProcesses())
        self.addStep(CompileJSC(skipUpload=True))
        if runTests.lower() == 'true':
            self.addStep(RunJavaScriptCoreTests())


class APITestsFactory(TestFactory):
    APITestClass = RunAPITests


class iOSBuildFactory(BuildFactory):
    pass


class iOSTestsFactory(TestFactory):
    LayoutTestClass = RunWebKitTests


class macOSBuildFactory(BuildFactory):
    pass


class macOSWK1Factory(TestFactory):
    LayoutTestClass = RunWebKit1Tests


class macOSWK2Factory(TestFactory):
    LayoutTestClass = RunWebKitTests


class WindowsFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, triggers=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, triggers=triggers, additionalArguments=additionalArguments)
        self.addStep(KillOldProcesses())
        self.addStep(CompileWebKit(skipUpload=True))
        self.addStep(ValidatePatch(verifyBugClosed=False, addURLs=False))
        self.addStep(RunWebKit1Tests())


class WinCairoFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, triggers=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=True, triggers=triggers, additionalArguments=additionalArguments)
        self.addStep(KillOldProcesses())
        self.addStep(CompileWebKit(skipUpload=True))


class GTKBuildFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, triggers=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=True, triggers=triggers, additionalArguments=additionalArguments)
        self.addStep(KillOldProcesses())
        self.addStep(InstallGtkDependencies())
        self.addStep(CompileWebKit(skipUpload=True))


class GTKBuildAndTestFactory(GTKBuildFactory):
    LayoutTestClass = None
    APITestClass = None

    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        GTKBuildFactory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=True, additionalArguments=additionalArguments)
        self.addStep(KillOldProcesses())
        self.addStep(ValidatePatch(verifyBugClosed=False, addURLs=False))
        if self.LayoutTestClass:
            self.addStep(self.LayoutTestClass())
        if self.APITestClass:
            self.addStep(self.APITestClass())


class GTKAPIBuildAndTestFactory(GTKBuildAndTestFactory):
    APITestClass = RunAPITests


class WPEFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, triggers=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=True, triggers=triggers, additionalArguments=additionalArguments)
        self.addStep(KillOldProcesses())
        self.addStep(InstallWpeDependencies())
        self.addStep(CompileWebKit(skipUpload=True))


class ServicesFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, additionalArguments=additionalArguments, checkRelevance=True)
        self.addStep(RunEWSUnitTests())
        self.addStep(RunEWSBuildbotCheckConfig())
        self.addStep(RunBuildWebKitOrgUnitTests())


class CommitQueueFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, triggers=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, triggers=triggers, additionalArguments=additionalArguments, verifycqplus=True)
        self.addStep(KillOldProcesses())
        self.addStep(CompileWebKit(skipUpload=True))
        self.addStep(KillOldProcesses())
        self.addStep(ValidatePatch(addURLs=False, verifycqplus=True))
        self.addStep(RunWebKit1Tests())
