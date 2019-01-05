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


from buildbot.process import factory, properties
from buildbot.steps import trigger

from steps import *

Property = properties.Property


class Factory(factory.BuildFactory):
    def __init__(self, platform, configuration=None, architectures=None, buildOnly=True, additionalArguments=None, checkRelevance=False, **kwargs):
        factory.BuildFactory.__init__(self)
        self.addStep(ConfigureBuild(platform, configuration, architectures, buildOnly, additionalArguments))
        if checkRelevance:
            self.addStep(CheckPatchRelevance())
        self.addStep(ValidatePatch())
        self.addStep(CheckOutSource())
        self.addStep(ApplyPatch())


class StyleFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments)
        self.addStep(CheckStyle())


class BindingsFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, checkRelevance=True)
        self.addStep(RunBindingsTests())


class WebKitPerlFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments)
        self.addStep(RunWebKitPerlTests())


class WebKitPyFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, checkRelevance=True)
        self.addStep(RunWebKitPyTests())


class BuildFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, triggers=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments)
        self.addStep(KillOldProcesses())
        self.addStep(CleanBuild())
        self.addStep(CompileWebKit())
        self.addStep(UnApplyPatchIfRequired())
        self.addStep(CompileWebKitToT())
        if triggers:
            self.addStep(ArchiveBuiltProduct())
            self.addStep(UploadBuiltProduct())
            self.addStep(trigger.Trigger(schedulerNames=triggers, set_properties=self.propertiesToPassToTriggers() or {}))

    def propertiesToPassToTriggers(self):
        return {
            "ewspatchid": Property("ewspatchid"),
            "configuration": Property("configuration"),
            "platform": Property("platform"),
            "fullPlatform": Property("fullPlatform"),
            "architecture": Property("architecture"),
        }


class TestFactory(Factory):
    LayoutTestClass = None
    APITestClass = None

    def getProduct(self):
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())

    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments)
        self.getProduct()
        if self.LayoutTestClass:
            self.addStep(self.LayoutTestClass())
        if self.APITestClass:
            self.addStep(self.APITestClass())


class JSCTestsFactory(Factory):
    def __init__(self, platform, configuration='release', architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments, checkRelevance=True)
        self.addStep(CompileJSCOnly())
        self.addStep(UnApplyPatchIfRequired())
        self.addStep(CompileJSCOnlyToT())
        self.addStep(RunJavaScriptCoreTests())
        self.addStep(ReRunJavaScriptCoreTests())
        self.addStep(UnApplyPatchIfRequired())
        self.addStep(RunJavaScriptCoreTestsToT())


class APITestsFactory(TestFactory):
    APITestClass = RunAPITests


class GTKFactory(Factory):
    pass


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
    pass


class WinCairoFactory(Factory):
    pass


class WPEFactory(Factory):
    pass
