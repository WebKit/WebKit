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

from buildbot.plugins import steps, util
from buildbot.process import buildstep, logobserver, properties
from buildbot.process.results import Results, SUCCESS, FAILURE, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.steps import master, shell, transfer, trigger
from buildbot.steps.source import git
from buildbot.steps.worker import CompositeStepMixin
from twisted.internet import defer

from layout_test_failures import LayoutTestFailures
from send_email import send_email_to_bot_watchers

import json
import re
import requests
import os

BUG_SERVER_URL = 'https://bugs.webkit.org/'
S3URL = 'https://s3-us-west-2.amazonaws.com/'
S3_RESULTS_URL = 'https://ews-build.s3-us-west-2.amazonaws.com/'
EWS_BUILD_URL = 'https://ews-build.webkit.org/'
EWS_URL = 'https://ews.webkit.org/'
WithProperties = properties.WithProperties
Interpolate = properties.Interpolate


class ConfigureBuild(buildstep.BuildStep):
    name = 'configure-build'
    description = ['configuring build']
    descriptionDone = ['Configured build']

    def __init__(self, platform, configuration, architectures, buildOnly, triggers, remotes, additionalArguments):
        super(ConfigureBuild, self).__init__()
        self.platform = platform
        if platform != 'jsc-only':
            self.platform = platform.split('-', 1)[0]
        self.fullPlatform = platform
        self.configuration = configuration
        self.architecture = ' '.join(architectures) if architectures else None
        self.buildOnly = buildOnly
        self.triggers = triggers
        self.remotes = remotes
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
        if self.remotes:
            self.setProperty('remotes', self.remotes, 'config.json')
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

    def start(self):
        platform = self.getProperty('platform', '*')
        if platform == 'wincairo':
            self.command = ['del', '.git\index.lock']
        return shell.ShellCommand.start(self)

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

    def start(self):
        platform = self.getProperty('platform')
        if platform in ('gtk', 'wpe'):
            self.setCommand(self.command + ['--keep-jhbuild-directory'])
        return shell.ShellCommand.start(self)


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
    haltOnFailure = False
    command = ['perl', 'Tools/Scripts/svn-apply', '--force', '.buildbot-diff']

    def __init__(self, **kwargs):
        super(ApplyPatch, self).__init__(timeout=10 * 60, logEnviron=False, **kwargs)

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

        patch_reviewer_name = self.getProperty('patch_reviewer_full_name', '')
        if patch_reviewer_name:
            self.command.extend(['--reviewer', patch_reviewer_name])
        d = self.downloadFileContentToWorker('.buildbot-diff', patch)
        d.addCallback(lambda res: shell.ShellCommand.start(self))

    def hideStepIf(self, results, step):
        return results == SUCCESS and self.getProperty('sensitive', False)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {u'step': u'svn-apply failed to apply patch to trunk'}
        return super(ApplyPatch, self).getResultSummary()

    def evaluateCommand(self, cmd):
        rc = shell.ShellCommand.evaluateCommand(self, cmd)
        patch_id = self.getProperty('patch_id', '')
        if rc == FAILURE:
            message = 'Tools/Scripts/svn-apply failed to apply patch {} to trunk'.format(patch_id)
            if self.getProperty('buildername', '').lower() == 'commit-queue':
                comment_text = '{}.\nPlease resolve the conflicts and upload a new patch.'.format(message.replace('patch', 'attachment'))
                self.setProperty('bugzilla_comment_text', comment_text)
                self.setProperty('build_finish_summary', message)
                self.build.addStepsAfterCurrentStep([CommentOnBug(), SetCommitQueueMinusFlagOnPatch()])
            else:
                self.build.buildFinished([message], FAILURE)
        return rc


class CheckPatchRelevance(buildstep.BuildStep):
    name = 'check-patch-relevance'
    description = ['check-patch-relevance running']
    descriptionDone = ['Patch contains relevant changes']
    flunkOnFailure = True
    haltOnFailure = True

    bindings_paths = [
        'Source/WebCore',
        'Tools',
    ]

    services_paths = [
        'Tools/BuildSlaveSupport/build.webkit.org-config',
        'Tools/BuildSlaveSupport/ews-build',
        'Tools/BuildSlaveSupport/Shared',
        'Tools/resultsdbpy',
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

    wk1_paths = [
        'Source/WebKitLegacy',
        'Source/WebCore',
        'Source/WebInspectorUI',
        'Source/WebDriver',
        'Source/WTF',
        'Source/bmalloc',
        'Source/JavaScriptCore',
        'Source/ThirdParty',
        'LayoutTests',
        'Tools',
    ]

    webkitpy_paths = [
        'Tools/Scripts/webkitpy/',
    ]

    group_to_paths_mapping = {
        'bindings': bindings_paths,
        'services-ews': services_paths,
        'jsc': jsc_paths,
        'webkitpy': webkitpy_paths,
        'wk1-tests': wk1_paths,
        'windows': wk1_paths,
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

    def getResultSummary(self):
        if self.results == FAILURE:
            return {u'step': u'Patch doesn\'t have relevant changes'}
        return super(CheckPatchRelevance, self).getResultSummary()

class BugzillaMixin(object):
    addURLs = False
    bug_open_statuses = ['UNCONFIRMED', 'NEW', 'ASSIGNED', 'REOPENED']
    bug_closed_statuses = ['RESOLVED', 'VERIFIED', 'CLOSED']
    revert_preamble = 'REVERT of r'

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)

    def fetch_data_from_url_with_authentication(self, url):
        response = None
        try:
            response = requests.get(url, params={'Bugzilla_api_key': self.get_bugzilla_api_key()})
            if response.status_code != 200:
                self._addToLog('stdio', 'Accessed {url} with unexpected status code {status_code}.\n'.format(url=url, status_code=response.status_code))
                return None
        except Exception as e:
            # Catching all exceptions here to safeguard api key.
            self._addToLog('stdio', 'Failed to access {url}.\n'.format(url=url))
            return None
        return response

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
        patch = self.fetch_data_from_url_with_authentication(patch_url)
        if not patch:
            return None
        patch_json = patch.json().get('attachments')
        if not patch_json or len(patch_json) == 0:
            return None
        return patch_json.get(str(patch_id))

    def get_bug_json(self, bug_id):
        bug_url = '{}rest/bug/{}'.format(BUG_SERVER_URL, bug_id)
        bug = self.fetch_data_from_url_with_authentication(bug_url)
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
        self.setProperty('patch_author', patch_author)
        patch_title = patch_json.get('summary')
        if patch_title.startswith(self.revert_preamble):
            self.setProperty('revert', True)
        if self.addURLs:
            self.addURL('Patch by: {}'.format(patch_author), '')
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

    def _is_patch_cq_plus(self, patch_id):
        patch_json = self.get_patch_json(patch_id)
        if not patch_json:
            self._addToLog('stdio', 'Unable to fetch patch {}.\n'.format(patch_id))
            return -1

        for flag in patch_json.get('flags', []):
            if flag.get('name') == 'commit-queue' and flag.get('status') == '+':
                self.setProperty('patch_committer', flag.get('setter', ''))
                return 1
        return 0

    def _does_patch_have_acceptable_review_flag(self, patch_id):
        patch_json = self.get_patch_json(patch_id)
        if not patch_json:
            self._addToLog('stdio', 'Unable to fetch patch {}.\n'.format(patch_id))
            return -1

        for flag in patch_json.get('flags', []):
            if flag.get('name') == 'review':
                review_status = flag.get('status')
                if review_status == '+':
                    patch_reviewer = flag.get('setter', '')
                    self.setProperty('patch_reviewer', patch_reviewer)
                    if self.addURLs:
                        self.addURL('Reviewed by: {}'.format(patch_reviewer), '')
                    return 1
                if review_status in ['-', '?']:
                    self._addToLog('stdio', 'Patch {} is marked r{}.\n'.format(patch_id, review_status))
                    return 0
        return 1  # Patch without review flag is acceptable, since the ChangeLog might have 'Reviewed by' in it.

    def _is_bug_closed(self, bug_id):
        if not bug_id:
            self._addToLog('stdio', 'Skipping bug status validation since bug id is None.\n')
            return -1

        bug_json = self.get_bug_json(bug_id)
        if not bug_json or not bug_json.get('status'):
            self._addToLog('stdio', 'Unable to fetch bug {}.\n'.format(bug_id))
            return -1

        bug_title = bug_json.get('summary')
        sensitive = bug_json.get('product') == 'Security'
        if sensitive:
            self.setProperty('sensitive', True)
            bug_title = ''
        if self.addURLs:
            self.addURL(u'Bug {} {}'.format(bug_id, bug_title), '{}show_bug.cgi?id={}'.format(BUG_SERVER_URL, bug_id))
        if bug_json.get('status') in self.bug_closed_statuses:
            return 1
        return 0

    def get_bugzilla_api_key(self):
        try:
            passwords = json.load(open('passwords.json'))
            return passwords.get('BUGZILLA_API_KEY', '')
        except:
            print('Error in reading Bugzilla api key')
            return ''

    def remove_flags_on_patch(self, patch_id):
        patch_url = '{}rest/bug/attachment/{}'.format(BUG_SERVER_URL, patch_id)
        flags = [{'name': 'review', 'status': 'X'}, {'name': 'commit-queue', 'status': 'X'}]
        try:
            response = requests.put(patch_url, json={'flags': flags, 'Bugzilla_api_key': self.get_bugzilla_api_key()})
            if response.status_code not in [200, 201]:
                self._addToLog('stdio', 'Unable to remove flags on patch {}. Unexpected response code from bugzilla: {}'.format(patch_id, response.status_code))
                return FAILURE
        except Exception as e:
            self._addToLog('stdio', 'Error in removing flags on Patch {}'.format(patch_id))
            return FAILURE
        return SUCCESS

    def set_cq_minus_flag_on_patch(self, patch_id):
        patch_url = '{}rest/bug/attachment/{}'.format(BUG_SERVER_URL, patch_id)
        flags = [{'name': 'commit-queue', 'status': '-'}]
        try:
            response = requests.put(patch_url, json={'flags': flags, 'Bugzilla_api_key': self.get_bugzilla_api_key()})
            if response.status_code not in [200, 201]:
                self._addToLog('stdio', 'Unable to set cq- flag on patch {}. Unexpected response code from bugzilla: {}'.format(patch_id, response.status_code))
                return FAILURE
        except Exception as e:
            self._addToLog('stdio', 'Error in setting cq- flag on patch {}'.format(patch_id))
            return FAILURE
        return SUCCESS

    def close_bug(self, bug_id):
        bug_url = '{}rest/bug/{}'.format(BUG_SERVER_URL, bug_id)
        try:
            response = requests.put(bug_url, json={'status': 'RESOLVED', 'resolution': 'FIXED', 'Bugzilla_api_key': self.get_bugzilla_api_key()})
            if response.status_code not in [200, 201]:
                self._addToLog('stdio', 'Unable to close bug {}. Unexpected response code from bugzilla: {}'.format(bug_id, response.status_code))
                return FAILURE
        except Exception as e:
            self._addToLog('stdio', 'Error in closing bug {}'.format(bug_id))
            return FAILURE
        return SUCCESS

    def comment_on_bug(self, bug_id, comment_text):
        bug_comment_url = '{}rest/bug/{}/comment'.format(BUG_SERVER_URL, bug_id)
        if not comment_text:
            return FAILURE
        try:
            response = requests.post(bug_comment_url, data={'comment': comment_text, 'Bugzilla_api_key': self.get_bugzilla_api_key()})
            if response.status_code not in [200, 201]:
                self._addToLog('stdio', 'Unable to comment on bug {}. Unexpected response code from bugzilla: {}'.format(bug_id, response.status_code))
                return FAILURE
        except Exception as e:
            self._addToLog('stdio', 'Error in commenting on bug {}'.format(bug_id))
            return FAILURE
        return SUCCESS

    def create_bug(self, bug_title, bug_description, component='Tools / Tests', cc_list=None):
        bug_url = '{}rest/bug'.format(BUG_SERVER_URL)
        if not (bug_title and bug_description):
            return FAILURE

        try:
            response = requests.post(bug_url, data={'product': 'WebKit',
                                                    'component': component,
                                                    'version': 'WebKit Nightly Build',
                                                    'summary': bug_title,
                                                    'description': bug_description,
                                                    'cc': cc_list,
                                                    'Bugzilla_api_key': self.get_bugzilla_api_key()})
            if response.status_code not in [200, 201]:
                self._addToLog('stdio', 'Unable to file bug. Unexpected response code from bugzilla: {}'.format(response.status_code))
                return FAILURE
        except Exception as e:
            self._addToLog('stdio', 'Error in creating bug: {}'.format(bug_title))
            return FAILURE
        self._addToLog('stdio', 'Filed bug: {}'.format(bug_title))
        return SUCCESS


class ValidatePatch(buildstep.BuildStep, BugzillaMixin):
    name = 'validate-patch'
    description = ['validate-patch running']
    descriptionDone = ['Validated patch']
    flunkOnFailure = True
    haltOnFailure = True

    def __init__(self, verifyObsolete=True, verifyBugClosed=True, verifyReviewDenied=True, addURLs=True, verifycqplus=False):
        self.verifyObsolete = verifyObsolete
        self.verifyBugClosed = verifyBugClosed
        self.verifyReviewDenied = verifyReviewDenied
        self.verifycqplus = verifycqplus
        self.addURLs = addURLs
        buildstep.BuildStep.__init__(self)

    def getResultSummary(self):
        if self.results == FAILURE:
            return {u'step': unicode(self.descriptionDone)}
        return super(ValidatePatch, self).getResultSummary()

    def skip_build(self, reason):
        self._addToLog('stdio', reason)
        self.finished(FAILURE)
        self.build.results = SKIPPED
        self.descriptionDone = reason
        self.build.buildFinished([reason], SKIPPED)

    def start(self):
        patch_id = self.getProperty('patch_id', '')
        if not patch_id:
            self._addToLog('stdio', 'No patch_id found. Unable to proceed without patch_id.\n')
            self.descriptionDone = 'No patch id found'
            self.finished(FAILURE)
            return None

        bug_id = self.getProperty('bug_id', '') or self.get_bug_id_from_patch(patch_id)

        bug_closed = self._is_bug_closed(bug_id) if self.verifyBugClosed else 0
        if bug_closed == 1:
            self.skip_build('Bug {} is already closed'.format(bug_id))
            return None

        obsolete = self._is_patch_obsolete(patch_id) if self.verifyObsolete else 0
        if obsolete == 1:
            self.skip_build('Patch {} is obsolete'.format(patch_id))
            return None

        review_denied = self._is_patch_review_denied(patch_id) if self.verifyReviewDenied else 0
        if review_denied == 1:
            self.skip_build('Patch {} is marked r-'.format(patch_id))
            return None

        cq_plus = self._is_patch_cq_plus(patch_id) if self.verifycqplus else 1
        if cq_plus != 1:
            self.skip_build('Patch {} is not marked cq+.'.format(patch_id))
            return None

        acceptable_review_flag = self._does_patch_have_acceptable_review_flag(patch_id) if self.verifycqplus else 1
        if acceptable_review_flag != 1:
            self.skip_build('Patch {} does not have acceptable review flag.'.format(patch_id))
            return None

        if obsolete == -1 or review_denied == -1 or bug_closed == -1:
            self.finished(WARNINGS)
            return None

        if self.verifyBugClosed:
            self._addToLog('stdio', 'Bug is open.\n')
        if self.verifyObsolete:
            self._addToLog('stdio', 'Patch is not obsolete.\n')
        if self.verifyReviewDenied:
            self._addToLog('stdio', 'Patch is not marked r-.\n')
        if self.verifycqplus:
            self._addToLog('stdio', 'Patch is marked cq+.\n')
            self._addToLog('stdio', 'Patch have acceptable review flag.\n')
        self.finished(SUCCESS)
        return None


class ValidateCommiterAndReviewer(buildstep.BuildStep):
    name = 'validate-commiter-and-reviewer'
    descriptionDone = ['Validated commiter and reviewer']
    url = 'https://svn.webkit.org/repository/webkit/trunk/Tools/Scripts/webkitpy/common/config/contributors.json'
    contributors = {}

    def load_contributors_from_disk(self):
        cwd = os.path.abspath(os.path.dirname(__file__))
        tools_dir_path = os.path.dirname(os.path.dirname(cwd))
        contributors_path = os.path.join(tools_dir_path, 'Scripts/webkitpy/common/config/contributors.json')
        try:
            return json.load(open(contributors_path))
        except Exception as e:
            self._addToLog('stdio', 'Failed to load {}\n'.format(contributors_path))
            return {}

    def load_contributors_from_trac(self):
        try:
            response = requests.get(self.url)
            if response.status_code != 200:
                self._addToLog('stdio', 'Failed to access {} with status code: {}\n'.format(self.url, response.status_code))
                return {}
            return response.json()
        except Exception as e:
            self._addToLog('stdio', 'Failed to access {url}\n'.format(url=self.url))
            return {}

    def load_contributors(self):
        contributors_json = self.load_contributors_from_trac()
        if not contributors_json:
            contributors_json = self.load_contributors_from_disk()

        contributors = {}
        for key, value in contributors_json.iteritems():
            emails = value.get('emails')
            for email in emails:
                contributors[email.lower()] = {'name': key, 'status': value.get('status')}
        return contributors

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)

    def getResultSummary(self):
        if self.results == FAILURE:
            return {u'step': unicode(self.descriptionDone)}
        return buildstep.BuildStep.getResultSummary(self)

    def fail_build(self, email, status):
        reason = '{} does not have {} permissions'.format(email, status)
        comment = '{} does not have {} permissions according to {}.'.format(email, status, self.url)
        comment += '\n\nRejecting attachment {} from commit queue.'.format(self.getProperty('patch_id', ''))
        self.setProperty('bugzilla_comment_text', comment)

        self._addToLog('stdio', reason)
        self.setProperty('build_finish_summary', reason)
        self.build.addStepsAfterCurrentStep([CommentOnBug(), SetCommitQueueMinusFlagOnPatch()])
        self.finished(FAILURE)
        self.descriptionDone = reason

    def is_reviewer(self, email):
        contributor = self.contributors.get(email)
        return contributor and contributor['status'] == 'reviewer'

    def is_committer(self, email):
        contributor = self.contributors.get(email)
        return contributor and contributor['status'] in ['reviewer', 'committer']

    def full_name_from_email(self, email):
        contributor = self.contributors.get(email)
        if not contributor:
            return ''
        return contributor.get('name')

    def start(self):
        self.contributors = self.load_contributors()
        if not self.contributors:
            self.finished(FAILURE)
            self.descriptionDone = 'Failed to get contributors information'
            self.build.buildFinished(['Failed to get contributors information'], FAILURE)
            return None
        patch_committer = self.getProperty('patch_committer', '').lower()
        if not self.is_committer(patch_committer):
            self.fail_build(patch_committer, 'committer')
            return None
        self._addToLog('stdio', '{} is a valid commiter.\n'.format(patch_committer))

        patch_reviewer = self.getProperty('patch_reviewer', '').lower()
        if not patch_reviewer:
            # Patch does not have r+ flag. This is acceptable, since the ChangeLog might have 'Reviewed by' in it.
            self.descriptionDone = 'Validated committer'
            self.finished(SUCCESS)
            return None

        self.setProperty('patch_reviewer_full_name', self.full_name_from_email(patch_reviewer))
        if not self.is_reviewer(patch_reviewer):
            self.fail_build(patch_reviewer, 'reviewer')
            return None
        self._addToLog('stdio', '{} is a valid reviewer.\n'.format(patch_reviewer))
        self.finished(SUCCESS)
        return None


class ValidateChangeLogAndReviewer(shell.ShellCommand):
    name = 'validate-changelog-and-reviewer'
    descriptionDone = ['Validated ChangeLog and Reviewer']
    command = ['python', 'Tools/Scripts/webkit-patch', 'validate-changelog', '--check-oops', '--non-interactive']
    haltOnFailure = False
    flunkOnFailure = True

    def __init__(self, **kwargs):
        shell.ShellCommand.__init__(self, timeout=3 * 60, logEnviron=False, **kwargs)

    def start(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)
        return shell.ShellCommand.start(self)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {u'step': u'ChangeLog validation failed'}
        return shell.ShellCommand.getResultSummary(self)

    def evaluateCommand(self, cmd):
        rc = shell.ShellCommand.evaluateCommand(self, cmd)
        if rc == FAILURE:
            log_text = self.log_observer.getStdout() + self.log_observer.getStderr()
            self.setProperty('bugzilla_comment_text', log_text)
            self.setProperty('build_finish_summary', 'ChangeLog validation failed')
            self.build.addStepsAfterCurrentStep([CommentOnBug(), SetCommitQueueMinusFlagOnPatch()])
        return rc


class SetCommitQueueMinusFlagOnPatch(buildstep.BuildStep, BugzillaMixin):
    name = 'set-cq-minus-flag-on-patch'

    def start(self):
        patch_id = self.getProperty('patch_id', '')
        build_finish_summary = self.getProperty('build_finish_summary', None)

        rc = self.set_cq_minus_flag_on_patch(patch_id)
        self.finished(rc)
        if build_finish_summary:
            self.build.buildFinished([build_finish_summary], FAILURE)
        return None

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {u'step': u'Set cq- flag on patch'}
        return {u'step': u'Failed to set cq- flag on patch'}


class RemoveFlagsOnPatch(buildstep.BuildStep, BugzillaMixin):
    name = 'remove-flags-from-patch'
    flunkOnFailure = False
    haltOnFailure = False

    def start(self):
        patch_id = self.getProperty('patch_id', '')
        if not patch_id:
            self._addToLog('stdio', 'patch_id build property not found.\n')
            self.descriptionDone = 'No patch id found'
            self.finished(FAILURE)
            return None

        rc = self.remove_flags_on_patch(patch_id)
        self.finished(rc)
        return None

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {u'step': u'Removed flags on bugzilla patch'}
        return {u'step': u'Failed to remove flags on bugzilla patch'}


class CloseBug(buildstep.BuildStep, BugzillaMixin):
    name = 'close-bugzilla-bug'
    flunkOnFailure = False
    haltOnFailure = False

    def start(self):
        self.bug_id = self.getProperty('bug_id', '')
        if not self.bug_id:
            self._addToLog('stdio', 'bug_id build property not found.\n')
            self.descriptionDone = 'No bug id found'
            self.finished(FAILURE)
            return None

        rc = self.close_bug(self.bug_id)
        self.finished(rc)
        return None

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {u'step': u'Closed bug {}'.format(self.bug_id)}
        return {u'step': u'Failed to close bug {}'.format(self.bug_id)}


class CommentOnBug(buildstep.BuildStep, BugzillaMixin):
    name = 'comment-on-bugzilla-bug'
    flunkOnFailure = False
    haltOnFailure = False

    def start(self):
        self.bug_id = self.getProperty('bug_id', '')
        self.comment_text = self.getProperty('bugzilla_comment_text', '')

        if not self.comment_text:
            self._addToLog('stdio', 'bugzilla_comment_text build property not found.\n')
            self.descriptionDone = 'No bugzilla comment found'
            self.finished(WARNINGS)
            return None

        rc = self.comment_on_bug(self.bug_id, self.comment_text)
        self.finished(rc)
        return None

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {u'step': u'Added comment on bug {}'.format(self.bug_id)}
        return {u'step': u'Failed to add comment on bug {}'.format(self.bug_id)}


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
    flunkOnFailure = False
    haltOnFailure = False
    command = ['perl', 'Tools/Scripts/test-webkitperl']

    def __init__(self, **kwargs):
        super(RunWebKitPerlTests, self).__init__(timeout=2 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results == SUCCESS:
            message = 'Passed webkitperl tests'
            self.build.buildFinished([message], SUCCESS)
            return {u'step': unicode(message)}
        return {u'step': u'Failed webkitperl tests'}

    def evaluateCommand(self, cmd):
        rc = shell.ShellCommand.evaluateCommand(self, cmd)
        if rc == FAILURE:
            self.build.addStepsAfterCurrentStep([KillOldProcesses(), ReRunWebKitPerlTests()])
        return rc


class ReRunWebKitPerlTests(RunWebKitPerlTests):
    name = 're-run-webkitperl-tests'
    flunkOnFailure = True
    haltOnFailure = True

    def evaluateCommand(self, cmd):
        return shell.ShellCommand.evaluateCommand(self, cmd)


class RunBuildWebKitOrgUnitTests(shell.ShellCommand):
    name = 'build-webkit-org-unit-tests'
    description = ['build-webkit-unit-tests running']
    command = ['python', 'steps_unittest.py']

    def __init__(self, **kwargs):
        shell.ShellCommand.__init__(self, workdir='build/Tools/BuildSlaveSupport/build.webkit.org-config', timeout=2 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {u'step': u'Passed build.webkit.org unit tests'}
        return {u'step': u'Failed build.webkit.org unit tests'}


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


class RunResultsdbpyTests(shell.ShellCommand):
    name = 'resultsdbpy-unit-tests'
    description = ['resultsdbpy-unit-tests running']
    command = [
        'python3',
        'Tools/resultsdbpy/resultsdbpy/run-tests',
        '--verbose',
        '--no-selenium',
        '--fast-tests',
    ]

    def __init__(self, **kwargs):
        super(RunResultsdbpyTests, self).__init__(timeout=2 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {u'step': u'Passed resultsdbpy unit tests'}
        return {u'step': u'Failed resultsdbpy unit tests'}


class WebKitPyTest(shell.ShellCommand):
    language = 'python'
    descriptionDone = ['webkitpy-tests']
    flunkOnFailure = True

    def __init__(self, **kwargs):
        super(WebKitPyTest, self).__init__(timeout=2 * 60, logEnviron=False, **kwargs)

    def start(self):
        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('json', self.log_observer)
        return shell.ShellCommand.start(self)

    def setBuildSummary(self, build_summary):
        previous_build_summary = self.getProperty('build_summary', '')
        if not previous_build_summary:
            self.setProperty('build_summary', build_summary)
            return

        if build_summary in previous_build_summary:
            # Ensure that we do not append same build summary multiple times in case
            # this method is called multiple times.
            return

        new_build_summary = previous_build_summary + ', ' + build_summary
        self.setProperty('build_summary', new_build_summary)

    def getResultSummary(self):
        if self.results == SUCCESS:
            message = 'Passed webkitpy {} tests'.format(self.language)
            self.setBuildSummary(message)
            return {u'step': unicode(message)}

        logLines = self.log_observer.getStdout()
        json_text = ''.join([line for line in logLines.splitlines()])
        try:
            webkitpy_results = json.loads(json_text)
        except Exception as ex:
            self._addToLog('stderr', 'ERROR: unable to parse data, exception: {}'.format(ex))
            return super(WebKitPyTest, self).getResultSummary()

        failures = webkitpy_results.get('failures', []) + webkitpy_results.get('errors', [])
        if not failures:
            return super(WebKitPyTest, self).getResultSummary()
        pluralSuffix = 's' if len(failures) > 1 else ''
        failures_string = ', '.join([failure.get('name') for failure in failures])
        message = 'Found {} webkitpy {} test failure{}: {}'.format(len(failures), self.language, pluralSuffix, failures_string)
        self.setBuildSummary(message)
        return {u'step': unicode(message)}

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)


class RunWebKitPyPython2Tests(WebKitPyTest):
    language = 'python2'
    name = 'webkitpy-tests-{}'.format(language)
    description = ['webkitpy-tests running ({})'.format(language)]
    jsonFileName = 'webkitpy_test_{}_results.json'.format(language)
    logfiles = {'json': jsonFileName}
    command = ['python', 'Tools/Scripts/test-webkitpy', '--json-output={0}'.format(jsonFileName)]


class RunWebKitPyPython3Tests(WebKitPyTest):
    language = 'python3'
    name = 'webkitpy-tests-{}'.format(language)
    description = ['webkitpy-tests running ({})'.format(language)]
    jsonFileName = 'webkitpy_test_{}_results.json'.format(language)
    logfiles = {'json': jsonFileName}
    command = ['python3', 'Tools/Scripts/test-webkitpy', '--json-output={0}'.format(jsonFileName)]


class InstallGtkDependencies(shell.ShellCommand):
    name = 'jhbuild'
    description = ['updating gtk dependencies']
    descriptionDone = ['Updated gtk dependencies']
    command = ['perl', 'Tools/Scripts/update-webkitgtk-libs', WithProperties('--%(configuration)s')]
    haltOnFailure = True

    def __init__(self, **kwargs):
        super(InstallGtkDependencies, self).__init__(logEnviron=False, **kwargs)


class InstallWpeDependencies(shell.ShellCommand):
    name = 'jhbuild'
    description = ['updating wpe dependencies']
    descriptionDone = ['Updated wpe dependencies']
    command = ['perl', 'Tools/Scripts/update-webkitwpe-libs', WithProperties('--%(configuration)s')]
    haltOnFailure = True

    def __init__(self, **kwargs):
        super(InstallWpeDependencies, self).__init__(logEnviron=False, **kwargs)


def appendCustomBuildFlags(step, platform, fullPlatform):
    # FIXME: Make a common 'supported platforms' list.
    if platform not in ('gtk', 'wincairo', 'ios', 'jsc-only', 'wpe', 'playstation', 'tvos', 'watchos'):
        return
    if 'simulator' in fullPlatform:
        platform = platform + '-simulator'
    elif platform in ['ios', 'tvos', 'watchos']:
        platform = platform + '-device'
    step.setCommand(step.command + ['--' + platform])


class BuildLogLineObserver(logobserver.LogLineObserver, object):
    def __init__(self, errorReceived=None):
        self.errorReceived = errorReceived
        self.error_context_buffer = []
        self.whitespace_re = re.compile('^[\s]*$')
        super(BuildLogLineObserver, self).__init__()

    def outLineReceived(self, line):
        is_whitespace = self.whitespace_re.search(line) is not None
        if is_whitespace:
            self.error_context_buffer = []
        else:
            self.error_context_buffer.append(line)

        if 'rror:' in line and self.errorReceived:
            map(self.errorReceived, self.error_context_buffer)
            self.error_context_buffer = []


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

    def doStepIf(self, step):
        return not (self.getProperty('revert') and self.getProperty('buildername', '').lower() == 'commit-queue')

    def start(self):
        platform = self.getProperty('platform')
        buildOnly = self.getProperty('buildOnly')
        architecture = self.getProperty('architecture')
        additionalArguments = self.getProperty('additionalArguments')

        if platform != 'windows':
            self.addLogObserver('stdio', BuildLogLineObserver(self.errorReceived))

        if additionalArguments:
            self.setCommand(self.command + additionalArguments)
        if platform in ('mac', 'ios', 'tvos', 'watchos') and architecture:
            self.setCommand(self.command + ['ARCHS=' + architecture])
            if platform in ['ios', 'tvos', 'watchos']:
                self.setCommand(self.command + ['ONLY_ACTIVE_ARCH=NO'])
        if platform in ('mac', 'ios', 'tvos', 'watchos') and buildOnly:
            # For build-only bots, the expectation is that tests will be run on separate machines,
            # so we need to package debug info as dSYMs. Only generating line tables makes
            # this much faster than full debug info, and crash logs still have line numbers.
            self.setCommand(self.command + ['DEBUG_INFORMATION_FORMAT=dwarf-with-dsym'])
            self.setCommand(self.command + ['CLANG_DEBUG_INFORMATION_LEVEL=line-tables-only'])

        appendCustomBuildFlags(self, platform, self.getProperty('fullPlatform'))

        return shell.Compile.start(self)

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)

    def errorReceived(self, error):
        self._addToLog('errors', error + '\n')

    def evaluateCommand(self, cmd):
        if cmd.didFail():
            self.setProperty('patchFailedToBuild', True)
            steps_to_add = [UnApplyPatchIfRequired(), ValidatePatch(verifyBugClosed=False, addURLs=False)]
            platform = self.getProperty('platform')
            if platform == 'wpe':
                steps_to_add.append(InstallWpeDependencies())
            elif platform == 'gtk':
                steps_to_add.append(InstallGtkDependencies())
            if self.getProperty('group') == 'jsc':
                steps_to_add.append(CompileJSCWithoutPatch())
            else:
                steps_to_add.append(CompileWebKitWithoutPatch())
            steps_to_add.append(AnalyzeCompileWebKitResults())
            # Using a single addStepsAfterCurrentStep because of https://github.com/buildbot/buildbot/issues/4874
            self.build.addStepsAfterCurrentStep(steps_to_add)
        else:
            triggers = self.getProperty('triggers', None)
            if triggers or not self.skipUpload:
                self.build.addStepsAfterCurrentStep([ArchiveBuiltProduct(), UploadBuiltProduct(), TransferToS3()])

        return super(CompileWebKit, self).evaluateCommand(cmd)

    def getResultSummary(self):
        if self.results == FAILURE:
            return {u'step': u'Failed to compile WebKit'}
        return shell.Compile.getResultSummary(self)


class CompileWebKitWithoutPatch(CompileWebKit):
    name = 'compile-webkit-without-patch'
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
        compile_without_patch_step = CompileWebKitWithoutPatch.name
        if self.getProperty('group') == 'jsc':
            compile_without_patch_step = CompileJSCWithoutPatch.name
        compile_without_patch_result = self.getStepResult(compile_without_patch_step)

        if compile_without_patch_result == FAILURE:
            self.finished(FAILURE)
            message = 'Unable to build WebKit without patch, retrying build'
            self.descriptionDone = message
            self.send_email_for_build_failure()
            self.build.buildFinished([message], RETRY)
            return defer.succeed(None)

        self.finished(FAILURE)
        self.build.results = FAILURE
        patch_id = self.getProperty('patch_id', '')
        message = 'Patch {} does not build'.format(patch_id)
        self.descriptionDone = message
        if self.getProperty('buildername', '').lower() == 'commit-queue':
            self.setProperty('bugzilla_comment_text', message)
            self.setProperty('build_finish_summary', message)
            self.build.addStepsAfterCurrentStep([CommentOnBug(), SetCommitQueueMinusFlagOnPatch()])
        else:
            self.build.buildFinished([message], FAILURE)

        return defer.succeed(None)

    def getStepResult(self, step_name):
        for step in self.build.executedSteps:
            if step.name == step_name:
                return step.results

    def send_email_for_build_failure(self):
        try:
            builder_name = self.getProperty('buildername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)

            email_subject = 'Build failure on trunk on {}'.format(builder_name)
            email_text = 'Failed to build WebKit without patch in {}\n\nBuilder: {}'.format(build_url, builder_name)
            send_email_to_bot_watchers(email_subject, email_text)
        except Exception as e:
            print('Error in sending email for build failure: {}'.format(e))


class CompileJSC(CompileWebKit):
    name = 'compile-jsc'
    descriptionDone = ['Compiled JSC']
    command = ['perl', 'Tools/Scripts/build-jsc', WithProperties('--%(configuration)s')]

    def start(self):
        self.setProperty('group', 'jsc')
        return CompileWebKit.start(self)

    def getResultSummary(self):
        if self.results == FAILURE:
            return {u'step': u'Failed to compile JSC'}
        return shell.Compile.getResultSummary(self)


class CompileJSCWithoutPatch(CompileJSC):
    name = 'compile-jsc-without-patch'

    def evaluateCommand(self, cmd):
        return shell.Compile.evaluateCommand(self, cmd)


class RunJavaScriptCoreTests(shell.Test):
    name = 'jscore-test'
    description = ['jscore-tests running']
    descriptionDone = ['jscore-tests']
    flunkOnFailure = True
    jsonFileName = 'jsc_results.json'
    logfiles = {'json': jsonFileName}
    command = ['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', '--json-output={0}'.format(jsonFileName), WithProperties('--%(configuration)s')]
    prefix = 'jsc_'
    NUM_FAILURES_TO_DISPLAY_IN_STATUS = 5

    def __init__(self, **kwargs):
        shell.Test.__init__(self, logEnviron=False, **kwargs)
        self.binaryFailures = []
        self.stressTestFailures = []

    def start(self):
        self.log_observer_json = logobserver.BufferLogObserver()
        self.addLogObserver('json', self.log_observer_json)

        # add remotes configuration file path to the command line if needed
        remotesfile = self.getProperty('remotes', False)
        if remotesfile:
            self.command.append('--remote-config-file={0}'.format(remotesfile))

        platform = self.getProperty('platform')
        if platform == 'jsc-only' and remotesfile:
            self.command.extend(['--no-testmasm', '--no-testair', '--no-testb3', '--no-testdfg', '--no-testapi', '--memory-limited'])
        appendCustomBuildFlags(self, self.getProperty('platform'), self.getProperty('fullPlatform'))
        return shell.Test.start(self)

    def evaluateCommand(self, cmd):
        rc = shell.Test.evaluateCommand(self, cmd)
        if rc == SUCCESS or rc == WARNINGS:
            message = 'Passed JSC tests'
            self.descriptionDone = message
            self.build.results = SUCCESS
            self.build.buildFinished([message], SUCCESS)
        else:
            self.build.addStepsAfterCurrentStep([ValidatePatch(verifyBugClosed=False, addURLs=False), KillOldProcesses(), ReRunJavaScriptCoreTests()])
        return rc

    def commandComplete(self, cmd):
        shell.Test.commandComplete(self, cmd)
        logLines = self.log_observer_json.getStdout()
        json_text = ''.join([line for line in logLines.splitlines()])
        try:
            jsc_results = json.loads(json_text)
        except Exception as ex:
            self._addToLog('stderr', 'ERROR: unable to parse data, exception: {}'.format(ex))
            return

        if jsc_results.get('allMasmTestsPassed') == False:
            self.binaryFailures.append('testmasm')
        if jsc_results.get('allAirTestsPassed') == False:
            self.binaryFailures.append('testair')
        if jsc_results.get('allB3TestsPassed') == False:
            self.binaryFailures.append('testb3')
        if jsc_results.get('allDFGTestsPassed') == False:
            self.binaryFailures.append('testdfg')
        if jsc_results.get('allApiTestsPassed') == False:
            self.binaryFailures.append('testapi')

        self.stressTestFailures = jsc_results.get('stressTestFailures')
        if self.stressTestFailures:
            self.setProperty(self.prefix + 'stress_test_failures', self.stressTestFailures)
        if self.binaryFailures:
            self.setProperty(self.prefix + 'binary_failures', self.binaryFailures)

    def getResultSummary(self):
        if self.results != SUCCESS and (self.stressTestFailures or self.binaryFailures):
            status = ''
            if self.stressTestFailures:
                num_failures = len(self.stressTestFailures)
                pluralSuffix = 's' if num_failures > 1 else ''
                failures_to_display = self.stressTestFailures[:self.NUM_FAILURES_TO_DISPLAY_IN_STATUS]
                status = 'Found {} jsc stress test failure{}: '.format(num_failures, pluralSuffix) + ', '.join(failures_to_display)
                if num_failures > self.NUM_FAILURES_TO_DISPLAY_IN_STATUS:
                    status += ' ...'
            if self.binaryFailures:
                if status:
                    status += ', '
                pluralSuffix = 's' if len(self.binaryFailures) > 1 else ''
                status += 'JSC test binary failure{}: {}'.format(pluralSuffix, ', '.join(self.binaryFailures))

            return {u'step': unicode(status)}

        return shell.Test.getResultSummary(self)

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)


class ReRunJavaScriptCoreTests(RunJavaScriptCoreTests):
    name = 'jscore-test-rerun'
    prefix = 'jsc_rerun_'

    def evaluateCommand(self, cmd):
        rc = shell.Test.evaluateCommand(self, cmd)
        first_run_failures = set(self.getProperty('jsc_stress_test_failures', []) + self.getProperty('jsc_binary_failures', []))
        second_run_failures = set(self.getProperty('jsc_rerun_stress_test_failures', []) + self.getProperty('jsc_rerun_binary_failures', []))
        flaky_failures = first_run_failures.union(second_run_failures) - first_run_failures.intersection(second_run_failures)
        flaky_failures_string = ', '.join(flaky_failures)

        if rc == SUCCESS or rc == WARNINGS:
            pluralSuffix = 's' if len(flaky_failures) > 1 else ''
            message = 'Found flaky test{}: {}'.format(pluralSuffix, flaky_failures_string)
            self.descriptionDone = message
            self.build.results = SUCCESS
            self.build.buildFinished([message], SUCCESS)
        else:
            self.setProperty('patchFailedTests', True)
            self.build.addStepsAfterCurrentStep([UnApplyPatchIfRequired(),
                                                ValidatePatch(verifyBugClosed=False, addURLs=False),
                                                CompileJSCWithoutPatch(),
                                                ValidatePatch(verifyBugClosed=False, addURLs=False),
                                                KillOldProcesses(),
                                                RunJSCTestsWithoutPatch(),
                                                AnalyzeJSCTestsResults()])
        return rc


class RunJSCTestsWithoutPatch(RunJavaScriptCoreTests):
    name = 'jscore-test-without-patch'
    prefix = 'jsc_clean_tree_'

    def evaluateCommand(self, cmd):
        return shell.Test.evaluateCommand(self, cmd)


class AnalyzeJSCTestsResults(buildstep.BuildStep):
    name = 'analyze-jsc-tests-results'
    description = ['analyze-jsc-test-results']
    descriptionDone = ['analyze-jsc-tests-results']
    NUM_FAILURES_TO_DISPLAY = 10

    def start(self):
        first_run_stress_failures = set(self.getProperty('jsc_stress_test_failures', []))
        first_run_binary_failures = set(self.getProperty('jsc_binary_failures', []))
        second_run_stress_failures = set(self.getProperty('jsc_rerun_stress_test_failures', []))
        second_run_binary_failures = set(self.getProperty('jsc_rerun_binary_failures', []))
        clean_tree_stress_failures = set(self.getProperty('jsc_clean_tree_stress_test_failures', []))
        clean_tree_binary_failures = set(self.getProperty('jsc_clean_tree_binary_failures', []))
        clean_tree_failures = list(clean_tree_binary_failures) + list(clean_tree_stress_failures)
        clean_tree_failures_string = ', '.join(clean_tree_failures[:self.NUM_FAILURES_TO_DISPLAY])

        stress_failures_with_patch = first_run_stress_failures.intersection(second_run_stress_failures)
        binary_failures_with_patch = first_run_binary_failures.intersection(second_run_binary_failures)

        flaky_stress_failures = first_run_stress_failures.union(second_run_stress_failures) - first_run_stress_failures.intersection(second_run_stress_failures)
        flaky_binary_failures = first_run_binary_failures.union(second_run_binary_failures) - first_run_binary_failures.intersection(second_run_binary_failures)
        flaky_failures = list(flaky_binary_failures) + list(flaky_stress_failures)
        flaky_failures_string = ', '.join(flaky_failures)

        new_stress_failures = stress_failures_with_patch - clean_tree_stress_failures
        new_binary_failures = binary_failures_with_patch - clean_tree_binary_failures
        new_stress_failures_to_display = ', '.join(list(new_stress_failures)[:self.NUM_FAILURES_TO_DISPLAY])
        new_binary_failures_to_display = ', '.join(list(new_binary_failures)[:self.NUM_FAILURES_TO_DISPLAY])

        self._addToLog('stderr', '\nFailures in first run: {}'.format(list(first_run_binary_failures) + list(first_run_stress_failures)))
        self._addToLog('stderr', '\nFailures in second run: {}'.format(list(second_run_binary_failures) + list(second_run_stress_failures)))
        self._addToLog('stderr', '\nFlaky Tests: {}'.format(flaky_failures_string))
        self._addToLog('stderr', '\nFailures on clean tree: {}'.format(list(clean_tree_stress_failures) + list(clean_tree_binary_failures)))

        if new_stress_failures or new_binary_failures:
            self._addToLog('stderr', '\nNew failures: {}\n'.format(list(new_binary_failures) + list(new_stress_failures)))
            self.finished(FAILURE)
            self.build.results = FAILURE
            message = ''
            if new_binary_failures:
                pluralSuffix = 's' if len(new_binary_failures) > 1 else ''
                message = 'Found {} new JSC binary failure{}: {}'.format(len(new_binary_failures), pluralSuffix, new_binary_failures_to_display)
            if new_stress_failures:
                if message:
                    message += ', '
                pluralSuffix = 's' if len(new_stress_failures) > 1 else ''
                message += 'Found {} new JSC stress test failure{}: {}'.format(len(new_stress_failures), pluralSuffix, new_stress_failures_to_display)
                if len(new_stress_failures) > self.NUM_FAILURES_TO_DISPLAY:
                    message += ' ...'
            self.descriptionDone = message
            self.build.buildFinished([message], FAILURE)
        else:
            self._addToLog('stderr', '\nNo new failures\n')
            self.finished(SUCCESS)
            self.build.results = SUCCESS
            self.descriptionDone = 'Passed JSC tests'
            pluralSuffix = 's' if len(clean_tree_failures) > 1 else ''
            message = ''
            if clean_tree_failures:
                message = 'Found {} pre-existing JSC test failure{}: {}'.format(len(clean_tree_failures), pluralSuffix, clean_tree_failures_string)
            if len(clean_tree_failures) > self.NUM_FAILURES_TO_DISPLAY:
                message += ' ...'
            if flaky_failures:
                message += ' Found flaky tests: {}'.format(flaky_failures_string)
            self.build.buildFinished([message], SUCCESS)
        return defer.succeed(None)

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)


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

    def evaluateCommand(self, cmd):
        if self.results in [FAILURE, EXCEPTION]:
            self.build.buildFinished(['Failed to kill old processes, retrying build'], RETRY)
        return shell.Compile.evaluateCommand(self, cmd)

    def getResultSummary(self):
        if self.results in [FAILURE, EXCEPTION]:
            return {u'step': u'Failed to kill old processes'}
        return shell.Compile.getResultSummary(self)


class TriggerCrashLogSubmission(shell.Compile):
    name = 'trigger-crash-log-submission'
    description = ['triggering crash log submission']
    descriptionDone = ['Triggered crash log submission']
    command = ['python', 'Tools/BuildSlaveSupport/trigger-crash-log-submission']

    def __init__(self, **kwargs):
        super(TriggerCrashLogSubmission, self).__init__(timeout=60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results in [FAILURE, EXCEPTION]:
            return {u'step': u'Failed to trigger crash log submission'}
        return shell.Compile.getResultSummary(self)


class WaitForCrashCollection(shell.Compile):
    name = 'wait-for-crash-collection'
    description = ['waiting-for-crash-collection-to-quiesce']
    descriptionDone = ['Crash collection has quiesced']
    command = ['python', 'Tools/BuildSlaveSupport/wait-for-crash-collection', '--timeout', str(5 * 60)]

    def __init__(self, **kwargs):
        super(WaitForCrashCollection, self).__init__(timeout=6 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results in [FAILURE, EXCEPTION]:
            return {u'step': u'Crash log collection process still running'}
        return shell.Compile.getResultSummary(self)


class RunWebKitTests(shell.Test):
    name = 'layout-tests'
    description = ['layout-tests running']
    descriptionDone = ['layout-tests']
    resultDirectory = 'layout-test-results'
    jsonFileName = 'layout-test-results/full_results.json'
    logfiles = {'json': jsonFileName}
    test_failures_log_name = 'test-failures'
    command = ['python', 'Tools/Scripts/run-webkit-tests',
               '--no-build',
               '--no-show-results',
               '--no-new-test-results',
               '--clobber-old-results',
               WithProperties('--%(configuration)s')]

    def __init__(self, **kwargs):
        shell.Test.__init__(self, logEnviron=False, **kwargs)
        self.incorrectLayoutLines = []

    def doStepIf(self, step):
        return not ((self.getProperty('buildername', '').lower() == 'commit-queue') and
                    (self.getProperty('revert') or self.getProperty('passed_mac_wk2')))

    def start(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)
        self.log_observer_json = logobserver.BufferLogObserver()
        self.addLogObserver('json', self.log_observer_json)

        platform = self.getProperty('platform')
        appendCustomBuildFlags(self, platform, self.getProperty('fullPlatform'))
        additionalArguments = self.getProperty('additionalArguments')

        if self.getProperty('use-dump-render-tree', False):
            self.setCommand(self.command + ['--dump-render-tree'])

        self.setCommand(self.command + ['--results-directory', self.resultDirectory])
        self.setCommand(self.command + ['--debug-rwt-logging'])

        patch_author = self.getProperty('patch_author')
        if patch_author in ['webkit-wpt-import-bot@igalia.com']:
            self.setCommand(self.command + ['imported/w3c/web-platform-tests'])
        else:
            self.setCommand(self.command + ['--exit-after-n-failures', '30', '--skip-failing-tests'])

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

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)

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
            if first_results.failing_tests:
                self._addToLog(self.test_failures_log_name, '\n'.join(first_results.failing_tests))
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
            self.setProperty('build_summary', message)
        else:
            self.build.addStepsAfterCurrentStep([
                ArchiveTestResults(),
                UploadTestResults(),
                ExtractTestResults(),
                ValidatePatch(verifyBugClosed=False, addURLs=False),
                KillOldProcesses(),
                ReRunWebKitTests(),
            ])
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
        first_results_did_exceed_test_failure_limit = self.getProperty('first_results_exceed_failure_limit')
        first_results_failing_tests = set(self.getProperty('first_run_failures', []))
        second_results_did_exceed_test_failure_limit = self.getProperty('second_results_exceed_failure_limit')
        second_results_failing_tests = set(self.getProperty('second_run_failures', []))
        tests_that_consistently_failed = first_results_failing_tests.intersection(second_results_failing_tests)
        flaky_failures = first_results_failing_tests.union(second_results_failing_tests) - first_results_failing_tests.intersection(second_results_failing_tests)
        flaky_failures_string = ', '.join(flaky_failures)

        if rc == SUCCESS or rc == WARNINGS:
            message = 'Passed layout tests'
            self.descriptionDone = message
            self.build.results = SUCCESS
            if not first_results_did_exceed_test_failure_limit:
                pluralSuffix = 's' if len(flaky_failures) > 1 else ''
                message = 'Found flaky test{}: {}'.format(pluralSuffix, flaky_failures_string)
            self.setProperty('build_summary', message)
            for flaky_failure in flaky_failures:
                self.send_email_for_flaky_failure(flaky_failure)
        else:
            self.setProperty('patchFailedTests', True)
            self.build.addStepsAfterCurrentStep([ArchiveTestResults(),
                                                UploadTestResults(identifier='rerun'),
                                                ExtractTestResults(identifier='rerun'),
                                                UnApplyPatchIfRequired(),
                                                ValidatePatch(verifyBugClosed=False, addURLs=False),
                                                CompileWebKitWithoutPatch(),
                                                ValidatePatch(verifyBugClosed=False, addURLs=False),
                                                KillOldProcesses(),
                                                RunWebKitTestsWithoutPatch()])
        return rc

    def commandComplete(self, cmd):
        shell.Test.commandComplete(self, cmd)
        logText = self.log_observer.getStdout() + self.log_observer.getStderr()
        logTextJson = self.log_observer_json.getStdout()

        second_results = LayoutTestFailures.results_from_string(logTextJson)

        if second_results:
            self.setProperty('second_results_exceed_failure_limit', second_results.did_exceed_test_failure_limit)
            self.setProperty('second_run_failures', second_results.failing_tests)
            if second_results.failing_tests:
                self._addToLog(self.test_failures_log_name, '\n'.join(second_results.failing_tests))
        self._parseRunWebKitTestsOutput(logText)

    def send_email_for_flaky_failure(self, test_name):
        try:
            builder_name = self.getProperty('buildername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)

            email_subject = 'Flaky test: {}'.format(test_name)
            email_text = 'Test {} flaked in {}\n\nBuilder: {}'.format(test_name, build_url, builder_name)
            send_email_to_bot_watchers(email_subject, email_text)
        except Exception as e:
            # Catching all exceptions here to ensure that failure to send email doesn't impact the build
            print('Error in sending email for flaky failures: {}'.format(e))


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
            if clean_tree_results.failing_tests:
                self._addToLog(self.test_failures_log_name, '\n'.join(clean_tree_results.failing_tests))
        self._parseRunWebKitTestsOutput(logText)


class AnalyzeLayoutTestsResults(buildstep.BuildStep):
    name = 'analyze-layout-tests-results'
    description = ['analyze-layout-test-results']
    descriptionDone = ['analyze-layout-tests-results']
    NUM_FAILURES_TO_DISPLAY = 10

    def report_failure(self, new_failures):
        self.finished(FAILURE)
        self.build.results = FAILURE
        pluralSuffix = 's' if len(new_failures) > 1 else ''
        new_failures_string = ', '.join(sorted(new_failures)[:self.NUM_FAILURES_TO_DISPLAY])
        message = 'Found {} new test failure{}: {}'.format(len(new_failures), pluralSuffix, new_failures_string)
        if len(new_failures) > self.NUM_FAILURES_TO_DISPLAY:
            message += ' ...'
        self.descriptionDone = message

        if self.getProperty('buildername', '').lower() == 'commit-queue':
            self.setProperty('bugzilla_comment_text', message)
            self.setProperty('build_finish_summary', message)
            self.build.addStepsAfterCurrentStep([CommentOnBug(), SetCommitQueueMinusFlagOnPatch()])
        else:
            self.build.buildFinished([message], FAILURE)
        return defer.succeed(None)

    def report_pre_existing_failures(self, clean_tree_failures, flaky_failures):
        self.finished(SUCCESS)
        self.build.results = SUCCESS
        self.descriptionDone = 'Passed layout tests'
        message = ''
        if clean_tree_failures:
            clean_tree_failures_string = ', '.join(sorted(clean_tree_failures)[:self.NUM_FAILURES_TO_DISPLAY])
            pluralSuffix = 's' if len(clean_tree_failures) > 1 else ''
            message = 'Found {} pre-existing test failure{}: {}'.format(len(clean_tree_failures), pluralSuffix, clean_tree_failures_string)
            if len(clean_tree_failures) > self.NUM_FAILURES_TO_DISPLAY:
                message += ' ...'
            for clean_tree_failure in clean_tree_failures:
                self.send_email_for_pre_existing_failure(clean_tree_failure)

        if flaky_failures:
            flaky_failures_string = ', '.join(sorted(flaky_failures)[:self.NUM_FAILURES_TO_DISPLAY])
            pluralSuffix = 's' if len(flaky_failures) > 1 else ''
            message += ' Found flaky test{}: {}'.format(pluralSuffix, flaky_failures_string)
            if len(flaky_failures) > self.NUM_FAILURES_TO_DISPLAY:
                message += ' ...'
            for flaky_failure in flaky_failures:
                self.send_email_for_flaky_failure(flaky_failure)

        self.setProperty('build_summary', message)
        return defer.succeed(None)

    def retry_build(self, message=''):
        self.finished(RETRY)
        message = 'Unable to confirm if test failures are introduced by patch, retrying build'
        self.descriptionDone = message
        self.build.buildFinished([message], RETRY)
        return defer.succeed(None)

    def _results_failed_different_tests(self, first_results_failing_tests, second_results_failing_tests):
        return first_results_failing_tests != second_results_failing_tests

    def send_email_for_flaky_failure(self, test_name):
        try:
            builder_name = self.getProperty('buildername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)

            email_subject = 'Flaky test: {}'.format(test_name)
            email_text = 'Test {} flaked in {}\n\nBuilder: {}'.format(test_name, build_url, builder_name)
            send_email_to_bot_watchers(email_subject, email_text)
        except Exception as e:
            print('Error in sending email for flaky failure: {}'.format(e))

    def send_email_for_pre_existing_failure(self, test_name):
        try:
            builder_name = self.getProperty('buildername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)

            email_subject = 'Pre-existing test failure: {}'.format(test_name)
            email_text = 'Test {} failed on clean tree run in {}.\nBuilder: {}'.format(test_name, build_url, builder_name)
            send_email_to_bot_watchers(email_subject, email_text)
        except Exception as e:
            print('Error in sending email for pre-existing failure: {}'.format(e))

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
        flaky_failures = first_results_failing_tests.union(second_results_failing_tests) - first_results_failing_tests.intersection(second_results_failing_tests)

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
            # were introduced. This is a bit of a grey-zone. It's possible that the patch introduced some flakiness.
            # We still mark the build as SUCCESS.
            return self.report_pre_existing_failures(clean_tree_results_failing_tests, flaky_failures)

        if clean_tree_results_did_exceed_test_failure_limit:
            return self.retry_build()
        failures_introduced_by_patch = first_results_failing_tests - clean_tree_results_failing_tests
        if failures_introduced_by_patch:
            return self.report_failure(failures_introduced_by_patch)

        # At this point, we know that the first and second runs had the exact same failures,
        # and that those failures are all present on the clean tree, so we can say with certainty
        # that the patch is good.
        return self.report_pre_existing_failures(clean_tree_results_failing_tests, flaky_failures)


class RunWebKit1Tests(RunWebKitTests):
    def start(self):
        self.setProperty('use-dump-render-tree', True)
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
        kwargs['mode'] = 0o0644
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
        return results == SUCCESS and self.getProperty('sensitive', False)

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
    flunkOnFailure = False

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {u'step': u'Failed to download built product from S3'}
        return super(DownloadBuiltProduct, self).getResultSummary()

    def __init__(self, **kwargs):
        super(DownloadBuiltProduct, self).__init__(logEnviron=False, **kwargs)

    def evaluateCommand(self, cmd):
        rc = shell.ShellCommand.evaluateCommand(self, cmd)
        if rc == FAILURE:
            self.build.addStepsAfterCurrentStep([DownloadBuiltProductFromMaster()])
        return rc


class DownloadBuiltProductFromMaster(DownloadBuiltProduct):
    command = ['python', 'Tools/BuildSlaveSupport/download-built-product',
        WithProperties('--%(configuration)s'),
        WithProperties(EWS_BUILD_URL + 'archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(patch_id)s.zip')]
    haltOnFailure = True
    flunkOnFailure = True

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {u'step': u'Failed to download built product from build master'}
        return shell.ShellCommand.getResultSummary(self)

    def evaluateCommand(self, cmd):
        return shell.ShellCommand.evaluateCommand(self, cmd)


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
        platform = self.getProperty('platform')
        if platform == 'gtk':
            command = ['python', 'Tools/Scripts/run-gtk-tests',
                       '--{0}'.format(self.getProperty('configuration')),
                       '--json-output={0}'.format(self.jsonFileName)]
            self.setCommand(command)
        else:
            appendCustomBuildFlags(self, platform, self.getProperty('fullPlatform'))
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
            self.build.addStepsAfterCurrentStep([ValidatePatch(verifyBugClosed=False, addURLs=False), KillOldProcesses(), ReRunAPITests()])
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
            steps_to_add = [UnApplyPatchIfRequired(), ValidatePatch(verifyBugClosed=False, addURLs=False)]
            platform = self.getProperty('platform')
            if platform == 'wpe':
                steps_to_add.append(InstallWpeDependencies())
            elif platform == 'gtk':
                steps_to_add.append(InstallGtkDependencies())
            steps_to_add.append(CompileWebKitWithoutPatch())
            steps_to_add.append(ValidatePatch(verifyBugClosed=False, addURLs=False))
            steps_to_add.append(KillOldProcesses())
            steps_to_add.append(RunAPITestsWithoutPatch())
            steps_to_add.append(AnalyzeAPITestsResults())
            # Using a single addStepsAfterCurrentStep because of https://github.com/buildbot/buildbot/issues/4874
            self.build.addStepsAfterCurrentStep(steps_to_add)

        return rc


class RunAPITestsWithoutPatch(RunAPITests):
    name = 'run-api-tests-without-patch'

    def evaluateCommand(self, cmd):
        return TestWithFailureCount.evaluateCommand(self, cmd)


class AnalyzeAPITestsResults(buildstep.BuildStep):
    name = 'analyze-api-tests-results'
    description = ['analyze-api-test-results']
    descriptionDone = ['analyze-api-tests-results']
    NUM_API_FAILURES_TO_DISPLAY = 10

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

        if not (first_run_results and second_run_results):
            self.finished(RETRY)
            self.build.buildFinished(['Unable to parse API test results'], RETRY)
            return -1

        def getAPITestFailures(result):
            if not result:
                return set([])
            # TODO: Analyze Time-out, Crash and Failure independently
            return set([failure.get('name') for failure in result.get('Timedout', [])] +
                [failure.get('name') for failure in result.get('Crashed', [])] +
                [failure.get('name') for failure in result.get('Failed', [])])

        first_run_failures = getAPITestFailures(first_run_results)
        second_run_failures = getAPITestFailures(second_run_results)
        clean_tree_failures = getAPITestFailures(clean_tree_results)
        clean_tree_failures_to_display = list(clean_tree_failures)[:self.NUM_API_FAILURES_TO_DISPLAY]
        clean_tree_failures_string = ', '.join(clean_tree_failures_to_display)

        failures_with_patch = first_run_failures.intersection(second_run_failures)
        flaky_failures = first_run_failures.union(second_run_failures) - first_run_failures.intersection(second_run_failures)
        flaky_failures_string = ', '.join(flaky_failures)
        new_failures = failures_with_patch - clean_tree_failures
        new_failures_to_display = list(new_failures)[:self.NUM_API_FAILURES_TO_DISPLAY]
        new_failures_string = ', '.join(new_failures_to_display)

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
            if len(new_failures) > self.NUM_API_FAILURES_TO_DISPLAY:
                message += ' ...'
            self.descriptionDone = message
            self.build.buildFinished([message], FAILURE)
        else:
            self._addToLog('stderr', '\nNo new failures\n')
            self.finished(SUCCESS)
            self.build.results = SUCCESS
            self.descriptionDone = 'Passed API tests'
            pluralSuffix = 's' if len(clean_tree_failures) > 1 else ''
            message = ''
            if clean_tree_failures:
                message = 'Found {} pre-existing API test failure{}: {}'.format(len(clean_tree_failures), pluralSuffix, clean_tree_failures_string)
            if len(clean_tree_failures) > self.NUM_API_FAILURES_TO_DISPLAY:
                message += ' ...'
            if flaky_failures:
                message += ' Found flaky tests: {}'.format(flaky_failures_string)
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
        kwargs['mode'] = 0o0644
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
        self.command = ['unzip', '-q', self.zipFile, '-d', self.resultDirectory]

        master.MasterShellCommand.__init__(self, command=self.command, logEnviron=False)

    def resultDirectoryURL(self):
        path = self.resultDirectory.replace('public_html/results/', '') + '/'
        return '{}{}'.format(S3_RESULTS_URL, path)

    def resultsDownloadURL(self):
        path = self.zipFile.replace('public_html/results/', '')
        return '{}{}'.format(S3_RESULTS_URL, path)

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
    command_list_win = [['df', '-hl']]

    def __init__(self, **kwargs):
        super(PrintConfiguration, self).__init__(timeout=60, **kwargs)
        self.commands = []
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

    def run(self):
        command_list = list(self.command_list_generic)
        platform = self.getProperty('platform', '*')
        if platform != 'jsc-only':
            platform = platform.split('-')[0]
        if platform in ('mac', 'ios', 'tvos', 'watchos', '*'):
            command_list.extend(self.command_list_apple)
        elif platform in ('gtk', 'wpe', 'jsc-only'):
            command_list.extend(self.command_list_linux)
        elif platform in ('win'):
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


class SetBuildSummary(buildstep.BuildStep):
    name = 'set-build-summary'
    descriptionDone = ['Set build summary']
    alwaysRun = True
    haltOnFailure = False
    flunkOnFailure = False

    def doStepIf(self, step):
        return self.getProperty('build_summary', False)

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)

    def start(self):
        build_summary = self.getProperty('build_summary', 'build successful')
        self.finished(SUCCESS)
        self.build.buildFinished([build_summary], self.build.results)
        return defer.succeed(None)


class FindModifiedChangeLogs(shell.ShellCommand):
    name = 'find-modified-changelogs'
    descriptionDone = ['Found modified ChangeLogs']
    command = ['git', 'diff', '-r', '--name-status', '--no-renames', '--no-ext-diff', '--full-index']
    haltOnFailure = False

    def __init__(self, **kwargs):
        shell.ShellCommand.__init__(self, timeout=3 * 60, logEnviron=False, **kwargs)

    def start(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)
        return shell.ShellCommand.start(self)

    def getResultSummary(self):
        if self.results != SUCCESS:
            patch_id = self.getProperty('patch_id', '')
            return {u'step': u'Failed to find any modified ChangeLog in Patch {}'.format(patch_id)}
        return shell.ShellCommand.getResultSummary(self)

    def evaluateCommand(self, cmd):
        rc = shell.ShellCommand.evaluateCommand(self, cmd)
        log_text = self.log_observer.getStdout() + self.log_observer.getStderr()
        modified_changelogs = self.extract_changelogs(log_text, self._status_regexp('MA'))
        self.setProperty('modified_changelogs', modified_changelogs)
        if rc == FAILURE or not modified_changelogs:
            patch_id = self.getProperty('patch_id', '')
            message = 'Unable to find any modified ChangeLog in Patch {}'.format(patch_id)
            if self.getProperty('buildername', '').lower() == 'commit-queue':
                self.setProperty('bugzilla_comment_text', message.replace('Patch', 'Attachment'))
                self.setProperty('build_finish_summary', message)
                self.build.addStepsAfterCurrentStep([CommentOnBug(), SetCommitQueueMinusFlagOnPatch()])
            else:
                self.build.buildFinished([message], FAILURE)
        return rc

    def is_path_to_changelog(self, path):
        return os.path.basename(path) == 'ChangeLog'

    def _status_regexp(self, expected_types):
        return '^(?P<status>[{}])\t(?P<filename>.+)$'.format(expected_types)

    def extract_changelogs(self, output, status_regexp):
        filenames = []
        for line in output.splitlines():
            match = re.search(status_regexp, line)
            if not match:
                continue
            filename = match.group('filename')
            if self.is_path_to_changelog(filename):
                filenames.append(filename)
        return filenames


class CreateLocalGITCommit(shell.ShellCommand):
    name = 'create-local-git-commit'
    descriptionDone = ['Created local git commit']
    haltOnFailure = False

    def __init__(self, **kwargs):
        shell.ShellCommand.__init__(self, timeout=5 * 60, logEnviron=False, **kwargs)

    def start(self):
        self.failure_message = None
        modified_changelogs = self.getProperty('modified_changelogs')
        patch_id = self.getProperty('patch_id', '')
        if not modified_changelogs:
            self.failure_message = u'No modified ChangeLog file found for Patch {}'.format(patch_id)
            self.finished(FAILURE)
            return None

        modified_changelogs = ' '.join(modified_changelogs)
        self.command = 'perl Tools/Scripts/commit-log-editor --print-log {}'.format(modified_changelogs)
        self.command += ' | git commit --all -F -'
        return shell.ShellCommand.start(self)

    def getResultSummary(self):
        if self.failure_message:
            return {u'step': self.failure_message}
        if self.results != SUCCESS:
            return {u'step': u'Failed to create git commit'}
        return shell.ShellCommand.getResultSummary(self)

    def evaluateCommand(self, cmd):
        rc = shell.ShellCommand.evaluateCommand(self, cmd)
        if rc == FAILURE:
            patch_id = self.getProperty('patch_id', '')
            message = self.failure_message or 'Failed to create git commit for Patch {}'.format(patch_id)
            if self.getProperty('buildername', '').lower() == 'commit-queue':
                self.setProperty('bugzilla_comment_text', message.replace('Patch', 'Attachment'))
                self.setProperty('build_finish_summary', message)
                self.build.addStepsAfterCurrentStep([CommentOnBug(), SetCommitQueueMinusFlagOnPatch()])
            else:
                self.build.buildFinished([message], FAILURE)
        return rc


class PushCommitToWebKitRepo(shell.ShellCommand):
    name = 'push-commit-to-webkit-repo'
    descriptionDone = ['Pushed commit to WebKit repository']
    command = ['git', 'svn', 'dcommit', '--rmdir']
    commit_success_regexp = '^Committed r(?P<svn_revision>\d+)$'
    haltOnFailure = False

    def __init__(self, **kwargs):
        shell.ShellCommand.__init__(self, timeout=5 * 60, logEnviron=False, **kwargs)

    def start(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)
        return shell.ShellCommand.start(self)

    def evaluateCommand(self, cmd):
        rc = shell.ShellCommand.evaluateCommand(self, cmd)
        if rc == SUCCESS:
            log_text = self.log_observer.getStdout() + self.log_observer.getStderr()
            svn_revision = self.svn_revision_from_commit_text(log_text)
            self.setProperty('bugzilla_comment_text', self.comment_text_for_bug(svn_revision))
            commit_summary = 'Committed r{}'.format(svn_revision)
            self.descriptionDone = commit_summary
            self.setProperty('build_summary', 'Committed r{}'.format(svn_revision))
            self.build.addStepsAfterCurrentStep([CommentOnBug(), RemoveFlagsOnPatch(), CloseBug()])
            self.addURL('r{}'.format(svn_revision), self.url_for_revision(svn_revision))
        else:
            self.setProperty('bugzilla_comment_text', self.comment_text_for_bug())
            self.setProperty('build_finish_summary', 'Failed to commit to WebKit repository')
            self.build.addStepsAfterCurrentStep([CommentOnBug(), SetCommitQueueMinusFlagOnPatch()])
        return rc

    def url_for_revision(self, revision):
        return 'https://trac.webkit.org/changeset/{}'.format(revision)

    def comment_text_for_bug(self, svn_revision=None):
        patch_id = self.getProperty('patch_id', '')
        if not svn_revision:
            return 'commit-queue failed to commit attachment {} to WebKit repository.'.format(patch_id)
        comment = 'Committed r{}: <{}>'.format(svn_revision, self.url_for_revision(svn_revision))
        comment += '\n\nAll reviewed patches have been landed. Closing bug and clearing flags on attachment {}.'.format(patch_id)
        return comment

    def svn_revision_from_commit_text(self, commit_text):
        match = re.search(self.commit_success_regexp, commit_text, re.MULTILINE)
        return match.group('svn_revision')

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {u'step': u'Failed to push commit to Webkit repository'}
        return shell.ShellCommand.getResultSummary(self)


class CheckPatchStatusOnEWSQueues(buildstep.BuildStep, BugzillaMixin):
    name = 'check-status-on-other-ewses'
    descriptionDone = ['Checked patch status on other queues']

    def get_patch_status(self, patch_id, queue):
        url = '{}status/{}'.format(EWS_URL, patch_id)
        try:
            response = requests.get(url)
            if response.status_code != 200:
                self._addToLog('stdio', 'Failed to access {} with status code: {}\n'.format(url, response.status_code))
                return -1
            queue_data = response.json().get(queue)
            if not queue_data:
                return -1
            return queue_data.get('state')
        except Exception as e:
            self._addToLog('stdio', 'Failed to access {}\n'.format(url))
            return -1

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)

    def start(self):
        patch_id = self.getProperty('patch_id', '')
        patch_status_on_mac_wk2 = self.get_patch_status(patch_id, 'mac-wk2')
        if patch_status_on_mac_wk2 == SUCCESS:
            self.setProperty('passed_mac_wk2', True)
        self.finished(SUCCESS)
        return None
