# Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

from buildbot.plugins import steps, util
from buildbot.process import buildstep, logobserver, properties
from buildbot.process.results import Results, SUCCESS, FAILURE, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.steps import master, shell, transfer, trigger
from buildbot.steps.source import git
from buildbot.steps.worker import CompositeStepMixin
from twisted.internet import defer

from layout_test_failures import LayoutTestFailures

import json
import re
import requests

BUG_SERVER_URL = 'https://bugs.webkit.org/'
S3URL = 'https://s3-us-west-2.amazonaws.com/'
WithProperties = properties.WithProperties
Interpolate = properties.Interpolate


class ConfigureBuild(buildstep.BuildStep):
    name = 'configure-build'
    description = ['configuring build']
    descriptionDone = ['Configured build']

    def __init__(self, platform, configuration, architectures, buildOnly, triggers, additionalArguments):
        super(ConfigureBuild, self).__init__()
        self.platform = platform
        if platform != 'jsc-only':
            self.platform = platform.split('-', 1)[0]
        self.fullPlatform = platform
        self.configuration = configuration
        self.architecture = ' '.join(architectures) if architectures else None
        self.buildOnly = buildOnly
        self.triggers = triggers
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
            self.setProperty('buildOnly', self.buildOnly, 'config.json')
        if self.triggers:
            self.setProperty('triggers', self.triggers, 'config.json')
        if self.additionalArguments:
            self.setProperty('additionalArguments', self.additionalArguments, 'config.json')

        self.add_patch_id_url()
        self.finished(SUCCESS)
        return defer.succeed(None)

    def add_patch_id_url(self):
        patch_id = self.getProperty('patch_id', '')
        if patch_id:
            self.addURL('Patch {}'.format(patch_id), self.getPatchURL(patch_id))

    def getPatchURL(self, patch_id):
        if not patch_id:
            return None
        return '{}attachment.cgi?id={}&action=prettypatch'.format(BUG_SERVER_URL, patch_id)


class CheckOutSource(git.Git):
    name = 'clean-and-update-working-directory'
    CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR = (0, 2)
    haltOnFailure = False

    def __init__(self, **kwargs):
        self.repourl = 'https://git.webkit.org/git/WebKit.git'
        super(CheckOutSource, self).__init__(repourl=self.repourl,
                                                retry=self.CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR,
                                                timeout=2 * 60 * 60,
                                                alwaysUseLatest=True,
                                                logEnviron=False,
                                                method='clean',
                                                progress=True,
                                                **kwargs)

    def getResultSummary(self):
        if self.results == FAILURE:
            self.build.addStepsAfterCurrentStep([CleanUpGitIndexLock()])

        if self.results != SUCCESS:
            return {u'step': u'Failed to updated working directory'}
        else:
            return {u'step': u'Cleaned and updated working directory'}


class CleanUpGitIndexLock(shell.ShellCommand):
    name = 'clean-git-index-lock'
    command = ['rm', '-f', '.git/index.lock']
    descriptionDone = ['Deleted .git/index.lock']

    def __init__(self, **kwargs):
        super(CleanUpGitIndexLock, self).__init__(timeout=2 * 60, logEnviron=False, **kwargs)

    def evaluateCommand(self, cmd):
        self.build.buildFinished(['Git issue, retrying build'], RETRY)
        return super(CleanUpGitIndexLock, self).evaluateCommand(cmd)


class CheckOutSpecificRevision(shell.ShellCommand):
    name = 'checkout-specific-revision'
    descriptionDone = ['Checked out required revision']
    flunkOnFailure = False
    haltOnFailure = False

    def __init__(self, **kwargs):
        super(CheckOutSpecificRevision, self).__init__(logEnviron=False, **kwargs)

    def doStepIf(self, step):
        return self.getProperty('ews_revision', False)

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)

    def start(self):
        self.setCommand(['git', 'checkout', self.getProperty('ews_revision')])
        return shell.ShellCommand.start(self)


class CleanWorkingDirectory(shell.ShellCommand):
    name = 'clean-working-directory'
    description = ['clean-working-directory running']
    descriptionDone = ['Cleaned working directory']
    flunkOnFailure = True
    haltOnFailure = True
    command = ['python', 'Tools/Scripts/clean-webkit']

    def __init__(self, **kwargs):
        super(CleanWorkingDirectory, self).__init__(logEnviron=False, **kwargs)


class UpdateWorkingDirectory(shell.ShellCommand):
    name = 'update-working-directory'
    description = ['update-workring-directory running']
    descriptionDone = ['Updated working directory']
    flunkOnFailure = True
    haltOnFailure = True
    command = ['perl', 'Tools/Scripts/update-webkit']

    def __init__(self, **kwargs):
        super(UpdateWorkingDirectory, self).__init__(logEnviron=False, **kwargs)


class ApplyPatch(shell.ShellCommand, CompositeStepMixin):
    name = 'apply-patch'
    description = ['applying-patch']
    descriptionDone = ['Applied patch']
    flunkOnFailure = True
    haltOnFailure = True
    command = ['perl', 'Tools/Scripts/svn-apply', '--force', '.buildbot-diff']

    def __init__(self, **kwargs):
        super(ApplyPatch, self).__init__(timeout=5 * 60, logEnviron=False, **kwargs)

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
        d.addCallback(lambda res: shell.ShellCommand.start(self))

    def hideStepIf(self, results, step):
        return results == SUCCESS and self.getProperty('validated', '') == False

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {u'step': u'Patch does not apply'}
        return super(ApplyPatch, self).getResultSummary()


class CheckPatchRelevance(buildstep.BuildStep):
    name = 'check-patch-relevance'
    description = ['check-patch-relevance running']
    descriptionDone = ['Checked patch relevance']
    flunkOnFailure = True
    haltOnFailure = True

    bindings_paths = [
        'Source/WebCore',
        'Tools',
    ]

    services_paths = [
        'Tools/BuildSlaveSupport/ews-build',
    ]

    jsc_paths = [
        'JSTests/',
        'Source/JavaScriptCore/',
        'Source/WTF/',
        'Source/bmalloc/',
        'Makefile',
        'Makefile.shared',
        'Source/Makefile',
        'Source/Makefile.shared',
        'Tools/Scripts/build-webkit',
        'Tools/Scripts/build-jsc',
        'Tools/Scripts/jsc-stress-test-helpers/',
        'Tools/Scripts/run-jsc',
        'Tools/Scripts/run-jsc-benchmarks',
        'Tools/Scripts/run-jsc-stress-tests',
        'Tools/Scripts/run-javascriptcore-tests',
        'Tools/Scripts/run-layout-jsc',
        'Tools/Scripts/update-javascriptcore-test-results',
        'Tools/Scripts/webkitdirs.pm',
    ]

    webkitpy_paths = [
        'Tools/Scripts/webkitpy/',
        'Tools/QueueStatusServer/',
    ]

    group_to_paths_mapping = {
        'bindings': bindings_paths,
        'services-ews': services_paths,
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
    descriptionDone = ['Validated patch']
    flunkOnFailure = True
    haltOnFailure = True
    bug_open_statuses = ['UNCONFIRMED', 'NEW', 'ASSIGNED', 'REOPENED']
    bug_closed_statuses = ['RESOLVED', 'VERIFIED', 'CLOSED']

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

        patch_author = patch_json.get('creator')
        self.addURL('Patch by: {}'.format(patch_author), 'mailto:{}'.format(patch_author))
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
        if not bug_id:
            self._addToLog('stdio', 'Skipping bug status validation since bug id is None.\n')
            return -1

        bug_json = self.get_bug_json(bug_id)
        if not bug_json or not bug_json.get('status'):
            self._addToLog('stdio', 'Unable to fetch bug {}.\n'.format(bug_id))
            return -1

        bug_title = bug_json.get('summary')
        self.addURL(u'Bug {} {}'.format(bug_id, bug_title), '{}show_bug.cgi?id={}'.format(BUG_SERVER_URL, bug_id))
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

        bug_id = self.getProperty('bug_id', '') or self.get_bug_id_from_patch(patch_id)

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
            self.setProperty('validated', False)
            return None

        self._addToLog('stdio', 'Bug is open.\nPatch is not obsolete.\nPatch is not marked r-.\n')
        self.finished(SUCCESS)
        return None


class UnApplyPatchIfRequired(CleanWorkingDirectory):
    name = 'unapply-patch'
    descriptionDone = ['Unapplied patch']

    def doStepIf(self, step):
        return self.getProperty('patchFailedToBuild') or self.getProperty('patchFailedTests')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


class Trigger(trigger.Trigger):
    def __init__(self, schedulerNames, **kwargs):
        set_properties = self.propertiesToPassToTriggers() or {}
        super(Trigger, self).__init__(schedulerNames=schedulerNames, set_properties=set_properties, **kwargs)

    def propertiesToPassToTriggers(self):
        return {
            'patch_id': properties.Property('patch_id'),
            'bug_id': properties.Property('bug_id'),
            'configuration': properties.Property('configuration'),
            'platform': properties.Property('platform'),
            'fullPlatform': properties.Property('fullPlatform'),
            'architecture': properties.Property('architecture'),
            'owner': properties.Property('owner'),
            'ews_revision': properties.Property('got_revision'),
        }


class TestWithFailureCount(shell.Test):
    failedTestsFormatString = '%d test%s failed'
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
        self.failedTestPluralSuffix = '' if self.failedTestCount == 1 else 's'

    def evaluateCommand(self, cmd):
        if self.failedTestCount:
            return FAILURE

        if cmd.rc != 0:
            return FAILURE

        return SUCCESS

    def getResultSummary(self):
        status = self.name

        if self.results != SUCCESS:
            if self.failedTestCount:
                status = self.failedTestsFormatString % (self.failedTestCount, self.failedTestPluralSuffix)
            else:
                status += u' ({})'.format(Results[self.results])

        return {u'step': unicode(status)}


class CheckStyle(TestWithFailureCount):
    name = 'check-webkit-style'
    description = ['check-webkit-style running']
    descriptionDone = ['check-webkit-style']
    flunkOnFailure = True
    failedTestsFormatString = '%d style error%s'
    command = ['python', 'Tools/Scripts/check-webkit-style']

    def __init__(self, **kwargs):
        super(CheckStyle, self).__init__(logEnviron=False, **kwargs)

    def countFailures(self, cmd):
        log_text = self.log_observer.getStdout() + self.log_observer.getStderr()

        match = re.search(r'Total errors found: (?P<errors>\d+) in (?P<files>\d+) files', log_text)
        if not match:
            return 0
        return int(match.group('errors'))


class RunBindingsTests(shell.ShellCommand):
    name = 'bindings-tests'
    description = ['bindings-tests running']
    descriptionDone = ['bindings-tests']
    flunkOnFailure = True
    jsonFileName = 'bindings_test_results.json'
    logfiles = {'json': jsonFileName}
    command = ['python', 'Tools/Scripts/run-bindings-tests', '--json-output={0}'.format(jsonFileName)]

    def __init__(self, **kwargs):
        super(RunBindingsTests, self).__init__(timeout=5 * 60, logEnviron=False, **kwargs)

    def start(self):
        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('json', self.log_observer)
        return shell.ShellCommand.start(self)

    def getResultSummary(self):
        if self.results == SUCCESS:
            message = 'Passed bindings tests'
            self.build.buildFinished([message], SUCCESS)
            return {u'step': unicode(message)}

        logLines = self.log_observer.getStdout()
        json_text = ''.join([line for line in logLines.splitlines()])
        try:
            webkitpy_results = json.loads(json_text)
        except Exception as ex:
            self._addToLog('stderr', 'ERROR: unable to parse data, exception: {}'.format(ex))
            return super(RunBindingsTests, self).getResultSummary()

        failures = webkitpy_results.get('failures')
        if not failures:
            return super(RunBindingsTests, self).getResultSummary()
        pluralSuffix = 's' if len(failures) > 1 else ''
        failures_string = ', '.join([failure.replace('(JS) ', '') for failure in failures])
        message = 'Found {} Binding test failure{}: {}'.format(len(failures), pluralSuffix, failures_string)
        self.build.buildFinished([message], FAILURE)
        return {u'step': unicode(message)}

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)


class RunWebKitPerlTests(shell.ShellCommand):
    name = 'webkitperl-tests'
    description = ['webkitperl-tests running']
    descriptionDone = ['webkitperl-tests']
    flunkOnFailure = True
    command = ['perl', 'Tools/Scripts/test-webkitperl']

    def __init__(self, **kwargs):
        super(RunWebKitPerlTests, self).__init__(timeout=2 * 60, logEnviron=False, **kwargs)


class RunEWSUnitTests(shell.ShellCommand):
    name = 'ews-unit-tests'
    description = ['ews-unit-tests running']
    command = ['python', 'Tools/BuildSlaveSupport/ews-build/runUnittests.py']

    def __init__(self, **kwargs):
        super(RunEWSUnitTests, self).__init__(timeout=2 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {u'step': u'Passed EWS unit tests'}
        return {u'step': u'Failed EWS unit tests'}


class RunEWSBuildbotCheckConfig(shell.ShellCommand):
    name = 'buildbot-check-config'
    description = ['buildbot-checkconfig running']
    command = ['buildbot', 'checkconfig']

    def __init__(self, **kwargs):
        super(RunEWSBuildbotCheckConfig, self).__init__(workdir='build/Tools/BuildSlaveSupport/ews-build', timeout=2 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {u'step': u'Passed buildbot checkconfig'}
        return {u'step': u'Failed buildbot checkconfig'}


class RunWebKitPyTests(shell.ShellCommand):
    name = 'webkitpy-tests'
    description = ['webkitpy-tests running']
    descriptionDone = ['webkitpy-tests']
    flunkOnFailure = True
    jsonFileName = 'webkitpy_test_results.json'
    logfiles = {'json': jsonFileName}
    command = ['python', 'Tools/Scripts/test-webkitpy', '--json-output={0}'.format(jsonFileName)]

    def __init__(self, **kwargs):
        super(RunWebKitPyTests, self).__init__(timeout=2 * 60, logEnviron=False, **kwargs)

    def start(self):
        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('json', self.log_observer)
        return shell.ShellCommand.start(self)

    def getResultSummary(self):
        if self.results == SUCCESS:
            message = 'Passed webkitpy tests'
            self.build.buildFinished([message], SUCCESS)
            return {u'step': unicode(message)}

        logLines = self.log_observer.getStdout()
        json_text = ''.join([line for line in logLines.splitlines()])
        try:
            webkitpy_results = json.loads(json_text)
        except Exception as ex:
            self._addToLog('stderr', 'ERROR: unable to parse data, exception: {}'.format(ex))
            return super(RunWebKitPyTests, self).getResultSummary()

        failures = webkitpy_results.get('failures') + webkitpy_results.get('errors')
        if not failures:
            return super(RunWebKitPyTests, self).getResultSummary()
        pluralSuffix = 's' if len(failures) > 1 else ''
        failures_string = ', '.join([failure.get('name').replace('webkitpy.', '') for failure in failures])
        message = 'Found {} WebKitPy test failure{}: {}'.format(len(failures), pluralSuffix, failures_string)
        self.build.buildFinished([message], FAILURE)
        return {u'step': unicode(message)}

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)


class InstallGtkDependencies(shell.ShellCommand):
    name = 'jhbuild'
    description = ['updating gtk dependencies']
    descriptionDone = ['Updated gtk dependencies']
    command = ['perl', 'Tools/Scripts/update-webkitgtk-libs']
    haltOnFailure = False

    def __init__(self, **kwargs):
        super(InstallGtkDependencies, self).__init__(logEnviron=False, **kwargs)


class InstallWpeDependencies(shell.ShellCommand):
    name = 'jhbuild'
    description = ['updating wpe dependencies']
    descriptionDone = ['Updated wpe dependencies']
    command = ['perl', 'Tools/Scripts/update-webkitwpe-libs']
    haltOnFailure = False

    def __init__(self, **kwargs):
        super(InstallWpeDependencies, self).__init__(logEnviron=False, **kwargs)


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
    name = 'compile-webkit'
    description = ['compiling']
    descriptionDone = ['Compiled WebKit']
    env = {'MFLAGS': ''}
    warningPattern = '.*arning: .*'
    haltOnFailure = False
    command = ['perl', 'Tools/Scripts/build-webkit', WithProperties('--%(configuration)s')]

    def __init__(self, skipUpload=False, **kwargs):
        self.skipUpload = skipUpload
        super(CompileWebKit, self).__init__(logEnviron=False, **kwargs)

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
            steps_to_add = [UnApplyPatchIfRequired()]
            platform = self.getProperty('platform')
            if platform == 'wpe':
                steps_to_add.append(InstallWpeDependencies())
            elif platform == 'gtk':
                steps_to_add.append(InstallGtkDependencies())
            steps_to_add.append(CompileWebKitToT())
            steps_to_add.append(AnalyzeCompileWebKitResults())
            # Using a single addStepsAfterCurrentStep because of https://github.com/buildbot/buildbot/issues/4874
            self.build.addStepsAfterCurrentStep(steps_to_add)
        else:
            triggers = self.getProperty('triggers', None)
            if triggers or not self.skipUpload:
                self.build.addStepsAfterCurrentStep([ArchiveBuiltProduct(), UploadBuiltProduct(), TransferToS3()])

        return super(CompileWebKit, self).evaluateCommand(cmd)


class CompileWebKitToT(CompileWebKit):
    name = 'compile-webkit-tot'
    haltOnFailure = False

    def doStepIf(self, step):
        return self.getProperty('patchFailedToBuild') or self.getProperty('patchFailedTests')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)

    def evaluateCommand(self, cmd):
        return shell.Compile.evaluateCommand(self, cmd)


class AnalyzeCompileWebKitResults(buildstep.BuildStep):
    name = 'analyze-compile-webkit-results'
    description = ['analyze-compile-webkit-results']
    descriptionDone = ['analyze-compile-webkit-results']

    def start(self):
        compile_webkit_tot_result = self.getStepResult(CompileWebKitToT.name)

        if compile_webkit_tot_result == FAILURE:
            self.finished(FAILURE)
            message = 'Unable to build WebKit without patch, retrying build'
            self.descriptionDone = message
            self.build.buildFinished([message], RETRY)
            return defer.succeed(None)

        self.finished(FAILURE)
        self.build.results = FAILURE
        message = 'Patch does not build'
        self.descriptionDone = message
        self.build.buildFinished([message], FAILURE)

        return defer.succeed(None)

    def getStepResult(self, step_name):
        for step in self.build.executedSteps:
            if step.name == step_name:
                return step.results


class CompileJSCOnly(CompileWebKit):
    name = 'build-jsc'
    descriptionDone = ['Compiled JSC']
    command = ['perl', 'Tools/Scripts/build-jsc', WithProperties('--%(configuration)s')]


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
    logfiles = {'json': jsonFileName}
    command = ['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', '--json-output={0}'.format(jsonFileName), WithProperties('--%(configuration)s')]

    def start(self):
        appendCustomBuildFlags(self, self.getProperty('platform'), self.getProperty('fullPlatform'))
        return shell.Test.start(self)

    def evaluateCommand(self, cmd):
        if cmd.didFail():
            self.setProperty('patchFailedTests', True)

        return super(RunJavaScriptCoreTests, self).evaluateCommand(cmd)


class ReRunJavaScriptCoreTests(RunJavaScriptCoreTests):
    name = 'jscore-test-rerun'

    def doStepIf(self, step):
        return self.getProperty('patchFailedTests')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)

    def evaluateCommand(self, cmd):
        self.setProperty('patchFailedTests', cmd.didFail())
        return super(RunJavaScriptCoreTests, self).evaluateCommand(cmd)


class RunJavaScriptCoreTestsToT(RunJavaScriptCoreTests):
    name = 'jscore-test-tot'
    jsonFileName = 'jsc_results.json'
    command = ['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-fail-fast', '--json-output={0}'.format(jsonFileName), WithProperties('--%(configuration)s')]

    def doStepIf(self, step):
        return self.getProperty('patchFailedTests')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


class CleanBuild(shell.Compile):
    name = 'delete-WebKitBuild-directory'
    description = ['deleting WebKitBuild directory']
    descriptionDone = ['Deleted WebKitBuild directory']
    command = ['python', 'Tools/BuildSlaveSupport/clean-build', WithProperties('--platform=%(fullPlatform)s'), WithProperties('--%(configuration)s')]


class KillOldProcesses(shell.Compile):
    name = 'kill-old-processes'
    description = ['killing old processes']
    descriptionDone = ['Killed old processes']
    command = ['python', 'Tools/BuildSlaveSupport/kill-old-processes', 'buildbot']

    def __init__(self, **kwargs):
        super(KillOldProcesses, self).__init__(timeout=60, logEnviron=False, **kwargs)


class RunWebKitTests(shell.Test):
    name = 'layout-tests'
    description = ['layout-tests running']
    descriptionDone = ['layout-tests']
    resultDirectory = 'layout-test-results'
    jsonFileName = 'layout-test-results/full_results.json'
    logfiles = {'json': jsonFileName}
    command = ['python', 'Tools/Scripts/run-webkit-tests',
               '--no-build',
               '--no-new-test-results',
               '--no-show-results',
               '--exit-after-n-failures', '30',
               '--skip-failing-tests',
               WithProperties('--%(configuration)s')]

    def start(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)
        self.log_observer_json = logobserver.BufferLogObserver()
        self.addLogObserver('json', self.log_observer_json)

        self.incorrectLayoutLines = []
        platform = self.getProperty('platform')
        appendCustomBuildFlags(self, platform, self.getProperty('fullPlatform'))
        additionalArguments = self.getProperty('additionalArguments')

        self.setCommand(self.command + ['--results-directory', self.resultDirectory])
        self.setCommand(self.command + ['--debug-rwt-logging'])

        if additionalArguments:
            self.setCommand(self.command + additionalArguments)
        return shell.Test.start(self)

    # FIXME: This will break if run-webkit-tests changes its default log formatter.
    nrwt_log_message_regexp = re.compile(r'\d{2}:\d{2}:\d{2}(\.\d+)?\s+\d+\s+(?P<message>.*)')

    def _strip_python_logging_prefix(self, line):
        match_object = self.nrwt_log_message_regexp.match(line)
        if match_object:
            return match_object.group('message')
        return line

    def _parseRunWebKitTestsOutput(self, logText):
        incorrectLayoutLines = []
        expressions = [
            ('flakes', re.compile(r'Unexpected flakiness.+\((\d+)\)')),
            ('new passes', re.compile(r'Expected to .+, but passed:\s+\((\d+)\)')),
            ('missing results', re.compile(r'Regressions: Unexpected missing results\s+\((\d+)\)')),
            ('failures', re.compile(r'Regressions: Unexpected.+\((\d+)\)')),
        ]
        testFailures = {}

        for line in logText.splitlines():
            if line.find('Exiting early') >= 0 or line.find('leaks found') >= 0:
                incorrectLayoutLines.append(self._strip_python_logging_prefix(line))
                continue
            for name, expression in expressions:
                match = expression.search(line)

                if match:
                    testFailures[name] = testFailures.get(name, 0) + int(match.group(1))
                    break

                # FIXME: Parse file names and put them in results

        for name in testFailures:
            incorrectLayoutLines.append(str(testFailures[name]) + ' ' + name)

        self.incorrectLayoutLines = incorrectLayoutLines

    def commandComplete(self, cmd):
        shell.Test.commandComplete(self, cmd)
        logText = self.log_observer.getStdout() + self.log_observer.getStderr()
        logTextJson = self.log_observer_json.getStdout()

        first_results = LayoutTestFailures.results_from_string(logTextJson)

        if first_results:
            self.setProperty('first_results_exceed_failure_limit', first_results.did_exceed_test_failure_limit)
            self.setProperty('first_run_failures', first_results.failing_tests)
        self._parseRunWebKitTestsOutput(logText)

    def evaluateResult(self, cmd):
        result = SUCCESS

        if self.incorrectLayoutLines:
            if len(self.incorrectLayoutLines) == 1:
                line = self.incorrectLayoutLines[0]
                if line.find('were new') >= 0 or line.find('was new') >= 0 or line.find(' leak') >= 0:
                    return WARNINGS

            for line in self.incorrectLayoutLines:
                if line.find('flakes') >= 0 or line.find('new passes') >= 0 or line.find('missing results') >= 0:
                    result = WARNINGS
                else:
                    return FAILURE

        # Return code from Tools/Scripts/layout_tests/run_webkit_tests.py.
        # This means that an exception was raised when running run-webkit-tests and
        # was never handled.
        if cmd.rc == 254:
            return RETRY
        if cmd.rc != 0:
            return FAILURE

        return result

    def evaluateCommand(self, cmd):
        rc = self.evaluateResult(cmd)
        if rc == SUCCESS or rc == WARNINGS:
            message = 'Passed layout tests'
            self.descriptionDone = message
            self.build.results = SUCCESS
            self.build.buildFinished([message], SUCCESS)
        else:
            self.build.addStepsAfterCurrentStep([ArchiveTestResults(), UploadTestResults(), ExtractTestResults(), ReRunWebKitTests()])
        return rc

    def getResultSummary(self):
        status = self.name

        if self.results != SUCCESS and self.incorrectLayoutLines:
            status = u' '.join(self.incorrectLayoutLines)
            return {u'step': status}

        return super(RunWebKitTests, self).getResultSummary()


class ReRunWebKitTests(RunWebKitTests):
    name = 're-run-layout-tests'

    def evaluateCommand(self, cmd):
        rc = self.evaluateResult(cmd)
        if rc == SUCCESS or rc == WARNINGS:
            message = 'Passed layout tests'
            self.descriptionDone = message
            self.build.results = SUCCESS
            self.build.buildFinished([message], SUCCESS)
        else:
            self.setProperty('patchFailedTests', True)
            self.build.addStepsAfterCurrentStep([ArchiveTestResults(), UploadTestResults(identifier='rerun'), ExtractTestResults(identifier='rerun'), UnApplyPatchIfRequired(), CompileWebKitToT(), RunWebKitTestsWithoutPatch()])
        return rc

    def commandComplete(self, cmd):
        shell.Test.commandComplete(self, cmd)
        logText = self.log_observer.getStdout() + self.log_observer.getStderr()
        logTextJson = self.log_observer_json.getStdout()

        second_results = LayoutTestFailures.results_from_string(logTextJson)

        if second_results:
            self.setProperty('second_results_exceed_failure_limit', second_results.did_exceed_test_failure_limit)
            self.setProperty('second_run_failures', second_results.failing_tests)
        self._parseRunWebKitTestsOutput(logText)


class RunWebKitTestsWithoutPatch(RunWebKitTests):
    name = 'run-layout-tests-without-patch'

    def evaluateCommand(self, cmd):
        rc = shell.Test.evaluateCommand(self, cmd)
        self.build.addStepsAfterCurrentStep([ArchiveTestResults(), UploadTestResults(identifier='clean-tree'), ExtractTestResults(identifier='clean-tree'), AnalyzeLayoutTestsResults()])
        return rc

    def commandComplete(self, cmd):
        shell.Test.commandComplete(self, cmd)
        logText = self.log_observer.getStdout() + self.log_observer.getStderr()
        logTextJson = self.log_observer_json.getStdout()

        clean_tree_results = LayoutTestFailures.results_from_string(logTextJson)

        if clean_tree_results:
            self.setProperty('clean_tree_results_exceed_failure_limit', clean_tree_results.did_exceed_test_failure_limit)
            self.setProperty('clean_tree_run_failures', clean_tree_results.failing_tests)
        self._parseRunWebKitTestsOutput(logText)


class AnalyzeLayoutTestsResults(buildstep.BuildStep):
    name = 'analyze-layout-tests-results'
    description = ['analyze-layout-test-results']
    descriptionDone = ['analyze-layout-tests-results']

    def report_failure(self, new_failures):
        self.finished(FAILURE)
        self.build.results = FAILURE
        pluralSuffix = 's' if len(new_failures) > 1 else ''
        new_failures_string = ', '.join([failure_name for failure_name in new_failures])
        message = 'Found {} new test failure{}: {}'.format(len(new_failures), pluralSuffix, new_failures_string)
        self.descriptionDone = message
        self.build.buildFinished([message], FAILURE)
        return defer.succeed(None)

    def report_pre_existing_failures(self, clean_tree_failures):
        self.finished(SUCCESS)
        self.build.results = SUCCESS
        self.descriptionDone = 'Passed layout tests'
        pluralSuffix = 's' if len(clean_tree_failures) > 1 else ''
        clean_tree_failures_string = ', '.join([failure_name for failure_name in clean_tree_failures])
        message = 'Found {} pre-existing test failure{}: {}'.format(len(clean_tree_failures), pluralSuffix, clean_tree_failures_string)
        self.build.buildFinished([message], SUCCESS)
        return defer.succeed(None)

    def retry_build(self, message=''):
        self.finished(RETRY)
        message = 'Unable to confirm if test failures are introduced by patch, retrying build'
        self.descriptionDone = message
        self.build.buildFinished([message], RETRY)
        return defer.succeed(None)

    def _results_failed_different_tests(self, first_results_failing_tests, second_results_failing_tests):
        return first_results_failing_tests != second_results_failing_tests

    def _report_flaky_tests(self, flaky_tests):
        #TODO: implement this
        pass

    def start(self):
        first_results_did_exceed_test_failure_limit = self.getProperty('first_results_exceed_failure_limit')
        first_results_failing_tests = set(self.getProperty('first_run_failures', []))
        second_results_did_exceed_test_failure_limit = self.getProperty('second_results_exceed_failure_limit')
        second_results_failing_tests = set(self.getProperty('second_run_failures', []))
        clean_tree_results_did_exceed_test_failure_limit = self.getProperty('clean_tree_results_exceed_failure_limit')
        clean_tree_results_failing_tests = set(self.getProperty('clean_tree_run_failures', []))

        if first_results_did_exceed_test_failure_limit and second_results_did_exceed_test_failure_limit:
            if (len(first_results_failing_tests) - len(clean_tree_results_failing_tests)) <= 5:
                # If we've made it here, then many tests are failing with the patch applied, but
                # if the clean tree is also failing many tests, even if it's not quite as many,
                # then we can't be certain that the discrepancy isn't due to flakiness, and hence we must defer judgement.
                return self.retry_build()
            return self.report_failure(first_results_failing_tests)

        if second_results_did_exceed_test_failure_limit:
            if clean_tree_results_did_exceed_test_failure_limit:
                return self.retry_build()
            failures_introduced_by_patch = first_results_failing_tests - clean_tree_results_failing_tests
            if failures_introduced_by_patch:
                return self.report_failure(failures_introduced_by_patch)
            return self.retry_build()

        if first_results_did_exceed_test_failure_limit:
            if clean_tree_results_did_exceed_test_failure_limit:
                return self.retry_build()
            failures_introduced_by_patch = second_results_failing_tests - clean_tree_results_failing_tests
            if failures_introduced_by_patch:
                return self.report_failure(failures_introduced_by_patch)
            return self.retry_build()

        if self._results_failed_different_tests(first_results_failing_tests, second_results_failing_tests):
            tests_that_only_failed_first = first_results_failing_tests.difference(second_results_failing_tests)
            self._report_flaky_tests(tests_that_only_failed_first)

            tests_that_only_failed_second = second_results_failing_tests.difference(first_results_failing_tests)
            self._report_flaky_tests(tests_that_only_failed_second)

            tests_that_consistently_failed = first_results_failing_tests.intersection(second_results_failing_tests)
            if tests_that_consistently_failed:
                if clean_tree_results_did_exceed_test_failure_limit:
                    return self.retry_build()
                failures_introduced_by_patch = tests_that_consistently_failed - clean_tree_results_failing_tests
                if failures_introduced_by_patch:
                    return self.report_failure(failures_introduced_by_patch)

            # At this point we know that at least one test flaked, but no consistent failures
            # were introduced. This is a bit of a grey-zone.
            return self.retry_build()

        if clean_tree_results_did_exceed_test_failure_limit:
            return self.retry_build()
        failures_introduced_by_patch = first_results_failing_tests - clean_tree_results_failing_tests
        if failures_introduced_by_patch:
            return self.report_failure(failures_introduced_by_patch)

        # At this point, we know that the first and second runs had the exact same failures,
        # and that those failures are all present on the clean tree, so we can say with certainty
        # that the patch is good.
        return self.report_pre_existing_failures(clean_tree_results_failing_tests)


class RunWebKit1Tests(RunWebKitTests):
    def start(self):
        self.setCommand(self.command + ['--dump-render-tree'])

        return RunWebKitTests.start(self)


class ArchiveBuiltProduct(shell.ShellCommand):
    command = ['python', 'Tools/BuildSlaveSupport/built-product-archive',
               WithProperties('--platform=%(fullPlatform)s'), WithProperties('--%(configuration)s'), 'archive']
    name = 'archive-built-product'
    description = ['archiving built product']
    descriptionDone = ['Archived built product']
    haltOnFailure = True

    def __init__(self, **kwargs):
        super(ArchiveBuiltProduct, self).__init__(logEnviron=False, **kwargs)


class UploadBuiltProduct(transfer.FileUpload):
    name = 'upload-built-product'
    workersrc = WithProperties('WebKitBuild/%(configuration)s.zip')
    masterdest = WithProperties('public_html/archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(patch_id)s.zip')
    descriptionDone = ['Uploaded built product']
    haltOnFailure = True

    def __init__(self, **kwargs):
        kwargs['workersrc'] = self.workersrc
        kwargs['masterdest'] = self.masterdest
        kwargs['mode'] = 0644
        kwargs['blocksize'] = 1024 * 256
        transfer.FileUpload.__init__(self, **kwargs)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {u'step': u'Failed to upload built product'}
        return super(UploadBuiltProduct, self).getResultSummary()


class TransferToS3(master.MasterShellCommand):
    name = 'transfer-to-s3'
    description = ['transferring to s3']
    descriptionDone = ['Transferred archive to S3']
    archive = WithProperties('public_html/archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(patch_id)s.zip')
    identifier = WithProperties('%(fullPlatform)s-%(architecture)s-%(configuration)s')
    patch_id = WithProperties('%(patch_id)s')
    command = ['python', '../Shared/transfer-archive-to-s3', '--patch_id', patch_id, '--identifier', identifier, '--archive', archive]
    haltOnFailure = True
    flunkOnFailure = True

    def __init__(self, **kwargs):
        kwargs['command'] = self.command
        master.MasterShellCommand.__init__(self, logEnviron=False, **kwargs)

    def start(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)
        return super(TransferToS3, self).start()

    def finished(self, results):
        log_text = self.log_observer.getStdout() + self.log_observer.getStderr()
        match = re.search(r'S3 URL: (?P<url>[^\s]+)', log_text)
        # Sample log: S3 URL: https://s3-us-west-2.amazonaws.com/ews-archives.webkit.org/ios-simulator-12-x86_64-release/123456.zip
        if match:
            self.addURL('uploaded archive', match.group('url'))

        if results == SUCCESS:
            triggers = self.getProperty('triggers', None)
            if triggers:
                self.build.addStepsAfterCurrentStep([Trigger(schedulerNames=triggers)])

        return super(TransferToS3, self).finished(results)

    def hideStepIf(self, results, step):
        return results == SUCCESS and self.getProperty('validated', '') == False

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {u'step': u'Failed to transfer archive to S3'}
        return super(TransferToS3, self).getResultSummary()


class DownloadBuiltProduct(shell.ShellCommand):
    command = ['python', 'Tools/BuildSlaveSupport/download-built-product',
        WithProperties('--%(configuration)s'),
        WithProperties(S3URL + 'ews-archives.webkit.org/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(patch_id)s.zip')]
    name = 'download-built-product'
    description = ['downloading built product']
    descriptionDone = ['Downloaded built product']
    haltOnFailure = True
    flunkOnFailure = True

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {u'step': u'Failed to download built product from S3'}
        return super(DownloadBuiltProduct, self).getResultSummary()

    def __init__(self, **kwargs):
        super(DownloadBuiltProduct, self).__init__(logEnviron=False, **kwargs)


class ExtractBuiltProduct(shell.ShellCommand):
    command = ['python', 'Tools/BuildSlaveSupport/built-product-archive',
               WithProperties('--platform=%(fullPlatform)s'), WithProperties('--%(configuration)s'), 'extract']
    name = 'extract-built-product'
    description = ['extracting built product']
    descriptionDone = ['Extracted built product']
    haltOnFailure = True
    flunkOnFailure = True

    def __init__(self, **kwargs):
        super(ExtractBuiltProduct, self).__init__(logEnviron=False, **kwargs)


class RunAPITests(TestWithFailureCount):
    name = 'run-api-tests'
    description = ['api tests running']
    descriptionDone = ['api-tests']
    jsonFileName = 'api_test_results.json'
    logfiles = {'json': jsonFileName}
    command = ['python', 'Tools/Scripts/run-api-tests', '--no-build',
               WithProperties('--%(configuration)s'), '--verbose', '--json-output={0}'.format(jsonFileName)]
    failedTestsFormatString = '%d api test%s failed or timed out'

    def __init__(self, **kwargs):
        super(RunAPITests, self).__init__(logEnviron=False, **kwargs)

    def start(self):
        appendCustomBuildFlags(self, self.getProperty('platform'), self.getProperty('fullPlatform'))
        return TestWithFailureCount.start(self)

    def countFailures(self, cmd):
        log_text = self.log_observer.getStdout() + self.log_observer.getStderr()

        match = re.search(r'Ran (?P<ran>\d+) tests of (?P<total>\d+) with (?P<passed>\d+) successful', log_text)
        if not match:
            return 0
        return int(match.group('ran')) - int(match.group('passed'))

    def evaluateCommand(self, cmd):
        rc = super(RunAPITests, self).evaluateCommand(cmd)
        if rc == SUCCESS:
            message = 'Passed API tests'
            self.descriptionDone = message
            self.build.results = SUCCESS
            self.build.buildFinished([message], SUCCESS)
        else:
            self.build.addStepsAfterCurrentStep([ReRunAPITests()])
        return rc


class ReRunAPITests(RunAPITests):
    name = 're-run-api-tests'

    def evaluateCommand(self, cmd):
        rc = TestWithFailureCount.evaluateCommand(self, cmd)
        if rc == SUCCESS:
            message = 'Passed API tests'
            self.descriptionDone = message
            self.build.results = SUCCESS
            self.build.buildFinished([message], SUCCESS)
        else:
            self.setProperty('patchFailedTests', True)
            self.build.addStepsAfterCurrentStep([UnApplyPatchIfRequired(), CompileWebKitToT(), RunAPITestsWithoutPatch(), AnalyzeAPITestsResults()])
        return rc


class RunAPITestsWithoutPatch(RunAPITests):
    name = 'run-api-tests-without-patch'

    def evaluateCommand(self, cmd):
        return TestWithFailureCount.evaluateCommand(self, cmd)


class AnalyzeAPITestsResults(buildstep.BuildStep):
    name = 'analyze-api-tests-results'
    description = ['analyze-api-test-results']
    descriptionDone = ['analyze-api-tests-results']

    def start(self):
        self.results = {}
        d = self.getTestsResults(RunAPITests.name)
        d.addCallback(lambda res: self.getTestsResults(ReRunAPITests.name))
        d.addCallback(lambda res: self.getTestsResults(RunAPITestsWithoutPatch.name))
        d.addCallback(lambda res: self.analyzeResults())
        return defer.succeed(None)

    def analyzeResults(self):
        if not self.results or len(self.results) == 0:
            self._addToLog('stderr', 'Unable to parse API test results: {}'.format(self.results))
            self.finished(RETRY)
            self.build.buildFinished(['Unable to parse API test results'], RETRY)
            return -1

        first_run_results = self.results.get(RunAPITests.name)
        second_run_results = self.results.get(ReRunAPITests.name)
        clean_tree_results = self.results.get(RunAPITestsWithoutPatch.name)

        if not (first_run_results and second_run_results and clean_tree_results):
            self.finished(RETRY)
            self.build.buildFinished(['Unable to parse API test results'], RETRY)
            return -1

        def getAPITestFailures(result):
            # TODO: Analyze Time-out, Crash and Failure independently
            return set([failure.get('name') for failure in result.get('Timedout', [])] +
                [failure.get('name') for failure in result.get('Crashed', [])] +
                [failure.get('name') for failure in result.get('Failed', [])])

        first_run_failures = getAPITestFailures(first_run_results)
        second_run_failures = getAPITestFailures(second_run_results)
        clean_tree_failures = getAPITestFailures(clean_tree_results)

        failures_with_patch = first_run_failures.intersection(second_run_failures)
        flaky_failures = first_run_failures.union(second_run_failures) - first_run_failures.intersection(second_run_failures)
        flaky_failures_string = ', '.join([failure_name.replace('TestWebKitAPI.', '') for failure_name in flaky_failures])
        new_failures = failures_with_patch - clean_tree_failures
        new_failures_string = ', '.join([failure_name.replace('TestWebKitAPI.', '') for failure_name in new_failures])

        self._addToLog('stderr', '\nFailures in API Test first run: {}'.format(first_run_failures))
        self._addToLog('stderr', '\nFailures in API Test second run: {}'.format(second_run_failures))
        self._addToLog('stderr', '\nFlaky Tests: {}'.format(flaky_failures))
        self._addToLog('stderr', '\nFailures in API Test on clean tree: {}'.format(clean_tree_failures))

        if new_failures:
            self._addToLog('stderr', '\nNew failures: {}\n'.format(new_failures))
            self.finished(FAILURE)
            self.build.results = FAILURE
            pluralSuffix = 's' if len(new_failures) > 1 else ''
            message = 'Found {} new API test failure{}: {}'.format(len(new_failures), pluralSuffix, new_failures_string)
            self.descriptionDone = message
            self.build.buildFinished([message], FAILURE)
        else:
            self._addToLog('stderr', '\nNo new failures\n')
            self.finished(SUCCESS)
            self.build.results = SUCCESS
            self.descriptionDone = 'Passed API tests'
            pluralSuffix = 's' if len(clean_tree_failures) > 1 else ''
            message = 'Found {} pre-existing API test failure{}'.format(len(clean_tree_failures), pluralSuffix)
            if flaky_failures:
                message += '. Flaky tests: {}'.format(flaky_failures_string)
            self.build.buildFinished([message], SUCCESS)

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)

    def getBuildStepByName(self, name):
        for step in self.build.executedSteps:
            if step.name == name:
                return step
        return None

    @defer.inlineCallbacks
    def getTestsResults(self, name):
        step = self.getBuildStepByName(name)
        if not step:
            self._addToLog('stderr', 'ERROR: step not found: {}'.format(step))
            defer.returnValue(None)

        logs = yield self.master.db.logs.getLogs(step.stepid)
        log = next((log for log in logs if log['name'] == u'json'), None)
        if not log:
            self._addToLog('stderr', 'ERROR: log for step not found: {}'.format(step))
            defer.returnValue(None)

        lastline = int(max(0, log['num_lines'] - 1))
        logLines = yield self.master.db.logs.getLogLines(log['id'], 0, lastline)
        if log['type'] == 's':
            logLines = ''.join([line[1:] for line in logLines.splitlines()])

        try:
            self.results[name] = json.loads(logLines)
        except Exception as ex:
            self._addToLog('stderr', 'ERROR: unable to parse data, exception: {}'.format(ex))


class ArchiveTestResults(shell.ShellCommand):
    command = ['python', 'Tools/BuildSlaveSupport/test-result-archive',
               Interpolate('--platform=%(prop:platform)s'), Interpolate('--%(prop:configuration)s'), 'archive']
    name = 'archive-test-results'
    description = ['archiving test results']
    descriptionDone = ['Archived test results']
    haltOnFailure = True

    def __init__(self, **kwargs):
        super(ArchiveTestResults, self).__init__(logEnviron=False, **kwargs)


class UploadTestResults(transfer.FileUpload):
    name = 'upload-test-results'
    descriptionDone = ['Uploaded test results']
    workersrc = 'layout-test-results.zip'
    haltOnFailure = True

    def __init__(self, identifier='', **kwargs):
        if identifier and not identifier.startswith('-'):
            identifier = '-{}'.format(identifier)
        kwargs['workersrc'] = self.workersrc
        kwargs['masterdest'] = Interpolate('public_html/results/%(prop:buildername)s/r%(prop:patch_id)s-%(prop:buildnumber)s{}.zip'.format(identifier))
        kwargs['mode'] = 0644
        kwargs['blocksize'] = 1024 * 256
        transfer.FileUpload.__init__(self, **kwargs)


class ExtractTestResults(master.MasterShellCommand):
    name = 'extract-test-results'
    descriptionDone = ['Extracted test results']
    renderables = ['resultDirectory', 'zipFile']
    haltOnFailure = False
    flunkOnFailure = False

    def __init__(self, identifier=''):
        if identifier and not identifier.startswith('-'):
            identifier = '-{}'.format(identifier)

        self.zipFile = Interpolate('public_html/results/%(prop:buildername)s/r%(prop:patch_id)s-%(prop:buildnumber)s{}.zip'.format(identifier))
        self.resultDirectory = Interpolate('public_html/results/%(prop:buildername)s/r%(prop:patch_id)s-%(prop:buildnumber)s{}'.format(identifier))
        self.command = ['unzip', self.zipFile, '-d', self.resultDirectory]

        super(ExtractTestResults, self).__init__(self.command)

    def resultDirectoryURL(self):
        return self.resultDirectory.replace('public_html/', '/') + '/'

    def resultsDownloadURL(self):
        return self.zipFile.replace('public_html/', '/')

    def getLastBuildStepByName(self, name):
        for step in reversed(self.build.executedSteps):
            if name in step.name:
                return step
        return None

    def addCustomURLs(self):
        step = self.getLastBuildStepByName(RunWebKitTests.name)
        if not step:
            step = self
        step.addURL('view layout test results', self.resultDirectoryURL() + 'results.html')
        step.addURL('download layout test results', self.resultsDownloadURL())

    def finished(self, result):
        self.addCustomURLs()
        return master.MasterShellCommand.finished(self, result)


class PrintConfiguration(steps.ShellSequence):
    name = 'configuration'
    description = ['configuration']
    haltOnFailure = False
    flunkOnFailure = False
    warnOnFailure = False
    logEnviron = False
    command_list_generic = [['hostname']]
    command_list_apple = [['df', '-hl'], ['date'], ['sw_vers'], ['xcodebuild', '-sdk', '-version'], ['uptime']]
    command_list_linux = [['df', '-hl'], ['date'], ['uname', '-a'], ['uptime']]
    command_list_win = [[]]  # TODO: add windows specific commands here

    def __init__(self, **kwargs):
        super(PrintConfiguration, self).__init__(timeout=60, **kwargs)
        self.commands = []
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

    def run(self):
        command_list = list(self.command_list_generic)
        platform = self.getProperty('platform', '*')
        platform = platform.split('-')[0]
        if platform in ('mac', 'ios', '*'):
            command_list.extend(self.command_list_apple)
        elif platform in ('gtk', 'wpe'):
            command_list.extend(self.command_list_linux)
        elif platform in ('win', 'wincairo'):
            command_list.extend(self.command_list_win)

        for command in command_list:
            self.commands.append(util.ShellArg(command=command, logfile='stdio'))
        return super(PrintConfiguration, self).run()

    def convert_build_to_os_name(self, build):
        if not build:
            return 'Unknown'

        build_to_name_mapping = {
            '10.15': 'Catalina',
            '10.14': 'Mojave',
            '10.13': 'High Sierra',
            '10.12': 'Sierra',
            '10.11': 'El Capitan',
            '10.10': 'Yosemite',
            '10.9': 'Maverick',
            '10.8': 'Mountain Lion',
            '10.7': 'Lion',
            '10.6': 'Snow Leopard',
            '10.5': 'Leopard',
        }

        for key, value in build_to_name_mapping.iteritems():
            if build.startswith(key):
                return value
        return 'Unknown'

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {u'step': u'Failed to print configuration'}
        logText = self.log_observer.getStdout() + self.log_observer.getStderr()
        configuration = u'Printed configuration'
        match = re.search('ProductVersion:[ \t]*(.+?)\n', logText)
        if match:
            os_version = match.group(1).strip()
            os_name = self.convert_build_to_os_name(os_version)
            configuration = u'OS: {} ({})'.format(os_name, os_version)

        xcode_re = sdk_re = 'Xcode[ \t]+?([0-9.]+?)\n'
        match = re.search(xcode_re, logText)
        if match:
            xcode_version = match.group(1).strip()
            configuration += u', Xcode: {}'.format(xcode_version)
        return {u'step': configuration}


class ApplyWatchList(shell.ShellCommand):
    name = 'apply-watch-list'
    description = ['applying watchilist']
    descriptionDone = ['Applied WatchList']
    bug_id = WithProperties('%(bug_id)s')
    command = ['python', 'Tools/Scripts/webkit-patch', 'apply-watchlist-local', bug_id]
    haltOnFailure = True
    flunkOnFailure = True

    def __init__(self, **kwargs):
        shell.ShellCommand.__init__(self, timeout=2 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {u'step': u'Failed to apply watchlist'}
        return super(ApplyWatchList, self).getResultSummary()
