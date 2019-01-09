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

from buildbot.process import buildstep, logobserver, properties
from buildbot.process.results import Results, SUCCESS, FAILURE, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.steps import master, shell, transfer
from buildbot.steps.source import git
from buildbot.steps.worker import CompositeStepMixin
from twisted.internet import defer

import re
import requests

BUG_SERVER_URL = 'https://bugs.webkit.org/'
EWS_URL = 'http://ews-build.webkit-uat.org/'
WithProperties = properties.WithProperties
Interpolate = properties.Interpolate


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
        if self.platform and self.platform != '*':
            self.setProperty('platform', self.platform, 'config.json')
        if self.fullPlatform and self.fullPlatform != '*':
            self.setProperty('fullPlatform', self.fullPlatform, 'ConfigureBuild')
        if self.configuration:
            self.setProperty('configuration', self.configuration, 'config.json')
        if self.architecture:
            self.setProperty('architecture', self.architecture, 'config.json')
        if self.buildOnly:
            self.setProperty("buildOnly", self.buildOnly, 'config.json')
        if self.additionalArguments:
            self.setProperty("additionalArguments", self.additionalArguments, 'config.json')

        self.add_patch_id_url()
        self.add_bug_id_url()
        self.finished(SUCCESS)
        return defer.succeed(None)

    def add_patch_id_url(self):
        patch_id = self.getProperty('patch_id', '')
        if patch_id:
            self.addURL('Patch {}'.format(patch_id), self.getPatchURL(patch_id))

    def add_bug_id_url(self):
        bug_id = self.getProperty('bug_id', '')
        if bug_id:
            self.addURL('Bug {}'.format(bug_id), self.getBugURL(bug_id))

    def getPatchURL(self, patch_id):
        if not patch_id:
            return None
        return '{}attachment.cgi?id={}'.format(BUG_SERVER_URL, patch_id)

    def getBugURL(self, bug_id):
        if not bug_id:
            return None
        return '{}show_bug.cgi?id={}'.format(BUG_SERVER_URL, bug_id)


class CheckOutSource(git.Git):
    name = 'clean-and-update-working-directory'
    CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR = (0, 2)

    def __init__(self, **kwargs):
        self.repourl = 'https://git.webkit.org/git/WebKit.git'
        super(CheckOutSource, self).__init__(repourl=self.repourl,
                                                retry=self.CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR,
                                                timeout=2 * 60 * 60,
                                                alwaysUseLatest=True,
                                                progress=True,
                                                **kwargs)


class ApplyPatch(shell.ShellCommand, CompositeStepMixin):
    name = 'apply-patch'
    description = ['applying-patch']
    descriptionDone = ['apply-patch']
    flunkOnFailure = True
    haltOnFailure = True
    command = ['Tools/Scripts/svn-apply', '--force', '.buildbot-diff']

    def _get_patch(self):
        sourcestamp = self.build.getSourceStamp(self.getProperty('codebase', ''))
        if not sourcestamp or not sourcestamp.patch:
            return None
        return sourcestamp.patch[1]

    def start(self):
        patch = self._get_patch()
        if not patch:
            self.finished(FAILURE)
            return None

        d = self.downloadFileContentToWorker('.buildbot-diff', patch)
        d.addCallback(lambda _: self.downloadFileContentToWorker('.buildbot-patched', 'patched\n'))
        d.addCallback(lambda res: shell.ShellCommand.start(self))


class CheckPatchRelevance(buildstep.BuildStep):
    name = 'check-patch-relevance'
    description = ['check-patch-relevance running']
    descriptionDone = ['check-patch-relevance']
    flunkOnFailure = True
    haltOnFailure = True

    bindings_paths = [
        "Source/WebCore",
        "Tools",
    ]

    jsc_paths = [
        "JSTests/",
        "Source/JavaScriptCore/",
        "Source/WTF/",
        "Source/bmalloc/",
        "Makefile",
        "Makefile.shared",
        "Source/Makefile",
        "Source/Makefile.shared",
        "Tools/Scripts/build-webkit",
        "Tools/Scripts/build-jsc",
        "Tools/Scripts/jsc-stress-test-helpers/",
        "Tools/Scripts/run-jsc",
        "Tools/Scripts/run-jsc-benchmarks",
        "Tools/Scripts/run-jsc-stress-tests",
        "Tools/Scripts/run-javascriptcore-tests",
        "Tools/Scripts/run-layout-jsc",
        "Tools/Scripts/update-javascriptcore-test-results",
        "Tools/Scripts/webkitdirs.pm",
    ]

    webkitpy_paths = [
        "Tools/Scripts/webkitpy/",
        "Tools/QueueStatusServer/",
    ]

    group_to_paths_mapping = {
        'bindings': bindings_paths,
        'jsc': jsc_paths,
        'webkitpy': webkitpy_paths,
    }

    def _patch_is_relevant(self, patch, builderName):
        group = [group for group in self.group_to_paths_mapping.keys() if group in builderName.lower()]
        if not group:
            # This builder doesn't have paths defined, all patches are relevant.
            return True

        relevant_paths = self.group_to_paths_mapping[group[0]]

        for change in patch.splitlines():
            for path in relevant_paths:
                if re.search(path, change, re.IGNORECASE):
                    return True
        return False

    def _get_patch(self):
        sourcestamp = self.build.getSourceStamp(self.getProperty('codebase', ''))
        if not sourcestamp or not sourcestamp.patch:
            return None
        return sourcestamp.patch[1]

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)

    def start(self):
        patch = self._get_patch()
        if not patch:
            # This build doesn't have a patch, it might be a force build.
            self.finished(SUCCESS)
            return None

        if self._patch_is_relevant(patch, self.getProperty('buildername', '')):
            self._addToLog('stdio', 'This patch contains relevant changes.')
            self.finished(SUCCESS)
            return None

        self._addToLog('stdio', 'This patch does not have relevant changes.')
        self.finished(FAILURE)
        self.build.results = SKIPPED
        self.build.buildFinished(['Patch {} doesn\'t have relevant changes'.format(self.getProperty('patch_id', ''))], SKIPPED)
        return None


class ValidatePatch(buildstep.BuildStep):
    name = 'validate-patch'
    description = ['validate-patch running']
    descriptionDone = ['validate-patch']
    flunkOnFailure = True
    haltOnFailure = True
    bug_open_statuses = ["UNCONFIRMED", "NEW", "ASSIGNED", "REOPENED"]
    bug_closed_statuses = ["RESOLVED", "VERIFIED", "CLOSED"]

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)

    def fetch_data_from_url(self, url):
        response = None
        try:
            response = requests.get(url)
        except Exception as e:
            if response:
                self._addToLog('stdio', 'Failed to access {url} with status code {status_code}.\n'.format(url=url, status_code=response.status_code))
            else:
                self._addToLog('stdio', 'Failed to access {url} with exception: {exception}\n'.format(url=url, exception=e))
            return None
        if response.status_code != 200:
            self._addToLog('stdio', 'Accessed {url} with unexpected status code {status_code}.\n'.format(url=url, status_code=response.status_code))
            return None
        return response

    def get_patch_json(self, patch_id):
        patch_url = '{}rest/bug/attachment/{}'.format(BUG_SERVER_URL, patch_id)
        patch = self.fetch_data_from_url(patch_url)
        if not patch:
            return None
        patch_json = patch.json().get('attachments')
        if not patch_json or len(patch_json) == 0:
            return None
        return patch_json.get(str(patch_id))

    def get_bug_json(self, bug_id):
        bug_url = '{}rest/bug/{}'.format(BUG_SERVER_URL, bug_id)
        bug = self.fetch_data_from_url(bug_url)
        if not bug:
            return None
        bugs_json = bug.json().get('bugs')
        if not bugs_json or len(bugs_json) == 0:
            return None
        return bugs_json[0]

    def get_bug_id_from_patch(self, patch_id):
        patch_json = self.get_patch_json(patch_id)
        if not patch_json:
            self._addToLog('stdio', 'Unable to fetch patch {}.\n'.format(patch_id))
            return -1
        return patch_json.get('bug_id')

    def _is_patch_obsolete(self, patch_id):
        patch_json = self.get_patch_json(patch_id)
        if not patch_json:
            self._addToLog('stdio', 'Unable to fetch patch {}.\n'.format(patch_id))
            return -1

        if str(patch_json.get('id')) != self.getProperty('patch_id', ''):
            self._addToLog('stdio', 'Fetched patch id {} does not match with requested patch id {}. Unable to validate.\n'.format(patch_json.get('id'), self.getProperty('patch_id', '')))
            return -1

        return patch_json.get('is_obsolete')

    def _is_patch_review_denied(self, patch_id):
        patch_json = self.get_patch_json(patch_id)
        if not patch_json:
            self._addToLog('stdio', 'Unable to fetch patch {}.\n'.format(patch_id))
            return -1

        for flag in patch_json.get('flags', []):
            if flag.get('name') == 'review' and flag.get('status') == '-':
                return 1
        return 0

    def _is_bug_closed(self, bug_id):
        bug_json = self.get_bug_json(bug_id)
        if not bug_json or not bug_json.get('status'):
            self._addToLog('stdio', 'Unable to fetch bug {}.\n'.format(bug_id))
            return -1

        if bug_json.get('status') in self.bug_closed_statuses:
            return 1
        return 0

    def skip_build(self, reason):
        self._addToLog('stdio', reason)
        self.finished(FAILURE)
        self.build.results = SKIPPED
        self.build.buildFinished([reason], SKIPPED)

    def start(self):
        patch_id = self.getProperty('patch_id', '')
        if not patch_id:
            self._addToLog('stdio', 'No patch_id found. Unable to proceed without patch_id.\n')
            self.finished(FAILURE)
            return None

        bug_id = self.getProperty('bug_id', self.get_bug_id_from_patch(patch_id))

        bug_closed = self._is_bug_closed(bug_id)
        if bug_closed == 1:
            self.skip_build('Bug {} is already closed'.format(bug_id))
            return None

        obsolete = self._is_patch_obsolete(patch_id)
        if obsolete == 1:
            self.skip_build('Patch {} is obsolete'.format(patch_id))
            return None

        review_denied = self._is_patch_review_denied(patch_id)
        if review_denied == 1:
            self.skip_build('Patch {} is marked r-'.format(patch_id))
            return None

        if obsolete == -1 or review_denied == -1 or bug_closed == -1:
            self.finished(WARNINGS)
            return None

        self._addToLog('stdio', 'Bug is open.\nPatch is not obsolete.\nPatch is not marked r-.\n')
        self.finished(SUCCESS)
        return None


class UnApplyPatchIfRequired(CheckOutSource):
    name = 'unapply-patch'

    def doStepIf(self, step):
        return self.getProperty('patchFailedToBuild') or self.getProperty('patchFailedJSCTests')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


class CheckStyle(shell.ShellCommand):
    name = 'check-webkit-style'
    description = ['check-webkit-style running']
    descriptionDone = ['check-webkit-style']
    flunkOnFailure = True
    command = ['Tools/Scripts/check-webkit-style']


class TestWithFailureCount(shell.Test):
    failedTestsFormatString = "%d test%s failed"
    failedTestCount = 0

    def start(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)
        return shell.Test.start(self)

    def countFailures(self, cmd):
        raise NotImplementedError

    def commandComplete(self, cmd):
        shell.Test.commandComplete(self, cmd)
        self.failedTestCount = self.countFailures(cmd)
        self.failedTestPluralSuffix = "" if self.failedTestCount == 1 else "s"

    def evaluateCommand(self, cmd):
        if self.failedTestCount:
            return FAILURE

        if cmd.rc != 0:
            return FAILURE

        return SUCCESS

    def getResultSummary(self):
        status = self.name

        if self.results != SUCCESS and self.failedTestCount:
            status = self.failedTestsFormatString % (self.failedTestCount, self.failedTestPluralSuffix)

        if self.results != SUCCESS:
            status += u' ({})'.format(Results[self.results])

        return {u'step': status}


class RunBindingsTests(shell.ShellCommand):
    name = 'bindings-tests'
    description = ['bindings-tests running']
    descriptionDone = ['bindings-tests']
    flunkOnFailure = True
    jsonFileName = 'bindings_test_results.json'
    logfiles = {'json': jsonFileName}
    command = ['Tools/Scripts/run-bindings-tests', '--json-output={0}'.format(jsonFileName)]


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
    jsonFileName = 'webkitpy_test_results.json'
    logfiles = {'json': jsonFileName}
    command = ['Tools/Scripts/test-webkitpy', '--json-output={0}'.format(jsonFileName)]

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
    haltOnFailure = False
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

    def evaluateCommand(self, cmd):
        if cmd.didFail():
            self.setProperty('patchFailedToBuild', True)

        return super(CompileWebKit, self).evaluateCommand(cmd)


class CompileWebKitToT(CompileWebKit):
    name = 'compile-webkit-tot'
    haltOnFailure = True

    def doStepIf(self, step):
        return self.getProperty('patchFailedToBuild')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


class CompileJSCOnly(CompileWebKit):
    name = "build-jsc"
    command = ["perl", "Tools/Scripts/build-jsc", WithProperties("--%(configuration)s")]


class CompileJSCOnlyToT(CompileJSCOnly):
    name = 'build-jsc-tot'

    def doStepIf(self, step):
        return self.getProperty('patchFailedToBuild')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


class RunJavaScriptCoreTests(shell.Test):
    name = 'jscore-test'
    description = ['jscore-tests running']
    descriptionDone = ['jscore-tests']
    flunkOnFailure = True
    jsonFileName = 'jsc_results.json'
    logfiles = {"json": jsonFileName}
    command = ['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', '--json-output={0}'.format(jsonFileName), WithProperties('--%(configuration)s')]

    def start(self):
        appendCustomBuildFlags(self, self.getProperty('platform'), self.getProperty('fullPlatform'))
        return shell.Test.start(self)

    def evaluateCommand(self, cmd):
        if cmd.didFail():
            self.setProperty('patchFailedJSCTests', True)

        return super(RunJavaScriptCoreTests, self).evaluateCommand(cmd)


class ReRunJavaScriptCoreTests(RunJavaScriptCoreTests):
    name = 'jscore-test-rerun'

    def doStepIf(self, step):
        return self.getProperty('patchFailedJSCTests')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)

    def evaluateCommand(self, cmd):
        self.setProperty('patchFailedJSCTests', cmd.didFail())
        return super(RunJavaScriptCoreTests, self).evaluateCommand(cmd)


class RunJavaScriptCoreTestsToT(RunJavaScriptCoreTests):
    name = 'jscore-test-tot'
    jsonFileName = 'jsc_results.json'
    command = ['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-fail-fast', '--json-output={0}'.format(jsonFileName), WithProperties('--%(configuration)s')]

    def doStepIf(self, step):
        return self.getProperty('patchFailedJSCTests')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


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


class RunWebKitTests(shell.Test):
    name = 'layout-tests'
    description = ['layout-tests running']
    descriptionDone = ['layout-tests']
    resultDirectory = 'layout-test-results'
    command = ['python', 'Tools/Scripts/run-webkit-tests',
               '--no-build',
               '--no-new-test-results',
               '--no-show-results',
               '--exit-after-n-failures', '30',
               '--skip-failing-tests',
               WithProperties('--%(configuration)s')]

    def start(self):
        platform = self.getProperty('platform')
        appendCustomBuildFlags(self, platform, self.getProperty('fullPlatform'))
        additionalArguments = self.getProperty('additionalArguments')

        self.setCommand(self.command + ['--results-directory', self.resultDirectory])
        self.setCommand(self.command + ['--debug-rwt-logging'])

        if additionalArguments:
            self.setCommand(self.command + additionalArguments)
        return shell.Test.start(self)


class RunWebKit1Tests(RunWebKitTests):
    def start(self):
        self.setCommand(self.command + ['--dump-render-tree'])

        return RunWebKitTests.start(self)


class ArchiveBuiltProduct(shell.ShellCommand):
    command = ['python', 'Tools/BuildSlaveSupport/built-product-archive',
               WithProperties('--platform=%(fullPlatform)s'), WithProperties('--%(configuration)s'), 'archive']
    name = 'archive-built-product'
    description = ['archiving built product']
    descriptionDone = ['archived built product']
    haltOnFailure = True


class UploadBuiltProduct(transfer.FileUpload):
    name = 'upload-built-product'
    workersrc = WithProperties('WebKitBuild/%(configuration)s.zip')
    masterdest = WithProperties('public_html/archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(patch_id)s.zip')
    haltOnFailure = True

    def __init__(self, **kwargs):
        kwargs['workersrc'] = self.workersrc
        kwargs['masterdest'] = self.masterdest
        kwargs['mode'] = 0644
        kwargs['blocksize'] = 1024 * 256
        transfer.FileUpload.__init__(self, **kwargs)


class DownloadBuiltProduct(shell.ShellCommand):
    command = ['python', 'Tools/BuildSlaveSupport/download-built-product',
        WithProperties('--platform=%(platform)s'), WithProperties('--%(configuration)s'),
        WithProperties(EWS_URL + 'archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(patch_id)s.zip')]
    name = 'download-built-product'
    description = ['downloading built product']
    descriptionDone = ['downloaded built product']
    haltOnFailure = True
    flunkOnFailure = True


class ExtractBuiltProduct(shell.ShellCommand):
    command = ['python', 'Tools/BuildSlaveSupport/built-product-archive',
               WithProperties('--platform=%(fullPlatform)s'), WithProperties('--%(configuration)s'), 'extract']
    name = 'extract-built-product'
    description = ['extracting built product']
    descriptionDone = ['extracted built product']
    haltOnFailure = True
    flunkOnFailure = True


class RunAPITests(TestWithFailureCount):
    name = 'run-api-tests'
    description = ['api tests running']
    descriptionDone = ['api-tests']
    jsonFileName = 'api_test_results.json'
    logfiles = {'json': jsonFileName}
    command = ['python', 'Tools/Scripts/run-api-tests', '--no-build',
               WithProperties('--%(configuration)s'), '--verbose', '--json-output={0}'.format(jsonFileName)]
    failedTestsFormatString = '%d api test%s failed or timed out'

    def start(self):
        appendCustomBuildFlags(self, self.getProperty('platform'), self.getProperty('fullPlatform'))
        return TestWithFailureCount.start(self)

    def countFailures(self, cmd):
        log_text = self.log_observer.getStdout() + self.log_observer.getStderr()

        match = re.search(r'Ran (?P<ran>\d+) tests of (?P<total>\d+) with (?P<passed>\d+) successful', log_text)
        if not match:
            return 0
        return int(match.group('ran')) - int(match.group('passed'))


class ArchiveTestResults(shell.ShellCommand):
    command = ['python', 'Tools/BuildSlaveSupport/test-result-archive',
               Interpolate('--platform=%(prop:platform)s'), Interpolate('--%(prop:configuration)s'), 'archive']
    name = 'archive-test-results'
    description = ['archiving test results']
    descriptionDone = ['archived test results']
    haltOnFailure = True


class UploadTestResults(transfer.FileUpload):
    name = 'upload-test-results'
    workersrc = 'layout-test-results.zip'
    masterdest = Interpolate('public_html/results/%(prop:buildername)s/r%(prop:patch_id)s-%(prop:buildnumber)s.zip')
    haltOnFailure = True

    def __init__(self, **kwargs):
        kwargs['workersrc'] = self.workersrc
        kwargs['masterdest'] = self.masterdest
        kwargs['mode'] = 0644
        kwargs['blocksize'] = 1024 * 256
        transfer.FileUpload.__init__(self, **kwargs)


class ExtractTestResults(master.MasterShellCommand):
    name = 'extract-test-results'
    zipFile = Interpolate('public_html/results/%(prop:buildername)s/r%(prop:patch_id)s-%(prop:buildnumber)s.zip')
    resultDirectory = Interpolate('public_html/results/%(prop:buildername)s/r%(prop:patch_id)s-%(prop:buildnumber)s')

    descriptionDone = ['uploaded results']
    command = ['unzip', zipFile, '-d', resultDirectory]
    renderables = ['resultDirectory']

    def __init__(self):
        super(ExtractTestResults, self).__init__(self.command)

    def resultDirectoryURL(self):
        return self.resultDirectory.replace('public_html/', '/') + '/'

    def addCustomURLs(self):
        self.addURL('view layout test results', self.resultDirectoryURL() + 'results.html')

    def finished(self, result):
        self.addCustomURLs()
        return master.MasterShellCommand.finished(self, result)
