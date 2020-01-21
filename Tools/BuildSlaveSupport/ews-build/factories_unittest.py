# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import unittest

from buildbot.process.buildstep import _BuildStepFactory
from buildbot.steps.master import SetProperty

import factories
import steps


class TestCase(unittest.TestCase):
    def assertBuildSteps(self, actual_steps, expected_steps):
        assert all(map(lambda step: isinstance(step, _BuildStepFactory), actual_steps))
        assert all(map(lambda step: isinstance(step, _BuildStepFactory), expected_steps))

        # Convert to dictionaries because assertEqual() only knows how to diff Python built-in types.
        def step_to_dict(step):
            return {key: getattr(step, key) for key in step.compare_attrs}

        actual_steps = map(step_to_dict, actual_steps)
        expected_steps = map(step_to_dict, expected_steps)
        self.assertEqual(actual_steps, expected_steps)


class TestGenericFactory(TestCase):
    def setUp(self):
        self.longMessage = True

    def test_generic_factory(self):
        factory = factories.Factory(platform='ios-simulator-13', configuration='release', architectures='arm64')
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='ios-simulator-13', configuration='release', architectures='arm64', buildOnly=True, triggers=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch, verifycqplus=False),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.ApplyPatch),
        ])

    def test_generic_factory_with_check_relevance(self):
        factory = factories.Factory(platform='ios-simulator-13', configuration='release', architectures='arm64', checkRelevance=True)
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='ios-simulator-13', configuration='release', architectures='arm64', buildOnly=True, triggers=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch, verifycqplus=False),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.ApplyPatch),
        ])


class TestTestsFactory(TestCase):
    def setUp(self):
        self.longMessage = True

    def test_style_factory(self):
        factory = factories.StyleFactory(platform='*', configuration=None, architectures=None)
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='*', configuration=None, architectures=None, buildOnly=False, triggers=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.UpdateWorkingDirectory),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.CheckStyle),
        ])

    def test_watchlist_factory(self):
        factory = factories.WatchListFactory(platform='*', configuration=None, architectures=None)
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='*', configuration=None, architectures=None, buildOnly=False, triggers=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.UpdateWorkingDirectory),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.ApplyWatchList),
        ])

    def test_bindings_factory(self):
        factory = factories.BindingsFactory(platform='*', configuration=None, architectures=None)
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='*', configuration=None, architectures=None, buildOnly=False, triggers=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch, verifycqplus=False),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.RunBindingsTests),
        ])

    def test_webkitperl_factory(self):
        factory = factories.WebKitPerlFactory(platform='*', configuration=None, architectures=None)
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='*', configuration=None, architectures=None, buildOnly=False, triggers=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch, verifycqplus=False),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.RunWebKitPerlTests),
        ])

    def test_webkitpy_factory(self):
        factory = factories.WebKitPyFactory(platform='*', configuration=None, architectures=None)
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='*', configuration=None, architectures=None, buildOnly=False, triggers=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch, verifycqplus=False),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.RunWebKitPyPython2Tests),
            _BuildStepFactory(steps.RunWebKitPyPython3Tests),
        ])

    def test_services_factory(self):
        factory = factories.ServicesFactory(platform='*', configuration=None, architectures=None)
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='*', configuration=None, architectures=None, buildOnly=False, triggers=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch, verifycqplus=False),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.RunEWSUnitTests),
            _BuildStepFactory(steps.RunEWSBuildbotCheckConfig),
            _BuildStepFactory(steps.RunBuildWebKitOrgUnitTests),
        ])
