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


from buildbot.process import factory

from steps import *


class Factory(factory.BuildFactory):
    def __init__(self, platform, configuration=None, architectures=None, buildOnly=True, additionalArguments=None, **kwargs):
        factory.BuildFactory.__init__(self)
        self.addStep(ConfigureBuild(platform, configuration, architectures, buildOnly, additionalArguments))
        self.addStep(CheckOutSource())


class StyleFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments)
        self.addStep(CheckStyle())


class BindingsFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments)
        self.addStep(RunBindingsTests())


class WebKitPerlFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments)
        self.addStep(RunWebKitPerlTests())


class WebKitPyFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments)
        self.addStep(RunWebKitPyTests())


class BuildFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments)
        self.addStep(KillOldProcesses())
        self.addStep(CleanBuild())
        self.addStep(CompileWebKit())
        self.addStep(UnApplyPatchIfRequired())
        self.addStep(CompileWebKitToT())


class JSCTestsFactory(Factory):
    def __init__(self, platform, configuration='release', architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform, configuration, architectures, False, additionalArguments)
        self.addStep(CompileJSCOnly())
        self.addStep(RunJavaScriptCoreTests())


class GTKFactory(Factory):
    pass


class iOSFactory(BuildFactory):
    pass


class iOSSimulatorFactory(BuildFactory):
    pass


class MacWK1Factory(BuildFactory):
    pass


class MacWK2Factory(BuildFactory):
    pass


class WindowsFactory(Factory):
    pass


class WinCairoFactory(Factory):
    pass


class WPEFactory(Factory):
    pass
