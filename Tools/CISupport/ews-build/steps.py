# Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
from buildbot.process.results import Results, SUCCESS, FAILURE, CANCELLED, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.steps import master, shell, transfer, trigger
from buildbot.steps.source import git
from buildbot.steps.worker import CompositeStepMixin
from datetime import date
from twisted.internet import defer

from layout_test_failures import LayoutTestFailures
from send_email import send_email_to_patch_author, send_email_to_bot_watchers, send_email_to_github_admin

import json
import os
import re
import requests
import socket
import sys
import time

if sys.version_info < (3, 5):
    print('ERROR: Please use Python 3. This code is not compatible with Python 2.')
    sys.exit(1)

BUG_SERVER_URL = 'https://bugs.webkit.org/'
COMMITS_INFO_URL = 'https://commits.webkit.org/'
S3URL = 'https://s3-us-west-2.amazonaws.com/'
S3_RESULTS_URL = 'https://ews-build.s3-us-west-2.amazonaws.com/'
CURRENT_HOSTNAME = socket.gethostname().strip()
EWS_BUILD_HOSTNAME = 'ews-build.webkit.org'
EWS_URL = 'https://ews.webkit.org/'
RESULTS_DB_URL = 'https://results.webkit.org/'
WithProperties = properties.WithProperties
Interpolate = properties.Interpolate


class ConfigureBuild(buildstep.BuildStep):
    name = 'configure-build'
    description = ['configuring build']
    descriptionDone = ['Configured build']

    def __init__(self, platform, configuration, architectures, buildOnly, triggers, remotes, additionalArguments, triggered_by=None):
        super(ConfigureBuild, self).__init__()
        self.platform = platform
        if platform != 'jsc-only':
            self.platform = platform.split('-', 1)[0]
        self.fullPlatform = platform
        self.configuration = configuration
        self.architecture = ' '.join(architectures) if architectures else None
        self.buildOnly = buildOnly
        self.triggers = triggers
        self.triggered_by = triggered_by
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
        if self.triggers and not self.getProperty('triggers'):
            self.setProperty('triggers', self.triggers, 'config.json')
        if self.triggered_by:
            self.setProperty('triggered_by', self.triggered_by, 'config.json')
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
            self.addURL('Patch {}'.format(patch_id), Bugzilla.patch_url(patch_id))


class CheckOutSource(git.Git):
    name = 'clean-and-update-working-directory'
    CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR = (0, 2)
    haltOnFailure = False

    def __init__(self, repourl='https://github.com/WebKit/WebKit.git', **kwargs):
        super(CheckOutSource, self).__init__(repourl=repourl,
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
            return {'step': 'Failed to updated working directory'}
        else:
            return {'step': 'Cleaned and updated working directory'}


class CleanUpGitIndexLock(shell.ShellCommand):
    name = 'clean-git-index-lock'
    command = ['rm', '-f', '.git/index.lock']
    descriptionDone = ['Deleted .git/index.lock']

    def __init__(self, **kwargs):
        super(CleanUpGitIndexLock, self).__init__(timeout=2 * 60, logEnviron=False, **kwargs)

    def start(self):
        platform = self.getProperty('platform', '*')
        if platform == 'wincairo':
            self.command = ['del', r'.git\index.lock']

        self.send_email_for_git_issue()
        return shell.ShellCommand.start(self)

    def evaluateCommand(self, cmd):
        self.build.buildFinished(['Git issue, retrying build'], RETRY)
        return super(CleanUpGitIndexLock, self).evaluateCommand(cmd)

    def send_email_for_git_issue(self):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)

            email_subject = 'Git issue on {}'.format(worker_name)
            email_text = 'Git issue on {}\n\nBuild: {}\n\nBuilder: {}'.format(worker_name, build_url, builder_name)
            send_email_to_bot_watchers(email_subject, email_text, builder_name, worker_name)
        except Exception as e:
            print('Error in sending email for git issue: {}'.format(e))


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


class GitResetHard(shell.ShellCommand):
    name = 'git-reset-hard'
    descriptionDone = ['Performed git reset --hard']

    def __init__(self, **kwargs):
        super(GitResetHard, self).__init__(logEnviron=False, **kwargs)

    def start(self):
        self.setCommand(['git', 'reset', 'HEAD~10', '--hard'])
        return shell.ShellCommand.start(self)


class FetchBranches(shell.ShellCommand):
    name = 'fetch-branch-references'
    descriptionDone = ['Updated branch information']
    command = ['git', 'fetch']
    flunkOnFailure = False
    haltOnFailure = False

    def __init__(self, **kwargs):
        super(FetchBranches, self).__init__(timeout=5 * 60, logEnviron=False, **kwargs)

    def hideStepIf(self, results, step):
        return results == SUCCESS


class ShowIdentifier(shell.ShellCommand):
    name = 'show-identifier'
    identifier_re = '^Identifier: (.*)$'
    flunkOnFailure = False
    haltOnFailure = False

    def __init__(self, **kwargs):
        shell.ShellCommand.__init__(self, timeout=5 * 60, logEnviron=False, **kwargs)

    def start(self):
        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('stdio', self.log_observer)
        revision = self.getProperty('ews_revision', self.getProperty('got_revision'))
        if not revision:
            revision = 'HEAD'
        self.setCommand(['python3', 'Tools/Scripts/git-webkit', 'find', revision])
        return shell.ShellCommand.start(self)

    def evaluateCommand(self, cmd):
        rc = shell.ShellCommand.evaluateCommand(self, cmd)
        if rc != SUCCESS:
            return rc

        log_text = self.log_observer.getStdout()
        match = re.search(self.identifier_re, log_text, re.MULTILINE)
        if match:
            identifier = match.group(1)
            if identifier:
                identifier = identifier.replace('master', 'main')
            self.setProperty('identifier', identifier)
            ews_revision = self.getProperty('ews_revision')
            if ews_revision:
                step = self.getLastBuildStepByName(CheckOutSpecificRevision.name)
            else:
                step = self.getLastBuildStepByName(CheckOutSource.name)
            if not step:
                step = self
            step.addURL('Updated to {}'.format(identifier), self.url_for_identifier(identifier))
            self.descriptionDone = 'Identifier: {}'.format(identifier)
        else:
            self.descriptionDone = 'Failed to find identifier'
        return rc

    def getLastBuildStepByName(self, name):
        for step in reversed(self.build.executedSteps):
            if name in step.name:
                return step
        return None

    def url_for_identifier(self, identifier):
        return '{}{}'.format(COMMITS_INFO_URL, identifier)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to find identifier'}
        return shell.ShellCommand.getResultSummary(self)

    def hideStepIf(self, results, step):
        return results == SUCCESS


class CleanWorkingDirectory(shell.ShellCommand):
    name = 'clean-working-directory'
    description = ['clean-working-directory running']
    descriptionDone = ['Cleaned working directory']
    flunkOnFailure = True
    haltOnFailure = True
    command = ['python3', 'Tools/Scripts/clean-webkit']

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
    flunkOnFailure = True
    haltOnFailure = True
    command = ['perl', 'Tools/Scripts/update-webkit']

    def __init__(self, **kwargs):
        super(UpdateWorkingDirectory, self).__init__(logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to updated working directory'}
        else:
            return {'step': 'Updated working directory'}

    def evaluateCommand(self, cmd):
        rc = shell.ShellCommand.evaluateCommand(self, cmd)
        if rc == FAILURE:
            self.build.buildFinished(['Git issue, retrying build'], RETRY)
        return rc


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
            # Forced build, don't have patch_id raw data on the request, need to fech it.
            patch_id = self.getProperty('patch_id', '')
            self.command = ['/bin/sh', '-c', 'curl -L "https://bugs.webkit.org/attachment.cgi?id={}" -o .buildbot-diff && {}'.format(patch_id, ' '.join(self.command))]
            shell.ShellCommand.start(self)
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
            return {'step': 'svn-apply failed to apply patch to trunk'}
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


class AnalyzePatch(buildstep.BuildStep):
    flunkOnFailure = True
    haltOnFailure = True

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

    def getResultSummary(self):
        if self.results in [FAILURE, SKIPPED]:
            return {'step': 'Patch doesn\'t have relevant changes'}
        if self.results == SUCCESS:
            return {'step': 'Patch contains relevant changes'}
        return buildstep.BuildStep.getResultSummary(self)


class CheckPatchRelevance(AnalyzePatch):
    name = 'check-patch-relevance'
    description = ['check-patch-relevance running']
    descriptionDone = ['Patch contains relevant changes']
    MAX_LINE_SIZE = 250

    bindings_path_regexes = [
        re.compile(rb'Source/WebCore', re.IGNORECASE),
        re.compile(rb'Tools', re.IGNORECASE),
    ]

    services_path_regexes = [
        re.compile(rb'Tools/CISupport/build-webkit-org', re.IGNORECASE),
        re.compile(rb'Tools/CISupport/ews-build', re.IGNORECASE),
        re.compile(rb'Tools/CISupport/Shared', re.IGNORECASE),
        re.compile(rb'Tools/Scripts/libraries/resultsdbpy', re.IGNORECASE),
        re.compile(rb'Tools/Scripts/libraries/webkitcorepy', re.IGNORECASE),
        re.compile(rb'Tools/Scripts/libraries/webkitscmpy', re.IGNORECASE),
    ]

    jsc_path_regexes = [
        re.compile(rb'.*jsc.*', re.IGNORECASE),
        re.compile(rb'.*javascriptcore.*', re.IGNORECASE),
        re.compile(rb'JSTests/', re.IGNORECASE),
        re.compile(rb'Source/WTF/', re.IGNORECASE),
        re.compile(rb'Source/bmalloc/', re.IGNORECASE),
        re.compile(rb'.*Makefile.*', re.IGNORECASE),
        re.compile(rb'Tools/Scripts/build-webkit', re.IGNORECASE),
        re.compile(rb'Tools/Scripts/webkitdirs.pm', re.IGNORECASE),
    ]

    wk1_path_regexes = [
        re.compile(rb'Source/WebKitLegacy', re.IGNORECASE),
        re.compile(rb'Source/WebCore', re.IGNORECASE),
        re.compile(rb'Source/WebInspectorUI', re.IGNORECASE),
        re.compile(rb'Source/WebDriver', re.IGNORECASE),
        re.compile(rb'Source/WTF', re.IGNORECASE),
        re.compile(rb'Source/bmalloc', re.IGNORECASE),
        re.compile(rb'Source/JavaScriptCore', re.IGNORECASE),
        re.compile(rb'Source/ThirdParty', re.IGNORECASE),
        re.compile(rb'LayoutTests', re.IGNORECASE),
        re.compile(rb'Tools', re.IGNORECASE),
    ]

    big_sur_builder_path_regexes = [
        re.compile(rb'Source/', re.IGNORECASE),
        re.compile(rb'Tools/', re.IGNORECASE),
    ]
    webkitpy_path_regexes = [
        re.compile(rb'Tools/Scripts/webkitpy', re.IGNORECASE),
        re.compile(rb'Tools/Scripts/libraries', re.IGNORECASE),
        re.compile(rb'Tools/Scripts/commit-log-editor', re.IGNORECASE),
        re.compile(rb'Source/WebKit/Scripts', re.IGNORECASE),
    ]

    group_to_paths_mapping = {
        'bindings': bindings_path_regexes,
        'bigsur-release-build': big_sur_builder_path_regexes,
        'services-ews': services_path_regexes,
        'jsc': jsc_path_regexes,
        'webkitpy': webkitpy_path_regexes,
        'wk1-tests': wk1_path_regexes,
        'windows': wk1_path_regexes,
    }

    def _patch_is_relevant(self, patch, builderName, timeout=30):
        group = [group for group in self.group_to_paths_mapping.keys() if group.lower() in builderName.lower()]
        if not group:
            # This builder doesn't have paths defined, all patches are relevant.
            return True

        relevant_path_regexes = self.group_to_paths_mapping.get(group[0], [])
        start = time.time()

        for change in patch.splitlines():
            for regex in relevant_path_regexes:
                if type(change) == str:
                    change = change.encode(encoding='utf-8', errors='replace')
                if regex.search(change[:self.MAX_LINE_SIZE]):
                    return True
            if time.time() > start + timeout:
                return False
        return False

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


class FindModifiedLayoutTests(AnalyzePatch):
    name = 'find-modified-layout-tests'
    RE_LAYOUT_TEST = br'^(\+\+\+).*(LayoutTests.*\.html)'
    DIRECTORIES_TO_IGNORE = ['reference', 'reftest', 'resources', 'support', 'script-tests', 'tools']
    SUFFIXES_TO_IGNORE = ['-expected', '-expected-mismatch', '-ref', '-notref']

    def __init__(self, skipBuildIfNoResult=True):
        self.skipBuildIfNoResult = skipBuildIfNoResult
        buildstep.BuildStep.__init__(self)

    def find_test_names_from_patch(self, patch):
        tests = []
        for line in patch.splitlines():
            match = re.search(self.RE_LAYOUT_TEST, line, re.IGNORECASE)
            if match:
                if any((suffix + '.html').encode('utf-8') in line for suffix in self.SUFFIXES_TO_IGNORE):
                    continue
                test_name = match.group(2).decode('utf-8')
                if any(directory in test_name.split('/') for directory in self.DIRECTORIES_TO_IGNORE):
                    continue
                tests.append(test_name)
        return list(set(tests))

    def start(self):
        patch = self._get_patch()
        if not patch:
            self.finished(SUCCESS)
            return None

        tests = self.find_test_names_from_patch(patch)

        if tests:
            self._addToLog('stdio', 'This patch modifies following tests: {}'.format(tests))
            self.setProperty('modified_tests', tests)
            self.finished(SUCCESS)
            return None

        self._addToLog('stdio', 'This patch does not modify any layout tests')
        self.finished(SKIPPED)
        if self.skipBuildIfNoResult:
            self.build.results = SKIPPED
            self.build.buildFinished(['Patch {} doesn\'t have relevant changes'.format(self.getProperty('patch_id', ''))], SKIPPED)
        return None


class Bugzilla(object):
    @classmethod
    def bug_url(cls, bug_id):
        if not bug_id:
            return ''
        return '{}show_bug.cgi?id={}'.format(BUG_SERVER_URL, bug_id)

    @classmethod
    def patch_url(cls, patch_id):
        if not patch_id:
            return ''
        return '{}attachment.cgi?id={}&action=prettypatch'.format(BUG_SERVER_URL, patch_id)


class BugzillaMixin(object):
    addURLs = False
    bug_open_statuses = ['UNCONFIRMED', 'NEW', 'ASSIGNED', 'REOPENED']
    bug_closed_statuses = ['RESOLVED', 'VERIFIED', 'CLOSED']
    fast_cq_preambles = ('revert of r', 'fast-cq', '[fast-cq]')
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
            response = requests.get(url, timeout=60, params={'Bugzilla_api_key': self.get_bugzilla_api_key()})
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
            response = requests.get(url, timeout=60)
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
        try:
            patch_json = patch.json().get('attachments')
        except Exception as e:
            print('Failed to fetch patch json from {}, error: {}'.format(patch_url, e))
            return None
        if not patch_json or len(patch_json) == 0:
            return None
        return patch_json.get(str(patch_id))

    def get_bug_json(self, bug_id):
        bug_url = '{}rest/bug/{}'.format(BUG_SERVER_URL, bug_id)
        bug = self.fetch_data_from_url_with_authentication(bug_url)
        if not bug:
            return None
        try:
            bugs_json = bug.json().get('bugs')
        except Exception as e:
            print('Failed to fetch bug json from {}, error: {}'.format(bug_url, e))
            return None
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
        if patch_title.lower().startswith(self.fast_cq_preambles):
            self.setProperty('fast_commit_queue', True)
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
        self.setProperty('bug_title', bug_title)
        sensitive = bug_json.get('product') == 'Security'
        if sensitive:
            self.setProperty('sensitive', True)
            bug_title = ''
        if self.addURLs:
            self.addURL('Bug {} {}'.format(bug_id, bug_title), Bugzilla.bug_url(bug_id))
        if bug_json.get('status') in self.bug_closed_statuses:
            return 1
        return 0

    def should_send_email(self, patch_id):
        patch_json = self.get_patch_json(patch_id)
        if not patch_json:
            self._addToLog('stdio', 'Unable to fetch patch {}'.format(patch_id))
            return True

        obsolete = patch_json.get('is_obsolete')
        if obsolete == 1:
            self._addToLog('stdio', 'Skipping email since patch {} is obsolete'.format(patch_id))
            return False

        review_denied = False
        for flag in patch_json.get('flags', []):
            if flag.get('name') == 'review' and flag.get('status') == '-':
                review_denied = True

        if review_denied:
            self._addToLog('stdio', 'Skipping email since patch {} is marked r-'.format(patch_id))
            return False
        return True

    def get_bugzilla_api_key(self):
        try:
            passwords = json.load(open('passwords.json'))
            return passwords.get('BUGZILLA_API_KEY', '')
        except Exception as e:
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
            return {'step': self.descriptionDone}
        return super(ValidatePatch, self).getResultSummary()

    def doStepIf(self, step):
        return not self.getProperty('skip_validation', False)

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
    url = 'https://raw.githubusercontent.com/WebKit/WebKit/main/metadata/contributors.json'
    contributors = {}

    def load_contributors_from_disk(self):
        cwd = os.path.abspath(os.path.dirname(__file__))
        repo_root = os.path.dirname(os.path.dirname(os.path.dirname(cwd)))
        contributors_path = os.path.join(repo_root, 'metadata/contributors.json')
        try:
            with open(contributors_path, 'rb') as contributors_json:
                return json.load(contributors_json)
        except Exception as e:
            self._addToLog('stdio', 'Failed to load {}\n'.format(contributors_path))
            return {}

    def load_contributors_from_github(self):
        try:
            response = requests.get(self.url, timeout=60)
            if response.status_code != 200:
                self._addToLog('stdio', 'Failed to access {} with status code: {}\n'.format(self.url, response.status_code))
                return {}
            return response.json()
        except Exception as e:
            self._addToLog('stdio', 'Failed to access {url}\n'.format(url=self.url))
            return {}

    def load_contributors(self):
        contributors_json = self.load_contributors_from_github()
        if not contributors_json:
            contributors_json = self.load_contributors_from_disk()

        contributors = {}
        for value in contributors_json:
            name = value.get('name')
            emails = value.get('emails')
            if name and emails:
                bugzilla_email = emails[0].lower()  # We're requiring that the first email is the primary bugzilla email
                contributors[bugzilla_email] = {'name': name, 'status': value.get('status')}
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
            return {'step': self.descriptionDone}
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
    command = ['python3', 'Tools/Scripts/webkit-patch', 'validate-changelog', '--check-oops', '--non-interactive']
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
            return {'step': 'ChangeLog validation failed'}
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

        rc = SKIPPED
        if CURRENT_HOSTNAME == EWS_BUILD_HOSTNAME:
            rc = self.set_cq_minus_flag_on_patch(patch_id)
        self.finished(rc)
        if build_finish_summary:
            self.build.buildFinished([build_finish_summary], FAILURE)
        return None

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': 'Set cq- flag on patch'}
        elif self.results == SKIPPED:
            return buildstep.BuildStep.getResultSummary(self)
        return {'step': 'Failed to set cq- flag on patch'}


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
            return {'step': 'Removed flags on bugzilla patch'}
        return {'step': 'Failed to remove flags on bugzilla patch'}


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
            return {'step': 'Closed bug {}'.format(self.bug_id)}
        return {'step': 'Failed to close bug {}'.format(self.bug_id)}


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
            return {'step': 'Added comment on bug {}'.format(self.bug_id)}
        elif self.results == SKIPPED:
            return buildstep.BuildStep.getResultSummary(self)
        return {'step': 'Failed to add comment on bug {}'.format(self.bug_id)}

    def doStepIf(self, step):
        return CURRENT_HOSTNAME == EWS_BUILD_HOSTNAME


class UnApplyPatchIfRequired(CleanWorkingDirectory):
    name = 'unapply-patch'
    descriptionDone = ['Unapplied patch']

    def doStepIf(self, step):
        return self.getProperty('patchFailedToBuild') or self.getProperty('patchFailedTests')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


class Trigger(trigger.Trigger):
    def __init__(self, schedulerNames, include_revision=True, triggers=None, **kwargs):
        self.include_revision = include_revision
        self.triggers = triggers
        set_properties = self.propertiesToPassToTriggers() or {}
        super(Trigger, self).__init__(schedulerNames=schedulerNames, set_properties=set_properties, **kwargs)

    def propertiesToPassToTriggers(self):
        properties_to_pass = {
            'patch_id': properties.Property('patch_id'),
            'bug_id': properties.Property('bug_id'),
            'configuration': properties.Property('configuration'),
            'platform': properties.Property('platform'),
            'fullPlatform': properties.Property('fullPlatform'),
            'architecture': properties.Property('architecture'),
            'owner': properties.Property('owner'),
        }
        if self.include_revision:
            properties_to_pass['ews_revision'] = properties.Property('got_revision')
        if self.triggers:
            properties_to_pass['triggers'] = self.triggers
        return properties_to_pass


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
                status += ' ({})'.format(Results[self.results])

        return {'step': status}


class CheckStyle(TestWithFailureCount):
    name = 'check-webkit-style'
    description = ['check-webkit-style running']
    descriptionDone = ['check-webkit-style']
    flunkOnFailure = True
    failedTestsFormatString = '%d style error%s'
    command = ['python3', 'Tools/Scripts/check-webkit-style']

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
    command = ['python3', 'Tools/Scripts/run-bindings-tests', '--json-output={0}'.format(jsonFileName)]

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
            return {'step': message}

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
        return {'step': message}

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
            return {'step': message}
        return {'step': 'Failed webkitperl tests'}

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
    command = ['python3', 'runUnittests.py', 'build-webkit-org']

    def __init__(self, **kwargs):
        super(RunBuildWebKitOrgUnitTests, self).__init__(workdir='build/Tools/CISupport', timeout=2 * 60, logEnviron=False, **kwargs)

    def start(self):
        return shell.ShellCommand.start(self)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': 'Passed build.webkit.org unit tests'}
        return {'step': 'Failed build.webkit.org unit tests'}


class RunEWSUnitTests(shell.ShellCommand):
    name = 'ews-unit-tests'
    description = ['ews-unit-tests running']
    command = ['python3', 'runUnittests.py', 'ews-build']

    def __init__(self, **kwargs):
        super(RunEWSUnitTests, self).__init__(workdir='build/Tools/CISupport', timeout=2 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': 'Passed EWS unit tests'}
        return {'step': 'Failed EWS unit tests'}


class RunBuildbotCheckConfig(shell.ShellCommand):
    name = 'buildbot-check-config'
    description = ['buildbot-checkconfig running']
    command = ['buildbot', 'checkconfig']
    directory = 'build/Tools/CISupport/ews-build'
    timeout = 2 * 60

    def __init__(self, **kwargs):
        super(RunBuildbotCheckConfig, self).__init__(workdir=self.directory, timeout=self.timeout, logEnviron=False, **kwargs)

    def start(self):
        self.workerEnvironment['LC_CTYPE'] = 'en_US.UTF-8'
        return shell.ShellCommand.start(self)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': 'Passed buildbot checkconfig'}
        return {'step': 'Failed buildbot checkconfig'}


class RunBuildbotCheckConfigForEWS(RunBuildbotCheckConfig):
    name = 'buildbot-check-config-for-ews'
    directory = 'build/Tools/CISupport/ews-build'


class RunBuildbotCheckConfigForBuildWebKit(RunBuildbotCheckConfig):
    name = 'buildbot-check-config-for-build-webkit'
    directory = 'build/Tools/CISupport/build-webkit-org'


class RunResultsdbpyTests(shell.ShellCommand):
    name = 'resultsdbpy-unit-tests'
    description = ['resultsdbpy-unit-tests running']
    command = [
        'python3',
        'Tools/Scripts/libraries/resultsdbpy/resultsdbpy/run-tests',
        '--verbose',
        '--no-selenium',
        '--fast-tests',
    ]

    def __init__(self, **kwargs):
        super(RunResultsdbpyTests, self).__init__(timeout=2 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': 'Passed resultsdbpy unit tests'}
        return {'step': 'Failed resultsdbpy unit tests'}


class WebKitPyTest(shell.ShellCommand):
    language = 'python'
    descriptionDone = ['webkitpy-tests']
    flunkOnFailure = True
    NUM_FAILURES_TO_DISPLAY = 10

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
            return {'step': message}

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
        failures_string = ', '.join([failure.get('name') for failure in failures[:self.NUM_FAILURES_TO_DISPLAY]])
        message = 'Found {} webkitpy {} test failure{}: {}'.format(len(failures), self.language, pluralSuffix, failures_string)
        if len(failures) > self.NUM_FAILURES_TO_DISPLAY:
            message += ' ...'
        self.setBuildSummary(message)
        return {'step': message}

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
    command = ['python', 'Tools/Scripts/test-webkitpy', '--verbose', '--json-output={0}'.format(jsonFileName)]


class RunWebKitPyPython3Tests(WebKitPyTest):
    language = 'python3'
    name = 'webkitpy-tests-{}'.format(language)
    description = ['webkitpy-tests running ({})'.format(language)]
    jsonFileName = 'webkitpy_test_{}_results.json'.format(language)
    logfiles = {'json': jsonFileName}
    command = ['python3', 'Tools/Scripts/test-webkitpy', '--verbose', '--json-output={0}'.format(jsonFileName)]


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
    def __init__(self, errorReceived, searchString='rror:', includeRelatedLines=True):
        self.errorReceived = errorReceived
        self.searchString = searchString
        self.includeRelatedLines = includeRelatedLines
        self.error_context_buffer = []
        self.whitespace_re = re.compile(r'^[\s]*$')
        super(BuildLogLineObserver, self).__init__()

    def outLineReceived(self, line):
        if not self.errorReceived:
            return

        if not self.includeRelatedLines:
            if self.searchString in line:
                self.errorReceived(line)
            return

        is_whitespace = self.whitespace_re.search(line) is not None
        if is_whitespace:
            self.error_context_buffer = []
        else:
            self.error_context_buffer.append(line)

        if self.searchString in line:
            for log in self.error_context_buffer[-50:]:
                self.errorReceived(log)
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
        return not (self.getProperty('fast_commit_queue') and self.getProperty('buildername', '').lower() == 'commit-queue')

    def start(self):
        platform = self.getProperty('platform')
        buildOnly = self.getProperty('buildOnly')
        architecture = self.getProperty('architecture')
        additionalArguments = self.getProperty('additionalArguments')

        if platform in ['win', 'wincairo']:
            self.addLogObserver('stdio', BuildLogLineObserver(self.errorReceived, searchString='error ', includeRelatedLines=False))
        else:
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
        if platform == 'gtk':
            prefix = os.path.join("/app", "webkit", "WebKitBuild", self.getProperty("configuration"), "install")
            self.setCommand(self.command + [f'--prefix={prefix}'])

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
                steps_to_add = [ArchiveBuiltProduct(), UploadBuiltProduct(), TransferToS3()]
                if triggers:
                    steps_to_add.append(Trigger(schedulerNames=triggers))
                self.build.addStepsAfterCurrentStep(steps_to_add)

        return super(CompileWebKit, self).evaluateCommand(cmd)

    def getResultSummary(self):
        if self.results == FAILURE:
            return {'step': 'Failed to compile WebKit'}
        if self.results == SKIPPED:
            if self.getProperty('fast_commit_queue'):
                return {'step': 'Skipped compiling WebKit in fast-cq mode'}
            return {'step': 'Skipped compiling WebKit'}
        return shell.Compile.getResultSummary(self)


class CompileWebKitWithoutPatch(CompileWebKit):
    name = 'compile-webkit-without-patch'
    haltOnFailure = False

    def __init__(self, retry_build_on_failure=False, **kwargs):
        self.retry_build_on_failure = retry_build_on_failure
        super(CompileWebKitWithoutPatch, self).__init__(**kwargs)

    def doStepIf(self, step):
        return self.getProperty('patchFailedToBuild') or self.getProperty('patchFailedTests')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)

    def evaluateCommand(self, cmd):
        rc = shell.Compile.evaluateCommand(self, cmd)
        if rc == FAILURE and self.retry_build_on_failure:
            message = 'Unable to build WebKit without patch, retrying build'
            self.descriptionDone = message
            self.send_email_for_unexpected_build_failure()
            self.build.buildFinished([message], RETRY)
        return rc

    def send_email_for_unexpected_build_failure(self):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            email_subject = '{} might be in bad state, unable to build WebKit'.format(worker_name)
            email_text = '{} might be in bad state. It is unable to build WebKit.'.format(worker_name)
            email_text += ' Same patch was built successfuly on builder queue previously.\n\nBuild: {}\n\nBuilder: {}'.format(build_url, builder_name)
            reference = 'build-failure-{}'.format(worker_name)
            send_email_to_bot_watchers(email_subject, email_text, builder_name, reference)
        except Exception as e:
            print('Error in sending email for unexpected build failure: {}'.format(e))


class AnalyzeCompileWebKitResults(buildstep.BuildStep, BugzillaMixin):
    name = 'analyze-compile-webkit-results'
    description = ['analyze-compile-webkit-results']
    descriptionDone = ['analyze-compile-webkit-results']

    def start(self):
        self.error_logs = {}
        self.compile_webkit_step = CompileWebKit.name
        if self.getProperty('group') == 'jsc':
            self.compile_webkit_step = CompileJSC.name
        d = self.getResults(self.compile_webkit_step)
        d.addCallback(lambda res: self.analyzeResults())
        return defer.succeed(None)

    def analyzeResults(self):
        compile_without_patch_step = CompileWebKitWithoutPatch.name
        if self.getProperty('group') == 'jsc':
            compile_without_patch_step = CompileJSCWithoutPatch.name
        compile_without_patch_result = self.getStepResult(compile_without_patch_step)

        if compile_without_patch_result == FAILURE:
            message = 'Unable to build WebKit without patch, retrying build'
            self.descriptionDone = message
            self.send_email_for_preexisting_build_failure()
            self.finished(FAILURE)
            self.build.buildFinished([message], RETRY)
            return defer.succeed(None)

        self.build.results = FAILURE
        patch_id = self.getProperty('patch_id', '')
        message = 'Patch {} does not build'.format(patch_id)
        self.send_email_for_new_build_failure()

        self.descriptionDone = message
        self.finished(FAILURE)
        self.setProperty('build_finish_summary', message)
        if self.getProperty('buildername', '').lower() == 'commit-queue':
            self.setProperty('bugzilla_comment_text', message)
            self.build.addStepsAfterCurrentStep([CommentOnBug(), SetCommitQueueMinusFlagOnPatch()])
        else:
            self.build.addStepsAfterCurrentStep([SetCommitQueueMinusFlagOnPatch()])

    @defer.inlineCallbacks
    def getResults(self, name):
        step = self.getBuildStepByName(name)
        if not step:
            defer.returnValue(None)

        logs = yield self.master.db.logs.getLogs(step.stepid)
        log = next((log for log in logs if log['name'] == 'errors'), None)
        if not log:
            defer.returnValue(None)

        lastline = int(max(0, log['num_lines'] - 1))
        logLines = yield self.master.db.logs.getLogLines(log['id'], 0, lastline)
        if log['type'] == 's':
            logLines = '\n'.join([line[1:] for line in logLines.splitlines()])

        self.error_logs[name] = logLines

    def getStepResult(self, step_name):
        for step in self.build.executedSteps:
            if step.name == step_name:
                return step.results

    def getBuildStepByName(self, step_name):
        for step in self.build.executedSteps:
            if step.name == step_name:
                return step
        return None

    def filter_logs_containing_error(self, logs, searchString='rror:', max_num_lines=10):
        if not logs:
            return None
        filtered_logs = []
        for line in logs.splitlines():
            if searchString in line:
                filtered_logs.append(line)
        return '\n'.join(filtered_logs[-max_num_lines:])

    def send_email_for_new_build_failure(self):
        try:
            patch_id = self.getProperty('patch_id', '')
            if not self.should_send_email(patch_id):
                return
            builder_name = self.getProperty('buildername', '')
            bug_id = self.getProperty('bug_id', '')
            bug_title = self.getProperty('bug_title', '')
            worker_name = self.getProperty('workername', '')
            patch_author = self.getProperty('patch_author', '')
            platform = self.getProperty('platform', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            logs = self.error_logs.get(self.compile_webkit_step)
            if platform in ['win', 'wincairo']:
                logs = self.filter_logs_containing_error(logs, searchString='error ')
            else:
                logs = self.filter_logs_containing_error(logs)

            email_subject = 'Build failure for Patch {}: {}'.format(patch_id, bug_title)
            email_text = 'EWS has detected build failure on {}'.format(builder_name)
            email_text += ' while testing <a href="{}">Patch {}</a>'.format(Bugzilla.patch_url(patch_id), patch_id)
            email_text += ' for <a href="{}">Bug {}</a>.'.format(Bugzilla.bug_url(bug_id), bug_id)
            email_text += '\n\nFull details are available at: {}\n\nPatch author: {}'.format(build_url, patch_author)
            if logs:
                logs = logs.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
                email_text += '\n\nError lines:\n\n<code>{}</code>'.format(logs)
            email_text += '\n\nTo unsubscribe from these notifications or to provide any feedback please email aakash_jain@apple.com'
            self._addToLog('stdio', 'Sending email notification to {}'.format(patch_author))
            send_email_to_patch_author(patch_author, email_subject, email_text, patch_id)
        except Exception as e:
            print('Error in sending email for new build failure: {}'.format(e))

    def send_email_for_preexisting_build_failure(self):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            platform = self.getProperty('platform', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            logs = self.error_logs.get(self.compile_webkit_step)
            if platform in ['win', 'wincairo']:
                logs = self.filter_logs_containing_error(logs, searchString='error ')
            else:
                logs = self.filter_logs_containing_error(logs)

            email_subject = 'Build failure on trunk on {}'.format(builder_name)
            email_text = 'Failed to build WebKit without patch in {}\n\nBuilder: {}\n\nWorker: {}'.format(build_url, builder_name, worker_name)
            if logs:
                logs = logs.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
                email_text += '\n\nError lines:\n\n<code>{}</code>'.format(logs)
            reference = 'preexisting-build-failure-{}-{}'.format(builder_name, date.today().strftime("%Y-%d-%m"))
            send_email_to_bot_watchers(email_subject, email_text, builder_name, reference)
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
            return {'step': 'Failed to compile JSC'}
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
        shell.Test.__init__(self, logEnviron=False, sigtermTime=10, **kwargs)
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
            # FIXME: the bundle copied to the remote should include the testair, testb3, testapi, etc.. binaries
            self.command.extend(['--no-testmasm', '--no-testair', '--no-testb3', '--no-testdfg', '--no-testapi'])

        # Linux bots have currently problems with JSC tests that try to use large amounts of memory.
        # Check: https://bugs.webkit.org/show_bug.cgi?id=175140
        if platform in ('gtk', 'wpe', 'jsc-only'):
            self.command.extend(['--memory-limited', '--verbose'])

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

        if jsc_results.get('allMasmTestsPassed') is False:
            self.binaryFailures.append('testmasm')
        if jsc_results.get('allAirTestsPassed') is False:
            self.binaryFailures.append('testair')
        if jsc_results.get('allB3TestsPassed') is False:
            self.binaryFailures.append('testb3')
        if jsc_results.get('allDFGTestsPassed') is False:
            self.binaryFailures.append('testdfg')
        if jsc_results.get('allApiTestsPassed') is False:
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

            return {'step': status}

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
        flaky_failures_string = ', '.join(sorted(flaky_failures))

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
        rc = shell.Test.evaluateCommand(self, cmd)
        self.setProperty('clean_tree_run_status', rc)
        return rc


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
        flaky_failures = sorted(list(flaky_binary_failures) + list(flaky_stress_failures))[:self.NUM_FAILURES_TO_DISPLAY]
        flaky_failures_string = ', '.join(flaky_failures)

        new_stress_failures = stress_failures_with_patch - clean_tree_stress_failures
        new_binary_failures = binary_failures_with_patch - clean_tree_binary_failures
        self.new_stress_failures_to_display = ', '.join(sorted(list(new_stress_failures))[:self.NUM_FAILURES_TO_DISPLAY])
        self.new_binary_failures_to_display = ', '.join(sorted(list(new_binary_failures))[:self.NUM_FAILURES_TO_DISPLAY])

        self._addToLog('stderr', '\nFailures in first run: {}'.format((list(first_run_binary_failures) + list(first_run_stress_failures))[:self.NUM_FAILURES_TO_DISPLAY]))
        self._addToLog('stderr', '\nFailures in second run: {}'.format((list(second_run_binary_failures) + list(second_run_stress_failures))[:self.NUM_FAILURES_TO_DISPLAY]))
        self._addToLog('stderr', '\nFlaky Tests: {}'.format(flaky_failures_string))
        self._addToLog('stderr', '\nFailures on clean tree: {}'.format(clean_tree_failures_string))

        if (not first_run_stress_failures) and (not first_run_binary_failures) and (not second_run_stress_failures) and (not second_run_binary_failures):
            # If we've made it here, then jsc-tests and re-run-jsc-tests failed, which means
            # there should have been some test failures. Otherwise there is some unexpected issue.
            clean_tree_run_status = self.getProperty('clean_tree_run_status', FAILURE)
            if clean_tree_run_status == SUCCESS:
                return self.report_failure(set(), set())
            # TODO: email EWS admins
            return self.retry_build('Unexpected infrastructure issue, retrying build')

        if new_stress_failures or new_binary_failures:
            self._addToLog('stderr', '\nNew binary failures: {}.\nNew stress test failures: {}\n'.format(self.new_binary_failures_to_display, self.new_stress_failures_to_display))
            return self.report_failure(new_binary_failures, new_stress_failures)
        else:
            self._addToLog('stderr', '\nNo new failures\n')
            self.finished(SUCCESS)
            self.build.results = SUCCESS
            self.descriptionDone = 'Passed JSC tests'
            pluralSuffix = 's' if len(clean_tree_failures) > 1 else ''
            message = ''
            if clean_tree_failures:
                message = 'Found {} pre-existing JSC test failure{}: {}'.format(len(clean_tree_failures), pluralSuffix, clean_tree_failures_string)
                for clean_tree_failure in clean_tree_failures[:self.NUM_FAILURES_TO_DISPLAY]:
                    self.send_email_for_pre_existing_failure(clean_tree_failure)
            if len(clean_tree_failures) > self.NUM_FAILURES_TO_DISPLAY:
                message += ' ...'
            if flaky_failures:
                message += ' Found flaky tests: {}'.format(flaky_failures_string)
                for flaky_failure in flaky_failures:
                    self.send_email_for_flaky_failure(flaky_failure)
            self.build.buildFinished([message], SUCCESS)
        return defer.succeed(None)

    def retry_build(self, message):
        self.descriptionDone = message
        self.finished(RETRY)
        self.build.buildFinished([message], RETRY)
        return defer.succeed(None)

    def report_failure(self, new_binary_failures, new_stress_failures):
        message = ''
        if (not new_binary_failures) and (not new_stress_failures):
            message = 'Found unexpected failure with patch'
        if new_binary_failures:
            pluralSuffix = 's' if len(new_binary_failures) > 1 else ''
            message = 'Found {} new JSC binary failure{}: {}'.format(len(new_binary_failures), pluralSuffix, self.new_binary_failures_to_display)
        if new_stress_failures:
            if message:
                message += ', '
            pluralSuffix = 's' if len(new_stress_failures) > 1 else ''
            message += 'Found {} new JSC stress test failure{}: {}'.format(len(new_stress_failures), pluralSuffix, self.new_stress_failures_to_display)
            if len(new_stress_failures) > self.NUM_FAILURES_TO_DISPLAY:
                message += ' ...'

        self.finished(FAILURE)
        self.build.results = FAILURE
        self.descriptionDone = message
        self.build.buildFinished([message], FAILURE)
        return defer.succeed(None)

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)

    def send_email_for_flaky_failure(self, test_name):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            history_url = '{}?suite=javascriptcore-tests&test={}'.format(RESULTS_DB_URL, test_name)

            email_subject = 'Flaky test: {}'.format(test_name)
            email_text = 'Flaky test: {}\n\nBuild: {}\n\nBuilder: {}\n\nWorker: {}\n\nHistory: {}'.format(test_name, build_url, builder_name, worker_name, history_url)
            send_email_to_bot_watchers(email_subject, email_text, builder_name, 'flaky-{}'.format(test_name))
        except Exception as e:
            print('Error in sending email for flaky failure: {}'.format(e))

    def send_email_for_pre_existing_failure(self, test_name):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            history_url = '{}?suite=javascriptcore-tests&test={}'.format(RESULTS_DB_URL, test_name)

            email_subject = 'Pre-existing test failure: {}'.format(test_name)
            email_text = 'Test {} failed on clean tree run in {}.\n\nBuilder: {}\n\nWorker: {}\n\nHistory: {}'.format(test_name, build_url, builder_name, worker_name, history_url)
            send_email_to_bot_watchers(email_subject, email_text, builder_name, 'preexisting-{}'.format(test_name))
        except Exception as e:
            print('Error in sending email for pre-existing failure: {}'.format(e))


class InstallBuiltProduct(shell.ShellCommand):
    name = 'install-built-product'
    description = ['Installing Built Product']
    descriptionDone = ['Installed Built Product']
    command = ["python3", "Tools/Scripts/install-built-product",
               WithProperties("--platform=%(fullPlatform)s"), WithProperties("--%(configuration)s")]


class CleanBuild(shell.Compile):
    name = 'delete-WebKitBuild-directory'
    description = ['deleting WebKitBuild directory']
    descriptionDone = ['Deleted WebKitBuild directory']
    command = ['python3', 'Tools/CISupport/clean-build', WithProperties('--platform=%(fullPlatform)s'), WithProperties('--%(configuration)s')]


class KillOldProcesses(shell.Compile):
    name = 'kill-old-processes'
    description = ['killing old processes']
    descriptionDone = ['Killed old processes']
    command = ['python3', 'Tools/CISupport/kill-old-processes', 'buildbot']

    def __init__(self, **kwargs):
        super(KillOldProcesses, self).__init__(timeout=2 * 60, logEnviron=False, **kwargs)

    def evaluateCommand(self, cmd):
        rc = shell.Compile.evaluateCommand(self, cmd)
        if rc in [FAILURE, EXCEPTION]:
            self.build.buildFinished(['Failed to kill old processes, retrying build'], RETRY)
        return rc

    def getResultSummary(self):
        if self.results in [FAILURE, EXCEPTION]:
            return {'step': 'Failed to kill old processes'}
        return shell.Compile.getResultSummary(self)


class TriggerCrashLogSubmission(shell.Compile):
    name = 'trigger-crash-log-submission'
    description = ['triggering crash log submission']
    descriptionDone = ['Triggered crash log submission']
    command = ['python3', 'Tools/CISupport/trigger-crash-log-submission']

    def __init__(self, **kwargs):
        super(TriggerCrashLogSubmission, self).__init__(timeout=60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results in [FAILURE, EXCEPTION]:
            return {'step': 'Failed to trigger crash log submission'}
        return shell.Compile.getResultSummary(self)


class WaitForCrashCollection(shell.Compile):
    name = 'wait-for-crash-collection'
    description = ['waiting-for-crash-collection-to-quiesce']
    descriptionDone = ['Crash collection has quiesced']
    command = ['python3', 'Tools/CISupport/wait-for-crash-collection', '--timeout', str(5 * 60)]

    def __init__(self, **kwargs):
        super(WaitForCrashCollection, self).__init__(timeout=6 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results in [FAILURE, EXCEPTION]:
            return {'step': 'Crash log collection process still running'}
        return shell.Compile.getResultSummary(self)


class RunWebKitTests(shell.Test):
    name = 'layout-tests'
    description = ['layout-tests running']
    descriptionDone = ['layout-tests']
    resultDirectory = 'layout-test-results'
    jsonFileName = 'layout-test-results/full_results.json'
    logfiles = {'json': jsonFileName}
    test_failures_log_name = 'test-failures'
    ENABLE_GUARD_MALLOC = False
    EXIT_AFTER_FAILURES = '30'
    command = ['python', 'Tools/Scripts/run-webkit-tests',
               '--no-build',
               '--no-show-results',
               '--no-new-test-results',
               '--clobber-old-results',
               WithProperties('--%(configuration)s')]

    def __init__(self, **kwargs):
        shell.Test.__init__(self, logEnviron=False, **kwargs)
        self.incorrectLayoutLines = []

    def _get_patch(self):
        sourcestamp = self.build.getSourceStamp(self.getProperty('codebase', ''))
        if not sourcestamp or not sourcestamp.patch:
            return None
        return sourcestamp.patch[1]

    def doStepIf(self, step):
        return not ((self.getProperty('buildername', '').lower() == 'commit-queue') and
                    (self.getProperty('fast_commit_queue') or self.getProperty('passed_mac_wk2')))

    def setLayoutTestCommand(self):
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
            self.setCommand(self.command + ['--exit-after-n-failures', self.EXIT_AFTER_FAILURES, '--skip-failing-tests'])

        if additionalArguments:
            self.setCommand(self.command + additionalArguments)

        if self.ENABLE_GUARD_MALLOC:
            self.setCommand(self.command + ['--guard-malloc'])

        if self.name == 'run-layout-tests-without-patch':
            # In order to speed up testing, on the step that retries running the layout tests without patch
            # only run the subset of tests that failed on the previous steps.
            # But only do that if the previous steps didn't exceed the test failure limit and the patch doesn't
            # modify the TestExpectations files (there are corner cases where we can't guarantee the correctnes
            # of this optimization if the patch modifies the TestExpectations files, for example, if the patch
            # removes skipped tests but those tests still fail).
            first_results_did_exceed_test_failure_limit = self.getProperty('first_results_exceed_failure_limit', False)
            second_results_did_exceed_test_failure_limit = self.getProperty('second_results_exceed_failure_limit', False)
            if not first_results_did_exceed_test_failure_limit and not second_results_did_exceed_test_failure_limit:
                patch_modifies_expectation_files = False
                patch = self._get_patch()
                if patch:
                    for line in patch.splitlines():
                        line = line.strip()
                        # patch is stored by buildbot as bytes: https://github.com/buildbot/buildbot/issues/5812#issuecomment-790175979
                        if (b'LayoutTests/' in line and b'TestExpectations' in line) and (line.startswith(b'---') or line.startswith(b'+++')):
                            patch_modifies_expectation_files = True
                            break
                if not patch_modifies_expectation_files:
                    first_results_failing_tests = set(self.getProperty('first_run_failures', set()))
                    second_results_failing_tests = set(self.getProperty('second_run_failures', set()))
                    list_retry_tests = sorted(first_results_failing_tests.union(second_results_failing_tests))
                    self.setCommand(self.command + list_retry_tests)

    def start(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)
        self.log_observer_json = logobserver.BufferLogObserver()
        self.addLogObserver('json', self.log_observer_json)
        self.setLayoutTestCommand()
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
            self.setProperty('first_run_failures', sorted(first_results.failing_tests))
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
                if line.find('flakes') >= 0 or line.find('new passes') >= 0:
                    result = WARNINGS
                elif line.find('missing results') >= 0:
                    return FAILURE
                else:
                    return FAILURE

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
            status = ' '.join(self.incorrectLayoutLines)
            return {'step': status}
        if self.results == SKIPPED:
            if self.getProperty('fast_commit_queue'):
                return {'step': 'Skipped layout-tests in fast-cq mode'}
            return {'step': 'Skipped layout-tests'}

        return super(RunWebKitTests, self).getResultSummary()


class RunWebKitTestsInStressMode(RunWebKitTests):
    name = 'run-layout-tests-in-stress-mode'
    suffix = 'stress-mode'
    EXIT_AFTER_FAILURES = '10'

    def __init__(self, num_iterations=100):
        self.num_iterations = num_iterations
        super(RunWebKitTestsInStressMode, self).__init__()

    def setLayoutTestCommand(self):
        RunWebKitTests.setLayoutTestCommand(self)

        self.setCommand(self.command + ['--iterations', self.num_iterations])
        modified_tests = self.getProperty('modified_tests')
        if modified_tests:
            self.setCommand(self.command + modified_tests)

    def evaluateCommand(self, cmd):
        rc = self.evaluateResult(cmd)
        if rc == SUCCESS or rc == WARNINGS:
            message = 'Passed layout tests'
            self.descriptionDone = message
            self.build.results = SUCCESS
            self.setProperty('build_summary', message)
        else:
            self.setProperty('build_summary', 'Found test failures')
            self.build.addStepsAfterCurrentStep([
                ArchiveTestResults(),
                UploadTestResults(identifier=self.suffix),
                ExtractTestResults(identifier=self.suffix),
            ])
        return rc

    def doStepIf(self, step):
        return self.getProperty('modified_tests', False)


class RunWebKitTestsInStressGuardmallocMode(RunWebKitTestsInStressMode):
    name = 'run-layout-tests-in-guard-malloc-stress-mode'
    suffix = 'guard-malloc'
    ENABLE_GUARD_MALLOC = True


class ReRunWebKitTests(RunWebKitTests):
    name = 're-run-layout-tests'
    NUM_FAILURES_TO_DISPLAY = 10

    def evaluateCommand(self, cmd):
        rc = self.evaluateResult(cmd)
        first_results_did_exceed_test_failure_limit = self.getProperty('first_results_exceed_failure_limit')
        first_results_failing_tests = set(self.getProperty('first_run_failures', []))
        second_results_did_exceed_test_failure_limit = self.getProperty('second_results_exceed_failure_limit')
        second_results_failing_tests = set(self.getProperty('second_run_failures', []))
        tests_that_consistently_failed = first_results_failing_tests.intersection(second_results_failing_tests)
        flaky_failures = first_results_failing_tests.union(second_results_failing_tests) - first_results_failing_tests.intersection(second_results_failing_tests)
        flaky_failures = sorted(list(flaky_failures))[:self.NUM_FAILURES_TO_DISPLAY]
        flaky_failures_string = ', '.join(flaky_failures)

        if rc == SUCCESS or rc == WARNINGS:
            message = 'Passed layout tests'
            self.descriptionDone = message
            self.build.results = SUCCESS
            if (not first_results_did_exceed_test_failure_limit) and flaky_failures:
                pluralSuffix = 's' if len(flaky_failures) > 1 else ''
                message = 'Found flaky test{}: {}'.format(pluralSuffix, flaky_failures_string)
                for flaky_failure in flaky_failures:
                    self.send_email_for_flaky_failure(flaky_failure)
            self.setProperty('build_summary', message)
        else:
            self.setProperty('patchFailedTests', True)
            self.build.addStepsAfterCurrentStep([ArchiveTestResults(),
                                                UploadTestResults(identifier='rerun'),
                                                ExtractTestResults(identifier='rerun'),
                                                UnApplyPatchIfRequired(),
                                                ValidatePatch(verifyBugClosed=False, addURLs=False),
                                                CompileWebKitWithoutPatch(retry_build_on_failure=True),
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
            self.setProperty('second_run_failures', sorted(second_results.failing_tests))
            if second_results.failing_tests:
                self._addToLog(self.test_failures_log_name, '\n'.join(second_results.failing_tests))
        self._parseRunWebKitTestsOutput(logText)

    def send_email_for_flaky_failure(self, test_name):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            history_url = '{}?suite=layout-tests&test={}'.format(RESULTS_DB_URL, test_name)

            email_subject = 'Flaky test: {}'.format(test_name)
            email_text = 'Test {} flaked in {}\n\nBuilder: {}'.format(test_name, build_url, builder_name)
            email_text = 'Flaky test: {}\n\nBuild: {}\n\nBuilder: {}\n\nWorker: {}\n\nHistory: {}'.format(test_name, build_url, builder_name, worker_name, history_url)
            send_email_to_bot_watchers(email_subject, email_text, builder_name, 'flaky-{}'.format(test_name))
        except Exception as e:
            # Catching all exceptions here to ensure that failure to send email doesn't impact the build
            print('Error in sending email for flaky failures: {}'.format(e))


class RunWebKitTestsWithoutPatch(RunWebKitTests):
    name = 'run-layout-tests-without-patch'

    def evaluateCommand(self, cmd):
        rc = shell.Test.evaluateCommand(self, cmd)
        self.build.addStepsAfterCurrentStep([ArchiveTestResults(), UploadTestResults(identifier='clean-tree'), ExtractTestResults(identifier='clean-tree'), AnalyzeLayoutTestsResults()])
        self.setProperty('clean_tree_run_status', rc)
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


class AnalyzeLayoutTestsResults(buildstep.BuildStep, BugzillaMixin):
    name = 'analyze-layout-tests-results'
    description = ['analyze-layout-test-results']
    descriptionDone = ['analyze-layout-tests-results']
    NUM_FAILURES_TO_DISPLAY = 10

    def report_failure(self, new_failures):
        self.finished(FAILURE)
        self.build.results = FAILURE
        if not new_failures:
            message = 'Found unexpected failure with patch'
        else:
            pluralSuffix = 's' if len(new_failures) > 1 else ''
            new_failures_string = ', '.join(sorted(new_failures)[:self.NUM_FAILURES_TO_DISPLAY])
            message = 'Found {} new test failure{}: {}'.format(len(new_failures), pluralSuffix, new_failures_string)
            if len(new_failures) > self.NUM_FAILURES_TO_DISPLAY:
                message += ' ...'
            self.send_email_for_new_test_failures(new_failures)
        self.descriptionDone = message
        self.setProperty('build_finish_summary', message)

        if self.getProperty('buildername', '').lower() == 'commit-queue':
            self.setProperty('bugzilla_comment_text', message)
            self.build.addStepsAfterCurrentStep([CommentOnBug(), SetCommitQueueMinusFlagOnPatch()])
        else:
            self.build.addStepsAfterCurrentStep([SetCommitQueueMinusFlagOnPatch()])
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
            for clean_tree_failure in list(clean_tree_failures)[:self.NUM_FAILURES_TO_DISPLAY]:
                self.send_email_for_pre_existing_failure(clean_tree_failure)

        if flaky_failures:
            flaky_failures_string = ', '.join(sorted(flaky_failures)[:self.NUM_FAILURES_TO_DISPLAY])
            pluralSuffix = 's' if len(flaky_failures) > 1 else ''
            message += ' Found flaky test{}: {}'.format(pluralSuffix, flaky_failures_string)
            if len(flaky_failures) > self.NUM_FAILURES_TO_DISPLAY:
                message += ' ...'
            for flaky_failure in list(flaky_failures)[:self.NUM_FAILURES_TO_DISPLAY]:
                self.send_email_for_flaky_failure(flaky_failure)

        self.setProperty('build_summary', message)
        return defer.succeed(None)

    def retry_build(self, message=''):
        if not message:
            message = 'Unable to confirm if test failures are introduced by patch, retrying build'
        self.descriptionDone = message

        triggered_by = self.getProperty('triggered_by', None)
        if triggered_by:
            # Trigger parent build so that it can re-build ToT
            schduler_for_current_queue = self.getProperty('scheduler')
            self.build.addStepsAfterCurrentStep([Trigger(schedulerNames=triggered_by, include_revision=False, triggers=[schduler_for_current_queue])])
            self.setProperty('build_summary', message)
            self.finished(SUCCESS)
        else:
            self.finished(RETRY)
            self.build.buildFinished([message], RETRY)
        return defer.succeed(None)

    def _results_failed_different_tests(self, first_results_failing_tests, second_results_failing_tests):
        return first_results_failing_tests != second_results_failing_tests

    def send_email_for_flaky_failure(self, test_name):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            history_url = '{}?suite=layout-tests&test={}'.format(RESULTS_DB_URL, test_name)

            email_subject = 'Flaky test: {}'.format(test_name)
            email_text = 'Flaky test: {}\n\nBuild: {}\n\nBuilder: {}\n\nWorker: {}\n\nHistory: {}'.format(test_name, build_url, builder_name, worker_name, history_url)
            send_email_to_bot_watchers(email_subject, email_text, builder_name, 'flaky-{}'.format(test_name))
        except Exception as e:
            print('Error in sending email for flaky failure: {}'.format(e))

    def send_email_for_pre_existing_failure(self, test_name):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            history_url = '{}?suite=layout-tests&test={}'.format(RESULTS_DB_URL, test_name)

            email_subject = 'Pre-existing test failure: {}'.format(test_name)
            email_text = 'Test {} failed on clean tree run in {}.\n\nBuilder: {}\n\nWorker: {}\n\nHistory: {}'.format(test_name, build_url, builder_name, worker_name, history_url)
            send_email_to_bot_watchers(email_subject, email_text, builder_name, 'preexisting-{}'.format(test_name))
        except Exception as e:
            print('Error in sending email for pre-existing failure: {}'.format(e))

    def send_email_for_new_test_failures(self, test_names):
        try:
            patch_id = self.getProperty('patch_id', '')
            if not self.should_send_email(patch_id):
                return
            builder_name = self.getProperty('buildername', '')
            bug_id = self.getProperty('bug_id', '')
            bug_title = self.getProperty('bug_title', '')
            worker_name = self.getProperty('workername', '')
            patch_author = self.getProperty('patch_author', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            test_names_string = ''
            for test_name in sorted(test_names):
                history_url = '{}?suite=layout-tests&test={}'.format(RESULTS_DB_URL, test_name)
                test_names_string += '\n- {} (<a href="{}">test history</a>)'.format(test_name, history_url)

            pluralSuffix = 's' if len(test_names) > 1 else ''
            email_subject = 'Layout test failure for Patch {}: {} '.format(patch_id, bug_title)
            email_text = 'EWS has detected layout test failure{} on {}'.format(pluralSuffix, builder_name)
            email_text += ' while testing <a href="{}">Patch {}</a>'.format(Bugzilla.patch_url(patch_id), patch_id)
            email_text += ' for <a href="{}">Bug {}</a>.'.format(Bugzilla.bug_url(bug_id), bug_id)
            email_text += '\n\nFull details are available at: {}\n\nPatch author: {}'.format(build_url, patch_author)
            email_text += '\n\nLayout test failure{}:\n{}'.format(pluralSuffix, test_names_string)
            email_text += '\n\nTo unsubscribe from these notifications or to provide any feedback please email aakash_jain@apple.com'
            self._addToLog('stdio', 'Sending email notification to {}'.format(patch_author))
            send_email_to_patch_author(patch_author, email_subject, email_text, patch_id)
        except Exception as e:
            print('Error in sending email for new layout test failures: {}'.format(e))

    def _report_flaky_tests(self, flaky_tests):
        # TODO: implement this
        pass

    def start(self):
        first_results_did_exceed_test_failure_limit = self.getProperty('first_results_exceed_failure_limit')
        first_results_failing_tests = set(self.getProperty('first_run_failures', []))
        second_results_did_exceed_test_failure_limit = self.getProperty('second_results_exceed_failure_limit')
        second_results_failing_tests = set(self.getProperty('second_run_failures', []))
        clean_tree_results_did_exceed_test_failure_limit = self.getProperty('clean_tree_results_exceed_failure_limit')
        clean_tree_results_failing_tests = set(self.getProperty('clean_tree_run_failures', []))
        flaky_failures = first_results_failing_tests.union(second_results_failing_tests) - first_results_failing_tests.intersection(second_results_failing_tests)

        if (not first_results_failing_tests) and (not second_results_failing_tests):
            # If we've made it here, then layout-tests and re-run-layout-tests failed, which means
            # there should have been some test failures. Otherwise there is some unexpected issue.
            clean_tree_run_status = self.getProperty('clean_tree_run_status', FAILURE)
            if clean_tree_run_status == SUCCESS:
                return self.report_failure(set())
            # TODO: email EWS admins
            return self.retry_build('Unexpected infrastructure issue, retrying build')

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
    command = ['python3', 'Tools/CISupport/built-product-archive',
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
            return {'step': 'Failed to upload built product'}
        return super(UploadBuiltProduct, self).getResultSummary()


class TransferToS3(master.MasterShellCommand):
    name = 'transfer-to-s3'
    description = ['transferring to s3']
    descriptionDone = ['Transferred archive to S3']
    archive = WithProperties('public_html/archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(patch_id)s.zip')
    identifier = WithProperties('%(fullPlatform)s-%(architecture)s-%(configuration)s')
    patch_id = WithProperties('%(patch_id)s')
    command = ['python3', '../Shared/transfer-archive-to-s3', '--patch_id', patch_id, '--identifier', identifier, '--archive', archive]
    haltOnFailure = False
    flunkOnFailure = False

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
        return super(TransferToS3, self).finished(results)

    def doStepIf(self, step):
        return CURRENT_HOSTNAME == EWS_BUILD_HOSTNAME

    def hideStepIf(self, results, step):
        return results == SUCCESS and self.getProperty('sensitive', False)

    def getResultSummary(self):
        if self.results == FAILURE:
            return {'step': 'Failed to transfer archive to S3'}
        return super(TransferToS3, self).getResultSummary()


class DownloadBuiltProduct(shell.ShellCommand):
    command = ['python3', 'Tools/CISupport/download-built-product',
               WithProperties('--%(configuration)s'),
               WithProperties(S3URL + 'ews-archives.webkit.org/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(patch_id)s.zip')]
    name = 'download-built-product'
    description = ['downloading built product']
    descriptionDone = ['Downloaded built product']
    flunkOnFailure = False

    def getResultSummary(self):
        if self.results not in [SUCCESS, SKIPPED]:
            return {'step': 'Failed to download built product from S3'}
        return super(DownloadBuiltProduct, self).getResultSummary()

    def __init__(self, **kwargs):
        super(DownloadBuiltProduct, self).__init__(logEnviron=False, **kwargs)

    def start(self):
        # Only try to download from S3 on the official deployment <https://webkit.org/b/230006>
        if CURRENT_HOSTNAME == EWS_BUILD_HOSTNAME:
            return shell.ShellCommand.start(self)
        self.build.addStepsAfterCurrentStep([DownloadBuiltProductFromMaster()])
        self.finished(SKIPPED)
        return defer.succeed(None)

    def evaluateCommand(self, cmd):
        rc = shell.ShellCommand.evaluateCommand(self, cmd)
        if rc == FAILURE:
            self.build.addStepsAfterCurrentStep([DownloadBuiltProductFromMaster()])
        return rc


class DownloadBuiltProductFromMaster(transfer.FileDownload):
    mastersrc = WithProperties('public_html/archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(patch_id)s.zip')
    workerdest = WithProperties('WebKitBuild/%(configuration)s.zip')
    name = 'download-built-product-from-master'
    description = ['downloading built product from buildbot master']
    descriptionDone = ['Downloaded built product']
    haltOnFailure = True
    flunkOnFailure = True

    def __init__(self, **kwargs):
        # Allow the unit test to override mastersrc
        if 'mastersrc' not in kwargs:
            kwargs['mastersrc'] = self.mastersrc
        kwargs['workerdest'] = self.workerdest
        kwargs['mode'] = 0o0644
        kwargs['blocksize'] = 1024 * 256
        transfer.FileDownload.__init__(self, **kwargs)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to download built product from build master'}
        return super(DownloadBuiltProductFromMaster, self).getResultSummary()


class ExtractBuiltProduct(shell.ShellCommand):
    command = ['python3', 'Tools/CISupport/built-product-archive',
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
    command = ['python3', 'Tools/Scripts/run-api-tests', '--no-build',
               WithProperties('--%(configuration)s'), '--verbose', '--json-output={0}'.format(jsonFileName)]
    failedTestsFormatString = '%d api test%s failed or timed out'

    def __init__(self, **kwargs):
        super(RunAPITests, self).__init__(logEnviron=False, **kwargs)

    def start(self):
        platform = self.getProperty('platform')
        if platform == 'gtk':
            command = ['python3', 'Tools/Scripts/run-gtk-tests',
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
            steps_to_add.append(CompileWebKitWithoutPatch(retry_build_on_failure=True))
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
    NUM_FAILURES_TO_DISPLAY = 10

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
        clean_tree_failures_to_display = list(clean_tree_failures)[:self.NUM_FAILURES_TO_DISPLAY]
        clean_tree_failures_string = ', '.join(clean_tree_failures_to_display)

        failures_with_patch = first_run_failures.intersection(second_run_failures)
        flaky_failures = first_run_failures.union(second_run_failures) - first_run_failures.intersection(second_run_failures)
        flaky_failures = list(flaky_failures)[:self.NUM_FAILURES_TO_DISPLAY]
        flaky_failures_string = ', '.join(flaky_failures)
        new_failures = failures_with_patch - clean_tree_failures
        new_failures_to_display = list(new_failures)[:self.NUM_FAILURES_TO_DISPLAY]
        new_failures_string = ', '.join(new_failures_to_display)

        self._addToLog('stderr', '\nFailures in API Test first run: {}'.format(list(first_run_failures)[:self.NUM_FAILURES_TO_DISPLAY]))
        self._addToLog('stderr', '\nFailures in API Test second run: {}'.format(list(second_run_failures)[:self.NUM_FAILURES_TO_DISPLAY]))
        self._addToLog('stderr', '\nFlaky Tests: {}'.format(flaky_failures_string))
        self._addToLog('stderr', '\nFailures in API Test on clean tree: {}'.format(clean_tree_failures_string))

        if new_failures:
            self._addToLog('stderr', '\nNew failures: {}\n'.format(new_failures_string))
            self.finished(FAILURE)
            self.build.results = FAILURE
            pluralSuffix = 's' if len(new_failures) > 1 else ''
            message = 'Found {} new API test failure{}: {}'.format(len(new_failures), pluralSuffix, new_failures_string)
            if len(new_failures) > self.NUM_FAILURES_TO_DISPLAY:
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
                for clean_tree_failure in clean_tree_failures_to_display:
                    self.send_email_for_pre_existing_failure(clean_tree_failure)
            if len(clean_tree_failures) > self.NUM_FAILURES_TO_DISPLAY:
                message += ' ...'
            if flaky_failures:
                message += ' Found flaky tests: {}'.format(flaky_failures_string)
                for flaky_failure in flaky_failures:
                    self.send_email_for_flaky_failure(flaky_failure)
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
        log = next((log for log in logs if log['name'] == 'json'), None)
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

    def send_email_for_flaky_failure(self, test_name):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            history_url = '{}?suite=api-tests&test={}'.format(RESULTS_DB_URL, test_name)

            email_subject = 'Flaky test: {}'.format(test_name)
            email_text = 'Flaky test: {}\n\nBuild: {}\n\nBuilder: {}\n\nWorker: {}\n\nHistory: {}'.format(test_name, build_url, builder_name, worker_name, history_url)
            send_email_to_bot_watchers(email_subject, email_text, builder_name, 'flaky-{}'.format(test_name))
        except Exception as e:
            print('Error in sending email for flaky failure: {}'.format(e))

    def send_email_for_pre_existing_failure(self, test_name):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            history_url = '{}?suite=api-tests&test={}'.format(RESULTS_DB_URL, test_name)

            email_subject = 'Pre-existing test failure: {}'.format(test_name)
            email_text = 'Test {} failed on clean tree run in {}.\n\nBuilder: {}\n\nWorker: {}\n\nHistory: {}'.format(test_name, build_url, builder_name, worker_name, history_url)
            send_email_to_bot_watchers(email_subject, email_text, builder_name, 'preexisting-{}'.format(test_name))
        except Exception as e:
            print('Error in sending email for pre-existing failure: {}'.format(e))


class ArchiveTestResults(shell.ShellCommand):
    command = ['python3', 'Tools/CISupport/test-result-archive',
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
        self.command = ['unzip', '-q', '-o', self.zipFile, '-d', self.resultDirectory]

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
            self.commands.append(util.ShellArg(command=command, logname='stdio'))
        return super(PrintConfiguration, self).run()

    def convert_build_to_os_name(self, build):
        if not build:
            return 'Unknown'

        build_to_name_mapping = {
            '11': 'Big Sur',
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

        for key, value in build_to_name_mapping.items():
            if build.startswith(key):
                return value
        return 'Unknown'

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to print configuration'}
        logText = self.log_observer.getStdout() + self.log_observer.getStderr()
        configuration = 'Printed configuration'
        match = re.search('ProductVersion:[ \t]*(.+?)\n', logText)
        if match:
            os_version = match.group(1).strip()
            os_name = self.convert_build_to_os_name(os_version)
            configuration = 'OS: {} ({})'.format(os_name, os_version)

        xcode_re = sdk_re = 'Xcode[ \t]+?([0-9.]+?)\n'
        match = re.search(xcode_re, logText)
        if match:
            xcode_version = match.group(1).strip()
            configuration += ', Xcode: {}'.format(xcode_version)
        return {'step': configuration}


class CleanGitRepo(steps.ShellSequence):
    name = 'clean-up-git-repo'
    haltOnFailure = False
    flunkOnFailure = False
    logEnviron = False
    # This somewhat quirky sequence of steps seems to clear up all the broken
    # git situations we've gotten ourself into in the past.
    command_list = [['git', 'clean', '-f', '-d'],  # Remove any left-over layout test results, added files, etc.
                    ['git', 'fetch', 'origin'],  # Avoid updating the working copy to a stale revision.
                    ['git', 'checkout', 'origin/master', '-f'],
                    ['git', 'branch', '-D', 'master'],
                    ['git', 'checkout', 'origin/master', '-b', 'master']]

    def run(self):
        self.commands = []
        for command in self.command_list:
            self.commands.append(util.ShellArg(command=command, logname='stdio'))
        return super(CleanGitRepo, self).run()

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Encountered some issues during cleanup'}
        return {'step': 'Cleaned up git repository'}


class ApplyWatchList(shell.ShellCommand):
    name = 'apply-watch-list'
    description = ['applying watchilist']
    descriptionDone = ['Applied WatchList']
    bug_id = WithProperties('%(bug_id)s')
    command = ['python3', 'Tools/Scripts/webkit-patch', 'apply-watchlist-local', bug_id]
    haltOnFailure = True
    flunkOnFailure = True

    def __init__(self, **kwargs):
        shell.ShellCommand.__init__(self, timeout=2 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to apply watchlist'}
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
            return {'step': 'Failed to find any modified ChangeLog in Patch {}'.format(patch_id)}
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
            self.failure_message = 'No modified ChangeLog file found for Patch {}'.format(patch_id)
            self.finished(FAILURE)
            return None

        modified_changelogs = ' '.join(modified_changelogs)
        self.command = 'perl Tools/Scripts/commit-log-editor --print-log {}'.format(modified_changelogs)
        self.command += ' | git commit --all -F -'
        return shell.ShellCommand.start(self)

    def getResultSummary(self):
        if self.failure_message:
            return {'step': self.failure_message}
        if self.results != SUCCESS:
            return {'step': 'Failed to create git commit'}
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
    commit_success_regexp = r'^Committed r(?P<svn_revision>\d+)$'
    haltOnFailure = False
    MAX_RETRY = 2

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
            identifier = self.identifier_for_revision(svn_revision)
            self.setProperty('bugzilla_comment_text', self.comment_text_for_bug(svn_revision, identifier))
            commit_summary = 'Committed {}'.format(identifier)
            self.descriptionDone = commit_summary
            self.setProperty('build_summary', commit_summary)
            self.build.addStepsAfterCurrentStep([CommentOnBug(), RemoveFlagsOnPatch(), CloseBug()])
            self.addURL(identifier, self.url_for_identifier(identifier))
        else:
            retry_count = int(self.getProperty('retry_count', 0))
            if retry_count < self.MAX_RETRY:
                self.setProperty('retry_count', retry_count + 1)
                self.build.addStepsAfterCurrentStep([GitResetHard(), CheckOutSource(repourl='https://git.webkit.org/git/WebKit-https'), ShowIdentifier(), UpdateWorkingDirectory(), ApplyPatch(), CreateLocalGITCommit(), PushCommitToWebKitRepo()])
                return rc

            self.setProperty('bugzilla_comment_text', self.comment_text_for_bug())
            self.setProperty('build_finish_summary', 'Failed to commit to WebKit repository')
            self.build.addStepsAfterCurrentStep([CommentOnBug(), SetCommitQueueMinusFlagOnPatch()])
        return rc

    def url_for_revision_details(self, revision):
        return '{}r{}/json'.format(COMMITS_INFO_URL, revision)

    def url_for_identifier(self, identifier):
        return '{}{}'.format(COMMITS_INFO_URL, identifier)

    def identifier_for_revision(self, revision):
        try:
            response = requests.get(self.url_for_revision_details(revision), timeout=60)
            if response and response.status_code == 200:
                return response.json().get('identifier', 'r{}'.format(revision))
            else:
                print('Non-200 status code received from {}: {}'.format(COMMITS_INFO_URL, response.status_code))
                print(response.text)
        except Exception as e:
            print(e)
        return 'r{}'.format(revision)

    def comment_text_for_bug(self, svn_revision=None, identifier=None):
        patch_id = self.getProperty('patch_id', '')
        if not svn_revision:
            comment = 'commit-queue failed to commit attachment {} to WebKit repository.'.format(patch_id)
            comment += ' To retry, please set cq+ flag again.'
            return comment

        identifier_str = identifier if identifier and '@' in identifier else '?'
        comment = 'Committed r{} ({}): <{}>'.format(svn_revision, identifier_str, self.url_for_identifier(identifier))
        comment += '\n\nAll reviewed patches have been landed. Closing bug and clearing flags on attachment {}.'.format(patch_id)
        return comment

    def svn_revision_from_commit_text(self, commit_text):
        match = re.search(self.commit_success_regexp, commit_text, re.MULTILINE)
        return match.group('svn_revision')

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to push commit to Webkit repository'}
        return shell.ShellCommand.getResultSummary(self)

    def doStepIf(self, step):
        return CURRENT_HOSTNAME == EWS_BUILD_HOSTNAME


class CheckPatchStatusOnEWSQueues(buildstep.BuildStep, BugzillaMixin):
    name = 'check-status-on-other-ewses'
    descriptionDone = ['Checked patch status on other queues']

    def get_patch_status(self, patch_id, queue):
        url = '{}status/{}'.format(EWS_URL, patch_id)
        try:
            response = requests.get(url, timeout=60)
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


# FIXME: Only needed when GitHub is a mirror, remove once GitHub is the source of truth
class VerifyGitHubIntegrity(shell.ShellCommand):
    command = ['python3', 'Tools/Scripts/check-github-mirror-integrity']
    name = 'verify-github-integrity'
    haltOnFailure = True

    def __init__(self, **kwargs):
        super(VerifyGitHubIntegrity, self).__init__(logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'GitHub integrity check failed'}
        return {'step': 'Verified GitHub integrity'}

    def evaluateCommand(self, cmd):
        rc = shell.ShellCommand.evaluateCommand(self, cmd)
        if rc != SUCCESS:
            self.send_email_for_github_issue()
        return rc

    def send_email_for_github_issue(self):
        try:
            builder_name = self.getProperty('buildername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            email_subject = 'URGENT: GitHub integrity check failed'
            email_text = 'URGENT issue on github repository. Integrity check failed.\n\nBuild: {}\n\nBuilder: {}'.format(build_url, builder_name)
            send_email_to_github_admin(email_subject, email_text)
        except Exception as e:
            print('Error in sending email for github issue: {}'.format(e))
