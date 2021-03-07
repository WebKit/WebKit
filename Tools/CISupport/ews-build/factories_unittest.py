# Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
        assert all([isinstance(step, _BuildStepFactory) for step in actual_steps])
        assert all([isinstance(step, _BuildStepFactory) for step in expected_steps])

        # Convert to dictionaries because assertEqual() only knows how to diff Python built-in types.
        def step_to_dict(step):
            return {key: getattr(step, key) for key in step.compare_attrs}

        actual_steps = [step_to_dict(step) for step in actual_steps]
        expected_steps = [step_to_dict(step) for step in expected_steps]
        self.assertEqual(actual_steps, expected_steps)


class TestGenericFactory(TestCase):
    def setUp(self):
        self.longMessage = True

    def test_generic_factory(self):
        factory = factories.Factory(platform='ios-simulator-13', configuration='release', architectures='arm64')
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='ios-simulator-13', configuration='release', architectures='arm64',
                              buildOnly=True, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
        ])

    def test_generic_factory_with_check_relevance(self):
        factory = factories.Factory(platform='ios-simulator-13', configuration='release', architectures='arm64', checkRelevance=True)
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='ios-simulator-13', configuration='release', architectures='arm64',
                              buildOnly=True, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
        ])


class TestTestsFactory(TestCase):
    def setUp(self):
        self.longMessage = True

    def test_style_factory(self):
        factory = factories.StyleFactory(platform='*', configuration=None, architectures=None)
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='*', configuration=None, architectures=None,
                              buildOnly=False, triggers=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.UpdateWorkingDirectory),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.CheckStyle),
        ])

    def test_watchlist_factory(self):
        factory = factories.WatchListFactory(platform='*', configuration=None, architectures=None)
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='*', configuration=None, architectures=None,
                              buildOnly=False, triggers=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.UpdateWorkingDirectory),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.ApplyWatchList),
        ])

    def test_bindings_factory(self):
        factory = factories.BindingsFactory(platform='*', configuration=None, architectures=None)
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='*', configuration=None, architectures=None,
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.RunBindingsTests),
        ])

    def test_webkitperl_factory(self):
        factory = factories.WebKitPerlFactory(platform='*', configuration=None, architectures=None)
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='*', configuration=None, architectures=None,
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.RunWebKitPerlTests),
        ])

    def test_webkitpy_factory(self):
        factory = factories.WebKitPyFactory(platform='*', configuration=None, architectures=None)
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='*', configuration=None, architectures=None,
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.RunWebKitPyPython2Tests),
            _BuildStepFactory(steps.RunWebKitPyPython3Tests),
            _BuildStepFactory(steps.SetBuildSummary),
        ])

    def test_services_factory(self):
        factory = factories.ServicesFactory(platform='*', configuration=None, architectures=None)
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='*', configuration=None, architectures=None,
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.RunBuildWebKitOrgUnitTests),
            _BuildStepFactory(steps.RunEWSUnitTests),
            _BuildStepFactory(steps.RunEWSBuildbotCheckConfig),
            _BuildStepFactory(steps.RunResultsdbpyTests),
        ])


class TestBuildFactory(TestCase):
    def test_generic_build_factory(self):
        factory = factories.BuildFactory(platform='mac-catalina', configuration='release', architectures=["x86_64"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='mac-catalina', configuration='release', architectures=["x86_64"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileWebKit, skipUpload=False),
        ])

    def test_macos_build_factory(self):
        factory = factories.macOSBuildFactory(platform='mac-catalina', configuration='release', architectures=["x86_64"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='mac-catalina', configuration='release', architectures=["x86_64"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileWebKit, skipUpload=False),
        ])

    def test_macos_build_only_factory(self):
        factory = factories.macOSBuildOnlyFactory(platform='mac-bigsur', configuration='release', architectures=["arm64"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='mac-bigsur', configuration='release', architectures=["arm64"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileWebKit, skipUpload=True),
        ])

    def test_ios_build_factory(self):
        factory = factories.iOSBuildFactory(platform='ios-simulator-13', configuration='release', architectures=["x86_64"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='ios-simulator-13', configuration='release', architectures=["x86_64"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileWebKit, skipUpload=False),
        ])

    def test_ios_embedded_build_factory(self):
        factory = factories.iOSEmbeddedBuildFactory(platform='ios-13', configuration='release', architectures=["arm64"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='ios-13', configuration='release', architectures=["arm64"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileWebKit, skipUpload=True),
        ])

    def test_watchos_build_factory(self):
        factory = factories.watchOSBuildFactory(platform='watchos-6', configuration='release', architectures=["arm64_32"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='watchos-6', configuration='release', architectures=["arm64_32"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileWebKit, skipUpload=True),
        ])

    def test_tvos_build_factory(self):
        factory = factories.tvOSBuildFactory(platform='tvos-13', configuration='release', architectures=["arm64"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='tvos-13', configuration='release', architectures=["arm64"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileWebKit, skipUpload=True),
        ])

    def test_gtk_build_factory(self):
        factory = factories.GTKBuildFactory(platform='gtk', configuration='release', architectures=['x86_64'])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='gtk', configuration='release', architectures=['x86_64'],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.InstallGtkDependencies),
            _BuildStepFactory(steps.CompileWebKit, skipUpload=False),
        ])

    def test_wpe_factory(self):
        factory = factories.WPEFactory(platform='wpe', configuration='release', architectures=['x86_64'])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='wpe', configuration='release', architectures=['x86_64'],
                              buildOnly=True, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.InstallWpeDependencies),
            _BuildStepFactory(steps.CompileWebKit, skipUpload=True),
        ])

    def test_wincairo_factory(self):
        factory = factories.WinCairoFactory(platform='wincairo', configuration='release', architectures=['x86_64'])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='wincairo', configuration='release', architectures=['x86_64'],
                              buildOnly=True, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileWebKit, skipUpload=True),
        ])

    def test_jsc_mipsel_factory(self):
        factory = factories.JSCBuildFactory(platform='jsc', configuration='release', architectures=["mipsel"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='jsc', configuration='release', architectures=["mipsel"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileJSC),
        ])

    def test_jsc_armv7_factory(self):
        factory = factories.JSCBuildFactory(platform='jsc', configuration='release', architectures=["armv7"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='jsc', configuration='release', architectures=["armv7"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileJSC),
        ])


class TestBuildAndTestsFactory(TestCase):
    def test_windows_factory(self):
        factory = factories.WindowsFactory(platform='win', configuration='release', architectures=["x86_64"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='win', configuration='release', architectures=["x86_64"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileWebKit, skipUpload=True),
            _BuildStepFactory(steps.ValidatePatch, verifyBugClosed=False, addURLs=False),
            _BuildStepFactory(steps.RunWebKit1Tests),
            _BuildStepFactory(steps.SetBuildSummary),
        ])


class TestCommitQueueFactory(TestCase):
    def test_commit_queue_factory(self):
        factory = factories.CommitQueueFactory(platform='mac-catalina', configuration='release', architectures=["x86_64"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='mac-catalina', configuration='release', architectures=["x86_64"],
                              buildOnly=False, triggers=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch, verifycqplus=True),
            _BuildStepFactory(steps.ValidateCommiterAndReviewer),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.UpdateWorkingDirectory),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.ValidateChangeLogAndReviewer),
            _BuildStepFactory(steps.FindModifiedChangeLogs),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileWebKit, skipUpload=True),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.ValidatePatch, addURLs=False, verifycqplus=True),
            _BuildStepFactory(steps.CheckPatchStatusOnEWSQueues),
            _BuildStepFactory(steps.RunWebKitTests),
            _BuildStepFactory(steps.ValidatePatch, addURLs=False, verifycqplus=True),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.UpdateWorkingDirectory),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.CreateLocalGITCommit),
            _BuildStepFactory(steps.PushCommitToWebKitRepo),
            _BuildStepFactory(steps.SetBuildSummary),
        ])


class TestLayoutTestsFactory(TestCase):
    def test_macos_wk1_release_factory(self):
        factory = factories.macOSWK1Factory(platform='mac-catalina', configuration='release', architectures=["x86_64"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='mac-catalina', configuration='release', architectures=["x86_64"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.DownloadBuiltProduct),
            _BuildStepFactory(steps.ExtractBuiltProduct),
            _BuildStepFactory(steps.WaitForCrashCollection),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.RunWebKit1Tests),
            _BuildStepFactory(steps.TriggerCrashLogSubmission),
            _BuildStepFactory(steps.SetBuildSummary),
        ])

    def test_macos_wk1_debug_factory(self):
        factory = factories.macOSWK1Factory(platform='mac-catalina', configuration='debug', architectures=["x86_64"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='mac-catalina', configuration='debug', architectures=["x86_64"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.DownloadBuiltProduct),
            _BuildStepFactory(steps.ExtractBuiltProduct),
            _BuildStepFactory(steps.WaitForCrashCollection),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.RunWebKit1Tests),
            _BuildStepFactory(steps.TriggerCrashLogSubmission),
            _BuildStepFactory(steps.SetBuildSummary),
        ])

    def test_macos_wk2_factory(self):
        factory = factories.macOSWK2Factory(platform='mac-catalina', configuration='release', architectures=["x86_64"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='mac-catalina', configuration='release', architectures=["x86_64"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.DownloadBuiltProduct),
            _BuildStepFactory(steps.ExtractBuiltProduct),
            _BuildStepFactory(steps.WaitForCrashCollection),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.RunWebKitTests),
            _BuildStepFactory(steps.TriggerCrashLogSubmission),
            _BuildStepFactory(steps.SetBuildSummary),
        ])

    def test_ios_wk2_factory(self):
        factory = factories.iOSTestsFactory(platform='ios-simulator-13', configuration='release', architectures=["x86_64"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='ios-simulator-13', configuration='release', architectures=["x86_64"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.DownloadBuiltProduct),
            _BuildStepFactory(steps.ExtractBuiltProduct),
            _BuildStepFactory(steps.WaitForCrashCollection),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.RunWebKitTests),
            _BuildStepFactory(steps.TriggerCrashLogSubmission),
            _BuildStepFactory(steps.SetBuildSummary),
        ])

    def test_gtk_factory(self):
        factory = factories.GTKTestsFactory(platform='gtk', configuration='release', architectures=["x86_64"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='gtk', configuration='release', architectures=["x86_64"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.InstallGtkDependencies),
            _BuildStepFactory(steps.DownloadBuiltProduct),
            _BuildStepFactory(steps.ExtractBuiltProduct),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.RunWebKitTests),
            _BuildStepFactory(steps.SetBuildSummary),
        ])


class TestJSCBuildFactory(TestCase):
    def test_jsc_mipsel_factory(self):
        factory = factories.JSCBuildFactory(platform='jsc', configuration='release', architectures=["mipsel"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='jsc', configuration='release', architectures=["mipsel"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileJSC),
        ])

    def test_jsc_armv7_factory(self):
        factory = factories.JSCBuildFactory(platform='jsc', configuration='release', architectures=["armv7"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='jsc', configuration='release', architectures=["armv7"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileJSC),
        ])


class TestJSCBuildAndTestsFactory(TestCase):
    def test_jsc_mipsel_factory(self):
        factory = factories.JSCBuildFactory(platform='jsc', configuration='release', architectures=["mipsel"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='jsc', configuration='release', architectures=["mipsel"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileJSC),
        ])

    def test_jsc_armv7_factory(self):
        factory = factories.JSCBuildFactory(platform='jsc', configuration='release', architectures=["armv7"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='jsc', configuration='release', architectures=["armv7"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.CompileJSC),
        ])


class TestJSCTestsFactory(TestCase):
    def test_jsc_mipsel_factory(self):
        factory = factories.JSCTestsFactory(platform='jsc', configuration='release', architectures=["mipsel"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='jsc', configuration='release', architectures=["mipsel"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.DownloadBuiltProduct),
            _BuildStepFactory(steps.ExtractBuiltProduct),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.RunJavaScriptCoreTests),
        ])

    def test_jsc_armv7_factory(self):
        factory = factories.JSCTestsFactory(platform='jsc', configuration='release', architectures=["armv7"])
        self.assertBuildSteps(factory.steps, [
            _BuildStepFactory(steps.ConfigureBuild, platform='jsc', configuration='release', architectures=["armv7"],
                              buildOnly=False, triggers=None, triggered_by=None, remotes=None, additionalArguments=None),
            _BuildStepFactory(steps.CheckPatchRelevance),
            _BuildStepFactory(steps.ValidatePatch),
            _BuildStepFactory(steps.PrintConfiguration),
            _BuildStepFactory(steps.CheckOutSource),
            _BuildStepFactory(steps.CheckOutSpecificRevision),
            _BuildStepFactory(steps.FetchBranches),
            _BuildStepFactory(steps.ShowIdentifier),
            _BuildStepFactory(steps.ApplyPatch),
            _BuildStepFactory(steps.DownloadBuiltProduct),
            _BuildStepFactory(steps.ExtractBuiltProduct),
            _BuildStepFactory(steps.KillOldProcesses),
            _BuildStepFactory(steps.RunJavaScriptCoreTests),
        ])
