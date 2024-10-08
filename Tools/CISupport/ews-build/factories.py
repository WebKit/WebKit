# Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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

from .steps import (AddReviewerToCommitMessage, ApplyPatch, ApplyWatchList, Canonicalize,
                    CheckOutPullRequest, CheckOutSource, CheckOutSpecificRevision, CheckChangeRelevance, CheckOutLLVMProject,
                    CheckStatusOnEWSQueues, CheckStyle, CleanGitRepo, CleanDerivedSources, CompileJSC, CompileWebKit, ConfigureBuild, DetermineLabelOwner,
                    DownloadBuiltProduct, ExtractBuiltProduct, FetchBranches, FindModifiedLayoutTests, GetTestExpectationsBaseline, GetUpdatedTestExpectations, GitHub, InstallCMake, InstallNinja,
                    InstallGtkDependencies, InstallHooks, InstallWpeDependencies, InstallWinDependencies, KillOldProcesses, PrintClangVersion, PrintConfiguration, PushCommitToWebKitRepo, PushPullRequestBranch,
                    MapBranchAlias, RemoveAndAddLabels, RetrievePRDataFromLabel, RunAPITests, RunBindingsTests, RunBuildWebKitOrgUnitTests, RunBuildbotCheckConfigForBuildWebKit, RunBuildbotCheckConfigForEWS,
                    RunEWSUnitTests, RunResultsdbpyTests, RunJavaScriptCoreTests, RunWebKit1Tests, RunWebKitPerlTests,
                    RunWebKitPyTests, RunWebKitTests, RunWebKitTestsRedTree, RunWebKitTestsInStressMode, RunWebKitTestsInStressGuardmallocMode,
                    ScanBuild, SetBuildSummary, ShowIdentifier, TriggerCrashLogSubmission, UpdateClang, UpdateWorkingDirectory, UpdatePullRequest,
                    ValidateCommitMessage, ValidateChange, ValidateCommitterAndReviewer, WaitForCrashCollection,
                    InstallBuiltProduct, ValidateRemote, ValidateSquashed, GITHUB_PROJECTS)

class Factory(factory.BuildFactory):
    findModifiedLayoutTests = False
    skipBuildIfNoResult = True
    branches = None

    def __init__(self, platform, configuration=None, architectures=None, buildOnly=True, triggers=None, triggered_by=None, remotes=None, additionalArguments=None, checkRelevance=False, **kwargs):
        factory.BuildFactory.__init__(self)
        self.addStep(ConfigureBuild(platform=platform, configuration=configuration, architectures=architectures, buildOnly=buildOnly, triggers=triggers, triggered_by=triggered_by, remotes=remotes, additionalArguments=additionalArguments))
        if checkRelevance:
            self.addStep(CheckChangeRelevance())
        self.addStep(ValidateChange(branches=self.branches))
        self.addStep(PrintConfiguration())
        self.addStep(CleanGitRepo())
        self.addStep(CheckOutSource())
        self.addStep(FetchBranches())
        # CheckOutSource step pulls the latest revision, since we use alwaysUseLatest=True. Without alwaysUseLatest Buildbot will
        # automatically apply the patch to the repo, and that doesn't handle ChangeLogs well. See https://webkit.org/b/193138
        # Therefore we add CheckOutSpecificRevision step to checkout required revision.
        self.addStep(CheckOutSpecificRevision())
        if self.findModifiedLayoutTests:
            self.addStep(GetTestExpectationsBaseline())
        self.addStep(ShowIdentifier())
        self.addStep(ApplyPatch())
        self.addStep(CheckOutPullRequest())
        if self.findModifiedLayoutTests:
            self.addStep(GetUpdatedTestExpectations())
            self.addStep(FindModifiedLayoutTests(skipBuildIfNoResult=self.skipBuildIfNoResult))


class StyleFactory(factory.BuildFactory):
    def __init__(self, platform, configuration=None, architectures=None, triggers=None, remotes=None, additionalArguments=None, **kwargs):
        factory.BuildFactory.__init__(self)
        self.addStep(ConfigureBuild(platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, triggers=triggers, remotes=remotes, additionalArguments=additionalArguments))
        self.addStep(ValidateChange())
        self.addStep(PrintConfiguration())
        self.addStep(CleanGitRepo())
        self.addStep(CheckOutSource())
        self.addStep(FetchBranches())
        self.addStep(UpdateWorkingDirectory())
        self.addStep(ShowIdentifier())
        self.addStep(ApplyPatch())
        self.addStep(CheckOutPullRequest())
        self.addStep(CheckStyle())


class WatchListFactory(factory.BuildFactory):
    def __init__(self, platform, configuration=None, architectures=None, triggers=None, remotes=None, additionalArguments=None, **kwargs):
        factory.BuildFactory.__init__(self)
        self.addStep(ConfigureBuild(platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, triggers=triggers, remotes=remotes, additionalArguments=additionalArguments))
        self.addStep(ValidateChange())
        self.addStep(PrintConfiguration())
        self.addStep(CleanGitRepo())
        self.addStep(CheckOutSource())
        self.addStep(FetchBranches())
        self.addStep(UpdateWorkingDirectory())
        self.addStep(ShowIdentifier())
        self.addStep(ApplyPatch())
        self.addStep(CheckOutPullRequest())
        self.addStep(ApplyWatchList())


class SaferCPPStaticAnalyzerFactory(factory.BuildFactory):
    findModifiedLayoutTests = False

    def __init__(self, platform, configuration=None, architectures=None, buildOnly=True, triggers=None, triggered_by=None, remotes=None, additionalArguments=None, checkRelevance=False, **kwargs):
        factory.BuildFactory.__init__(self)
        self.addStep(ConfigureBuild(platform=platform, configuration=configuration, architectures=architectures, buildOnly=buildOnly, triggers=triggers, triggered_by=triggered_by, remotes=remotes, additionalArguments=additionalArguments))
        self.addStep(CheckChangeRelevance())
        self.addStep(ValidateChange())
        self.addStep(PrintConfiguration())
        self.addStep(CleanGitRepo())
        self.addStep(CheckOutSource())
        self.addStep(FetchBranches())
        self.addStep(ShowIdentifier())
        self.addStep(InstallCMake())
        self.addStep(InstallNinja())
        self.addStep(PrintClangVersion())
        self.addStep(CheckOutLLVMProject())
        self.addStep(UpdateClang())
        self.addStep(CheckOutPullRequest())
        self.addStep(KillOldProcesses())
        self.addStep(ValidateChange(addURLs=False))
        self.addStep(ScanBuild())


class BindingsFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, additionalArguments=additionalArguments, checkRelevance=True)
        self.addStep(ValidateChange(addURLs=False))
        self.addStep(RunBindingsTests())


class WebKitPerlFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, additionalArguments=additionalArguments)
        self.addStep(ValidateChange(addURLs=False))
        self.addStep(RunWebKitPerlTests())


class WebKitPyFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, additionalArgument=additionalArguments, checkRelevance=True)
        self.addStep(ValidateChange(addURLs=False))
        self.addStep(RunWebKitPyTests())
        self.addStep(SetBuildSummary())


class BuildFactory(Factory):
    skipUpload = False

    def __init__(self, platform, configuration=None, architectures=None, triggers=None, additionalArguments=None, checkRelevance=False, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, triggers=triggers, additionalArguments=additionalArguments, checkRelevance=checkRelevance)
        self.addStep(KillOldProcesses())
        if platform == 'gtk':
            self.addStep(InstallGtkDependencies())
        elif platform == 'wpe':
            self.addStep(InstallWpeDependencies())
        self.addStep(ValidateChange(addURLs=False))
        if platform in ['gtk', 'wpe']:
            self.addStep(CleanDerivedSources())
        self.addStep(CompileWebKit(skipUpload=self.skipUpload))
        if platform == 'gtk':
            self.addStep(InstallBuiltProduct())


class TestFactory(Factory):
    LayoutTestClass = None
    APITestClass = None
    willTriggerCrashLogSubmission = False
    skipBuildIfNoResult = False

    def getProduct(self):
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())

    def __init__(self, platform, configuration=None, architectures=None, triggered_by=None, additionalArguments=None, checkRelevance=False, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, triggered_by=triggered_by, additionalArguments=additionalArguments, checkRelevance=checkRelevance)
        if platform == 'gtk':
            self.addStep(InstallGtkDependencies())
        elif platform == 'wpe':
            self.addStep(InstallWpeDependencies())
        elif platform == 'win':
            self.addStep(InstallWinDependencies())
        self.getProduct()
        if self.willTriggerCrashLogSubmission:
            self.addStep(WaitForCrashCollection())
        self.addStep(KillOldProcesses())
        if self.LayoutTestClass:
            self.addStep(RunWebKitTestsInStressMode(num_iterations=10, layout_test_class=self.LayoutTestClass))
            self.addStep(self.LayoutTestClass())
        if self.APITestClass:
            self.addStep(self.APITestClass())
        if self.willTriggerCrashLogSubmission:
            self.addStep(TriggerCrashLogSubmission())
        if self.LayoutTestClass or self.APITestClass:
            self.addStep(SetBuildSummary())


class StressTestFactory(TestFactory):
    findModifiedLayoutTests = True

    def __init__(self, platform, configuration=None, architectures=None, triggered_by=None, additionalArguments=None, checkRelevance=False, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, triggered_by=triggered_by, additionalArguments=additionalArguments, checkRelevance=checkRelevance)
        self.getProduct()
        self.addStep(WaitForCrashCollection())
        self.addStep(KillOldProcesses())
        self.addStep(RunWebKitTestsInStressMode())
        self.addStep(TriggerCrashLogSubmission())
        self.addStep(SetBuildSummary())


class JSCBuildFactory(Factory):
    def __init__(self, platform, configuration='release', architectures=None, triggers=None, remotes=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, triggers=triggers, remotes=remotes, additionalArguments=additionalArguments, checkRelevance=True)
        self.addStep(KillOldProcesses())
        self.addStep(ValidateChange(addURLs=False))
        self.addStep(CompileJSC())


class JSCBuildAndTestsFactory(Factory):
    def __init__(self, platform, configuration='release', architectures=None, remotes=None, additionalArguments=None, runTests='true', **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, remotes=remotes, additionalArguments=additionalArguments, checkRelevance=True)
        self.addStep(KillOldProcesses())
        self.addStep(ValidateChange(addURLs=False))
        self.addStep(CompileJSC(skipUpload=True))
        if runTests.lower() == 'true':
            self.addStep(RunJavaScriptCoreTests())


class JSCTestsFactory(Factory):
    def __init__(self, platform, configuration='release', architectures=None, remotes=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, remotes=remotes, additionalArguments=additionalArguments, checkRelevance=True)
        self.addStep(DownloadBuiltProduct())
        self.addStep(ExtractBuiltProduct())
        self.addStep(KillOldProcesses())
        self.addStep(RunJavaScriptCoreTests())


class APITestsFactory(TestFactory):
    APITestClass = RunAPITests


class iOSBuildFactory(BuildFactory):
    pass


class iOSEmbeddedBuildFactory(BuildFactory):
    skipUpload = True


class iOSTestsFactory(TestFactory):
    LayoutTestClass = RunWebKitTests
    findModifiedLayoutTests = True
    willTriggerCrashLogSubmission = True


class visionOSBuildFactory(BuildFactory):
    pass


class visionOSEmbeddedBuildFactory(BuildFactory):
    skipUpload = True


class visionOSTestsFactory(TestFactory):
    LayoutTestClass = RunWebKitTests
    findModifiedLayoutTests = True
    willTriggerCrashLogSubmission = True


class macOSBuildFactory(BuildFactory):
    pass


class macOSBuildOnlyFactory(BuildFactory):
    skipUpload = True

    def __init__(self, platform, configuration=None, architectures=None, triggers=None, additionalArguments=None, checkRelevance=True, **kwargs):
        super().__init__(platform=platform, configuration=configuration, architectures=architectures, triggers=triggers, additionalArguments=additionalArguments, checkRelevance=checkRelevance, **kwargs)


class watchOSBuildFactory(BuildFactory):
    skipUpload = True


class tvOSBuildFactory(BuildFactory):
    skipUpload = True


class macOSWK1Factory(TestFactory):
    LayoutTestClass = RunWebKit1Tests
    findModifiedLayoutTests = True
    willTriggerCrashLogSubmission = True

    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, checkRelevance=False, **kwargs):
        super(macOSWK1Factory, self).__init__(platform=platform, configuration=configuration, architectures=architectures, additionalArguments=additionalArguments, checkRelevance=True, **kwargs)


class macOSWK2Factory(TestFactory):
    LayoutTestClass = RunWebKitTests
    findModifiedLayoutTests = True
    willTriggerCrashLogSubmission = True


class WinBuildFactory(BuildFactory):
    branches = [r'main']


class WinTestsFactory(TestFactory):
    LayoutTestClass = RunWebKitTests


class GTKBuildFactory(BuildFactory):
    branches = [r'main', r'webkit.+']


class GTKTestsFactory(TestFactory):
    LayoutTestClass = RunWebKitTestsRedTree
    findModifiedLayoutTests = True


class WPEBuildFactory(BuildFactory):
    branches = [r'main', r'webkit.+']


class WPECairoBuildFactory(WPEBuildFactory):
    skipUpload = True


class WPETestsFactory(TestFactory):
    LayoutTestClass = RunWebKitTestsRedTree
    findModifiedLayoutTests = True


class ServicesFactory(Factory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        Factory.__init__(self, platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, additionalArguments=additionalArguments, checkRelevance=True)
        self.addStep(ValidateChange(verifyBugClosed=False, addURLs=False))
        self.addStep(RunBuildWebKitOrgUnitTests())
        self.addStep(RunBuildbotCheckConfigForBuildWebKit())
        self.addStep(RunEWSUnitTests())
        self.addStep(RunBuildbotCheckConfigForEWS())
        self.addStep(RunResultsdbpyTests())


class CommitQueueFactory(factory.BuildFactory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        factory.BuildFactory.__init__(self)
        self.addStep(ConfigureBuild(platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, triggers=None, remotes=None, additionalArguments=additionalArguments))
        self.addStep(ValidateChange(verifycqplus=True))
        self.addStep(ValidateCommitterAndReviewer())
        self.addStep(PrintConfiguration())
        self.addStep(CleanGitRepo())
        self.addStep(CheckOutSource())
        self.addStep(FetchBranches())
        self.addStep(UpdateWorkingDirectory())
        self.addStep(ShowIdentifier())
        self.addStep(InstallHooks())
        self.addStep(ApplyPatch())

        self.addStep(ValidateSquashed())
        self.addStep(AddReviewerToCommitMessage())
        self.addStep(ValidateCommitMessage())

        self.addStep(KillOldProcesses())
        self.addStep(CompileWebKit(skipUpload=True))
        self.addStep(KillOldProcesses())

        self.addStep(ValidateChange(addURLs=False, verifycqplus=True))
        self.addStep(CheckStatusOnEWSQueues())
        self.addStep(RunWebKitTests())
        self.addStep(ValidateChange(addURLs=False, verifycqplus=True))

        self.addStep(Canonicalize())
        self.addStep(PushCommitToWebKitRepo())
        self.addStep(SetBuildSummary())


class MergeQueueFactoryBase(factory.BuildFactory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        super(MergeQueueFactoryBase, self).__init__()
        self.addStep(ConfigureBuild(platform=platform, configuration=configuration, architectures=architectures, buildOnly=False, triggers=None, remotes=None, additionalArguments=additionalArguments))
        self.addStep(ValidateChange(verifyMergeQueue=True, verifyNoDraftForMergeQueue=True, enableSkipEWSLabel=False))
        self.addStep(DetermineLabelOwner())
        self.addStep(ValidateCommitterAndReviewer())
        self.addStep(PrintConfiguration())
        self.addStep(CleanGitRepo())
        self.addStep(CheckOutSource())
        self.addStep(FetchBranches())
        self.addStep(MapBranchAlias())
        self.addStep(UpdateWorkingDirectory())
        self.addStep(ShowIdentifier())
        self.addStep(InstallHooks())
        self.addStep(CheckOutPullRequest())
        self.addStep(ValidateRemote())
        self.addStep(ValidateSquashed())
        self.addStep(AddReviewerToCommitMessage())
        self.addStep(ValidateCommitMessage())


class MergeQueueFactory(MergeQueueFactoryBase):
    def __init__(self, platform, **kwargs):
        super(MergeQueueFactory, self).__init__(platform, **kwargs)

        self.addStep(KillOldProcesses())
        self.addStep(CompileWebKit(skipUpload=True))
        self.addStep(KillOldProcesses())

        self.addStep(ValidateChange(verifyMergeQueue=True, verifyNoDraftForMergeQueue=True, enableSkipEWSLabel=False))
        self.addStep(CheckStatusOnEWSQueues())
        self.addStep(RunWebKitTests())

        self.addStep(ValidateChange(verifyMergeQueue=True, verifyNoDraftForMergeQueue=True, enableSkipEWSLabel=False))
        self.addStep(Canonicalize())
        self.addStep(PushPullRequestBranch())
        self.addStep(UpdatePullRequest())
        self.addStep(PushCommitToWebKitRepo())
        self.addStep(SetBuildSummary())


class UnsafeMergeQueueFactory(MergeQueueFactoryBase):
    def __init__(self, platform, **kwargs):
        super(UnsafeMergeQueueFactory, self).__init__(platform, **kwargs)

        self.addStep(Canonicalize())
        self.addStep(ValidateChange(verifyMergeQueue=True, verifyNoDraftForMergeQueue=True, enableSkipEWSLabel=False))
        self.addStep(PushPullRequestBranch())
        self.addStep(UpdatePullRequest())
        self.addStep(PushCommitToWebKitRepo())
        self.addStep(SetBuildSummary())


class SafeMergeQueueFactory(factory.BuildFactory):
    def __init__(self, platform, configuration=None, architectures=None, additionalArguments=None, **kwargs):
        super().__init__()
        for project in GITHUB_PROJECTS:
            self.addStep(RetrievePRDataFromLabel(project, label=GitHub.SAFE_MERGE_QUEUE_LABEL))
