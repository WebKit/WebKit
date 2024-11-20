# Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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

from base64 import b64encode
from buildbot.plugins import steps, util
from buildbot.process import buildstep, logobserver, properties, remotecommand
from buildbot.process.results import Results, SUCCESS, FAILURE, CANCELLED, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.steps import master, shell, transfer, trigger
from buildbot.steps.source import git
from buildbot.steps.worker import CompositeStepMixin
from datetime import date
from shlex import quote

from twisted.internet import defer, reactor, task

from .layout_test_failures import LayoutTestFailures
from .send_email import send_email_to_patch_author, send_email_to_bot_watchers, send_email_to_github_admin, FROM_EMAIL
from .results_db import ResultsDatabase
from .twisted_additions import TwistedAdditions
from .utils import load_password, get_custom_suffix

import json
import mock
import os
import re
import socket
import sys
import time

if sys.version_info < (3, 9):  # noqa: UP036
    print('ERROR: Minimum supported Python version for this code is Python 3.9')
    sys.exit(1)

custom_suffix = get_custom_suffix()
BUG_SERVER_URL = 'https://bugs.webkit.org/'
COMMITS_INFO_URL = 'https://commits.webkit.org/'
S3URL = 'https://s3-us-west-2.amazonaws.com/'
S3_BUCKET = f'ews-archives.webkit{custom_suffix}.org'
S3_RESULTS_URL = f'https://ews-build{custom_suffix}.s3-us-west-2.amazonaws.com/'
CURRENT_HOSTNAME = socket.gethostname().strip()
EWS_BUILD_HOSTNAMES = ['ews-build.webkit.org', 'ews-build']
TESTING_ENVIRONMENT_HOSTNAMES = ['ews-build.webkit-uat.org', 'ews-build-uat', 'ews-build.webkit-dev.org', 'ews-build-dev']
EWS_URL = 'https://ews.webkit.org/'
RESULTS_DB_URL = 'https://results.webkit.org/'
RESULTS_SERVER_API_KEY = 'RESULTS_SERVER_API_KEY'
WithProperties = properties.WithProperties
Interpolate = properties.Interpolate
GITHUB_URL = 'https://github.com/'
# First project is treated as the default
GITHUB_PROJECTS = ['WebKit/WebKit', 'WebKit/WebKit-security', 'apple/WebKit']
HASH_LENGTH_TO_DISPLAY = 8
DEFAULT_BRANCH = 'main'
DEFAULT_REMOTE = 'origin'
LAYOUT_TESTS_URL = '{}{}/blob/{}/LayoutTests/'.format(GITHUB_URL, GITHUB_PROJECTS[0], DEFAULT_BRANCH)
MAX_COMMITS_IN_PR_SERIES = 50
QUEUES_WITH_PUSH_ACCESS = ('commit-queue', 'merge-queue', 'unsafe-merge-queue')
THRESHOLD_FOR_EXCESSIVE_LOGS_DEFAULT = 1000000
MSG_FOR_EXCESSIVE_LOGS = f'Stopped due to excessive logging, limit: {THRESHOLD_FOR_EXCESSIVE_LOGS_DEFAULT}'
SCAN_BUILD_OUTPUT_DIR = 'scan-build-output'
LLVM_DIR = 'llvm-project'
STATIC_ANALYSIS_ARCHIVE_PATH = '/tmp/static-analysis.zip'

if CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES:
    CURRENT_HOSTNAME = 'ews-build.webkit.org'


class ParseByLineLogObserver(logobserver.LineConsumerLogObserver):
    """A pretty wrapper for LineConsumerLogObserver to avoid
       repeatedly setting up generator processors."""
    def __init__(self, consumeLineFunc):
        if not callable(consumeLineFunc):
            raise Exception("Error: ParseByLineLogObserver requires consumeLineFunc to be callable.")
        self.consumeLineFunc = consumeLineFunc
        super(ParseByLineLogObserver, self).__init__(self.consumeLineGenerator)

    def consumeLineGenerator(self):
        """The generator LineConsumerLogObserver expects."""
        try:
            while True:
                stream, line = yield
                self.consumeLineFunc(line)
        except GeneratorExit:
            return


class BufferLogHeaderObserver(logobserver.BufferLogObserver):

    def __init__(self, **kwargs):
        self.headers = []
        super().__init__(**kwargs)

    def headerReceived(self, data):
        self.headers.append(data)

    def getHeaders(self):
        return self._get(self.headers)


class GitHub(object):
    _cache = {}
    MERGE_QUEUE_LABEL = 'merge-queue'
    UNSAFE_MERGE_QUEUE_LABEL = 'unsafe-merge-queue'
    SAFE_MERGE_QUEUE_LABEL = 'safe-merge-queue'
    REQUEST_MERGE_QUEUE_LABEL = 'request-merge-queue'
    BLOCKED_LABEL = 'merging-blocked'
    SKIP_EWS_LABEL = 'skip-ews'
    NO_FAILURE_LIMITS_LABEL = 'no-failure-limits'
    LABELS = [
        MERGE_QUEUE_LABEL, UNSAFE_MERGE_QUEUE_LABEL, SAFE_MERGE_QUEUE_LABEL,
        REQUEST_MERGE_QUEUE_LABEL, BLOCKED_LABEL,
        SKIP_EWS_LABEL, NO_FAILURE_LIMITS_LABEL,
    ]

    @classmethod
    def repository_urls(cls):
        return [GITHUB_URL + project for project in GITHUB_PROJECTS]

    @classmethod
    def user_for_queue(cls, queue):
        if queue.lower() in QUEUES_WITH_PUSH_ACCESS:
            return GitHub.MERGE_QUEUE_LABEL
        return None

    @classmethod
    def pr_url(cls, pr_number, repository_url=None):
        if not repository_url:
            repository_url = '{}{}'.format(GITHUB_URL, GITHUB_PROJECTS[0])

        if repository_url not in GitHub.repository_urls():
            return ''
        if not pr_number or not isinstance(pr_number, int):
            return ''
        return '{}/pull/{}'.format(repository_url, pr_number)

    @classmethod
    def commit_url(cls, sha, repository_url=None):
        if not repository_url:
            repository_url = '{}{}'.format(GITHUB_URL, GITHUB_PROJECTS[0])
        if repository_url not in GitHub.repository_urls():
            return ''
        if not sha:
            return ''
        return '{}/commit/{}'.format(repository_url, sha)

    @classmethod
    def api_url(cls, repository_url=None):
        if not repository_url:
            repository_url = '{}{}'.format(GITHUB_URL, GITHUB_PROJECTS[0])

        if repository_url not in GitHub.repository_urls():
            return ''
        _, url_base = repository_url.split('://', 1)
        host, path = url_base.split('/', 1)
        return 'https://api.{}/repos/{}'.format(host, path)

    @classmethod
    def commit_status_url(cls, sha, repository_url=None):
        api_url = cls.api_url(repository_url)
        if not sha or not api_url:
            return ''
        return '{}/statuses/{}'.format(api_url, sha)

    @classmethod
    def credentials(cls, user=None):
        prefix = f"GITHUB_COM_{user.upper().replace('-', '_')}_" if user else 'GITHUB_COM_'

        if prefix in cls._cache:
            return cls._cache[prefix]

        username, password = load_password(f'{prefix}USERNAME'), load_password(f'{prefix}ACCESS_TOKEN')
        if username and password:
            cls._cache[prefix] = username, password
        else:
            print('Error reading GitHub credentials')
            cls._cache[prefix] = None, None

        return cls._cache[prefix]

    @classmethod
    @defer.inlineCallbacks
    def email_for_owners(cls, owners):
        if not owners:
            return defer.returnValue((None, ['No owners defined, so email cannot be extracted']))
        contributors, errors = yield Contributors.load()
        return defer.returnValue((contributors.get(owners[0].lower(), {}).get('email'), errors))


class GitHubMixin(object):
    addURLs = False
    pr_open_states = ['open']
    pr_closed_states = ['closed']
    PER_PAGE_LIMIT = 100
    NUM_PAGE_LIMIT = 10

    @defer.inlineCallbacks
    def fetch_data_from_url_with_authentication_github(self, url):
        headers = {'Accept': ['application/vnd.github.v3+json']}
        username, access_token = GitHub.credentials(user=GitHub.user_for_queue(self.getProperty('buildername', '')))
        if username and access_token:
            auth_header = b64encode('{}:{}'.format(username, access_token).encode('utf-8')).decode('utf-8')
            headers['Authorization'] = ['Basic {}'.format(auth_header)]

        response = yield TwistedAdditions.request(
            url, type=b'GET',
            headers=headers,
            logger=lambda content: self._addToLog('stdio', content),
        )
        if response and response.status_code // 100 != 2:
            yield self._addToLog('stdio', f'Accessed {url} with unexpected status code {response.status_code}.\n')
            defer.returnValue(False if response.status_code // 100 == 4 else None)
        else:
            defer.returnValue(response)

    @defer.inlineCallbacks
    def query_graph_ql(self, payload):
        headers = {'Accept': ['application/vnd.github.v3+json']}
        graphql_url = 'https://api.github.com/graphql'
        username, access_token = GitHub.credentials(user=GitHub.user_for_queue(self.getProperty('buildername', '')))
        if username and access_token:
            auth_header = b64encode('{}:{}'.format(username, access_token).encode('utf-8')).decode('utf-8')
            headers['Authorization'] = ['bearer {}'.format(access_token)]

        response = yield TwistedAdditions.request(
            graphql_url, type=b'POST',
            headers=headers,
            logger=lambda content: self._addToLog('stdio', content),
            json=payload,
        )
        if response and response.status_code // 100 != 2:
            yield self._addToLog('stdio', f'Accessed {graphql_url} with unexpected status code {response.status_code}.\n')
            defer.returnValue(False if response.status_code // 100 == 4 else None)
        elif not response:
            defer.returnValue(False)
        else:
            data = json.loads(response.text)
            defer.returnValue(data)

    @defer.inlineCallbacks
    def get_number_of_prs_with_label(self, label, retry=0):
        project = self.getProperty('project') or GITHUB_PROJECTS[0]
        owner, name = project.split('/', 1)
        query_body = '{repository(owner:"%s", name:"%s") { pullRequests(labels: "%s") { totalCount } } }' % (owner, name, label)
        query = {'query': query_body}

        for attempt in range(retry + 1):
            try:
                response = yield self.query_graph_ql(query)
                if 'errors' in response:
                    yield self._addToLog('stdio', response['errors'][0]['message'])
                else:
                    num_prs = response['data']['repository']['pullRequests']['totalCount']
                    break
            except Exception as e:
                yield self._addToLog('stdio', 'Failed to retrieve number of PRs.\n')

            if attempt > retry:
                return defer.returnValue(None)
            wait_for = (attempt + 1) * 15
            yield self._addToLog('stdio', 'Backing off for {} seconds before retrying.\n'.format(wait_for))
            yield task.deferLater(reactor, wait_for, lambda: None)

        yield self._addToLog('stdio', 'There are {} PR(s) in safe-merge-queue.\n'.format(num_prs))
        defer.returnValue(num_prs)

    @defer.inlineCallbacks
    def get_pr_json(self, pr_number, repository_url=None, retry=0):
        api_url = GitHub.api_url(repository_url)
        if not api_url:
            return defer.returnValue(None)

        pr_url = '{}/pulls/{}'.format(api_url, pr_number)
        content = yield self.fetch_data_from_url_with_authentication_github(pr_url)
        if not content:
            return defer.returnValue(content)

        for attempt in range(retry + 1):
            try:
                pr_json = content.json()
                if pr_json and len(pr_json):
                    return defer.returnValue(pr_json)
            except Exception as e:
                yield self._addToLog('stdio', 'Failed to get pull request data from {}, error: {}'.format(pr_url, e))

            yield self._addToLog('stdio', 'Unable to fetch pull request {}.\n'.format(pr_number))
            if attempt > retry:
                return defer.returnValue(None)
            wait_for = (attempt + 1) * 15
            yield self._addToLog('stdio', 'Backing off for {} seconds before retrying.\n'.format(wait_for))
            yield task.deferLater(reactor, wait_for, lambda: None)

        return defer.returnValue(None)

    @defer.inlineCallbacks
    def get_reviewers(self, pr_number, repository_url=None):
        api_url = GitHub.api_url(repository_url)
        if not api_url:
            defer.returnValue([])
            return

        reviews = []
        reviews_url = f'{api_url}/pulls/{pr_number}/reviews?per_page={self.PER_PAGE_LIMIT}'
        for page in range(1, self.NUM_PAGE_LIMIT + 1):
            content = yield self.fetch_data_from_url_with_authentication_github(
                f'{api_url}/pulls/{pr_number}/reviews?per_page={self.PER_PAGE_LIMIT}&page={page}'
            )
            if not content:
                break
            response_content = content.json() or []
            if not isinstance(response_content, list):
                yield self._addToLog('stdio', f"Malformed response when listing reviews with '{url}'\n")
                break
            reviews += response_content
            if len(response_content) < self.PER_PAGE_LIMIT:
                break
            page += 1

        last_approved = dict()
        last_rejected = dict()
        for review in reviews:
            reviewer = review.get('user', {}).get('login')
            if not reviewer:
                continue
            review_id = review.get('id', 0)
            if review.get('state') == 'APPROVED':
                last_approved[reviewer] = max(review_id, last_approved.get(reviewer, 0))
            elif review.get('state') == 'CHANGES_REQUESTED':
                last_rejected[reviewer] = max(review_id, last_rejected.get(reviewer, 0))
        defer.returnValue(sorted([reviewer for reviewer, _id in last_approved.items() if _id > last_rejected.get(reviewer, 0)]))

    @defer.inlineCallbacks
    def _is_pr_closed(self, pr_json):
        # If pr_json is "False", we received a 400 family error, which likely means the PR was deleted
        if pr_json is False:
            return defer.returnValue(1)
        if not pr_json or not pr_json.get('state'):
            yield self._addToLog('stdio', 'Cannot determine pull request status.\n')
            return defer.returnValue(-1)
        if pr_json.get('state') in self.pr_closed_states:
            return defer.returnValue(1)
        return defer.returnValue(0)

    @defer.inlineCallbacks
    def _is_hash_outdated(self, pr_json):
        # If pr_json is "False", we received a 400 family error, which likely means the PR was deleted
        if pr_json is False:
            return defer.returnValue(1)
        pr_sha = (pr_json or {}).get('head', {}).get('sha', '')
        if not pr_sha:
            yield self._addToLog('stdio', 'Cannot determine if hash is outdated or not.\n')
            return defer.returnValue(-1)
        return defer.returnValue(0 if pr_sha == self.getProperty('github.head.sha', '?') else 1)

    def _is_pr_blocked(self, pr_json):
        for label in (pr_json or {}).get('labels', {}):
            if label.get('name', '') == GitHub.BLOCKED_LABEL:
                return 1
        return 0

    def _does_pr_has_skip_label(self, pr_json):
        for label in (pr_json or {}).get('labels', {}):
            if label.get('name', '') == GitHub.SKIP_EWS_LABEL:
                return 1
        return 0

    def _is_pr_in_merge_queue(self, pr_json):
        for label in (pr_json or {}).get('labels', {}):
            if label.get('name', '') in (GitHub.MERGE_QUEUE_LABEL, GitHub.UNSAFE_MERGE_QUEUE_LABEL):
                return 1
        return 0

    def _is_pr_draft(self, pr_json):
        if pr_json.get('draft', False):
            return 1
        return 0

    @defer.inlineCallbacks
    def should_send_email_for_pr(self, pr_number, repository_url=None):
        pr_json = yield self.get_pr_json(pr_number, repository_url=repository_url)
        if not pr_json:
            return defer.returnValue(True)

        is_hash_outdated = yield self._is_hash_outdated(pr_json)
        if 1 == is_hash_outdated:
            yield self._addToLog('stdio', 'Skipping email since hash {} on PR #{} is outdated\n'.format(
                self.getProperty('github.head.sha', '?')[:HASH_LENGTH_TO_DISPLAY], pr_number,
            ))
            return defer.returnValue(False)
        return defer.returnValue(True)

    @defer.inlineCallbacks
    def add_label(self, pr_number, label, repository_url=None):
        api_url = GitHub.api_url(repository_url)
        if not api_url:
            return defer.returnValue(False)

        pr_label_url = '{}/issues/{}/labels'.format(api_url, pr_number)
        try:
            headers = {'Accept': ['application/vnd.github.v3+json']}
            username, access_token = GitHub.credentials(user=GitHub.user_for_queue(self.getProperty('buildername', '')))
            if username and access_token:
                auth_header = b64encode('{}:{}'.format(username, access_token).encode('utf-8')).decode('utf-8')
                headers['Authorization'] = ['Basic {}'.format(auth_header)]

            response = yield TwistedAdditions.request(
                pr_label_url, type=b'POST', timeout=60,
                headers=headers, json=dict(labels=[label]),
                logger=lambda content: self._addToLog('stdio', content),
            )
            if response.status_code // 100 != 2:
                yield self._addToLog('stdio', f"Unable to add '{label}' label on PR {pr_number}. Unexpected response code from GitHub: {response.status_code}\n")
                return defer.returnValue(False)
            defer.returnValue(True)
        except Exception as e:
            yield self._addToLog('stdio', f"Error in adding '{label}' label on PR {pr_number}\n")
            defer.returnValue(False)

    @defer.inlineCallbacks
    def remove_labels(self, pr_number, labels=None, repository_url=None):
        labels = labels or []
        if not labels:
            defer.returnValue(True)
            return
        api_url = GitHub.api_url(repository_url)
        if not api_url:
            defer.returnValue(False)
            return

        pr_label_url = f'{api_url}/issues/{pr_number}/labels'
        content = yield self.fetch_data_from_url_with_authentication_github(pr_label_url)
        if not content:
            yield self._addToLog('stdio', "Failed to fetch existing labels, cannot remove labels\n")
            defer.returnValue(True)
            return

        existing_labels = [label.get('name') for label in (content.json() or [])]
        new_labels = list(filter(lambda label: label not in labels, existing_labels))
        if len(existing_labels) == len(new_labels):
            defer.returnValue(True)
            return

        try:
            headers = {'Accept': ['application/vnd.github.v3+json']}
            username, access_token = GitHub.credentials(user=GitHub.user_for_queue(self.getProperty('buildername', '')))
            if username and access_token:
                auth_header = b64encode('{}:{}'.format(username, access_token).encode('utf-8')).decode('utf-8')
                headers['Authorization'] = ['Basic {}'.format(auth_header)]

            response = yield TwistedAdditions.request(
                pr_label_url, type=b'PUT', timeout=60,
                headers=headers, json=dict(labels=new_labels),
                logger=lambda content: self._addToLog('stdio', content),
            )
            if response.status_code // 100 != 2:
                for label in labels:
                    yield self._addToLog('stdio', f"Unable to remove '{label}' label on PR {pr_number}. Unexpected response code from GitHub: {response.status_code}\n")
                defer.returnValue(False)
                return
            defer.returnValue(True)
        except Exception as e:
            for label in labels:
                yield self._addToLog('stdio', f"Error in removing '{label}' label on PR {pr_number}\n")
            defer.returnValue(False)

    @defer.inlineCallbacks
    def comment_on_pr(self, pr_number, content, repository_url=None):
        api_url = GitHub.api_url(repository_url)
        if not api_url:
            defer.returnValue(FAILURE)
            return

        comment_url = f'{api_url}/issues/{pr_number}/comments'
        try:
            headers = {'Accept': ['application/vnd.github.v3+json']}
            username, access_token = GitHub.credentials(user=GitHub.user_for_queue(self.getProperty('buildername', '')))
            if username and access_token:
                auth_header = b64encode(f'{username}:{access_token}'.encode('utf-8')).decode('utf-8')
                headers['Authorization'] = [f'Basic {auth_header}']
            response = yield TwistedAdditions.request(
                comment_url, type=b'POST', timeout=60,
                headers=headers, json=dict(body=content),
                logger=lambda content: self._addToLog('stdio', content),
            )
            if response.status_code // 100 != 2:
                yield self._addToLog('stdio', f"Failed to post comment to PR {pr_number}. Unexpected response code from GitHub: {response.status_code}\n")
                defer.returnValue(FAILURE)
                return

            defer.returnValue(SUCCESS)
        except Exception as e:
            yield self._addToLog('stdio', f'Error in posting comment to PR {pr_number}\n    {e}\n')
            defer.returnValue(FAILURE)

    @defer.inlineCallbacks
    def update_pr(self, pr_number, title, description, base=None, head=None, repository_url=None):
        api_url = GitHub.api_url(repository_url)
        if not api_url:
            return defer.returnValue(False)

        pr_info = {}
        if title:
            pr_info['title'] = title
        if description:
            pr_info['body'] = description
        if base:
            pr_info['base'] = base
        if head:
            pr_info['head'] = head

        if not pr_info:
            yield self._addToLog('stdio', f"Not enough details provided to update {pr_number}.\n")
            return defer.returnValue(False)

        update_url = f'{api_url}/pulls/{pr_number}'
        try:
            headers = {'Accept': ['application/vnd.github.v3+json']}
            username, access_token = GitHub.credentials(user=GitHub.user_for_queue(self.getProperty('buildername', '')))
            if username and access_token:
                auth_header = b64encode(f'{username}:{access_token}'.encode('utf-8')).decode('utf-8')
                headers['Authorization'] = [f'Basic {auth_header}']
            response = yield TwistedAdditions.request(
                update_url, type=b'PATCH', timeout=60,
                headers=headers, json=pr_info,
                logger=lambda content: self._addToLog('stdio', content),
            )
            if response.status_code // 100 != 2:
                yield self._addToLog('stdio', f"Failed to update PR {pr_number}. Unexpected response code from GitHub: {response.status_code}\n")
                return defer.returnValue(False)
            defer.returnValue(True)
        except Exception as e:
            yield self._addToLog('stdio', f"Error in updating PR {pr_number}\n    {e}\n")
            defer.returnValue(False)


class ShellMixin(object):
    WINDOWS_SHELL_PLATFORMS = ['win']

    def has_windows_shell(self):
        return self.getProperty('platform', '*') in self.WINDOWS_SHELL_PLATFORMS

    def shell_command(self, command):
        if self.has_windows_shell():
            return ['sh', '-c', command]
        return ['/bin/sh', '-c', command]

    def shell_exit_0(self):
        if self.has_windows_shell():
            return 'exit 0'
        return 'true'


class AddToLogMixin(object):
    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)


class Contributors(object):
    url = 'https://raw.githubusercontent.com/WebKit/WebKit/main/metadata/contributors.json'
    contributors = {}
    last_update = 0
    REFRESH = 4 * 60 * 60

    @classmethod
    def load_from_disk(cls):
        cwd = os.path.abspath(os.path.dirname(__file__))
        repo_root = os.path.dirname(os.path.dirname(os.path.dirname(cwd)))
        contributors_path = os.path.join(repo_root, 'metadata/contributors.json')
        try:
            with open(contributors_path, 'rb') as contributors_json:
                return json.load(contributors_json), None
        except Exception as e:
            return {}, 'Failed to load {}\n'.format(contributors_path)

    @classmethod
    @defer.inlineCallbacks
    def load_from_github(cls):
        try:
            response = yield TwistedAdditions.request(cls.url, type=b'GET', timeout=60)
            if response.status_code != 200:
                return defer.returnValue((
                    {}, 'Failed to access {} with status code: {}\n'.format(cls.url, response.status_code)
                ))
            return defer.returnValue((response.json(), None))
        except Exception as e:
            return defer.returnValue(({}, 'Failed to access {url}\n'.format(url=cls.url)))

    @classmethod
    @defer.inlineCallbacks
    def load(cls, use_network=None):
        errors = []

        now = int(time.time())
        if cls.last_update < now - cls.REFRESH:
            cls.contributors = {}

        if use_network:
            cls.contributors = {}
            contributors_json, error = yield cls.load_from_github()
            if error:
                errors.append(error)
            else:
                cls.last_update = now
        elif use_network is False:
            contributors_json, error = cls.load_from_disk()
            if error:
                errors.append(error)
        elif cls.contributors:
            return cls.contributors, errors
        else:
            contributors_json, error = yield cls.load_from_github()
            if error:
                errors.append(error)
            else:
                cls.last_update = now

        if not contributors_json:
            errors.append('Loading data from disk...\n')
            contributors_json, error = cls.load_from_disk()
            if error:
                errors.append(error)
            if contributors_json:
                errors = []

        for value in contributors_json:
            name = value.get('name')
            emails = value.get('emails')
            github_username = value.get('github')
            if name and emails:
                bugzilla_email = emails[0].lower()  # We're requiring that the first email is the primary bugzilla email
                cls.contributors[bugzilla_email] = {
                    'name': name,
                    'status': value.get('status'),
                    'email': bugzilla_email,
                }
                cls.contributors[name] = {
                    'name': name,
                    'status': value.get('status'),
                    'email': bugzilla_email,
                }
            if github_username and name and emails:
                cls.contributors[github_username.lower()] = dict(
                    name=name,
                    status=value.get('status'),
                    email=emails[0].lower(),
                )
        return defer.returnValue((cls.contributors, errors))


class ConfigureBuild(buildstep.BuildStep, AddToLogMixin):
    name = 'configure-build'
    description = ['configuring build']
    descriptionDone = ['Configured build']

    def __init__(self, platform, configuration, architectures, buildOnly, triggers, remotes, additionalArguments, triggered_by=None):
        super().__init__()
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

    @defer.inlineCallbacks
    def run(self):
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
        yield self.add_pr_details()

        defer.returnValue(SUCCESS)

    def add_patch_id_url(self):
        patch_id = self.getProperty('patch_id', '')
        if patch_id:
            self.setProperty('remote', 'origin')
            self.setProperty('change_id', patch_id, 'ConfigureBuild')
            self.addURL('Patch {}'.format(patch_id), Bugzilla.patch_url(patch_id))

    @defer.inlineCallbacks
    def add_pr_details(self):
        pr_number = self.getProperty('github.number')
        if not pr_number:
            return

        repository_url = self.getProperty('repository', '')
        title = self.getProperty('github.title', '')
        owners = self.getProperty('owners', [])
        revision = self.getProperty('github.head.sha')

        project = self.getProperty('project')
        if project == GITHUB_PROJECTS[0]:
            self.setProperty('remote', DEFAULT_REMOTE)
            self.setProperty('sensitive', False)
        elif project in GITHUB_PROJECTS:
            self.setProperty('remote', project.split('-')[-1] if '-' in project else project.split('/')[0])
            self.setProperty('sensitive', True)
        else:
            self.setProperty('sensitive', True)

        self.setProperty('change_id', revision[:HASH_LENGTH_TO_DISPLAY], 'ConfigureBuild')

        title = f': {title}' if title else ''
        self.addURL(f'PR {pr_number}{title}', GitHub.pr_url(pr_number, repository_url))
        if owners:
            email, errors = yield GitHub.email_for_owners(owners)
            for error in errors:
                yield self._addToLog('stdio', error)
            if email:
                display_name = '{} ({})'.format(email, owners[0])
            else:
                display_name = owners[0]
            self.addURL('PR by: {}'.format(display_name), '{}{}'.format(GITHUB_URL, owners[0]))
        if revision:
            self.addURL('Hash: {}'.format(revision[:HASH_LENGTH_TO_DISPLAY]), GitHub.commit_url(revision, repository_url))


class CheckOutSource(git.Git):
    name = 'checkout-source'
    CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR = (0, 2)
    haltOnFailure = False

    def __init__(self, repourl=f'{GITHUB_URL}{GITHUB_PROJECTS[0]}.git', **kwargs):
        self.default_repourl = repourl
        super().__init__(
            repourl=repourl,
            retry=self.CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR,
            timeout=2 * 60 * 60,
            alwaysUseLatest=True,
            logEnviron=False,
            method='clean',
            progress=True,
            **kwargs
        )

    def getResultSummary(self):
        if self.results == FAILURE:
            self.build.addStepsAfterCurrentStep([CleanUpGitIndexLock()])

        if self.results != SUCCESS:
            return {'step': 'Failed to updated working directory'}
        else:
            return {'step': 'Cleaned and updated working directory'}

    def _dovccmd(self, *args, **kwargs):
        if kwargs.get('collectStdout') or not self.getProperty('sensitive', False):
            return super()._dovccmd(*args, **kwargs)

        class ScrubbedRemoteCommand(remotecommand.RemoteShellCommand):
            def useLog(self, *args, **kwargs):
                pass

        with mock.patch('buildbot.process.remotecommand.RemoteShellCommand', ScrubbedRemoteCommand):
            return super()._dovccmd(*args, **kwargs)

    @defer.inlineCallbacks
    def _fetch(self, _):
        project = self.getProperty('project', '') or GITHUB_PROJECTS[0]
        self.repourl = f'{GITHUB_URL}{project}.git'

        try:
            rc = yield super()._fetch(_)
        finally:
            self.repourl = self.default_repourl
        defer.returnValue(rc)

    @defer.inlineCallbacks
    def run(self):
        self.branch = self.getProperty('github.base.ref') or self.branch

        username, access_token = GitHub.credentials(user=GitHub.user_for_queue(self.getProperty('buildername', '')))
        self.env = dict(
            GIT_USER=username,
            GIT_PASSWORD=access_token,
        )

        rc = yield super().run()
        if rc == SUCCESS and self.getProperty('buildername', '').lower() not in QUEUES_WITH_PUSH_ACCESS:
            yield self._dovccmd(['remote', 'set-url', '--push', 'origin', 'PUSH_DISABLED_BY_ADMIN'])
        defer.returnValue(rc)


class CleanUpGitIndexLock(shell.ShellCommand):
    name = 'clean-git-index-lock'
    command = ['rm', '-f', '.git/index.lock']
    descriptionDone = ['Deleted .git/index.lock']

    def __init__(self, **kwargs):
        super().__init__(timeout=2 * 60, logEnviron=False, **kwargs)

    def start(self):
        platform = self.getProperty('platform', '*')
        if platform == 'win':
            self.command = ['del', r'.git\index.lock']

        self.send_email_for_git_issue()
        return shell.ShellCommand.start(self)

    def evaluateCommand(self, cmd):
        self.build.buildFinished(['Git issue, retrying build'], RETRY)
        return super().evaluateCommand(cmd)

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


class CheckOutSpecificRevision(shell.ShellCommandNewStyle):
    name = 'checkout-specific-revision'
    descriptionDone = ['Checked out required revision']
    flunkOnFailure = False
    haltOnFailure = False

    @staticmethod
    def doCheckOutSpecificRevision(obj):
        return obj.getProperty('ews_revision', False)

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, **kwargs)

    def doStepIf(self, step):
        return self.doCheckOutSpecificRevision(self)

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)

    def buildCommandKwargs(self, warnings):
        result = super().buildCommandKwargs(warnings)
        if self.getProperty('sensitive', False):
            result['stdioLogName'] = None
        return result

    def run(self):
        self.command = ['git', 'checkout', self.getProperty('ews_revision')]
        return super().run()


class FetchBranches(steps.ShellSequence, ShellMixin):
    name = 'fetch-branch-references'
    descriptionDone = ['Updated branch information']
    flunkOnFailure = False
    haltOnFailure = False

    def __init__(self, **kwargs):
        super().__init__(timeout=5 * 60, logEnviron=False, **kwargs)

    def run(self):
        self.commands = [
            util.ShellArg(command=['git', 'fetch', DEFAULT_REMOTE, '--prune'], logname='stdio'),
            util.ShellArg(command=['git', 'config', 'credential.helper', '!echo_credentials() { sleep 1; echo "username=${GIT_USER}"; echo "password=${GIT_PASSWORD}"; }; echo_credentials'], logname='stdio'),
        ]

        project = self.getProperty('project', GITHUB_PROJECTS[0])
        remote = self.getProperty('remote', DEFAULT_REMOTE)
        if remote != DEFAULT_REMOTE:
            for command in [
                self.shell_command('git remote add {} {}{}.git || {}'.format(remote, GITHUB_URL, project, self.shell_exit_0())),
                ['git', 'remote', 'set-url', remote, '{}{}.git'.format(GITHUB_URL, project)],
                ['git', 'fetch', remote, '--prune'],
            ]:
                self.commands.append(util.ShellArg(command=command, logname='stdio', haltOnFailure=True))

            username, access_token = GitHub.credentials(user=GitHub.user_for_queue(self.getProperty('buildername', '')))
            self.env = dict(
                GIT_USER=username,
                GIT_PASSWORD=access_token,
            )

        return super().run()

    def hideStepIf(self, results, step):
        return results == SUCCESS


class ShowIdentifier(shell.ShellCommand):
    name = 'show-identifier'
    identifier_re = '^Identifier: (.*)$'
    flunkOnFailure = False
    haltOnFailure = False

    def __init__(self, **kwargs):
        super().__init__(timeout=5 * 60, logEnviron=False, **kwargs)

    def start(self):
        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('stdio', self.log_observer)

        revision = 'HEAD'
        # Note that these properties are delibrately in priority order.
        for property_ in ['ews_revision', 'got_revision']:
            candidate = self.getProperty(property_)
            if candidate:
                revision = candidate
                break

        self.setCommand(['python3', 'Tools/Scripts/git-webkit', 'find', revision])
        return super().start()

    def evaluateCommand(self, cmd):
        rc = super().evaluateCommand(cmd)
        if rc != SUCCESS:
            return rc

        log_text = self.log_observer.getStdout()
        match = re.search(self.identifier_re, log_text, re.MULTILINE)
        if match:
            identifier = match.group(1)
            if identifier:
                identifier = identifier.replace('master', DEFAULT_BRANCH)
            self.setProperty('identifier', identifier)
            if CheckOutSpecificRevision.doCheckOutSpecificRevision(self):
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
        return super().getResultSummary()

    def hideStepIf(self, results, step):
        return results == SUCCESS


class InstallHooks(steps.ShellSequence):
    name = 'install-hooks'
    flunkOnFailure = False
    haltOnFailure = False

    def __init__(self, **kwargs):
        super().__init__(timeout=30, logEnviron=False, **kwargs)

    def run(self):
        install_hooks_command = [
            'python3', 'Tools/Scripts/git-webkit',
            'install-hooks', 'pre-push',
            '--mode', 'no-radar',
        ]
        source = self.getProperty('github.head.repo.full_name', None)
        project = self.getProperty('project', None)
        if project in GITHUB_PROJECTS and source:
            install_hooks_command += ['--level', 'github.com:{}={}'.format(source, GITHUB_PROJECTS.index(project))]

        self.commands = []
        for command in [
            ['git', 'config', 'include.path', '../metadata/git_config_extension'],
            install_hooks_command,
        ]:
            self.commands.append(util.ShellArg(command=command, logname='stdio'))

        return super().run()

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': 'Installed hooks to checkout'}
        return {'step': 'Failed to install hooks to checkout'}


class CleanWorkingDirectory(shell.ShellCommandNewStyle):
    name = 'clean-working-directory'
    description = ['clean-working-directory running']
    descriptionDone = ['Cleaned working directory']
    flunkOnFailure = True
    haltOnFailure = True
    command = ['python3', 'Tools/Scripts/clean-webkit']

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, **kwargs)

    def run(self):
        platform = self.getProperty('platform')
        if platform in ('gtk', 'wpe'):
            self.command = self.command + ['--keep-jhbuild-directory']
        return super().run()


class CleanDerivedSources(shell.ShellCommandNewStyle):
    name = 'clean-derived-sources'
    description = ['clean-derived-sources running']
    descriptionDone = ['Cleaned derived sources directories']
    command = ['python3', 'Tools/Scripts/clean-webkit', '--derived-sources-only']

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, **kwargs)

    def run(self):
        return super().run()

class UpdateWorkingDirectory(steps.ShellSequence, ShellMixin):
    name = 'update-working-directory'
    description = ['update-working-directory running']
    flunkOnFailure = True
    haltOnFailure = True

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to updated working directory'}
        else:
            return {'step': 'Updated working directory'}

    @defer.inlineCallbacks
    def run(self):
        remote = self.getProperty('remote', DEFAULT_REMOTE)
        base = self.getProperty('github.base.ref', DEFAULT_BRANCH)

        commands = [
            ['git', 'checkout', 'remotes/{}/{}'.format(remote, base), '-f'],
            self.shell_command('git branch -D {} || {}'.format(base, self.shell_exit_0())),
            ['git', 'checkout', '-b', base],
        ]
        if base != DEFAULT_BRANCH:
            commands.append(self.shell_command('git branch -D {} || {}'.format(DEFAULT_BRANCH, self.shell_exit_0())))
            commands.append(['git', 'branch', '--track', DEFAULT_BRANCH, f'remotes/{DEFAULT_REMOTE}/{DEFAULT_BRANCH}'])

        self.commands = []
        for command in commands:
            self.commands.append(util.ShellArg(command=command, logname='stdio', haltOnFailure=True))

        rc = yield super().run()
        if rc == FAILURE:
            self.build.buildFinished(['Git issue, retrying build'], RETRY)
        defer.returnValue(rc)


class ApplyPatch(steps.ShellSequence, CompositeStepMixin, ShellMixin):
    name = 'apply-patch'
    description = ['apply-patch']
    descriptionDone = ['Applied patch']
    haltOnFailure = True
    env = dict(FILTER_BRANCH_SQUELCH_WARNING='1')
    FILTER_BRANCH_PROGRAM = '''import re
import sys

lines = [l for l in sys.stdin]
for s in re.split(r' (Need the bug URL \\(OOPS!\\).)|(\\S+:\\/\\/\\S+)', lines[0].rstrip()):
    if s and s != ' ':
        print(s)
for l in lines[1:]:
    sys.stdout.write(l)
'''

    def __init__(self, **kwargs):
        super().__init__(timeout=10 * 60, logEnviron=False, **kwargs)

    def doStepIf(self, step):
        return self.getProperty('patch_id', False)

    def hideStepIf(self, results, step):
        return not self.doStepIf(step) or (results == SUCCESS and self.getProperty('sensitive', False))

    def _get_patch(self):
        sourcestamp = self.build.getSourceStamp(self.getProperty('codebase', ''))
        if not sourcestamp or not sourcestamp.patch:
            return None
        return sourcestamp.patch[1]

    @defer.inlineCallbacks
    def run(self):
        self.commands = []
        patch = self._get_patch()

        commands = []
        if not patch:
            commands += [['curl', '-L', 'https://bugs.webkit.org/attachment.cgi?id={}'.format(self.getProperty('patch_id', '')), '-o', '.buildbot-diff']]
        commands += [
            ['git', 'config', 'user.name', 'EWS'],
            ['git', 'config', 'user.email', FROM_EMAIL],
            ['git', 'am', '--keep-non-patch', '.buildbot-diff'],
        ]
        if not self.has_windows_shell():
            commands.append(['git', 'filter-branch', '-f', '--msg-filter', 'python3 -c "{}"'.format(self.FILTER_BRANCH_PROGRAM), 'HEAD...HEAD~1'])

        for command in commands:
            self.commands.append(util.ShellArg(command=command, logname='stdio', haltOnFailure=True))

        _ = yield self.downloadFileContentToWorker('.buildbot-diff', patch)
        res = yield super().run()
        defer.returnValue(res)

    def getResultSummary(self):
        if self.results == SKIPPED:
            return {'step': "Skipping applying patch since patch_id isn't provided"}
        if self.results != SUCCESS:
            return {'step': 'git failed to apply patch to trunk'}
        return super().getResultSummary()


class CheckOutPullRequest(steps.ShellSequence, ShellMixin):
    name = 'checkout-pull-request'
    description = ['checking-out-pull-request']
    descriptionDone = ['Checked out pull request']
    haltOnFailure = True

    def __init__(self, **kwargs):
        super().__init__(timeout=10 * 60, logEnviron=False, **kwargs)

    def doStepIf(self, step):
        return self.getProperty('github.number', False)

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)

    def makeRemoteShellCommand(self, collectStdout=False, collectStderr=False, stdioLogName='stdio', **overrides):
        if self.getProperty('sensitive', False):
            stdioLogName = None
        return super().makeRemoteShellCommand(
            collectStdout=collectStdout, collectStderr=collectStderr,
            stdioLogName=stdioLogName, **overrides
        )

    def run(self):
        self.commands = []

        remote, repo_name = self.getProperty('github.head.repo.full_name', DEFAULT_REMOTE).split('/', 1)
        if '-' in repo_name:
            remote = f"{remote}-{repo_name.split('-', 1)[-1]}"
        project = self.getProperty('github.head.repo.full_name', self.getProperty('project'))
        pr_branch = self.getProperty('github.head.ref', DEFAULT_BRANCH)
        rebase_target_hash = self.getProperty('ews_revision') or self.getProperty('got_revision')

        commands = [
            ['git', 'config', 'credential.helper', '!echo_credentials() { sleep 1; echo "username=${GIT_USER}"; echo "password=${GIT_PASSWORD}"; }; echo_credentials'],
            self.shell_command('git remote add {} {}{}.git || {}'.format(remote, GITHUB_URL, project, self.shell_exit_0())),
            ['git', 'remote', 'set-url', remote, '{}{}.git'.format(GITHUB_URL, project)],
            ['git', 'fetch', remote, pr_branch],
            ['git', 'checkout', '-b', pr_branch],
            ['git', 'cherry-pick', 'HEAD..remotes/{}/{}'.format(remote, pr_branch)],
        ]
        for command in commands:
            self.commands.append(util.ShellArg(command=command, logname='stdio', haltOnFailure=True))

        username, access_token = GitHub.credentials(user=GitHub.user_for_queue(self.getProperty('buildername', '')))
        self.env = dict(
            GIT_COMMITTER_NAME='EWS',
            GIT_COMMITTER_EMAIL=FROM_EMAIL,
            GIT_USER=username,
            GIT_PASSWORD=access_token,
        )

        return super().run()

    def getResultSummary(self):
        if self.results == SKIPPED:
            return {'step': 'No pull request to checkout'}
        if self.results != SUCCESS:
            return {'step': 'Failed to checkout and rebase branch from PR {}'.format(self.getProperty('github.number'))}
        return super().getResultSummary()


class AnalyzeChange(buildstep.BuildStep, AddToLogMixin):
    flunkOnFailure = True
    haltOnFailure = True

    def _get_patch(self):
        sourcestamp = self.build.getSourceStamp(self.getProperty('codebase', ''))
        if sourcestamp and sourcestamp.changes:
            return '\n'.join(sourcestamp.changes[0].files).encode('utf-8')
        if sourcestamp and sourcestamp.patch:
            return sourcestamp.patch[1]
        return None

    @property
    def change_type(self):
        if self.getProperty('github.number', False):
            return 'Pull request'
        return 'Patch'

    def getResultSummary(self):
        if self.results in [FAILURE, SKIPPED]:
            return {'step': '{} doesn\'t have relevant changes'.format(self.change_type)}
        if self.results == SUCCESS:
            return {'step': '{} contains relevant changes'.format(self.change_type)}
        return buildstep.BuildStep.getResultSummary(self)


class CheckChangeRelevance(AnalyzeChange):
    name = 'check-change-relevance'
    description = ['check-change-relevance running']
    descriptionDone = ['Change contains relevant changes']
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
        re.compile(rb'.*javascriptcore.*', re.IGNORECASE),
        re.compile(rb'JSTests/', re.IGNORECASE),
        re.compile(rb'LayoutTests/js.*', re.IGNORECASE),  # This catches both js/ and jsc-layout-tests.yaml
        re.compile(rb'Source/WTF/', re.IGNORECASE),
        re.compile(rb'Source/bmalloc/', re.IGNORECASE),
        re.compile(rb'Source/cmake/', re.IGNORECASE),
        re.compile(rb'.*Makefile.*', re.IGNORECASE),
        re.compile(rb'Tools/.*jsc.*', re.IGNORECASE),
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

    monterey_builder_path_regexes = [
        re.compile(rb'Source/', re.IGNORECASE),
        re.compile(rb'Tools/', re.IGNORECASE),
    ]

    webkitpy_path_regexes = [
        re.compile(rb'Tools/Scripts/webkitpy', re.IGNORECASE),
        re.compile(rb'Tools/Scripts/libraries', re.IGNORECASE),
        re.compile(rb'Tools/Scripts/commit-log-editor', re.IGNORECASE),
        re.compile(rb'Source/WebKit/Scripts', re.IGNORECASE),
        re.compile(rb'metadata/contributors.json', re.IGNORECASE),
    ]

    safer_cpp_path_regexes = [
        re.compile(rb'Source/WebKit', re.IGNORECASE),
        re.compile(rb'Source/WebCore', re.IGNORECASE),
        re.compile(rb'Tools/Scripts/build-and-analyze', re.IGNORECASE),
        re.compile(rb'Tools/Scripts/generate-dirty-files', re.IGNORECASE),
        re.compile(rb'Tools/Scripts/compare-static-analysis-results', re.IGNORECASE),
        re.compile(rb'Tools/Scripts/generate-dirty-files', re.IGNORECASE),
        re.compile(rb'Tools/CISupport/Shared/download-and-install-build-tools', re.IGNORECASE),
    ]

    group_to_paths_mapping = {
        'bindings': bindings_path_regexes,
        'monterey-release-build': monterey_builder_path_regexes,
        'services-ews': services_path_regexes,
        'jsc': jsc_path_regexes,
        'webkitpy': webkitpy_path_regexes,
        'wk1-tests': wk1_path_regexes,
        'safer-cpp': safer_cpp_path_regexes,
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
            self._addToLog('stdio', 'This {} contains relevant changes.'.format(self.change_type.lower()))
            self.finished(SUCCESS)
            return None

        self._addToLog('stdio', 'This {} does not have relevant changes.'.format(self.change_type.lower()))
        self.finished(FAILURE)
        self.build.results = SKIPPED
        self.build.buildFinished(['{} {} doesn\'t have relevant changes'.format(
            self.change_type,
            self.getProperty('patch_id', '') or self.getProperty('github.number', ''),
        )], SKIPPED)
        return None


class GetTestExpectationsBaseline(shell.ShellCommand, ShellMixin):
    name = 'get-test-expectations-baseline'
    description = 'get-test-expectations-baseline running'
    descriptionDone = 'Found baseline expectations for layout tests'
    command = ['python3', 'Tools/Scripts/run-webkit-tests', '--print-expectations', WithProperties('--%(configuration)s')]

    @defer.inlineCallbacks
    def run(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

        platform = self.getProperty('platform')
        self.setCommand(self.command + customBuildFlag(platform, self.getProperty('fullPlatform')))

        patch_author = self.getProperty('patch_author')
        if patch_author in ['webkit-wpt-import-bot@igalia.com']:
            self.setCommand(self.command + ['imported/w3c/web-platform-tests'])

        additionalArguments = self.getProperty('additionalArguments', '')
        if additionalArguments:
            self.setCommand(self.command + additionalArguments)

        self.setCommand(self.shell_command(' '.join(self.command) + ' > base-expectations.txt'))
        rc = yield super().run()

        log_text = log_text = self.log_observer.getStdout() + self.log_observer.getStderr()
        match = re.search(r'Found.*', log_text)
        if match:
            defer.returnValue(SUCCESS)
        defer.returnValue(rc)


class GetUpdatedTestExpectations(steps.ShellSequence, ShellMixin):
    name = 'get-updated-test-expectations'
    description = 'get-updated-test-expectations running'
    descriptionDone = 'Found updated expectations for layout tests'

    @defer.inlineCallbacks
    def run(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

        configuration_flag = [f"--{self.getProperty('configuration')}"] if self.getProperty('configuration') else []
        platform_flag = customBuildFlag(self.getProperty('platform'), self.getProperty('fullPlatform'))
        run_webkit_command = ['python3', 'Tools/Scripts/run-webkit-tests', '--print-expectations'] + configuration_flag + platform_flag

        patch_author = self.getProperty('patch_author')
        if patch_author in ['webkit-wpt-import-bot@igalia.com']:
            run_webkit_command += ['imported/w3c/web-platform-tests']

        additionalArguments = self.getProperty('additionalArguments', '')
        if additionalArguments:
            run_webkit_command += additionalArguments

        run_webkit_command = ' '.join(run_webkit_command) + ' > new-expectations.txt'

        self.commands = []
        for command in [
            self.shell_command(run_webkit_command),
            self.shell_command("perl -p -i -e 's/\\].*/\\]/' base-expectations.txt"),
            self.shell_command("perl -p -i -e 's/\\].*/\\]/' new-expectations.txt"),
        ]:
            self.commands.append(util.ShellArg(command=command, logname='stdio'))

        rc = yield super().run()

        log_text = log_text = self.log_observer.getStdout() + self.log_observer.getStderr()
        match = re.search(r'Found.*', log_text)
        if match:
            defer.returnValue(SUCCESS)
        defer.returnValue(rc)


class FindModifiedLayoutTests(shell.ShellCommandNewStyle, AnalyzeChange):
    name = 'find-modified-layout-tests'
    description = 'find-modified-layout tests running'
    descriptionDone = 'Found modified layout tests'
    RE_LAYOUT_TEST = br'^(\+\+\+).*(LayoutTests.*\.html|LayoutTests.*\.svg|LayoutTests.*\.xml)'
    DIRECTORIES_TO_IGNORE = ['reference', 'reftest', 'resources', 'support', 'script-tests', 'tools']
    SUFFIXES_TO_IGNORE = ['-expected', '-expected-mismatch', '-ref', '-notref']
    command = ['diff', '-u', '-w', 'base-expectations.txt', 'new-expectations.txt']

    def __init__(self, skipBuildIfNoResult=True):
        self.skipBuildIfNoResult = skipBuildIfNoResult
        super().__init__(logEnviron=False)

    @defer.inlineCallbacks
    def run(self):
        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('stdio', self.log_observer)
        rc = yield super().run()
        modified_tests = set()
        log_text = self.log_observer.getStdout()
        match = re.findall(r'\+(.*\.html)', log_text) + re.findall(r'\+(.*\.svg)', log_text) + re.findall(r'\+(.*\.xml)', log_text)
        yield self._addToLog('stdio', '\nLooking for test expectation changes...\n')
        for test in match:
            yield self._addToLog('stdio', f'    LayoutTests/{test}\n')
            modified_tests.add(f'LayoutTests/{test}')
        modified_tests = list(modified_tests)

        patch = self._get_patch()
        if not patch:
            yield self._addToLog('stdio', 'Unable to access the patch/PR content.\n')
            self.results = WARNINGS
            if self.skipBuildIfNoResult:
                self.build.buildFinished(['{} {} could not be accessed'.format(
                    self.change_type,
                    self.getProperty('patch_id', '') or self.getProperty('github.number', ''),
                )], WARNINGS)
            return defer.returnValue(self.results)

        yield self._addToLog('stdio', '\nLooking for layout test changes...\n')
        tests_from_patch = self.find_test_names_from_patch(patch)
        modified_tests += tests_from_patch

        if modified_tests:
            yield self._addToLog('stdio', '\nThis change modifies following tests: {}\n'.format(modified_tests))
            self.setProperty('modified_tests', modified_tests)
            self.results = SUCCESS
        else:
            yield self._addToLog('stdio', 'This change does not modify any layout tests\n')
            self.results = SKIPPED
            if self.skipBuildIfNoResult:
                self.build.results = SKIPPED
                self.build.buildFinished(['{} {} doesn\'t have relevant changes'.format(
                    self.change_type,
                    self.getProperty('patch_id', '') or self.getProperty('github.number', ''),
                )], SKIPPED)
        return defer.returnValue(self.results)

    def find_test_names_from_patch(self, patch):
        tests = []
        for line in patch.splitlines():
            match = re.search(self.RE_LAYOUT_TEST, line, re.IGNORECASE)
            if match:
                if any(((suffix + '.html').encode('utf-8') or (suffix + '.svg').encode('utf-8') or (suffix + '.xml').encode('utf-8')) in line for suffix in self.SUFFIXES_TO_IGNORE):
                    continue
                test_name = match.group(2).decode('utf-8')
                if any(directory in test_name.split('/') for directory in self.DIRECTORIES_TO_IGNORE):
                    continue
                tests.append(test_name)
        return list(set(tests))

    def getResultSummary(self):
        if self.results == WARNINGS:
            return {'step': '{} could not be accessed'.format(self.change_type)}
        return super().getResultSummary()


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


class BugzillaMixin(AddToLogMixin):
    addURLs = False
    bug_open_statuses = ['UNCONFIRMED', 'NEW', 'ASSIGNED', 'REOPENED']
    bug_closed_statuses = ['RESOLVED', 'VERIFIED', 'CLOSED']
    fast_cq_preambles = ('revert of ', 'fast-cq', '[fast-cq]')

    @defer.inlineCallbacks
    def fetch_data_from_url_with_authentication_bugzilla(self, url):
        response = None
        try:
            response = yield TwistedAdditions.request(
                url, type=b'GET', timeout=60,
                params={'Bugzilla_api_key': self.get_bugzilla_api_key()},
                logger=lambda content: self._addToLog('stdio', content),
            )
            if response.status_code != 200:
                yield self._addToLog('stdio', 'Accessed {url} with unexpected status code {status_code}.\n'.format(url=url, status_code=response.status_code))
                return defer.returnValue(None)
        except Exception as e:
            # Catching all exceptions here to safeguard api key.
            yield self._addToLog('stdio', 'Failed to access {url}.\n'.format(url=url))
            return defer.returnValue(None)
        return defer.returnValue(response)

    @defer.inlineCallbacks
    def get_patch_json(self, patch_id):
        patch_url = '{}rest/bug/attachment/{}'.format(BUG_SERVER_URL, patch_id)
        patch = yield self.fetch_data_from_url_with_authentication_bugzilla(patch_url)
        if not patch:
            return defer.returnValue(None)
        try:
            patch_json = patch.json().get('attachments')
        except Exception as e:
            yield self._addToLog('stdio', f'Failed to fetch patch json from {patch_url}, error: {e}')
            return defer.returnValue(None)
        if not patch_json or len(patch_json) == 0:
            return defer.returnValue(None)
        return defer.returnValue(patch_json.get(str(patch_id)))

    @defer.inlineCallbacks
    def get_bug_json(self, bug_id):
        bug_url = '{}rest/bug/{}'.format(BUG_SERVER_URL, bug_id)
        bug = yield self.fetch_data_from_url_with_authentication_bugzilla(bug_url)
        if not bug:
            return defer.returnValue(None)
        try:
            bugs_json = bug.json().get('bugs')
        except Exception as e:
            yield self._addToLog('stdio', f'Failed to fetch bug json from {bug_url}, error: {e}')
            return defer.returnValue(None)
        if not bugs_json or len(bugs_json) == 0:
            return defer.returnValue(None)
        return defer.returnValue(bugs_json[0])

    @defer.inlineCallbacks
    def get_bug_id_from_patch(self, patch_id):
        patch_json = yield self.get_patch_json(patch_id)
        if not patch_json:
            yield self._addToLog('stdio', 'Unable to fetch patch {}.\n'.format(patch_id))
            return defer.returnValue(-1)
        return defer.returnValue(patch_json.get('bug_id'))

    @defer.inlineCallbacks
    def _is_patch_obsolete(self, patch_id):
        patch_json = yield self.get_patch_json(patch_id)
        if not patch_json:
            yield self._addToLog('stdio', 'Unable to fetch patch {}.\n'.format(patch_id))
            return defer.returnValue(-1)

        if str(patch_json.get('id')) != self.getProperty('patch_id', ''):
            yield self._addToLog('stdio', 'Fetched patch id {} does not match with requested patch id {}. Unable to validate.\n'.format(patch_json.get('id'), self.getProperty('patch_id', '')))
            return defer.returnValue(-1)

        patch_author = patch_json.get('creator')
        self.setProperty('patch_author', patch_author)
        patch_title = patch_json.get('summary')
        if patch_title.lower().startswith(self.fast_cq_preambles):
            self.setProperty('fast_commit_queue', True)
        if self.addURLs:
            self.addURL('Patch by: {}'.format(patch_author), '')
        return defer.returnValue(patch_json.get('is_obsolete'))

    @defer.inlineCallbacks
    def _is_patch_review_denied(self, patch_id):
        patch_json = yield self.get_patch_json(patch_id)
        if not patch_json:
            yield self._addToLog('stdio', 'Unable to fetch patch {}.\n'.format(patch_id))
            return defer.returnValue(-1)

        for flag in patch_json.get('flags', []):
            if flag.get('name') == 'review' and flag.get('status') == '-':
                return defer.returnValue(1)
        return defer.returnValue(0)

    @defer.inlineCallbacks
    def _is_patch_cq_plus(self, patch_id):
        patch_json = yield self.get_patch_json(patch_id)
        if not patch_json:
            yield self._addToLog('stdio', 'Unable to fetch patch {}.\n'.format(patch_id))
            return defer.returnValue(-1)

        for flag in patch_json.get('flags', []):
            if flag.get('name') == 'commit-queue' and flag.get('status') == '+':
                self.setProperty('patch_committer', flag.get('setter', ''))
                return defer.returnValue(1)
        return defer.returnValue(0)

    @defer.inlineCallbacks
    def _does_patch_have_acceptable_review_flag(self, patch_id):
        patch_json = yield self.get_patch_json(patch_id)
        if not patch_json:
            yield self._addToLog('stdio', 'Unable to fetch patch {}.\n'.format(patch_id))
            return defer.returnValue(-1)

        for flag in patch_json.get('flags', []):
            if flag.get('name') == 'review':
                review_status = flag.get('status')
                if review_status == '+':
                    reviewer = flag.get('setter', '')
                    self.setProperty('reviewer', reviewer)
                    if self.addURLs:
                        self.addURL('Reviewed by: {}'.format(reviewer), '')
                    return defer.returnValue(1)
                if review_status in ['-', '?']:
                    self._addToLog('stdio', 'Patch {} is marked r{}.\n'.format(patch_id, review_status))
                    return defer.returnValue(0)
        return defer.returnValue(1)  # Patch without review flag is acceptable, since the ChangeLog might have 'Reviewed by' in it.

    @defer.inlineCallbacks
    def _is_bug_closed(self, bug_id):
        if not bug_id:
            yield self._addToLog('stdio', 'Skipping bug status validation since bug id is None.\n')
            return defer.returnValue(-1)

        bug_json = yield self.get_bug_json(bug_id)
        if not bug_json or not bug_json.get('status'):
            yield self._addToLog('stdio', 'Unable to fetch bug {}.\n'.format(bug_id))
            return defer.returnValue(-1)

        bug_title = bug_json.get('summary')
        sensitive = bug_json.get('product') == 'Security'
        if sensitive:
            self.setProperty('sensitive', True)
            bug_title = ''
        self.setProperty('bug_title', bug_title)
        if self.addURLs:
            self.addURL('Bug {} {}'.format(bug_id, bug_title), Bugzilla.bug_url(bug_id))
        if bug_json.get('status') in self.bug_closed_statuses:
            return defer.returnValue(1)
        return defer.returnValue(0)

    @defer.inlineCallbacks
    def should_send_email_for_patch(self, patch_id):
        patch_json = yield self.get_patch_json(patch_id)
        if not patch_json:
            yield self._addToLog('stdio', 'Unable to fetch patch {}'.format(patch_id))
            return defer.returnValue(True)

        obsolete = patch_json.get('is_obsolete')
        if obsolete == 1:
            yield self._addToLog('stdio', 'Skipping email since patch {} is obsolete'.format(patch_id))
            return defer.returnValue(False)

        review_denied = False
        for flag in patch_json.get('flags', []):
            if flag.get('name') == 'review' and flag.get('status') == '-':
                review_denied = True

        if review_denied:
            yield self._addToLog('stdio', 'Skipping email since patch {} is marked r-'.format(patch_id))
            return defer.returnValue(False)
        return defer.returnValue(True)

    def send_email_for_infrastructure_issue(self, infrastructure_issue_text):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            pr_number = self.getProperty('github.number')
            sha = self.getProperty('github.head.sha', '')[:HASH_LENGTH_TO_DISPLAY]
            owners = self.getProperty('owners', [])
            author = owners[0] if owners else '?'
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            email_subject = f'Infrastructure issue at {builder_name}'
            email_text = f'The following infrastructure issue happened at:\n\n'
            email_text += f'    - Build : <a href="{build_url}">{build_url}</a>\n'
            email_text += f'    - Builder : {builder_name}\n'
            email_text += f'    - Worker : {worker_name}\n'
            email_text += f'    - PR: {pr_number}, Hash: {sha}, By: {author}\n'
            email_text += f'    - Issue: {infrastructure_issue_text}\n'
            send_email_to_bot_watchers(email_subject, email_text, builder_name, f'infrastructure-{builder_name}')
        except Exception as e:
            print(f'Error in sending email for infrastructure issue: {e}')

    def get_bugzilla_api_key(self):
        password = load_password('BUGZILLA_API_KEY', default='')
        if not password:
            print('Error in reading Bugzilla api key')
        return password

    @defer.inlineCallbacks
    def remove_flags_on_patch(self, patch_id):
        flags = [{'name': 'review', 'status': 'X'}, {'name': 'commit-queue', 'status': 'X'}]
        try:
            response = yield TwistedAdditions.request(
                f'{BUG_SERVER_URL}rest/bug/attachment/{patch_id}', type=b'PUT',
                json={'flags': flags, 'Bugzilla_api_key': self.get_bugzilla_api_key()},
                logger=lambda content: self._addToLog('stdio', content),
            )
            if response.status_code not in [200, 201]:
                yield self._addToLog('stdio', f'Unable to remove flags on patch {patch_id}. Unexpected response code from bugzilla: {response.status_code}\n')
                defer.returnValue(FAILURE)
            defer.returnValue(SUCCESS)
        except Exception as e:
            yield self._addToLog('stdio', f'Error in removing flags on Patch {patch_id}\n    {e}\n')
            defer.returnValue(FAILURE)

    @defer.inlineCallbacks
    def set_cq_minus_flag_on_patch(self, patch_id):
        flags = [{'name': 'commit-queue', 'status': '-'}]
        try:
            patch_url = f'{BUG_SERVER_URL}rest/bug/attachment/{patch_id}'
            response = yield TwistedAdditions.request(
                patch_url, type=b'PUT',
                json={'flags': flags, 'Bugzilla_api_key': self.get_bugzilla_api_key()},
                logger=lambda content: self._addToLog('stdio', content),
            )
            if response.status_code not in [200, 201]:
                yield self._addToLog('stdio', f'Unable to set cq- flag on patch {patch_id}. Unexpected response code from bugzilla: {response.status_code}')
                defer.returnValue(FAILURE)
                return
            defer.returnValue(SUCCESS)
        except Exception as e:
            yield self._addToLog('stdio', f'Error in setting cq- flag on patch {patch_id}\n    {e}\n')
            defer.returnValue(FAILURE)

    @defer.inlineCallbacks
    def close_bug(self, bug_id):
        try:
            response = yield TwistedAdditions.request(
                f'{BUG_SERVER_URL}rest/bug/{bug_id}', type=b'PUT',
                json={'status': 'RESOLVED', 'resolution': 'FIXED', 'Bugzilla_api_key': self.get_bugzilla_api_key()},
                logger=lambda content: self._addToLog('stdio', content),
            )
            if response.status_code not in [200, 201]:
                yield self._addToLog('stdio', f'Unable to close bug {bug_id}. Unexpected response code from bugzilla: {response.status_code}\n')
                defer.returnValue(FAILURE)
                return
            defer.returnValue(SUCCESS)
        except Exception as e:
            yield self._addToLog('stdio', f'Error in closing bug {bug_id}\n    {e}\n')
            defer.returnValue(FAILURE)

    @defer.inlineCallbacks
    def comment_on_bug(self, bug_id, comment_text):
        bug_comment_url = f'{BUG_SERVER_URL}rest/bug/{bug_id}/comment'
        if not comment_text:
            defer.returnValue(FAILURE)
            return
        try:
            response = yield TwistedAdditions.request(
                bug_comment_url, type=b'POST',
                json={'comment': comment_text, 'Bugzilla_api_key': self.get_bugzilla_api_key()},
                logger=lambda content: self._addToLog('stdio', content),
            )
            if response.status_code not in [200, 201]:
                yield self._addToLog('stdio', f'Unable to comment on bug {bug_id}. Unexpected response code from bugzilla: {response.status_code}')
                defer.returnValue(FAILURE)
                return
            defer.returnValue(SUCCESS)
        except Exception as e:
            yield self._addToLog('stdio', f'Error in commenting on bug {bug_id}\n    {e}\n')
            defer.returnValue(FAILURE)


class ValidateChange(buildstep.BuildStep, BugzillaMixin, GitHubMixin):
    name = 'validate-change'
    description = ['validate-change running']
    descriptionDone = ['Validated change']
    flunkOnFailure = True
    haltOnFailure = True

    def __init__(
        self,
        verifyObsolete=True,
        verifyBugClosed=True,
        verifyReviewDenied=True,
        addURLs=True,
        verifycqplus=False,
        verifyMergeQueue=False,
        verifyNoDraftForMergeQueue=False,
        enableSkipEWSLabel=True,
        branches=None,
    ):
        self.verifyObsolete = verifyObsolete
        self.verifyBugClosed = verifyBugClosed
        self.verifyReviewDenied = verifyReviewDenied
        self.verifycqplus = verifycqplus
        self.verifyMergeQueue = verifyMergeQueue
        self.verifyNoDraftForMergeQueue = verifyNoDraftForMergeQueue
        self.enableSkipEWSLabel = enableSkipEWSLabel
        self.addURLs = addURLs

        branches = branches or [r'.+']
        self.branches = [re.compile(branch) if isinstance(branch, str) else branch for branch in branches]

        super().__init__()

    def getResultSummary(self):
        if self.results == FAILURE:
            return {'step': self.descriptionDone}
        return super().getResultSummary()

    def doStepIf(self, step):
        return not self.getProperty('skip_validation', False)

    @defer.inlineCallbacks
    def skip_build(self, reason):
        yield self._addToLog('stdio', reason)
        self.build.results = SKIPPED
        self.descriptionDone = reason
        self.build.buildFinished([reason], SKIPPED)
        defer.returnValue(FAILURE)

    @defer.inlineCallbacks
    def fail_build(self, reason):
        yield self._addToLog('stdio', reason)
        self.build.results = FAILURE
        self.descriptionDone = reason
        self.build.buildFinished([reason], FAILURE)
        defer.returnValue(FAILURE)

    @defer.inlineCallbacks
    def run(self):
        patch_id = self.getProperty('patch_id', '')
        pr_number = self.getProperty('github.number', '')
        branch = self.getProperty('github.base.ref', DEFAULT_BRANCH)

        if not any(candidate.match(branch) for candidate in self.branches):
            rc = yield self.skip_build(f"Changes to '{branch}' are not tested")
            return defer.returnValue(rc)

        if not patch_id and not pr_number:
            yield self._addToLog('stdio', 'No patch_id or pr_number found. Unable to proceed without one of them.\n')
            self.descriptionDone = 'No change found'
            defer.returnValue(FAILURE)
            return

        if patch_id and pr_number:
            yield self._addToLog('stdio', 'Both patch_id and pr_number found. Unable to proceed with both.\n')
            self.descriptionDone = 'Error: both PR and patch number found'
            defer.returnValue(FAILURE)
            return

        if patch_id:
            status = yield self.validate_bugzilla(patch_id)
        if pr_number:
            status = yield self.validate_github(pr_number)
        if status != SUCCESS:
            defer.returnValue(status)
            return

        if self.verifyBugClosed and patch_id:
            yield self._addToLog('stdio', 'Bug is open.\n')
        if self.verifyObsolete:
            yield self._addToLog('stdio', 'Change is not obsolete.\n')
        if self.verifyReviewDenied and patch_id:
            yield self._addToLog('stdio', 'Change has not been denied.\n')
        if self.verifycqplus and patch_id:
            yield self._addToLog('stdio', 'Change is in commit queue.\n')
            yield self._addToLog('stdio', 'Change has been reviewed.\n')
        if self.verifyNoDraftForMergeQueue and pr_number:
            yield self._addToLog('stdio', 'Change is not a draft.\n')
        if self.verifyMergeQueue and pr_number:
            yield self._addToLog('stdio', 'Change is in merge queue.\n')
        if self.enableSkipEWSLabel and pr_number:
            yield self._addToLog('stdio', f'PR does not have {GitHub.SKIP_EWS_LABEL} label.\n')
        defer.returnValue(SUCCESS)

    @defer.inlineCallbacks
    def validate_bugzilla(self, patch_id):
        if self.getProperty('sensitive', False) and self.getProperty('buildername', '').lower() == 'commit-queue':
            message = 'Cannot land security changes with Commit-Queue, please use a GitHub PR against a secret remote'
            self.build.results = FAILURE
            self.descriptionDone = message
            self.setProperty('build_finish_summary', message)
            self.setProperty('comment_text', message)
            self.build.addStepsAfterCurrentStep([LeaveComment(), SetCommitQueueMinusFlagOnPatch()])
            return defer.returnValue(FAILURE)

        bug_id = self.getProperty('bug_id', '')
        if not bug_id:
            bug_id = yield self.get_bug_id_from_patch(patch_id)

        bug_closed = yield self._is_bug_closed(bug_id) if self.verifyBugClosed else 0
        if bug_closed == 1:
            rc = yield self.skip_build('Bug {} is already closed'.format(bug_id))
            return defer.returnValue(rc)

        obsolete = yield self._is_patch_obsolete(patch_id) if self.verifyObsolete else 0
        if obsolete == 1:
            rc = yield self.skip_build('Patch {} is obsolete'.format(patch_id))
            return defer.returnValue(rc)

        review_denied = yield self._is_patch_review_denied(patch_id) if self.verifyReviewDenied else 0
        if review_denied == 1:
            rc = yield self.skip_build('Patch {} is marked r-'.format(patch_id))
            return defer.returnValue(rc)

        cq_plus = yield self._is_patch_cq_plus(patch_id) if self.verifycqplus else 1
        if cq_plus != 1:
            rc = yield self.skip_build('Patch {} is not marked cq+.'.format(patch_id))
            return defer.returnValue(rc)

        acceptable_review_flag = yield self._does_patch_have_acceptable_review_flag(patch_id) if self.verifycqplus else 1
        if acceptable_review_flag != 1:
            rc = yield self.skip_build('Patch {} does not have acceptable review flag.'.format(patch_id))
            return defer.returnValue(rc)

        if obsolete == -1 or review_denied == -1 or bug_closed == -1:
            return defer.returnValue(WARNINGS)
        return defer.returnValue(SUCCESS)

    @defer.inlineCallbacks
    def validate_github(self, pr_number):
        if not pr_number:
            return defer.returnValue(FAILURE)

        repository_url = self.getProperty('repository', '')
        pr_json = yield self.get_pr_json(pr_number, repository_url, retry=3)

        if pr_json:
            # Only track actionable labels, since bug category labels may reveal information about security bugs
            self.setProperty('github_labels', [
                data.get('name')
                for data in pr_json.get('labels', [])
                if data.get('name') in GitHub.LABELS
            ])

        pr_closed = yield self._is_pr_closed(pr_json) if self.verifyBugClosed else 0
        if pr_closed == 1:
            rc = yield self.skip_build('Pull request {} is already closed'.format(pr_number))
            return defer.returnValue(rc)

        obsolete = yield self._is_hash_outdated(pr_json) if self.verifyObsolete else 0
        if obsolete == 1:
            rc = yield self.skip_build('Hash {} on PR {} is outdated'.format(self.getProperty('github.head.sha', '?')[:HASH_LENGTH_TO_DISPLAY], pr_number))
            return defer.returnValue(rc)

        blocked = self._is_pr_blocked(pr_json) if self.verifyMergeQueue else 0
        if blocked == 1:
            rc = yield self.skip_build(f"PR {pr_number} has been marked as '{GitHub.BLOCKED_LABEL}'")
            return defer.returnValue(rc)

        skip_ews = self._does_pr_has_skip_label(pr_json) if self.enableSkipEWSLabel else 0
        if skip_ews == 1:
            rc = yield self.skip_build(f'Skipping as PR {pr_number} has {GitHub.SKIP_EWS_LABEL} label')
            return defer.returnValue(rc)

        if self.verifyMergeQueue:
            if not pr_json:
                yield self.send_email_for_github_failure()
                rc = yield self.skip_build('Infrastructure issue: unable to check PR status, please contact an admin')
                return defer.returnValue(rc)
            merge_queue = self._is_pr_in_merge_queue(pr_json)
            if merge_queue == 0:
                rc = yield self.skip_build("PR {} does not have a merge queue label".format(pr_number))
                return defer.returnValue(rc)

        draft = self._is_pr_draft(pr_json) if self.verifyNoDraftForMergeQueue else 0
        if draft == 1:
            rc = yield self.fail_build("PR {} is a draft pull request".format(pr_number))
            return defer.returnValue(rc)

        if -1 in (obsolete, pr_closed, blocked, draft):
            return defer.returnValue(WARNINGS)

        return defer.returnValue(SUCCESS)

    @defer.inlineCallbacks
    def send_email_for_github_failure(self):
        try:
            pr_number = self.getProperty('github.number', '')
            sha = self.getProperty('github.head.sha', '')[:HASH_LENGTH_TO_DISPLAY]

            change_string = 'Hash {}'.format(sha)
            change_author, errors = yield GitHub.email_for_owners(self.getProperty('owners', []))
            for error in errors:
                yield self._addToLog('stdio', error)

            if not change_author:
                yield self._addToLog('stderr', 'Unable to determine email address for {} from metadata/contributors.json. Skipping sending email.'.format(self.getProperty('owners', [])))
                return defer.returnValue(None)

            builder_name = self.getProperty('buildername', '')
            title = self.getProperty('github.title', '')
            worker_name = self.getProperty('workername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)

            email_subject = f'Infrastructure failure on {builder_name} for PR #{pr_number}: {title}'
            email_text = f'EWS has encountered infrastructure failure on {builder_name}'
            repository = self.getProperty('repository')
            email_text += ' while testing <a href="{}">{}</a>'.format(GitHub.commit_url(sha, repository), change_string)
            email_text += ' for <a href="{}">PR #{}: {}</a>.'.format(GitHub.pr_url(pr_number, repository), pr_number, title)
            email_text += '\n\nFull details are available at: {}\n\nChange author: {}'.format(build_url, change_author)
            email_text += '\n\nPlease contact one of the WebKit administrators on Slack or email admin@webkit.org to fix the issue.'
            yield self._addToLog('stdio', 'Sending email notification to {}.\nPlease contact an admin to fix the issue.\n'.format(change_author))
            send_email_to_patch_author(change_author, email_subject, email_text, self.getProperty('github.head.sha', ''))
        except Exception as e:
            yield self._addToLog('stdio', f'Error in sending email for github failure: {e}')
        return defer.returnValue(None)


class ValidateCommitterAndReviewer(buildstep.BuildStep, GitHubMixin, AddToLogMixin):
    name = 'validate-committer-and-reviewer'
    descriptionDone = ['Validated committer and reviewer']
    VALIDATORS_FOR = {
        # FIXME: Remove manual validators once bot is finished
        'apple': ['webkit-bug-bridge'],
    }

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.contributors = {}

    def getResultSummary(self):
        if self.results == FAILURE:
            return {'step': self.descriptionDone}
        return buildstep.BuildStep.getResultSummary(self)

    def fail_build_due_to_invalid_status(self, email_or_username, status):
        patch_id = self.getProperty('patch_id', '')
        pr_number = self.getProperty('github.number', '')

        reason = f'{email_or_username} does not have {status} permissions'
        comment = f'{"@" if pr_number else ""}{email_or_username} does not have {status} permissions according to {Contributors.url}.'
        if patch_id:
            comment += f'\n\nRejecting attachment {patch_id} from commit queue.'
        elif pr_number:
            comment += f'\n\nIf you do have {status} permmissions, please ensure that your GitHub username is added to contributors.json.'
            comment += f'\n\nRejecting {self.getProperty("github.head.sha", f"#{pr_number}")} from merge queue.'
        return self.fail_build(reason, comment)

    def fail_build_due_to_no_validators(self, validators):
        patch_id = self.getProperty('patch_id', '')
        pr_number = self.getProperty('github.number', '')
        remote = self.getProperty('remote', DEFAULT_REMOTE)

        user_prefix = "@" if pr_number else ""
        if len(validators) == 1:
            validator_list = f'{user_prefix}{validators[0]}'
        else:
            validator_list = f'{", ".join(f"{user_prefix}{v}" for v in validators[:-1])} or {user_prefix}{validators[-1]}'
        reason = f"Landing changes on '{remote}' remote requires validation from {validator_list}"
        comment = reason
        if patch_id:
            comment += f'\n\nRejecting attachment {patch_id} from commit queue.'
        elif pr_number:
            comment += f'\n\nRejecting {self.getProperty("github.head.sha", f"#{pr_number}")} from merge queue.'

        return self.fail_build(reason, comment)

    @defer.inlineCallbacks
    def fail_build(self, reason, comment):
        yield self._addToLog('stdio', reason)
        self.setProperty('build_finish_summary', reason)
        if self.getProperty('buildername', '') == 'Safe-Merge-Queue':
            build_url = f'{self.master.config.buildbotURL}#/builders/{self.build._builderid}/builds/{self.build.number}'
            url = '[#{}]({})'.format(self.getProperty("buildnumber", ""), build_url)
            comment += f'\n\nSafe-Merge-Queue: Build {url}.'
            self.build.addStepsAfterCurrentStep([LeaveComment(), CheckStatusOfPR(pr_number=self.getProperty('github.number'))])
        else:
            self.build.addStepsAfterCurrentStep([LeaveComment(), BlockPullRequest(), SetCommitQueueMinusFlagOnPatch()])
        self.setProperty('comment_text', comment)
        self.descriptionDone = reason
        defer.returnValue(FAILURE)

    def is_reviewer(self, email):
        contributor = self.contributors.get(email.lower())
        return contributor and contributor['status'] == 'reviewer'

    def is_committer(self, email):
        contributor = self.contributors.get(email.lower())
        return contributor and contributor['status'] in ['reviewer', 'committer']

    def full_name_from_email(self, email):
        contributor = self.contributors.get(email.lower())
        if not contributor:
            return ''
        return contributor.get('name')

    @defer.inlineCallbacks
    def run(self):
        self.contributors, errors = yield Contributors.load(use_network=True)
        for error in errors:
            yield self._addToLog('stdio', error)

        if not self.contributors:
            self.descriptionDone = 'Failed to get contributors information'
            self.build.buildFinished(['Failed to get contributors information'], FAILURE)
            defer.returnValue(FAILURE)
            return

        pr_number = self.getProperty('github.number', '')
        builder_name = self.getProperty('buildername', '')

        if pr_number:
            committer = (self.getProperty('owners', []) or [''])[0]
        else:
            committer = self.getProperty('patch_committer', '').lower()

        if not self.is_committer(committer):
            if builder_name == 'Safe-Merge-Queue':
                failed_status_check = self.getProperty('failed_status_check', [])
                failed_status_check.append(pr_number)
                self.setProperty('failed_status_check', failed_status_check)
            rc = yield self.fail_build_due_to_invalid_status(committer, 'committer')
            defer.returnValue(rc)
            return
        yield self._addToLog('stdio', f'{committer} is a valid committer.\n')

        if builder_name == 'Safe-Merge-Queue':
            self.build.addStepsAfterCurrentStep([CheckStatusOfPR(pr_number=self.getProperty('github.number'))])
            return defer.returnValue(SUCCESS)

        if pr_number:
            reviewers = yield self.get_reviewers(pr_number, self.getProperty('repository', ''))
        else:
            reviewer = self.getProperty('reviewer', '').lower()
            reviewers = [reviewer] if reviewer else []

        remote = self.getProperty('remote', DEFAULT_REMOTE)
        lower_case_reviewers = [reviewer.lower() for reviewer in reviewers]
        validators = [validator.lower() for validator in self.VALIDATORS_FOR.get(remote, [])]
        if validators and not any([validator in lower_case_reviewers for validator in validators]):
            rc = yield self.fail_build_due_to_no_validators(self.VALIDATORS_FOR.get(remote, []))
            defer.returnValue(rc)
            return

        # Validators are a special case, not all validators are WebKit reviewers. If we have a reviewer that
        # is a validator but NOT a WebKit reviewer, remove them
        def filter_out_non_reviewer_validators(candidate):
            if candidate.lower() not in validators:
                return True
            return self.is_reviewer(candidate)
        reviewers = list(filter(filter_out_non_reviewer_validators, reviewers))

        valid_reviewers = set()
        invalid_reviewers = set()
        for reviewer in reviewers:
            if not self.is_reviewer(reviewer):
                invalid_reviewers.add(reviewer)
                yield self._addToLog('stdio', f'{reviewer} is not a valid reviewer, ignoring their review.\n')
            else:
                valid_reviewers.add(reviewer)
                yield self._addToLog('stdio', f'{reviewer} is a valid reviewer.\n')

        self.setProperty('valid_reviewers', [self.full_name_from_email(reviewer) for reviewer in valid_reviewers])
        self.setProperty('invalid_reviewers', [self.full_name_from_email(reviewer) for reviewer in invalid_reviewers])

        if not valid_reviewers:
            # Change has not been reviewed in bug tracker. This is acceptable, since the ChangeLog might have 'Reviewed by' in it.
            yield self._addToLog('stdio', f'Valid reviewer not found. Commit message will be checked for reviewer name in later steps\n')
            self.descriptionDone = 'Validated committer, valid reviewer not found'
            defer.returnValue(SUCCESS)
            return

        defer.returnValue(SUCCESS)


class DetermineLabelOwner(buildstep.BuildStep, GitHubMixin, AddToLogMixin):
    name = 'determine-label-owner'
    flunkOnFailure = True
    haltOnFailure = True

    @defer.inlineCallbacks
    def run(self):
        builder_name = self.getProperty('buildername', '')
        pr_number = self.getProperty('github.number', '')
        if builder_name == 'Safe-Merge-Queue':
            # Get PR and set up properties from safe-merge-queue.
            list_of_prs = self.getProperty('list_of_prs', [])
            pr_number = list_of_prs.pop()
            all_pr_data = self.getProperty('all_pr_data', [])
            pr_data = [i for i in all_pr_data if i['node']['number'] == pr_number][0]
            pr_title = pr_data['node']['title']
            commit_hash = pr_data['node']['commits']['nodes'][0]['commit']['commitUrl'][40:]
            self.setProperty('github.number', pr_number)
            self.setProperty('list_of_prs', list_of_prs)
            self.setProperty('github.title', pr_title)
            self.setProperty('github.head.sha', commit_hash)

        if not pr_number:
            yield self._addToLog('stdio', 'Unable to fetch PR number.\n')
            return defer.returnValue(FAILURE)

        project = self.getProperty('project') or GITHUB_PROJECTS[0]
        owner, name = project.split('/', 1)
        query_body = '{repository(owner:"%s", name:"%s") { pullRequest(number: %s) {timelineItems(itemTypes: LABELED_EVENT, last: 5) {nodes {... on LabeledEvent {actor { login } label { name } createdAt } } } } } }' % (owner, name, pr_number)
        query = {'query': query_body}

        response = yield self.query_graph_ql(query)
        if 'errors' in response:
            yield self._addToLog('stdio', response['errors'][0]['message'])
            return defer.returnValue(FAILURE)
        if response:
            yield self._addToLog('stdio', 'Retrieved labels.\n')
            label_events = response['data']['repository']['pullRequest']['timelineItems']['nodes']
        else:
            yield self._addToLog('stdio', 'Failed to retrieve label author.\n')
            return defer.returnValue(FAILURE)

        owner = None
        label = builder_name.lower()
        for event in reversed(label_events):
            if event['label']['name'] == label:
                owner = event['actor']['login']
                yield self._addToLog('stdio', f'Label: {label}, Owner: {owner}\n')
                if owner == 'webkit-commit-queue' or owner == 'webkit-ews-buildbot':
                    label = 'safe-merge-queue'
                    continue
                else:
                    break
        if owner:
            self.setProperty('owners', [owner])
            if builder_name == 'Safe-Merge-Queue':
                self.build.addStepsAfterCurrentStep([ValidateCommitterAndReviewer()])
                yield ConfigureBuild.add_pr_details(self)
            defer.returnValue(SUCCESS)
        else:
            yield self._addToLog('stdio', f'Did not change owner because owner not found from labels.\n')
            defer.returnValue(FAILURE)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': f"Owner of PR {self.getProperty('github.number')} determined to be {self.getProperty('owners')[0]}\n"}
        elif self.results == FAILURE:
            return {'step': f"Unable to determine owner of PR {self.getProperty('github.number')}\n"}


class SetCommitQueueMinusFlagOnPatch(buildstep.BuildStep, BugzillaMixin):
    name = 'set-cq-minus-flag-on-patch'

    @defer.inlineCallbacks
    def run(self):
        patch_id = self.getProperty('patch_id', '')
        build_finish_summary = self.getProperty('build_finish_summary', None)

        rc = SKIPPED
        if CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES:
            rc = yield self.set_cq_minus_flag_on_patch(patch_id)
        if build_finish_summary:
            self.build.buildFinished([build_finish_summary], FAILURE)
        defer.returnValue(rc)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': 'Set cq- flag on patch'}
        elif self.results == SKIPPED:
            return buildstep.BuildStep.getResultSummary(self)
        return {'step': 'Failed to set cq- flag on patch'}

    def doStepIf(self, step):
        return self.getProperty('patch_id', False)

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


class BlockPullRequest(buildstep.BuildStep, GitHubMixin, AddToLogMixin):
    name = 'block-pull-request'

    @defer.inlineCallbacks
    def run(self):
        pr_number = self.getProperty('github.number', '')
        build_finish_summary = self.getProperty('build_finish_summary', None)

        rc = SKIPPED
        repository_url = self.getProperty('repository', '')
        pr_json = yield self.get_pr_json(pr_number, repository_url)

        if CURRENT_HOSTNAME not in EWS_BUILD_HOSTNAMES:
            yield self._addToLog('stdio', 'Skipping this step on non-production instance.\n')
        else:
            is_hash_outdated = yield self._is_hash_outdated(pr_json)
            if is_hash_outdated != 0:
                pr_sha = (pr_json or {}).get('head', {}).get('sha', '')
                yield self._addToLog('stdio', f'Skipping this step as hash {pr_sha} is outdated.\n')
            else:
                repository_url = self.getProperty('repository', '')
                rc = SUCCESS
                did_remove_labels = yield self.remove_labels(pr_number, [GitHub.MERGE_QUEUE_LABEL, GitHub.UNSAFE_MERGE_QUEUE_LABEL, GitHub.REQUEST_MERGE_QUEUE_LABEL], repository_url=repository_url)
                did_add_label = yield self.add_label(pr_number, GitHub.BLOCKED_LABEL, repository_url=repository_url)
                if any((
                    not did_remove_labels,
                    not did_add_label,
                )):
                    rc = FAILURE
        if build_finish_summary:
            self.build.buildFinished([build_finish_summary], FAILURE)
        defer.returnValue(rc)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': f"Added '{GitHub.BLOCKED_LABEL}' label to pull request"}
        elif self.results == SKIPPED:
            return buildstep.BuildStep.getResultSummary(self)
        return {'step': f"Failed to add '{GitHub.BLOCKED_LABEL}' label to pull request"}

    def doStepIf(self, step):
        return self.getProperty('github.number')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


class RemoveFlagsOnPatch(buildstep.BuildStep, BugzillaMixin):
    name = 'remove-flags-from-patch'
    flunkOnFailure = False
    haltOnFailure = False

    @defer.inlineCallbacks
    def run(self):
        patch_id = self.getProperty('patch_id', '')
        if not patch_id:
            yield self._addToLog('stdio', 'patch_id build property not found.\n')
            self.descriptionDone = 'No patch id found'
            defer.returnValue(FAILURE)
            return None

        rc = yield self.remove_flags_on_patch(patch_id)
        defer.returnValue(rc)

    def getResultSummary(self):
        if self.results == SKIPPED:
            return buildstep.BuildStep.getResultSummary(self)
        if self.results == SUCCESS:
            return {'step': 'Removed flags on bugzilla patch'}
        return {'step': 'Failed to remove flags on bugzilla patch'}

    def doStepIf(self, step):
        return self.getProperty('patch_id')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


class RemoveLabelsFromPullRequest(buildstep.BuildStep, GitHubMixin, AddToLogMixin):
    name = 'remove-labels-from-pull-request'
    flunkOnFailure = False
    haltOnFailure = False
    LABELS_TO_REMOVE = [
        GitHub.SAFE_MERGE_QUEUE_LABEL,
        GitHub.MERGE_QUEUE_LABEL,
        GitHub.UNSAFE_MERGE_QUEUE_LABEL,
        GitHub.BLOCKED_LABEL,
        GitHub.REQUEST_MERGE_QUEUE_LABEL,
    ]

    @defer.inlineCallbacks
    def run(self):
        pr_number = self.getProperty('github.number', '')

        repository_url = self.getProperty('repository', '')
        did_remove_labels = yield self.remove_labels(pr_number, self.LABELS_TO_REMOVE, repository_url=repository_url)
        defer.returnValue(SUCCESS if did_remove_labels else FAILURE)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': f"Removed labels from pull request"}
        elif self.results == FAILURE:
            return {'step': f"Failed to remove labels from pull request"}
        return buildstep.BuildStep.getResultSummary(self)

    def doStepIf(self, step):
        return self.getProperty('github.number') and CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


class RemoveAndAddLabels(buildstep.BuildStep, GitHubMixin, AddToLogMixin):
    name = 'remove-and-add-labels'
    flunkOnFailure = False
    haltOnFailure = False

    def __init__(self, label_to_add='', labels_to_remove=None, **kwargs):
        self.label_to_add = label_to_add
        self.labels_to_remove = [GitHub.SAFE_MERGE_QUEUE_LABEL] if labels_to_remove is None else labels_to_remove
        super().__init__(**kwargs)

    @defer.inlineCallbacks
    def run(self):
        if self.label_to_add == GitHub.MERGE_QUEUE_LABEL:
            pr_status = 'passed_status_check'
        elif self.label_to_add == GitHub.BLOCKED_LABEL:
            pr_status = 'failed_status_check'
        else:
            yield self._addToLog('stdio', f'{self.label_to_add} not supported.\n')
            return defer.returnValue(FAILURE)
        prs_to_label = self.getProperty(pr_status)
        if not prs_to_label:
            yield self._addToLog('stdio', f'There are no PRs to label with {self.label_to_add}.\n')
            return defer.returnValue(FAILURE)
        pr_number = prs_to_label.pop()
        self.setProperty('github.number', pr_number)
        yield self._addToLog('stdio', f'Updating labels for PR {pr_number}...\n')
        self.setProperty(pr_status, prs_to_label)
        rc = yield self.update_labels(pr_number)

        # When a PR is removed from safe-merge-queue, add a comment with the build link.
        build_url = f'{self.master.config.buildbotURL}#/builders/{self.build._builderid}/builds/{self.build.number}'
        url = '[#{}]({})'.format(self.getProperty("buildnumber", ""), build_url)
        comment = f'\n\nSafe-Merge-Queue: Build {url}.'
        self.setProperty('comment_text', comment)
        steps_to_add = [LeaveComment()]
        if len(prs_to_label):
            steps_to_add += [RemoveAndAddLabels(label_to_add=self.label_to_add)]
        self.build.addStepsAfterCurrentStep(steps_to_add)

        defer.returnValue(rc)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': f"Labelled PR {self.getProperty('github.number')} with {self.label_to_add}"}
        elif self.results == FAILURE:
            return {'step': f"Failed to label PR {self.getProperty('github.number', '')} with {self.label_to_add}"}
        return buildstep.BuildStep.getResultSummary(self)

    @defer.inlineCallbacks
    def update_labels(self, pr_number):
        repository_url = self.getProperty('repository', '')

        did_remove_labels = yield self.remove_labels(pr_number, self.labels_to_remove, repository_url=repository_url)
        if did_remove_labels:
            yield self._addToLog('stdio', f'Successfully removed {self.labels_to_remove} label(s) from PR {pr_number}.\n')
        else:
            yield self._addToLog('stdio', f'Failed to remove {self.labels_to_remove} label(s) from PR {pr_number}.\n')

        did_add_label = yield self.add_label(pr_number, self.label_to_add, repository_url=repository_url)
        if did_add_label:
            yield self._addToLog('stdio', f'Successfully added {self.label_to_add} label to PR {pr_number}.\n')
        else:
            yield self._addToLog('stdio', f'Failed to add {self.label_to_add} label to PR {pr_number}.\n')
        defer.returnValue(SUCCESS if did_remove_labels and did_add_label else FAILURE)


class RetrievePRDataFromLabel(buildstep.BuildStep, GitHubMixin, AddToLogMixin):
    name = 'retrieve-pr-data-from-label'

    def __init__(self, project="WebKit/WebKit", label='', **kwargs):
        self.project = project
        owner, name = project.lower().split('/', 1)
        self.name = f'{self.name}-{owner}-{name}'
        self.label = label
        super().__init__(**kwargs)

    @defer.inlineCallbacks
    def run(self):
        yield self._addToLog('stdio', f'Starting process for {self.project}...\n')
        self.setProperty('project', self.project)
        project = self.getProperty('project')
        self.setProperty('repository', f'{GITHUB_URL}{project}')

        num_prs = yield self.get_number_of_prs_with_label(self.label, retry=3)
        if num_prs == 0:
            yield self._addToLog('stdio', f'Ending process as there are no PRs in {self.label}.\n')
            return defer.returnValue(SUCCESS)
        if not num_prs:
            yield self._addToLog('stdio', f'Failed to retrieve number of PRs in {self.label}.\n')
            return defer.returnValue(FAILURE)

        self.setProperty('passed_status_check', [])
        self.setProperty('failed_status_check', [])
        self.setProperty('pending_prs', [])

        retrieved_pr_data = yield self.getAllPRData(num_prs, self.label)
        if retrieved_pr_data:
            self.build.addStepsAfterCurrentStep([DetermineLabelOwner()])

        defer.returnValue(SUCCESS if retrieved_pr_data else FAILURE)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': f"Successfully retrieved pull request data"}
        elif self.results == FAILURE:
            return {'step': f"Failed to retrieve pull request data"}
        return buildstep.BuildStep.getResultSummary(self)

    @defer.inlineCallbacks
    def getAllPRData(self, limit, label):
        project = self.getProperty('project') or GITHUB_PROJECTS[0]
        owner, name = project.split('/', 1)
        query_body = '{repository(owner:"%s", name:"%s") { pullRequests(labels: "%s", last: %s) { edges { node { title number commits(last: 3) { nodes { commit { commitUrl status { state contexts { context state } } } } } } } } } }' % (owner, name, label, limit)
        query = {'query': query_body}

        yield self._addToLog('stdio', "Fetching all PRs with label {}...\n".format(label))

        response = yield self.query_graph_ql(query)
        if not response:
            yield self._addToLog('stderr', 'Failed to retrieve list of PRs.\n')
            return defer.returnValue(None)

        all_pr_data = response['data']['repository']['pullRequests']['edges']
        list_of_prs = [pr_data['node']['number'] for pr_data in all_pr_data]

        self.setProperty('list_of_prs', list_of_prs)
        self.setProperty('all_pr_data', all_pr_data)
        yield self._addToLog('stdio', 'All PRs in safe-merge-queue: {}\n'.format(list_of_prs))
        yield self._addToLog('stdio', 'Done!\n')

        defer.returnValue(True)


class CheckStatusOfPR(buildstep.BuildStep, GitHubMixin, AddToLogMixin):
    name = 'check-status-of-pr'
    flunkOnFailure = False
    haltOnFailure = False
    EMBEDDED_CHECKS = ['ios', 'ios-sim', 'ios-wk2', 'ios-wk2-wpt', 'api-ios', 'vision', 'vision-sim', 'vision-wk2', 'tv', 'tv-sim', 'watch', 'watch-sim']
    MACOS_CHECKS = ['mac', 'mac-AS-debug', 'api-mac', 'mac-wk1', 'mac-wk2', 'mac-AS-debug-wk2', 'mac-wk2-stress', 'jsc', 'jsc-arm64']
    LINUX_CHECKS = ['gtk', 'gtk-wk2', 'api-gtk', 'wpe', 'wpe-cairo', 'wpe-wk2', 'api-wpe']
    WINDOWS_CHECKS = ['win']
    EWS_WEBKIT_FAILED = 0
    EWS_WEBKIT_PASSED = 1
    EWS_WEBKIT_PENDING = 2

    def __init__(self, pr_number='', **kwargs):
        self.name = f'{self.name}-{pr_number}'
        self.steps_to_add = []
        self.passed_status_check = None
        super().__init__(**kwargs)

    @defer.inlineCallbacks
    def run(self):
        pr_number = self.getProperty('github.number', '')
        repository = self.getProperty('repository', '')

        yield self._addToLog('stdio', f'Performing status check on PR {pr_number}.\n')
        yield self._addToLog('stdio', f'Link to PR: {repository}/pull/{pr_number}\n')

        if pr_number not in self.getProperty('failed_status_check', []):
            self.passed_status_check = yield self.checkPRStatus(pr_number)

        if len(self.getProperty('list_of_prs')):
            self.steps_to_add += [DetermineLabelOwner()]
        else:
            self.steps_to_add += [AddMergeLabelsToPRs()]
        self.build.addStepsAfterCurrentStep(self.steps_to_add)

        if self.passed_status_check == self.EWS_WEBKIT_PENDING:
            defer.returnValue(WARNINGS)
        defer.returnValue(SUCCESS if self.passed_status_check else FAILURE)

    def getResultSummary(self):
        pr_number = self.getProperty('github.number', '')
        if self.results == SUCCESS and self.passed_status_check == self.EWS_WEBKIT_PASSED:
            return {'step': f"PR {pr_number} marked safe for merge-queue"}
        elif self.results == WARNINGS and self.passed_status_check == self.EWS_WEBKIT_PENDING:
            return {'step': f"PR {pr_number} not ready for merge-queue"}
        elif self.results == FAILURE:
            return {'step': f"PR {pr_number} unsafe for merge-queue"}
        return buildstep.BuildStep.getResultSummary(self)

    @defer.inlineCallbacks
    def checkPRStatus(self, pr_number):
        yield self._addToLog('stdio', f'Checking the status of PR {pr_number}...\n')
        project = self.getProperty('project') or GITHUB_PROJECTS[0]
        owner, name = project.split('/', 1)
        query_body = '{repository(owner: "%s", name: "%s") {pullRequest(number: %s) {commits (last:1) {edges {node {commit {oid } } } } } } }' % (owner, name, pr_number)
        query = {'query': query_body}

        response = yield self.query_graph_ql(query)
        if not response:
            yield self._addToLog('stderr', 'Failed to retrieve commit hash.\n')
            defer.returnValue(None)

        try:
            head_sha = response['data']['repository']['pullRequest']['commits']['edges'][0]['node']['commit']['oid']
        except TypeError:
            yield self._addToLog('stderr', 'Failed to retrieve commit hash.\n')
            defer.returnValue(None)

        rc = yield self.getQueueStatusFromList(head_sha, pr_number)
        defer.returnValue(rc)

    @defer.inlineCallbacks
    def getQueueStatusFromList(self, change_id, pr_number):
        missing_checks = []
        failed_checks = []
        url = '{}status/{}/'.format(EWS_URL, change_id)

        for attempt in range(1, 4):
            response = yield TwistedAdditions.request(url, logger=lambda content: self._addToLog('stdio', content))
            if not response:
                yield self._addToLog('stdio', f'Could not retrieve data from {url}.\n')
                if attempt == 3:
                    return defer.returnValue(None)
                yield self._addToLog('stdio', f'Retrying, attempt {attempt + 1} of 3\n')
            else:
                if response.status_code // 100 != 2:
                    yield self._addToLog('stdio', f'Accessed {url} with unexpected status code {response.status_code}.\n')
                    if attempt == 3:
                        return defer.returnValue(False if response.status_code // 100 == 4 else None)
                    yield self._addToLog('stdio', f'Retrying, attempt {attempt + 1} of 3\n')
                else:
                    break

        # FIXME: safe-merge-queue should obtain skipped status from EWS instead of hardcoding
        queues_for_safe_merge = self.EMBEDDED_CHECKS + self.MACOS_CHECKS
        if self.getProperty('project') == GITHUB_PROJECTS[0]:
            queues_for_safe_merge += self.LINUX_CHECKS
            queues_for_safe_merge += self.WINDOWS_CHECKS

        for queue in queues_for_safe_merge:
            queue_data = response.json().get(queue, None)
            # jsc-arm7-tests will not set its status if skipped, so we condition on jsc-armv7
            if queue == 'jsc-armv7-tests' and response.json().get('jsc-armv7', {}).get('state', None) == 3:
                yield self._addToLog('stdio', f'{queue}: Skipped\n')
            elif queue_data:
                status = queue_data.get('state', None)
                if status == 0:  # success
                    yield self._addToLog('stdio', f'{queue}: Success\n')
                    continue
                elif status == 2:  # failure
                    failed_checks.append(queue)
                    yield self._addToLog('stdio', f'{queue}: Failure\n')
                elif status == 3:  # skipped
                    yield self._addToLog('stdio', f'{queue}: Skipped\n')
                else:  # null
                    missing_checks.append(queue)
                    yield self._addToLog('stdio', f'{queue}: Pending\n')
            else:
                missing_checks.append(queue)
                yield self._addToLog('stdio', f'{queue}: Pending\n')

        passed_status_check = self.getProperty('passed_status_check')
        failed_status_check = self.getProperty('failed_status_check')
        pending_prs = self.getProperty('pending_prs')

        if len(missing_checks):
            pending_prs.append(pr_number)
            self.setProperty('pending_prs', pending_prs)
            yield self._addToLog('stdio', 'Required checks are not completed. Waiting until all checks are completed before relabelling PR.\n')
            yield self._addToLog('stdio', f'Missing the following checks: {missing_checks}\n')
            defer.returnValue(self.EWS_WEBKIT_PENDING)
        elif len(failed_checks):
            failed_status_check.append(pr_number)
            self.setProperty('failed_status_check', failed_status_check)
            yield self._addToLog('stdio', 'Failed status check.\n')
            yield self._addToLog('stdio', f'Merged blocked due to failure: {failed_checks}\n')
            format_checks = ', '.join(failed_checks)
            comment = f'Failed {format_checks} checks. Please resolve failures and re-apply `safe-merge-queue` label.'
            comment += f'\n\nRejecting #{pr_number} from merge queue.'
            self.setProperty('comment_text', comment)
            self.steps_to_add += [LeaveComment()]
            defer.returnValue(self.EWS_WEBKIT_FAILED)
        else:
            passed_status_check.append(pr_number)
            self.setProperty('passed_status_check', passed_status_check)
            yield self._addToLog('stdio', 'Passed status check.\n')
            defer.returnValue(self.EWS_WEBKIT_PASSED)


class AddMergeLabelsToPRs(buildstep.BuildStep, GitHubMixin, AddToLogMixin):
    name = 'add-merge-labels-to-prs'
    flunkOnFailure = False
    haltOnFailure = False

    @defer.inlineCallbacks
    def run(self):
        steps_to_add = []
        label_as_safe = self.getProperty('passed_status_check', '')
        yield self._addToLog('stdio', f'PRs to merge: {label_as_safe}.\n')
        label_as_blocked = self.getProperty('failed_status_check', '')
        yield self._addToLog('stdio', f'PRs to block: {label_as_blocked}.\n')
        pending_prs = self.getProperty('pending_prs', '')
        yield self._addToLog('stdio', f'PRs with checks pending: {pending_prs}. No action taken.\n')

        if len(label_as_safe):
            steps_to_add.append(RemoveAndAddLabels(label_to_add=GitHub.MERGE_QUEUE_LABEL, labels_to_remove=[GitHub.SAFE_MERGE_QUEUE_LABEL]))
        if len(label_as_blocked):
            steps_to_add.append(RemoveAndAddLabels(label_to_add=GitHub.BLOCKED_LABEL, labels_to_remove=[GitHub.SAFE_MERGE_QUEUE_LABEL]))
        self.build.addStepsAfterCurrentStep(steps_to_add)
        defer.returnValue(SUCCESS)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': f"Started PR labelling process successfully"}
        elif self.results == FAILURE:
            return {'step': f"Failed to start PR labelling process"}
        return buildstep.BuildStep.getResultSummary(self)


class CloseBug(buildstep.BuildStep, BugzillaMixin):
    name = 'close-bugzilla-bug'
    flunkOnFailure = False
    haltOnFailure = False
    bug_id = ''

    def run(self):
        self.bug_id = self.getProperty('bug_id', '')
        return self.close_bug(self.bug_id)

    def getResultSummary(self):
        if self.results == SKIPPED:
            return buildstep.BuildStep.getResultSummary(self)
        if self.results == SUCCESS:
            return {'step': 'Closed bug {}'.format(self.bug_id)}
        return {'step': 'Failed to close bug {}'.format(self.bug_id)}

    def doStepIf(self, step):
        return self.getProperty('bug_id') and not self.getProperty('is_test_gardening')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


class LeaveComment(buildstep.BuildStep, BugzillaMixin, GitHubMixin):
    name = 'leave-comment'
    flunkOnFailure = False
    haltOnFailure = False

    @defer.inlineCallbacks
    def run(self):
        self.bug_id = self.getProperty('bug_id', '')
        self.pr_number = self.getProperty('github.number', '')
        self.comment_text = self.getProperty('comment_text', '')

        if not self.comment_text:
            yield self._addToLog('stdio', 'comment_text build property not found.\n')
            self.descriptionDone = 'No comment found'
            defer.returnValue(WARNINGS)
            return

        rc = SUCCESS
        if self.pr_number:
            rc = yield self.comment_on_pr(self.pr_number, self.comment_text, self.getProperty('repository'))
        if self.bug_id:
            rc = yield self.comment_on_bug(self.bug_id, self.comment_text)
        if not self.pr_number and not self.bug_id:
            yield self._addToLog('stdio', 'No bug or pull request to comment to.\n')
            self.descriptionDone = 'No bug or PR found'
            rc = FAILURE

        defer.returnValue(rc)

    def getResultSummary(self):
        if self.results == SUCCESS:
            if self.pr_number and self.bug_id:
                return {'step': f'Added comment on PR {self.pr_number} and added comment on bug {self.bug_id}'}
            if self.pr_number:
                return {'step': f'Added comment on PR {self.pr_number}'}
            if self.bug_id:
                return {'step': f'Added comment on bug {self.bug_id}'}

        if self.results == FAILURE:
            if self.pr_number and self.bug_id:
                return {'step': f'Failed to add comment on PR {self.pr_number} and failed to add comment on bug {self.bug_id}'}
            if self.pr_number:
                return {'step': f'Failed to add comment on PR {self.pr_number}'}
            if self.bug_id:
                return {'step': f'Failed to add comment on bug {self.bug_id}'}
            return {'step': 'Failed to add comment'}

        return buildstep.BuildStep.getResultSummary(self)

    def doStepIf(self, step):
        return CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES


class RevertAppliedChanges(steps.ShellSequence):
    name = 'revert-applied-changes'
    description = ['revert-applied-changes running']
    descriptionDone = ['Reverted applied changes']
    flunkOnFailure = True
    haltOnFailure = True

    def __init__(self, exclude=None, **kwargs):
        super().__init__(timeout=5 * 60, logEnviron=False, **kwargs)
        self.exclude = exclude or []  # Pattern(s) to ignore for git clean

    def run(self):
        self.commands = []
        exclude_patterns = []
        for pattern in self.exclude:
            exclude_patterns.extend(('-e', pattern))
        for command in [
            ['git', 'clean', '-f', '-d'] + exclude_patterns,
            ['git', 'checkout', self.getProperty('ews_revision') or self.getProperty('got_revision')],
        ]:
            self.commands.append(util.ShellArg(command=command, logname='stdio'))

        platform = self.getProperty('platform')
        if platform in ('gtk', 'wpe'):
            # Force cmake reconfigure to ensure the recovery after patches breaking cmake configure step
            platform = platform.upper()
            config = self.getProperty('configuration').capitalize()
            target = os.path.join("WebKitBuild", platform, config, "build-webkit-options.txt")
            self.commands.append(util.ShellArg(command=['rm', '-f', target], logname='stdio'))
        return super().run()


class Trigger(trigger.Trigger):
    # By default, set updateSourceStamp=False so that the triggered build uses the sourcestamp of the triggering build.
    def __init__(self, schedulerNames, include_revision=True, triggers=None, patch=True, pull_request=False, updateSourceStamp=False, **kwargs):
        self.include_revision = include_revision
        self.triggers = triggers
        set_properties = self.propertiesToPassToTriggers(patch=patch, pull_request=pull_request) or {}
        super().__init__(schedulerNames=schedulerNames, set_properties=set_properties, updateSourceStamp=updateSourceStamp, **kwargs)

    def propertiesToPassToTriggers(self, patch=True, pull_request=False):
        property_names = [
            'configuration',
            'platform',
            'fullPlatform',
            'architecture',
            'codebase',
        ]
        if patch:
            property_names += ['patch_id', 'bug_id', 'owner']
        if pull_request:
            property_names += [
                'github.base.ref', 'github.head.ref', 'github.head.sha',
                'github.head.repo.full_name', 'github.number', 'github.title',
                'repository', 'project', 'owners', 'classification',
            ]
        if self.triggers:
            property_names.append('triggers')

        properties_to_pass = {prop: properties.Property(prop) for prop in property_names}
        properties_to_pass['retry_count'] = properties.Property('retry_count', default=0)
        properties_to_pass['os_version_builder'] = properties.Property('os_version', default='')
        properties_to_pass['xcode_version_builder'] = properties.Property('xcode_version', default='')
        if self.include_revision:
            properties_to_pass['ews_revision'] = properties.Property('got_revision')
        return properties_to_pass


class TestWithFailureCount(shell.TestNewStyle):
    failedTestsFormatString = '%d test%s failed'
    failedTestCount = 0

    @defer.inlineCallbacks
    def run(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

        rc = yield super().run()

        self.failedTestCount = self.countFailures(rc)
        self.failedTestPluralSuffix = '' if self.failedTestCount == 1 else 's'

        if self.failedTestCount:
            defer.returnValue(FAILURE)
        else:
            defer.returnValue(rc)

    def countFailures(self, returncode):
        raise NotImplementedError

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
        super().__init__(logEnviron=False, **kwargs)

    def countFailures(self, returncode):
        log_text = self.log_observer.getStdout() + self.log_observer.getStderr()

        match = re.search(r'Total errors found: (?P<errors>\d+) in (?P<files>\d+) files', log_text)
        if not match:
            return 0
        return int(match.group('errors'))


class RunBindingsTests(shell.ShellCommand, AddToLogMixin):
    name = 'bindings-tests'
    description = ['bindings-tests running']
    descriptionDone = ['bindings-tests']
    flunkOnFailure = True
    jsonFileName = 'bindings_test_results.json'
    logfiles = {'json': jsonFileName}
    command = ['python3', 'Tools/Scripts/run-bindings-tests', '--json-output={0}'.format(jsonFileName)]

    def __init__(self, **kwargs):
        super().__init__(timeout=5 * 60, logEnviron=False, **kwargs)

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
            return super().getResultSummary()

        failures = webkitpy_results.get('failures')
        if not failures:
            return super().getResultSummary()
        pluralSuffix = 's' if len(failures) > 1 else ''
        failures_string = ', '.join([failure.replace('(JS) ', '') for failure in failures])
        message = 'Found {} Binding test failure{}: {}'.format(len(failures), pluralSuffix, failures_string)
        self.build.buildFinished([message], FAILURE)
        return {'step': message}


class RunWebKitPerlTests(shell.ShellCommandNewStyle):
    name = 'webkitperl-tests'
    description = ['webkitperl-tests running']
    descriptionDone = ['webkitperl-tests']
    flunkOnFailure = False
    haltOnFailure = False
    command = ['perl', 'Tools/Scripts/test-webkitperl']

    def __init__(self, **kwargs):
        super().__init__(timeout=2 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results == SUCCESS:
            message = 'Passed webkitperl tests'
            self.build.buildFinished([message], SUCCESS)
            return {'step': message}
        return {'step': 'Failed webkitperl tests'}

    def evaluateCommand(self, cmd):
        rc = shell.ShellCommandNewStyle.evaluateCommand(self, cmd)
        if rc == FAILURE:
            self.build.addStepsAfterCurrentStep([KillOldProcesses(), ReRunWebKitPerlTests()])
        return rc


class ReRunWebKitPerlTests(RunWebKitPerlTests):
    name = 're-run-webkitperl-tests'
    flunkOnFailure = True
    haltOnFailure = True

    def evaluateCommand(self, cmd):
        return shell.ShellCommandNewStyle.evaluateCommand(self, cmd)


class RunBuildWebKitOrgUnitTests(shell.ShellCommandNewStyle):
    name = 'build-webkit-org-unit-tests'
    description = ['build-webkit-unit-tests running']
    command = ['python3', 'runUnittests.py', 'build-webkit-org', '--autoinstall']

    def __init__(self, **kwargs):
        super().__init__(workdir='build/Tools/CISupport', timeout=2 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': 'Passed build.webkit.org unit tests'}
        return {'step': 'Failed build.webkit.org unit tests'}


class RunEWSUnitTests(shell.ShellCommandNewStyle):
    name = 'ews-unit-tests'
    description = ['ews-unit-tests running']
    command = ['python3', 'runUnittests.py', 'ews-build', '--autoinstall']

    def __init__(self, **kwargs):
        super().__init__(workdir='build/Tools/CISupport', timeout=2 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': 'Passed EWS unit tests'}
        return {'step': 'Failed EWS unit tests'}


class RunBuildbotCheckConfig(shell.ShellCommandNewStyle):
    name = 'buildbot-check-config'
    description = ['buildbot-checkconfig running']
    command = ['python3', '../buildbot-cmd', 'checkconfig']
    directory = 'build/Tools/CISupport/ews-build'
    timeout = 2 * 60

    def __init__(self, **kwargs):
        super().__init__(workdir=self.directory, timeout=self.timeout, logEnviron=False, **kwargs)

    def run(self):
        self.env['LC_CTYPE'] = 'en_US.UTF-8'
        return super().run()

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


class RunResultsdbpyTests(shell.ShellCommandNewStyle):
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
        super().__init__(timeout=15 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': 'Passed resultsdbpy unit tests'}
        return {'step': 'Failed resultsdbpy unit tests'}


class RunWebKitPyTests(shell.ShellCommandNewStyle, AddToLogMixin):
    name = 'webkitpy-tests'
    descriptionDone = ['webkitpy-tests']
    description = ['webkitpy-tests']
    jsonFileName = 'webkitpy_test_results.json'
    logfiles = {'json': jsonFileName}
    command = ['python3', 'Tools/Scripts/test-webkitpy', '--verbose', '--json-output={0}'.format(jsonFileName)]
    flunkOnFailure = True
    NUM_FAILURES_TO_DISPLAY = 10

    def __init__(self, **kwargs):
        super().__init__(timeout=2 * 60, logEnviron=False, **kwargs)

    def run(self):
        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('json', self.log_observer)
        return super().run()

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
            message = 'Passed webkitpy tests'
            self.setBuildSummary(message)
            return {'step': message}

        logLines = self.log_observer.getStdout()
        json_text = ''.join([line for line in logLines.splitlines()])
        try:
            webkitpy_results = json.loads(json_text)
        except Exception as ex:
            self._addToLog('stderr', 'ERROR: unable to parse data, exception: {}'.format(ex))
            return super().getResultSummary()

        failures = webkitpy_results.get('failures', []) + webkitpy_results.get('errors', [])
        if not failures:
            return super().getResultSummary()
        pluralSuffix = 's' if len(failures) > 1 else ''
        failures_string = ', '.join([failure.get('name') for failure in failures[:self.NUM_FAILURES_TO_DISPLAY]])
        message = 'Found {} webkitpy test failure{}: {}'.format(len(failures), pluralSuffix, failures_string)
        if len(failures) > self.NUM_FAILURES_TO_DISPLAY:
            message += ' ...'
        self.setBuildSummary(message)
        return {'step': message}


class InstallGtkDependencies(shell.ShellCommandNewStyle):
    name = 'jhbuild'
    description = ['updating gtk dependencies']
    descriptionDone = ['Updated gtk dependencies']
    command = ['perl', 'Tools/Scripts/update-webkitgtk-libs', WithProperties('--%(configuration)s')]
    haltOnFailure = True

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, **kwargs)


class InstallWpeDependencies(shell.ShellCommandNewStyle):
    name = 'jhbuild'
    description = ['updating wpe dependencies']
    descriptionDone = ['Updated wpe dependencies']
    command = ['perl', 'Tools/Scripts/update-webkitwpe-libs', WithProperties('--%(configuration)s')]
    haltOnFailure = True

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, **kwargs)


class InstallWinDependencies(shell.ShellCommandNewStyle):
    name = 'win-deps'
    description = ['Updating Win dependencies']
    descriptionDone = ['Updated Win dependencies']
    command = ['python3', 'Tools/Scripts/update-webkit-win-libs.py']
    haltOnFailure = True

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, **kwargs)


def customBuildFlag(platform, fullPlatform):
    # FIXME: Make a common 'supported platforms' list.
    if platform not in ('gtk', 'ios', 'visionos', 'jsc-only', 'wpe', 'playstation', 'tvos', 'watchos'):
        return []
    if 'simulator' in fullPlatform:
        platform = platform + '-simulator'
    elif platform in ['ios', 'visionos', 'tvos', 'watchos']:
        platform = platform + '-device'
    return ['--' + platform]


class BuildLogLineObserver(ParseByLineLogObserver):
    def __init__(self, errorReceived, searchString='rror:', includeRelatedLines=True, thresholdExceedCallBack=None):
        self.errorReceived = errorReceived
        self.searchString = searchString
        self.includeRelatedLines = includeRelatedLines
        self.error_context_buffer = []
        self.whitespace_re = re.compile(r'^[\s]*$')
        self.line_count = 0
        self.thresholdExceedCallBack = thresholdExceedCallBack
        super().__init__(self.parseOutputLine)

    def parseOutputLine(self, line):
        self.line_count += 1
        if self.line_count == THRESHOLD_FOR_EXCESSIVE_LOGS_DEFAULT:
            self.thresholdExceedCallBack()
            return

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


class CompileWebKit(shell.Compile, AddToLogMixin, ShellMixin):
    name = 'compile-webkit'
    description = ['compiling']
    descriptionDone = ['Compiled WebKit']
    env = {'MFLAGS': ''}
    warningPattern = '.*arning: .*'
    haltOnFailure = False
    build_command = ['perl', 'Tools/Scripts/build-webkit']
    filter_command = ['perl', 'Tools/Scripts/filter-build-webkit', '-logfile', 'build-log.txt']
    VALID_ADDITIONAL_ARGUMENTS_LIST = []  # If additionalArguments is added to config.json for CompileWebKit step, it should be added here as well.
    APPLE_PLATFORMS = ('mac', 'ios', 'visionos', 'tvos', 'watchos')

    def __init__(self, skipUpload=False, **kwargs):
        self.skipUpload = skipUpload
        self.cancelled_due_to_huge_logs = False
        super().__init__(timeout=60 * 60, logEnviron=False, **kwargs)

    def doStepIf(self, step):
        return not (self.getProperty('fast_commit_queue') and self.getProperty('buildername', '').lower() == 'commit-queue')

    def start(self):
        platform = self.getProperty('platform')
        buildOnly = self.getProperty('buildOnly')
        architecture = self.getProperty('architecture')
        configuration = self.getProperty('configuration')

        if platform == 'win':
            self.addLogObserver('stdio', BuildLogLineObserver(self.errorReceived, searchString='error ', includeRelatedLines=False, thresholdExceedCallBack=self.handleExcessiveLogging))
        else:
            self.addLogObserver('stdio', BuildLogLineObserver(self.errorReceived, thresholdExceedCallBack=self.handleExcessiveLogging))

        build_command = self.build_command + [f'--{configuration}']

        additionalArguments = self.getProperty('additionalArguments')
        for additionalArgument in (additionalArguments or []):
            if additionalArgument in self.VALID_ADDITIONAL_ARGUMENTS_LIST:
                build_command += [additionalArgument]
        if platform in self.APPLE_PLATFORMS:
            # FIXME: Once WK_VALIDATE_DEPENDENCIES is set via xcconfigs, it can
            # be removed here. We can't have build-webkit pass this by default
            # without invalidating local builds made by Xcode, and we set it
            # via xcconfigs until all building of Xcode-based webkit is done in
            # workspaces (rdar://88135402).
            if architecture:
                build_command += ['--architecture', f'"{architecture}"']
            if CompileJSC.name not in self.name:
                build_command += ['-hideShellScriptEnvironment']
            build_command += ['WK_VALIDATE_DEPENDENCIES=YES']
            if buildOnly:
                # For build-only bots, the expectation is that tests will be run on separate machines,
                # so we need to package debug info as dSYMs. Only generating line tables makes
                # this much faster than full debug info, and crash logs still have line numbers.
                # Some projects (namely lldbWebKitTester) require full debug info, and may override this.
                build_command += ['DEBUG_INFORMATION_FORMAT=dwarf-with-dsym', 'CLANG_DEBUG_INFORMATION_LEVEL=$(WK_OVERRIDE_DEBUG_INFORMATION_LEVEL:default=line-tables-only)']

        build_command += customBuildFlag(platform, self.getProperty('fullPlatform'))

        # filter-build-webkit is specifically designed for Xcode and doesn't work generally
        if platform in self.APPLE_PLATFORMS:
            self.setCommand(self.shell_command(f"{' '.join(build_command)} 2>&1 | {' '.join(self.filter_command)}"))
        else:
            self.setCommand(build_command)

        return shell.Compile.start(self)

    def errorReceived(self, error):
        self._addToLog('errors', error + '\n')

    def handleExcessiveLogging(self):
        build_url = f'{self.master.config.buildbotURL}#/builders/{self.build._builderid}/builds/{self.build.number}'
        print(f'\n{MSG_FOR_EXCESSIVE_LOGS}, {build_url}\n\n')
        self.cancelled_due_to_huge_logs = True
        self.build.stopBuild(reason=MSG_FOR_EXCESSIVE_LOGS, results=FAILURE)
        self.build.buildFinished([MSG_FOR_EXCESSIVE_LOGS], FAILURE)

    def follow_up_steps(self):
        if self.getProperty('platform') in self.APPLE_PLATFORMS and CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES + TESTING_ENVIRONMENT_HOSTNAMES:
            return [
                GenerateS3URL(
                    f"{self.getProperty('fullPlatform')}-{self.getProperty('architecture')}-{self.getProperty('configuration')}-{self.name}",
                    extension='txt',
                    content_type='text/plain',
                ), UploadFileToS3(
                    'build-log.txt',
                    links={self.name: 'Full build log'},
                    content_type='text/plain',
                )
            ]
        return []

    def evaluateCommand(self, cmd):
        steps_to_add = self.follow_up_steps()

        if cmd.didFail():
            steps_to_add += [RevertAppliedChanges(), CleanWorkingDirectory(), ValidateChange(verifyBugClosed=False, addURLs=False)]
            platform = self.getProperty('platform')
            if platform == 'wpe':
                steps_to_add.append(InstallWpeDependencies())
            elif platform == 'gtk':
                steps_to_add.append(InstallGtkDependencies())
            if self.getProperty('group') == 'jsc':
                steps_to_add.append(CompileJSCWithoutChange())
            else:
                steps_to_add.append(CompileWebKitWithoutChange())
            steps_to_add.append(AnalyzeCompileWebKitResults())
        else:
            triggers = self.getProperty('triggers', None)
            if triggers or not self.skipUpload:
                steps_to_add += [ArchiveBuiltProduct()]
                if CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES + TESTING_ENVIRONMENT_HOSTNAMES:
                    steps_to_add.extend([
                        GenerateS3URL(f"{self.getProperty('fullPlatform')}-{self.getProperty('architecture')}-{self.getProperty('configuration')}"),
                        UploadFileToS3(f"WebKitBuild/{self.getProperty('configuration')}.zip", links={self.name: 'Archive'}),
                    ])
                else:
                    # S3 might not be configured on local instances, achieve similar functionality without S3.
                    steps_to_add.extend([UploadBuiltProduct()])
                if triggers:
                    steps_to_add.append(Trigger(
                        schedulerNames=triggers,
                        patch=bool(self.getProperty('patch_id')),
                        pull_request=bool(self.getProperty('github.number')),
                    ))

        # Using a single addStepsAfterCurrentStep because of https://github.com/buildbot/buildbot/issues/4874
        self.build.addStepsAfterCurrentStep(steps_to_add)

        return super().evaluateCommand(cmd)

    def getResultSummary(self):
        if self.results == FAILURE:
            return {'step': 'Failed to compile WebKit'}
        if self.results == SKIPPED:
            if self.getProperty('fast_commit_queue'):
                return {'step': 'Skipped compiling WebKit in fast-cq mode'}
            return {'step': 'Skipped compiling WebKit'}
        if self.results == CANCELLED and self.cancelled_due_to_huge_logs:
            return {'step': 'Cancelled step due to huge logs', 'build': 'Cancelled build due to huge logs'}
        return shell.Compile.getResultSummary(self)


class CompileWebKitWithoutChange(CompileWebKit):
    name = 'compile-webkit-without-change'
    haltOnFailure = False

    def __init__(self, retry_build_on_failure=False, **kwargs):
        self.retry_build_on_failure = retry_build_on_failure
        super().__init__(**kwargs)

    def evaluateCommand(self, cmd):
        rc = shell.Compile.evaluateCommand(self, cmd)

        self.build.addStepsAfterCurrentStep(self.follow_up_steps())

        if rc == FAILURE and self.retry_build_on_failure:
            message = 'Unable to build WebKit without change, retrying build'
            self.descriptionDone = message
            self.send_email_for_unexpected_build_failure()
            self.build.buildFinished([message], RETRY)
        return rc

    def send_email_for_unexpected_build_failure(self):
        try:
            pr_number = self.getProperty('github.number')
            sha = self.getProperty('github.head.sha', '')[:HASH_LENGTH_TO_DISPLAY]
            owners = self.getProperty('owners', [])
            author = owners[0] if owners else '?'
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            build_url = f'{self.master.config.buildbotURL}#/builders/{self.build._builderid}/builds/{self.build.number}'
            email_subject = f'{worker_name} might be in bad state, unable to build WebKit'
            email_text = f'{worker_name} might be in bad state. It is unable to build WebKit.'
            email_text += f' Same code was built successfuly on builder queue previously.'
            email_text += f'\n\nBuild: {build_url}\n\nBuilder: {builder_name}'
            email_text += f'\n\nPR: {pr_number}, Hash: {sha}, By: {author}\n'
            send_email_to_bot_watchers(email_subject, email_text, builder_name, f'build-failure-{worker_name}')
        except Exception as e:
            print(f'Error in sending email for unexpected build failure: {e}')


class AnalyzeCompileWebKitResults(buildstep.BuildStep, BugzillaMixin, GitHubMixin):
    name = 'analyze-compile-webkit-results'
    description = ['analyze-compile-webkit-results']
    descriptionDone = ['analyze-compile-webkit-results']

    @defer.inlineCallbacks
    def run(self):
        self.error_logs = {}
        self.compile_webkit_step = CompileWebKit.name
        if self.getProperty('group') == 'jsc':
            self.compile_webkit_step = CompileJSC.name
        yield self.getResults(self.compile_webkit_step)
        rc = yield self.analyzeResults()
        defer.returnValue(rc)

    @defer.inlineCallbacks
    def analyzeResults(self):
        compile_without_patch_step = CompileWebKitWithoutChange.name
        if self.getProperty('group') == 'jsc':
            compile_without_patch_step = CompileJSCWithoutChange.name
        compile_without_patch_result = self.getStepResult(compile_without_patch_step)

        patch_id = self.getProperty('patch_id', '')
        pr_number = self.getProperty('github.number')

        if compile_without_patch_result == FAILURE:
            if pr_number and self.getProperty('github.base.ref') != 'main':
                message = 'Unable to build WebKit without PR, please check manually'
                self.descriptionDone = message
                self.build.buildFinished([message], FAILURE)
                return defer.returnValue(FAILURE)

            message = 'Unable to build WebKit without {}, retrying build'.format('PR' if pr_number else 'patch')
            self.descriptionDone = message
            yield self.send_email_for_preexisting_build_failure()
            self.build.buildFinished([message], RETRY)
            return defer.returnValue(FAILURE)

        self.build.results = FAILURE
        sha = self.getProperty('github.head.sha')
        if sha and pr_number:
            message = 'Hash {} for PR {} does not build'.format(sha[:HASH_LENGTH_TO_DISPLAY], pr_number)
        else:
            message = 'Patch {} does not build'.format(patch_id)
        yield self.send_email_for_new_build_failure()

        self.descriptionDone = message
        self.setProperty('build_finish_summary', message)

        if patch_id:
            if self.getProperty('buildername', '').lower() == 'commit-queue':
                self.setProperty('comment_text', message)
                self.build.addStepsAfterCurrentStep([LeaveComment(), SetCommitQueueMinusFlagOnPatch()])
            else:
                self.build.addStepsAfterCurrentStep([SetCommitQueueMinusFlagOnPatch()])
        else:
            self.build.addStepsAfterCurrentStep([BlockPullRequest()])

        return defer.returnValue(FAILURE)

    @defer.inlineCallbacks
    def getResults(self, name):
        step = self.getBuildStepByName(name)
        if not step:
            defer.returnValue(None)
            return

        logs = yield self.master.db.logs.getLogs(step.stepid)
        log = next((log for log in logs if log['name'] == 'errors'), None)
        if not log:
            defer.returnValue(None)
            return

        lastline = int(max(0, log['num_lines'] - 1))
        logLines = yield self.master.db.logs.getLogLines(log['id'], 0, lastline)
        if log['type'] == 's':
            logLines = '\n'.join([line[1:] for line in logLines.splitlines()])

        self.error_logs[name] = logLines
        defer.returnValue(logLines)

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

    @defer.inlineCallbacks
    def send_email_for_new_build_failure(self):
        try:
            patch_id = self.getProperty('patch_id', '')
            pr_number = self.getProperty('github.number', '')
            sha = self.getProperty('github.head.sha', '')[:HASH_LENGTH_TO_DISPLAY]

            if patch_id:
                should_send_email = yield self.should_send_email_for_patch(patch_id)
                if not should_send_email:
                    return
            if pr_number:
                should_send_email = yield self.should_send_email_for_pr(pr_number, self.getProperty('repository'))
                if not should_send_email:
                    return
            if not patch_id and not (pr_number and sha):
                yield self._addToLog('stderr', 'Unrecognized change type')
                return

            change_string = None
            change_author = None
            if patch_id:
                change_author = self.getProperty('patch_author', '')
                change_string = 'Patch {}'.format(patch_id)
            elif pr_number and sha:
                change_string = 'Hash {}'.format(sha)
                change_author, errors = yield GitHub.email_for_owners(self.getProperty('owners', []))
                for error in errors:
                    yield self._addToLog('stdio', error)

            if not change_author:
                yield self._addToLog('stderr', 'Unable to determine email address for {} from metadata/contributors.json. Skipping sending email.'.format(self.getProperty('owners', [])))
                return

            builder_name = self.getProperty('buildername', '')
            bug_id = self.getProperty('bug_id', '') or pr_number
            title = self.getProperty('bug_title', '') or self.getProperty('github.title', '')
            worker_name = self.getProperty('workername', '')
            platform = self.getProperty('platform', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            logs = self.error_logs.get(self.compile_webkit_step)
            if platform == 'win':
                logs = self.filter_logs_containing_error(logs, searchString='error ')
            else:
                logs = self.filter_logs_containing_error(logs)

            email_subject = 'Build failure for {}: {}'.format(change_string, title)
            email_text = 'EWS has detected build failure on {}'.format(builder_name)
            if patch_id:
                email_text += ' while testing <a href="{}">{}</a>'.format(Bugzilla.patch_url(patch_id), change_string)
                email_text += ' for <a href="{}">Bug {}</a>.'.format(Bugzilla.bug_url(bug_id), bug_id)
            if sha:
                repository = self.getProperty('repository')
                email_text += ' while testing <a href="{}">{}</a>'.format(GitHub.commit_url(sha, repository), change_string)
                email_text += ' for <a href="{}">PR #{}</a>.'.format(GitHub.pr_url(pr_number, repository), pr_number)
            email_text += '\n\nFull details are available at: {}\n\nChange author: {}'.format(build_url, change_author)
            if logs:
                logs = logs.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
                email_text += '\n\nError lines:\n\n<code>{}</code>'.format(logs)
            email_text += '\n\nTo unsubscribe from these notifications or to provide any feedback please email aakash_jain@apple.com'
            yield self._addToLog('stdio', 'Sending email notification to {}'.format(change_author))
            send_email_to_patch_author(change_author, email_subject, email_text, patch_id or self.getProperty('github.head.sha', ''))
        except Exception as e:
            yield self._addToLog('stdio', f'Error in sending email for new build failure: {e}')

    @defer.inlineCallbacks
    def send_email_for_preexisting_build_failure(self):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            platform = self.getProperty('platform', '')
            identifier = self.getProperty('identifier', None)
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            logs = self.error_logs.get(self.compile_webkit_step)
            if platform == 'win':
                logs = self.filter_logs_containing_error(logs, searchString='error ')
            else:
                logs = self.filter_logs_containing_error(logs)

            email_subject = 'Build failure on trunk on {}'.format(builder_name)
            email_text = f'Failed to build WebKit without change in {build_url}\n\nBuilder: {builder_name}\nWorker: {worker_name}'
            email_text += f'\nIdentifier: {identifier}'
            if logs:
                logs = logs.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
                email_text += '\n\nError lines:\n\n<code>{}</code>'.format(logs)
            reference = 'preexisting-build-failure-{}-{}'.format(builder_name, date.today().strftime("%Y-%d-%m"))
            send_email_to_bot_watchers(email_subject, email_text, builder_name, reference)
        except Exception as e:
            yield self._addToLog('stdio', f'Error in sending email for build failure: {e}')


class CompileJSC(CompileWebKit):
    name = 'compile-jsc'
    descriptionDone = ['Compiled JSC']
    build_command = ['perl', 'Tools/Scripts/build-jsc']

    def start(self):
        self.setProperty('group', 'jsc')
        return CompileWebKit.start(self)

    def getResultSummary(self):
        if self.results == FAILURE:
            return {'step': 'Failed to compile JSC'}
        return shell.Compile.getResultSummary(self)


class CompileJSCWithoutChange(CompileJSC):
    name = 'compile-jsc-without-change'

    def evaluateCommand(self, cmd):
        return shell.Compile.evaluateCommand(self, cmd)


class RunJavaScriptCoreTests(shell.Test, AddToLogMixin, ShellMixin):
    name = 'jscore-test'
    description = ['jscore-tests running']
    descriptionDone = ['jscore-tests']
    flunkOnFailure = True
    jsonFileName = 'jsc_results.json'
    logfiles = {'json': jsonFileName}
    command = ['perl', 'Tools/Scripts/run-javascriptcore-tests', '--no-build', '--no-fail-fast', '--json-output={0}'.format(jsonFileName), WithProperties('--%(configuration)s')]
    # We rely on run-jsc-stress-tests to weed out any flaky tests
    command_extra = ['--treat-failing-as-flaky=0.6,10,200']
    prefix = 'jsc_'
    NUM_FAILURES_TO_DISPLAY_IN_STATUS = 5

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, sigtermTime=10, timeout=3 * 60 * 60, **kwargs)
        self.binaryFailures = []
        self.stressTestFailures = []
        self.flaky = {}

    def start(self):
        self.log_observer_json = logobserver.BufferLogObserver()
        self.addLogObserver('json', self.log_observer_json)
        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('stdio', self.log_observer)

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

        self.setCommand(self.command + customBuildFlag(self.getProperty('platform'), self.getProperty('fullPlatform')))
        self.command.extend(self.command_extra)
        self.command = self.shell_command(' '.join(quote(str(c)) for c in self.command) + ' 2>&1 | Tools/Scripts/filter-test-logs jsc')
        return super().start()

    def evaluateCommand(self, cmd):
        rc = super().evaluateCommand(cmd)
        steps_to_add = [
            GenerateS3URL(
                f"{self.getProperty('fullPlatform')}-{self.getProperty('architecture')}-{self.getProperty('configuration')}-{self.name}",
                extension='txt',
                content_type='text/plain',
            ), UploadFileToS3(
                'logs.txt',
                links={self.name: 'Full logs'},
                content_type='text/plain',
            )
        ]
        if self.countFailures() != 0:
            rc = FAILURE
        if rc == SUCCESS or rc == WARNINGS:
            message = 'Passed JSC tests'
            self.descriptionDone = message
            self.build.results = SUCCESS
        else:
            steps_to_add += [
                RevertAppliedChanges(),
                CleanWorkingDirectory(),
                ValidateChange(verifyBugClosed=False, addURLs=False),
                CompileJSCWithoutChange(),
                ValidateChange(verifyBugClosed=False, addURLs=False),
                KillOldProcesses(),
                RunJSCTestsWithoutChange(),
                AnalyzeJSCTestsResults()
            ]
        self.build.addStepsAfterCurrentStep(steps_to_add)
        return rc

    def commandComplete(self, cmd):
        super().commandComplete(cmd)
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
        self.flaky = jsc_results.get('flakyAndPassed')
        if self.flaky:
            self.setProperty(self.prefix + 'flaky_and_passed', self.flaky)
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
        elif self.results == SUCCESS and self.flaky:
            return {'step': "Passed JSC tests (%d flaky)" % len(self.flaky)}

        return super().getResultSummary()

    def countFailures(self):
        logText = self.log_observer.getStdout()
        count = 0

        match = re.search(r'^Results for JSC stress tests:\r?\n\s+(\d+) failure', logText, re.MULTILINE)
        if match:
            count += int(match.group(1))

        match = re.search(r'Results for JSC test binaries:\r?\n\s+(\d+) failure', logText, re.MULTILINE)
        if match:
            count += int(match.group(1))

        match = re.search(r'^Results for Mozilla tests:\r?\n\s+(\d+) regression', logText, re.MULTILINE)
        if match:
            count += int(match.group(1))

        return count


class RunJSCTestsWithoutChange(RunJavaScriptCoreTests):
    name = 'jscore-test-without-change'
    prefix = 'jsc_clean_tree_'

    def evaluateCommand(self, cmd):
        rc = shell.Test.evaluateCommand(self, cmd)
        self.setProperty('clean_tree_run_status', rc)
        return rc


class AnalyzeJSCTestsResults(buildstep.BuildStep, AddToLogMixin):
    name = 'analyze-jsc-tests-results'
    description = ['analyze-jsc-test-results']
    descriptionDone = ['analyze-jsc-tests-results']
    NUM_FAILURES_TO_DISPLAY = 10

    def start(self):
        stress_failures_with_change = set(self.getProperty('jsc_stress_test_failures', []))
        binary_failures_with_change = set(self.getProperty('jsc_binary_failures', []))
        clean_tree_stress_failures = set(self.getProperty('jsc_clean_tree_stress_test_failures', []))
        clean_tree_binary_failures = set(self.getProperty('jsc_clean_tree_binary_failures', []))
        clean_tree_failures = list(clean_tree_binary_failures) + list(clean_tree_stress_failures)
        clean_tree_failures_string = ', '.join(clean_tree_failures[:self.NUM_FAILURES_TO_DISPLAY])

        flaky_stress_failures_with_change = set(self.getProperty('jsc_flaky_and_passed', {}).keys())
        clean_tree_flaky_stress_failures = set(self.getProperty('jsc_clean_tree_flaky_and_passed', {}).keys())
        flaky_stress_failures = sorted(list(flaky_stress_failures_with_change) + list(clean_tree_flaky_stress_failures))[:self.NUM_FAILURES_TO_DISPLAY]
        flaky_failures_string = ', '.join(flaky_stress_failures)

        new_stress_failures = stress_failures_with_change - clean_tree_stress_failures
        new_binary_failures = binary_failures_with_change - clean_tree_binary_failures
        self.new_stress_failures_to_display = ', '.join(sorted(list(new_stress_failures))[:self.NUM_FAILURES_TO_DISPLAY])
        self.new_binary_failures_to_display = ', '.join(sorted(list(new_binary_failures))[:self.NUM_FAILURES_TO_DISPLAY])

        self._addToLog('stderr', '\nFailures with change: {}'.format(list(binary_failures_with_change) + list(stress_failures_with_change))[:self.NUM_FAILURES_TO_DISPLAY])
        self._addToLog('stderr', '\nFlaky Tests with change: {}'.format(', '.join(flaky_stress_failures_with_change)))
        self._addToLog('stderr', '\nFailures on clean tree: {}'.format(clean_tree_failures_string))
        self._addToLog('stderr', '\nFlaky Tests on clean tree: {}'.format(', '.join(clean_tree_flaky_stress_failures)))

        if (not stress_failures_with_change) and (not binary_failures_with_change):
            # If we've made it here, then jsc-tests and re-run-jsc-tests failed, which means
            # there should have been some test failures. Otherwise there is some unexpected issue.
            clean_tree_run_status = self.getProperty('clean_tree_run_status', FAILURE)
            if clean_tree_run_status in [SUCCESS, WARNINGS]:
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
            if flaky_stress_failures:
                message += ' Found flaky tests: {}'.format(flaky_failures_string)
                for flaky_failure in flaky_stress_failures:
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
            message = 'Found unexpected failure with change'
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
        super().__init__(timeout=2 * 60, logEnviron=False, **kwargs)

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
    haltOnFailure = False
    flunkOnFailure = False
    command = ['python3', 'Tools/CISupport/trigger-crash-log-submission']

    def __init__(self, **kwargs):
        super().__init__(timeout=60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results in [FAILURE, EXCEPTION]:
            return {'step': 'Failed to trigger crash log submission'}
        return shell.Compile.getResultSummary(self)


class WaitForCrashCollection(shell.Compile):
    name = 'wait-for-crash-collection'
    description = ['waiting-for-crash-collection-to-quiesce']
    descriptionDone = ['Crash collection has quiesced']
    haltOnFailure = False
    flunkOnFailure = False
    command = ['python3', 'Tools/CISupport/wait-for-crash-collection', '--timeout', str(5 * 60)]

    def __init__(self, **kwargs):
        super().__init__(timeout=6 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results in [FAILURE, EXCEPTION]:
            return {'step': 'Crash log collection process still running'}
        return shell.Compile.getResultSummary(self)


class RunWebKitTests(shell.Test, AddToLogMixin, ShellMixin):
    name = 'layout-tests'
    description = ['layout-tests running']
    descriptionDone = ['layout-tests']
    resultDirectory = 'layout-test-results'
    jsonFileName = 'layout-test-results/full_results.json'
    logfiles = {'json': jsonFileName}
    test_failures_log_name = 'test-failures'
    results_db_log_name = 'results-db'
    ENABLE_GUARD_MALLOC = False
    ENABLE_ADDITIONAL_ARGUMENTS = True
    EXIT_AFTER_FAILURES = '60'
    STRESS_MODE = False
    command = ['python3', 'Tools/Scripts/run-webkit-tests',
               '--no-build',
               '--no-show-results',
               '--no-new-test-results',
               '--clobber-old-results',
               WithProperties('--%(configuration)s')]

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, timeout=5.5 * 60 * 60, **kwargs)
        self.incorrectLayoutLines = []
        self.failing_tests_filtered = []
        self.preexisting_failures_in_results_db = []

    def doStepIf(self, step):
        return not ((self.getProperty('buildername', '').lower() in ['commit-queue', 'merge-queue']) and
                    (self.getProperty('fast_commit_queue') or self.getProperty('passed_mac_wk2')))

    def setLayoutTestCommand(self):
        platform = self.getProperty('platform')
        self.setCommand(self.command + customBuildFlag(platform, self.getProperty('fullPlatform')))

        if self.getProperty('use-dump-render-tree', False):
            self.setCommand(self.command + ['--dump-render-tree'])

        self.setCommand(self.command + ['--results-directory', self.resultDirectory])
        self.setCommand(self.command + ['--debug-rwt-logging'])

        patch_author = self.getProperty('patch_author')
        self.maxTime = None
        if patch_author in ['webkit-wpt-import-bot@igalia.com']:
            self.setCommand(self.command + ['imported/w3c/web-platform-tests'])
        elif GitHub.NO_FAILURE_LIMITS_LABEL in self.getProperty('github_labels', []):
            self.setCommand(self.command + ['--no-retry'])
            self.maxTime = 60 * 90
        else:
            if self.EXIT_AFTER_FAILURES is not None:
                self.setCommand(self.command + ['--exit-after-n-failures', '{}'.format(self.EXIT_AFTER_FAILURES)])
            if not self.STRESS_MODE:
                self.setCommand(self.command + ['--skip-failing-tests'])

        if platform in ['gtk', 'wpe']:
            self.setCommand(self.command + ['--enable-core-dumps-nolimit'])

        additionalArguments = self.getProperty('additionalArguments')
        if additionalArguments and self.ENABLE_ADDITIONAL_ARGUMENTS:
            self.setCommand(self.command + additionalArguments)

        if self.ENABLE_GUARD_MALLOC:
            self.setCommand(self.command + ['--guard-malloc'])

    def buildCommandKwargs(self, warnings):
        result = super().buildCommandKwargs(warnings)
        if self.maxTime:
            result['maxTime'] = self.maxTime
        return result

    def start(self, BufferLogObserverClass=logobserver.BufferLogObserver):
        self.log_observer = BufferLogObserverClass(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)
        self.log_observer_json = logobserver.BufferLogObserver()
        self.addLogObserver('json', self.log_observer_json)
        self.setLayoutTestCommand()
        self.command = self.shell_command(' '.join(quote(str(c)) for c in self.command) + ' 2>&1 | Tools/Scripts/filter-test-logs layout')
        return super().start()

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

    @defer.inlineCallbacks
    def runCommand(self, command):
        yield super().runCommand(command)

        logText = self.log_observer.getStdout() + self.log_observer.getStderr()
        logTextJson = self.log_observer_json.getStdout()

        first_results = LayoutTestFailures.results_from_string(logTextJson)
        is_main = self.getProperty('github.base.ref', DEFAULT_BRANCH) == DEFAULT_BRANCH

        if first_results:
            self.setProperty('first_results_exceed_failure_limit', first_results.did_exceed_test_failure_limit)
            self.setProperty('first_run_failures', sorted(first_results.failing_tests))
            self.setProperty('first_run_flakies', sorted(first_results.flaky_tests))
            if first_results.failing_tests:
                self._addToLog(self.test_failures_log_name, '\n'.join(first_results.failing_tests))

            if is_main and first_results.failing_tests and not first_results.did_exceed_test_failure_limit:
                yield self.filter_failures_using_results_db(first_results.failing_tests)
                self.setProperty('first_run_failures_filtered', sorted(self.failing_tests_filtered))
                self.setProperty('results-db_first_run_pre_existing', sorted(self.preexisting_failures_in_results_db))

        self._parseRunWebKitTestsOutput(logText)

    @defer.inlineCallbacks
    def filter_failures_using_results_db(self, failing_tests):
        self.failing_tests_filtered = failing_tests.copy()
        identifier = self.getProperty('identifier', None)
        platform = self.getProperty('platform', None)
        configuration = {}
        if platform:
            configuration['platform'] = platform
        style = self.getProperty('configuration', None)
        if style and style in ['debug', 'release']:
            configuration['style'] = style

        if self.getProperty('use-dump-render-tree', False):
            configuration['flavor'] = 'wk1'
        else:
            configuration['flavor'] = 'wk2'

        self._addToLog(self.results_db_log_name, f'Checking Results database for failing tests. Identifier: {identifier}, configuration: {configuration}')
        has_commit = False
        if failing_tests and identifier:
            has_commit = yield ResultsDatabase.has_commit(commit=identifier)
            if not has_commit:
                self._addToLog(self.results_db_log_name, f"'{identifier}' could not be found on the results database, falling back to tip-of-tree\n")

        for test in failing_tests:
            data = yield ResultsDatabase.is_test_pre_existing_failure(
                test, configuration=configuration,
                commit=identifier if has_commit else None,
            )
            self._addToLog(self.results_db_log_name, f"\n{test}: pass_rate: {data['pass_rate']}, pre-existing-failure={data['is_existing_failure']}\nResponse from results-db: {data['raw_data']}\n{data['logs']}")
            if data['is_existing_failure']:
                self.preexisting_failures_in_results_db.append(test)
                self.failing_tests_filtered.remove(test)
            else:
                # Optimization to skip consulting results-db for every failure if we encounter any new failure,
                # since until there is atleast one failure which is not pre-existing, we will anayways have to continue with retry logic.
                break

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
        previous_build_summary = self.getProperty('build_summary', '')
        steps_to_add = [
            GenerateS3URL(
                f"{self.getProperty('fullPlatform')}-{self.getProperty('architecture')}-{self.getProperty('configuration')}-{self.name}",
                extension='txt',
                content_type='text/plain',
            ), UploadFileToS3(
                'logs.txt',
                links={self.name: 'Full logs'},
                content_type='text/plain',
            )
        ]
        if rc == SUCCESS or rc == WARNINGS:
            message = 'Passed layout tests'
            self.descriptionDone = message
            self.build.results = SUCCESS
            if RunWebKitTestsInStressMode.FAILURE_MSG_IN_STRESS_MODE not in previous_build_summary:
                self.setProperty('build_summary', message)
        elif (self.preexisting_failures_in_results_db and len(self.failing_tests_filtered) == 0):
            # This means all the tests which failed in this run were also failing or flaky in results database
            message = f"Ignored pre-existing failure: {', '.join(self.preexisting_failures_in_results_db)}"
            self.descriptionDone = message
            self.build.results = SUCCESS
            if RunWebKitTestsInStressMode.FAILURE_MSG_IN_STRESS_MODE not in previous_build_summary:
                self.setProperty('build_summary', message)
            steps_to_add += [ArchiveTestResults(), UploadTestResults(), ExtractTestResults()]
            self.finished(WARNINGS)
        else:
            steps_to_add += [
                ArchiveTestResults(),
                UploadTestResults(),
                ExtractTestResults(),
                ValidateChange(verifyBugClosed=False, addURLs=False),
            ]
            if GitHub.NO_FAILURE_LIMITS_LABEL not in self.getProperty('github_labels', []):
                steps_to_add += [
                    KillOldProcesses(),
                    ReRunWebKitTests(),
                ]
            else:
                steps_to_add += [
                    RevertAppliedChanges(),
                    CleanWorkingDirectory(),
                    ValidateChange(verifyBugClosed=False, addURLs=False),
                    CompileWebKitWithoutChange(retry_build_on_failure=True),
                    ValidateChange(verifyBugClosed=False, addURLs=False),
                    KillOldProcesses(),
                    RunWebKitTestsWithoutChange(),
                ]
        self.build.addStepsAfterCurrentStep(steps_to_add)

        return rc

    def getResultSummary(self):
        status = self.name

        if self.results != SUCCESS:
            if (self.preexisting_failures_in_results_db and len(self.failing_tests_filtered) == 0):
                status = f"Ignored {len(self.preexisting_failures_in_results_db)} pre-existing failure based on results-db"
                return {'step': status}
            if self.incorrectLayoutLines:
                status = ' '.join(self.incorrectLayoutLines)
                return {'step': status}
        if self.results == SKIPPED:
            if self.getProperty('fast_commit_queue'):
                return {'step': 'Skipped layout-tests in fast-cq mode'}
            return {'step': 'Skipped layout-tests'}

        return super().getResultSummary()


class RunWebKitTestsInStressMode(RunWebKitTests):
    name = 'run-layout-tests-in-stress-mode'
    suffix = 'stress-mode'
    EXIT_AFTER_FAILURES = '10'
    ENABLE_ADDITIONAL_ARGUMENTS = False
    STRESS_MODE = True
    FAILURE_MSG_IN_STRESS_MODE = 'Found test failures in stress mode'

    def __init__(self, num_iterations=100, layout_test_class=RunWebKitTests):
        self.num_iterations = num_iterations
        self.layout_test_class = layout_test_class
        super().__init__()

    def setLayoutTestCommand(self):
        if self.layout_test_class == RunWebKit1Tests:
            self.setProperty('use-dump-render-tree', True)
        RunWebKitTests.setLayoutTestCommand(self)

        self.setCommand(self.command + ['--iterations', self.num_iterations])
        modified_tests = self.getProperty('modified_tests')
        if modified_tests:
            self.setCommand(self.command + modified_tests)

    def evaluateCommand(self, cmd):
        rc = self.evaluateResult(cmd)
        steps_to_add = [
            GenerateS3URL(
                f"{self.getProperty('fullPlatform')}-{self.getProperty('architecture')}-{self.getProperty('configuration')}-{self.name}",
                extension='txt',
                content_type='text/plain',
            ), UploadFileToS3(
                'logs.txt',
                links={self.name: 'Full logs'},
                content_type='text/plain',
            )
        ]
        if rc == SUCCESS or rc == WARNINGS:
            message = 'Passed layout tests'
            self.descriptionDone = message
            self.build.results = SUCCESS
            self.setProperty('build_summary', message)
        else:
            self.setProperty('build_summary', self.FAILURE_MSG_IN_STRESS_MODE)
            steps_to_add += [
                ArchiveTestResults(),
                UploadTestResults(identifier=self.suffix),
                ExtractTestResults(identifier=self.suffix),
            ]
        self.build.addStepsAfterCurrentStep(steps_to_add)
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
        first_results_did_exceed_test_failure_limit = self.getProperty('first_results_exceed_failure_limit', False)
        first_results_failing_tests = set(self.getProperty('first_run_failures', []))
        second_results_did_exceed_test_failure_limit = self.getProperty('second_results_exceed_failure_limit', False)
        second_results_failing_tests = set(self.getProperty('second_run_failures', []))
        # FIXME: here it can be a good idea to also use the info from second_run_flakies and first_run_flakies
        tests_that_consistently_failed = first_results_failing_tests.intersection(second_results_failing_tests)
        flaky_failures = first_results_failing_tests.union(second_results_failing_tests) - first_results_failing_tests.intersection(second_results_failing_tests)
        num_flaky_failures = len(flaky_failures)
        flaky_failures = sorted(list(flaky_failures))[:self.NUM_FAILURES_TO_DISPLAY]
        flaky_failures_string = ', '.join(flaky_failures)
        previous_build_summary = self.getProperty('build_summary', '')
        steps_to_add = [
            GenerateS3URL(
                f"{self.getProperty('fullPlatform')}-{self.getProperty('architecture')}-{self.getProperty('configuration')}-{self.name}",
                extension='txt',
                content_type='text/plain',
            ), UploadFileToS3(
                'logs.txt',
                links={self.name: 'Full logs'},
                content_type='text/plain',
            )
        ]

        if rc == SUCCESS or rc == WARNINGS:
            message = 'Passed layout tests'
            self.descriptionDone = message
            self.build.results = SUCCESS
            if (not first_results_did_exceed_test_failure_limit) and flaky_failures:
                pluralSuffix = 's' if len(flaky_failures) > 1 else ''
                message = 'Found flaky test{}: {}'.format(pluralSuffix, flaky_failures_string)
                for flaky_failure in flaky_failures:
                    self.send_email_for_flaky_failure(flaky_failure)
            if RunWebKitTestsInStressMode.FAILURE_MSG_IN_STRESS_MODE not in previous_build_summary:
                self.setProperty('build_summary', message)
            self.build.addStepsAfterCurrentStep(steps_to_add)
        elif (self.preexisting_failures_in_results_db and len(self.failing_tests_filtered) == 0):
            # This means all the tests which failed in this run were also failing or flaky in results database
            message = f"Ignored pre-existing failure: {', '.join(self.preexisting_failures_in_results_db)}"
            self.descriptionDone = message
            self.build.results = SUCCESS
            if RunWebKitTestsInStressMode.FAILURE_MSG_IN_STRESS_MODE not in previous_build_summary:
                self.setProperty('build_summary', message)
            steps_to_add += [ArchiveTestResults(), UploadTestResults(identifier='rerun'), ExtractTestResults(identifier='rerun')]
            self.build.addStepsAfterCurrentStep(steps_to_add)
            self.finished(WARNINGS)
        else:
            if (first_results_failing_tests and second_results_failing_tests and len(tests_that_consistently_failed) == 0
                    and num_flaky_failures <= 10
                    and not first_results_did_exceed_test_failure_limit and not second_results_did_exceed_test_failure_limit):
                # This means that test failures in first and second run were different and limited.
                pluralSuffix = 's' if len(flaky_failures) > 1 else ''
                message = 'Found flaky test{} in re-run: {}'.format(pluralSuffix, flaky_failures_string)
                for flaky_failure in flaky_failures:
                    self.send_email_for_flaky_failure(flaky_failure)
                self.descriptionDone = message
                self.build.results = SUCCESS
                if RunWebKitTestsInStressMode.FAILURE_MSG_IN_STRESS_MODE not in previous_build_summary:
                    self.setProperty('build_summary', message)
                steps_to_add += [ArchiveTestResults(), UploadTestResults(identifier='rerun'), ExtractTestResults(identifier='rerun')]
                self.build.addStepsAfterCurrentStep(steps_to_add)
                self.finished(WARNINGS)
                return rc
            steps_to_add += [
                ArchiveTestResults(),
                UploadTestResults(identifier='rerun'),
                ExtractTestResults(identifier='rerun'),
                RevertAppliedChanges(),
                CleanWorkingDirectory(),
                ValidateChange(verifyBugClosed=False, addURLs=False),
                CompileWebKitWithoutChange(retry_build_on_failure=True),
                ValidateChange(verifyBugClosed=False, addURLs=False),
                KillOldProcesses(),
                RunWebKitTestsWithoutChange()
            ]
            self.build.addStepsAfterCurrentStep(steps_to_add)
        return rc

    @defer.inlineCallbacks
    def runCommand(self, command):
        yield shell.Test.runCommand(self, command)

        logText = self.log_observer.getStdout() + self.log_observer.getStderr()
        logTextJson = self.log_observer_json.getStdout()

        second_results = LayoutTestFailures.results_from_string(logTextJson)
        is_main = self.getProperty('github.base.ref', DEFAULT_BRANCH) == DEFAULT_BRANCH

        if second_results:
            self.setProperty('second_results_exceed_failure_limit', second_results.did_exceed_test_failure_limit)
            self.setProperty('second_run_failures', sorted(second_results.failing_tests))
            self.setProperty('second_run_flakies', sorted(second_results.flaky_tests))
            if second_results.failing_tests:
                self._addToLog(self.test_failures_log_name, '\n'.join(second_results.failing_tests))

            if is_main and second_results.failing_tests and not second_results.did_exceed_test_failure_limit:
                yield self.filter_failures_using_results_db(second_results.failing_tests)
                self.setProperty('second_run_failures_filtered', sorted(self.failing_tests_filtered))
                self.setProperty('results-db_second_run_pre_existing', sorted(self.preexisting_failures_in_results_db))
        self._parseRunWebKitTestsOutput(logText)

    def send_email_for_flaky_failure(self, test_name):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            test_url = '{}{}'.format(LAYOUT_TESTS_URL, test_name)
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            history_url = '{}?suite=layout-tests&test={}'.format(RESULTS_DB_URL, test_name)

            email_subject = 'Flaky test: {}'.format(test_name)
            email_text = 'Test <a href="{}">{}</a> flaked in {}\n\nBuilder: {}'.format(test_url, test_name, build_url, builder_name)
            email_text = 'Flaky test: <a href="{}">{}</a>\n\nBuild: {}\n\nBuilder: {}\n\nWorker: {}\n\nHistory: {}'.format(test_url, test_name, build_url, builder_name, worker_name, history_url)
            send_email_to_bot_watchers(email_subject, email_text, builder_name, 'flaky-{}'.format(test_name))
        except Exception as e:
            # Catching all exceptions here to ensure that failure to send email doesn't impact the build
            print('Error in sending email for flaky failures: {}'.format(e))


class RunWebKitTestsWithoutChange(RunWebKitTests):
    name = 'run-layout-tests-without-change'

    def start(self):
        api_key = os.getenv(RESULTS_SERVER_API_KEY)
        if api_key:
            self.workerEnvironment[RESULTS_SERVER_API_KEY] = api_key
        else:
            self._addToLog('stdio', 'No API key for {} found'.format(RESULTS_DB_URL))
        return super().start()

    def evaluateCommand(self, cmd):
        rc = shell.Test.evaluateCommand(self, cmd)
        steps_to_add = [
            GenerateS3URL(
                f"{self.getProperty('fullPlatform')}-{self.getProperty('architecture')}-{self.getProperty('configuration')}-{self.name}",
                extension='txt',
                content_type='text/plain',
            ), UploadFileToS3(
                'logs.txt',
                links={self.name: 'Full logs'},
                content_type='text/plain',
            )
        ]
        steps_to_add += [ArchiveTestResults(), UploadTestResults(identifier='clean-tree'), ExtractTestResults(identifier='clean-tree'), AnalyzeLayoutTestsResults()]
        self.build.addStepsAfterCurrentStep(steps_to_add)
        self.setProperty('clean_tree_run_status', rc)
        return rc

    @defer.inlineCallbacks
    def runCommand(self, command):
        yield shell.Test.runCommand(self, command)

        logText = self.log_observer.getStdout() + self.log_observer.getStderr()
        logTextJson = self.log_observer_json.getStdout()

        clean_tree_results = LayoutTestFailures.results_from_string(logTextJson)

        if clean_tree_results:
            self.setProperty('clean_tree_results_exceed_failure_limit', clean_tree_results.did_exceed_test_failure_limit)
            self.setProperty('clean_tree_run_failures', clean_tree_results.failing_tests)
            self.setProperty('clean_tree_run_flakies', sorted(clean_tree_results.flaky_tests))
            if clean_tree_results.failing_tests:
                self._addToLog(self.test_failures_log_name, '\n'.join(clean_tree_results.failing_tests))
        self._parseRunWebKitTestsOutput(logText)

    def setLayoutTestCommand(self):
        super().setLayoutTestCommand()
        if CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES and self.getProperty('github.base.ref', DEFAULT_BRANCH) == DEFAULT_BRANCH:
            self.setCommand(
                self.command + [
                    '--builder-name', self.getProperty('buildername', ''),
                    '--build-number', self.getProperty('buildnumber', ''),
                    '--buildbot-worker', self.getProperty('workername', ''),
                    '--buildbot-master', CURRENT_HOSTNAME,
                    '--report', RESULTS_DB_URL,
                ]
            )

        # In order to speed up testing, on the step that retries running the layout tests without change
        # only run the subset of tests that failed on the previous steps.
        # But only do that if the previous steps didn't exceed the test failure limit
        # Also pass '--skipped=always' to avoid running a test that is skipped on the clean tree and that
        # the change removed from the TestExpectations file meanwhile it still fails with the change (so
        # it is passed as an argument on the command-line)
        # The flag '--skip-failing-tests' that is passed by default (in combination with '--skipped=always')
        # avoids running tests marked as failing on the Expectation files even when those are passed as arguments.
        first_results_did_exceed_test_failure_limit = self.getProperty('first_results_exceed_failure_limit', False)
        second_results_did_exceed_test_failure_limit = self.getProperty('second_results_exceed_failure_limit', False)
        if not first_results_did_exceed_test_failure_limit and not second_results_did_exceed_test_failure_limit:
            first_results_failing_tests = set(self.getProperty('first_run_failures', set()))
            second_results_failing_tests = set(self.getProperty('second_run_failures', set()))
            list_failed_tests_with_change = sorted(first_results_failing_tests.union(second_results_failing_tests))
            if list_failed_tests_with_change:
                self.setCommand(self.command + ['--skipped=always'] + list_failed_tests_with_change)


class AnalyzeLayoutTestsResults(buildstep.BuildStep, BugzillaMixin, GitHubMixin):
    name = 'analyze-layout-tests-results'
    description = ['analyze-layout-test-results']
    descriptionDone = ['analyze-layout-tests-results']
    NUM_FAILURES_TO_DISPLAY = 10

    @defer.inlineCallbacks
    def report_failure(self, new_failures=None, exceed_failure_limit=False, failure_message=''):
        self.build.results = FAILURE
        if not new_failures:
            message = 'Found unexpected failure with change' if not failure_message else failure_message
        else:
            pluralSuffix = 's' if len(new_failures) > 1 else ''
            if exceed_failure_limit:
                message = 'Failure limit exceed. At least found'
            else:
                message = 'Found'
            new_failures_string = ' '.join(sorted(new_failures)[:self.NUM_FAILURES_TO_DISPLAY])
            message += ' {} new test failure{}: {}'.format(len(new_failures), pluralSuffix, new_failures_string)
            if len(new_failures) > self.NUM_FAILURES_TO_DISPLAY:
                message += ' ...'
            yield self.send_email_for_new_test_failures(new_failures, exceed_failure_limit)
        self.descriptionDone = message
        self.setProperty('build_finish_summary', message)

        if self.getProperty('buildername', '').lower() == 'commit-queue':
            self.setProperty('comment_text', message)
            self.build.addStepsAfterCurrentStep([LeaveComment(), SetCommitQueueMinusFlagOnPatch()])
        else:
            self.build.addStepsAfterCurrentStep([SetCommitQueueMinusFlagOnPatch(), BlockPullRequest()])
        defer.returnValue(FAILURE)

    def report_pre_existing_failures(self, clean_tree_failures, flaky_failures):
        self.build.results = SUCCESS
        self.descriptionDone = 'Passed layout tests'
        previous_build_summary = self.getProperty('build_summary', '')
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

        if RunWebKitTestsInStressMode.FAILURE_MSG_IN_STRESS_MODE not in previous_build_summary:
            self.setProperty('build_summary', message)
        return SUCCESS

    def retry_build(self, message=''):
        if not message:
            message = 'Unable to confirm if test failures are introduced by change, retrying build'
        self.descriptionDone = message

        triggered_by = self.getProperty('triggered_by', None)
        if triggered_by:
            # Trigger parent build so that it can re-build ToT
            schduler_for_current_queue = self.getProperty('scheduler')
            self.build.addStepsAfterCurrentStep([Trigger(
                schedulerNames=triggered_by,
                include_revision=False,
                triggers=[schduler_for_current_queue],
                patch=bool(self.getProperty('patch_id')),
                pull_request=bool(self.getProperty('github.number')),
            )])
            self.setProperty('build_summary', message)
            return SUCCESS
        else:
            self.build.buildFinished([message], RETRY)
            return RETRY

    def _results_failed_different_tests(self, first_results_failing_tests, second_results_failing_tests):
        return first_results_failing_tests != second_results_failing_tests

    def send_email_for_flaky_failure(self, test_name, step_str=None):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            test_url = '{}{}'.format(LAYOUT_TESTS_URL, test_name)
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            history_url = '{}?suite=layout-tests&test={}'.format(RESULTS_DB_URL, test_name)

            email_subject = 'Flaky test: {}'.format(test_name)
            email_text = 'Flaky test: <a href="{}">{}</a>\n\nBuild: {}\n\nBuilder: {}\n\nWorker: {}\n\nHistory: {}'.format(test_url, test_name, build_url, builder_name, worker_name, history_url)
            if step_str:
                email_text += '\nThis test was flaky on the steps: {}'.format(step_str)
            send_email_to_bot_watchers(email_subject, email_text, builder_name, 'flaky-{}'.format(test_name))
        except Exception as e:
            print('Error in sending email for flaky failure: {}'.format(e))

    def send_email_for_pre_existing_failure(self, test_name):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            test_url = '{}{}'.format(LAYOUT_TESTS_URL, test_name)
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            history_url = '{}?suite=layout-tests&test={}'.format(RESULTS_DB_URL, test_name)

            email_subject = 'Pre-existing test failure: {}'.format(test_name)
            email_text = 'Test <a href="{}">{}</a> failed on clean tree run in {}.\n\nBuilder: {}\n\nWorker: {}\n\nHistory: {}'.format(test_url, test_name, build_url, builder_name, worker_name, history_url)
            send_email_to_bot_watchers(email_subject, email_text, builder_name, 'preexisting-{}'.format(test_name))
        except Exception as e:
            print('Error in sending email for pre-existing failure: {}'.format(e))

    @defer.inlineCallbacks
    def send_email_for_new_test_failures(self, test_names, exceed_failure_limit=False):
        try:
            patch_id = self.getProperty('patch_id', '')
            pr_number = self.getProperty('github.number', '')
            sha = self.getProperty('github.head.sha', '')[:HASH_LENGTH_TO_DISPLAY]

            if patch_id:
                should_send_email = yield self.should_send_email_for_patch(patch_id)
                if not should_send_email:
                    return
            if pr_number:
                should_send_email = yield self.should_send_email_for_pr(pr_number, self.getProperty('repository'))
                if not should_send_email:
                    return
            if not patch_id and not (pr_number and sha):
                yield self._addToLog('stderr', 'Unrecognized change type')
                return

            change_string = None
            change_author = None
            if patch_id:
                change_author = self.getProperty('patch_author', '')
                change_string = 'Patch {}'.format(patch_id)
            elif pr_number and sha:
                change_string = 'Hash {}'.format(sha)
                change_author, errors = yield GitHub.email_for_owners(self.getProperty('owners', []))
                for error in errors:
                    yield self._addToLog('stdio', error)

            if not change_author:
                yield self._addToLog('stderr', 'Unable to determine email address for {} from metadata/contributors.json. Skipping sending email.'.format(self.getProperty('owners', [])))
                return

            builder_name = self.getProperty('buildername', '')
            bug_id = self.getProperty('bug_id', '') or pr_number
            title = self.getProperty('bug_title', '') or self.getProperty('github.title', '')
            worker_name = self.getProperty('workername', '')
            patch_author = self.getProperty('patch_author', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            test_names_string = ''
            for test_name in sorted(test_names):
                test_url = '{}{}'.format(LAYOUT_TESTS_URL, test_name)
                history_url = '{}?suite=layout-tests&test={}'.format(RESULTS_DB_URL, test_name)
                test_names_string += '\n- <a href="{}">{}</a> (<a href="{}">test history</a>)'.format(test_url, test_name, history_url)

            pluralSuffix = 's' if len(test_names) > 1 else ''
            email_subject = 'Layout test failure for {}: {}'.format(change_string, title)
            email_text = 'EWS has detected layout test failure{} on {}'.format(pluralSuffix, builder_name)
            if patch_id:
                email_text += ' while testing <a href="{}">{}</a>'.format(Bugzilla.patch_url(patch_id), change_string)
                email_text += ' for <a href="{}">Bug {}</a>.'.format(Bugzilla.bug_url(bug_id), bug_id)
            else:
                repository = self.getProperty('repository')
                email_text += ' while testing <a href="{}">{}</a>'.format(GitHub.commit_url(sha, repository), change_string)
                email_text += ' for <a href="{}">PR #{}</a>.'.format(GitHub.pr_url(pr_number, repository), pr_number)
            email_text += '\n\nFull details are available at: {}\n\nChange author: {}'.format(build_url, change_author)

            if exceed_failure_limit:
                email_text += '\n\nAditionally the failure limit has been exceeded, so the test suite has been terminated early. It is likely that there would be more failures than the ones listed below.'
            email_text += '\n\nLayout test failure{}:\n{}'.format(pluralSuffix, test_names_string)
            email_text += '\n\nTo unsubscribe from these notifications or to provide any feedback please email aakash_jain@apple.com'
            yield self._addToLog('stdio', 'Sending email notification to {}'.format(change_author))
            send_email_to_patch_author(change_author, email_subject, email_text, patch_id or self.getProperty('github.head.sha', ''))
        except Exception as e:
            print('Error in sending email for new layout test failures: {}'.format(e))

    def _report_flaky_tests(self, flaky_tests):
        # TODO: implement this
        pass

    @defer.inlineCallbacks
    def run(self):
        first_results_did_exceed_test_failure_limit = self.getProperty('first_results_exceed_failure_limit')
        first_results_failing_tests = set(self.getProperty('first_run_failures', []))
        second_results_did_exceed_test_failure_limit = self.getProperty('second_results_exceed_failure_limit')
        second_results_failing_tests = set(self.getProperty('second_run_failures', []))
        clean_tree_results_did_exceed_test_failure_limit = self.getProperty('clean_tree_results_exceed_failure_limit')
        clean_tree_results_failing_tests = set(self.getProperty('clean_tree_run_failures', []))
        flaky_failures = first_results_failing_tests.union(second_results_failing_tests) - first_results_failing_tests.intersection(second_results_failing_tests)
        num_flaky_failures = len(flaky_failures)
        flaky_failures_string = ', '.join(sorted(flaky_failures))

        if (not first_results_failing_tests) and (not second_results_failing_tests):
            # If we've made it here, then layout-tests and re-run-layout-tests failed, which means
            # there should have been some test failures. Otherwise there is some unexpected issue.
            clean_tree_run_status = self.getProperty('clean_tree_run_status', FAILURE)
            if clean_tree_run_status in [SUCCESS, WARNINGS]:
                rc = yield self.report_failure(set())
                return defer.returnValue(rc)
            self.send_email_for_infrastructure_issue('Both first and second layout-test runs with patch generated no list of results but exited with error, and the clean_tree without change retry also failed.')
            return defer.returnValue(self.retry_build('Unexpected infrastructure issue, retrying build'))

        if first_results_did_exceed_test_failure_limit and second_results_did_exceed_test_failure_limit:
            if (len(first_results_failing_tests) - len(clean_tree_results_failing_tests)) <= 5:
                # If we've made it here, then many tests are failing with the patch applied, but
                # if the clean tree is also failing many tests, even if it's not quite as many,
                # then we can't be certain that the discrepancy isn't due to flakiness, and hence we must defer judgement.
                return defer.returnValue(self.retry_build())
            rc = yield self.report_failure(first_results_failing_tests)
            return defer.returnValue(rc)

        if second_results_did_exceed_test_failure_limit:
            if clean_tree_results_did_exceed_test_failure_limit:
                return defer.returnValue(self.retry_build())
            failures_introduced_by_patch = first_results_failing_tests - clean_tree_results_failing_tests
            if failures_introduced_by_patch:
                rc = yield self.report_failure(failures_introduced_by_patch)
                return defer.returnValue(rc)
            return defer.returnValue(self.retry_build())

        if first_results_did_exceed_test_failure_limit:
            if clean_tree_results_did_exceed_test_failure_limit:
                return defer.returnValue(self.retry_build())
            failures_introduced_by_patch = second_results_failing_tests - clean_tree_results_failing_tests
            if failures_introduced_by_patch:
                rc = yield self.report_failure(failures_introduced_by_patch)
                return defer.returnValue(rc)
            return defer.returnValue(self.retry_build())

        # FIXME: Here it could be a good idea to also use the info of results.flaky_tests from the runs
        if self._results_failed_different_tests(first_results_failing_tests, second_results_failing_tests):
            tests_that_only_failed_first = first_results_failing_tests.difference(second_results_failing_tests)
            self._report_flaky_tests(tests_that_only_failed_first)

            tests_that_only_failed_second = second_results_failing_tests.difference(first_results_failing_tests)
            self._report_flaky_tests(tests_that_only_failed_second)

            tests_that_consistently_failed = first_results_failing_tests.intersection(second_results_failing_tests)
            if tests_that_consistently_failed:
                if clean_tree_results_did_exceed_test_failure_limit:
                    return defer.returnValue(self.retry_build())
                failures_introduced_by_patch = tests_that_consistently_failed - clean_tree_results_failing_tests
                if failures_introduced_by_patch:
                    rc = yield self.report_failure(failures_introduced_by_patch)
                    return defer.returnValue(rc)
            elif num_flaky_failures > 10:
                rc = yield self.report_failure(failure_message=f'Too many flaky failures: {flaky_failures_string}')
                return defer.returnValue(rc)

            # At this point we know that at least one test flaked, but no consistent failures
            # were introduced. This is a bit of a grey-zone. It's possible that the patch introduced some flakiness.
            # We still mark the build as SUCCESS.
            return defer.returnValue(self.report_pre_existing_failures(clean_tree_results_failing_tests, flaky_failures))

        if clean_tree_results_did_exceed_test_failure_limit:
            return defer.returnValue(self.retry_build())
        failures_introduced_by_patch = first_results_failing_tests - clean_tree_results_failing_tests
        if failures_introduced_by_patch:
            rc = yield self.report_failure(failures_introduced_by_patch)
            return defer.returnValue(rc)

        # At this point, we know that the first and second runs had the exact same failures,
        # and that those failures are all present on the clean tree, so we can say with certainty
        # that the patch is good.
        return defer.returnValue(self.report_pre_existing_failures(clean_tree_results_failing_tests, flaky_failures))


class RunWebKit1Tests(RunWebKitTests):
    def start(self):
        self.setProperty('use-dump-render-tree', True)
        return RunWebKitTests.start(self)


# This is a specialized class designed to cope with a tree that is not always green.
# It tries hard to avoid reporting any false positive, so it will only report new
# consistent failures (fail always with the patch and pass always without it).
class RunWebKitTestsRedTree(RunWebKitTests):
    EXIT_AFTER_FAILURES = 500

    def _did_command_timed_out(self, logHeadersText):
        timed_out_line_start = 'command timed out: {} seconds elapsed running'.format(self.MAX_SECONDS_STEP_RUN)
        timed_out_line_end = 'attempting to kill'
        for line in logHeadersText.splitlines():
            line = line.strip()
            if line.startswith(timed_out_line_start) and line.endswith(timed_out_line_end):
                return True
        return False

    def evaluateCommand(self, cmd):
        first_results_failing_tests = set(self.getProperty('first_run_failures', []))
        first_results_flaky_tests = set(self.getProperty('first_run_flakies', []))
        platform = self.getProperty('platform')
        rc = self.evaluateResult(cmd)
        steps_to_add = [
            GenerateS3URL(
                f"{self.getProperty('fullPlatform')}-{self.getProperty('architecture')}-{self.getProperty('configuration')}-{self.name}",
                extension='txt',
                content_type='text/plain',
            ), UploadFileToS3(
                'logs.txt',
                links={self.name: 'Full logs'},
                content_type='text/plain',
            )
        ]
        steps_to_add += [ArchiveTestResults(), UploadTestResults(), ExtractTestResults()]
        if first_results_failing_tests:
            steps_to_add.extend([ValidateChange(verifyBugClosed=False, addURLs=False), KillOldProcesses(), RunWebKitTestsRepeatFailuresRedTree()])
        elif first_results_flaky_tests:
            steps_to_add.append(AnalyzeLayoutTestsResultsRedTree())
        elif rc == SUCCESS or rc == WARNINGS:
            steps_to_add = None
            message = 'Passed layout tests'
            self.descriptionDone = message
            self.build.results = SUCCESS
            self.setProperty('build_summary', message)
        else:
            # We have a failure return code but not a list of failed or flaky tests, so we can't run the repeat steps.
            # If we are on the last retry then run the whole layout tests without patch.
            # If not, then go to analyze-layout-tests-results where we will retry everything hoping this was a random failure.
            retry_count = int(self.getProperty('retry_count', 0))
            if retry_count < AnalyzeLayoutTestsResultsRedTree.MAX_RETRY:
                steps_to_add.append(AnalyzeLayoutTestsResultsRedTree())
            else:
                steps_to_add.extend([RevertAppliedChanges(), CleanWorkingDirectory()])
                if platform == 'wpe':
                    steps_to_add.append(InstallWpeDependencies())
                elif platform == 'gtk':
                    steps_to_add.append(InstallGtkDependencies())
                steps_to_add.extend([
                    CompileWebKitWithoutChange(retry_build_on_failure=True),
                    ValidateChange(verifyBugClosed=False, addURLs=False),
                    RunWebKitTestsWithoutChangeRedTree(),
                ])
        if steps_to_add:
            self.build.addStepsAfterCurrentStep(steps_to_add)
        return rc


class RunWebKitTestsRepeatFailuresRedTree(RunWebKitTestsRedTree):
    name = 'layout-tests-repeat-failures'
    NUM_REPEATS_PER_TEST = 10
    EXIT_AFTER_FAILURES = None
    MAX_SECONDS_STEP_RUN = 18000  # 5h

    def __init__(self, **kwargs):
        super().__init__(maxTime=self.MAX_SECONDS_STEP_RUN, **kwargs)

    def setLayoutTestCommand(self):
        super().setLayoutTestCommand()
        # On the repeat steps we don't enable coredump generation (makes the run much slower if there are crashes)
        self.setCommand([arg for arg in self.command if arg != '--enable-core-dumps-nolimit'])
        first_results_failing_tests = set(self.getProperty('first_run_failures', []))
        self.setCommand(self.command + ['--fully-parallel', '--repeat-each=%s' % self.NUM_REPEATS_PER_TEST] + sorted(first_results_failing_tests))

    def evaluateCommand(self, cmd):
        with_change_repeat_failures_results_nonflaky_failures = set(self.getProperty('with_change_repeat_failures_results_nonflaky_failures', []))
        with_change_repeat_failures_results_flakies = set(self.getProperty('with_change_repeat_failures_results_flakies', []))
        with_change_repeat_failures_timedout = self.getProperty('with_change_repeat_failures_timedout', False)
        first_results_flaky_tests = set(self.getProperty('first_run_flakies', []))
        platform = self.getProperty('platform')
        rc = self.evaluateResult(cmd)
        self.setProperty('with_change_repeat_failures_retcode', rc)
        steps_to_add = [
            GenerateS3URL(
                f"{self.getProperty('fullPlatform')}-{self.getProperty('architecture')}-{self.getProperty('configuration')}-{self.name}",
                extension='txt',
                content_type='text/plain',
            ), UploadFileToS3(
                'logs.txt',
                links={self.name: 'Full logs'},
                content_type='text/plain',
            )
        ]
        steps_to_add += [ArchiveTestResults(), UploadTestResults(identifier='repeat-failures'), ExtractTestResults(identifier='repeat-failures')]
        if with_change_repeat_failures_results_nonflaky_failures or with_change_repeat_failures_timedout:
            steps_to_add.extend([
                ValidateChange(verifyBugClosed=False, addURLs=False),
                KillOldProcesses(),
                RevertAppliedChanges(),
                CleanWorkingDirectory(),
            ])
            if platform == 'wpe':
                steps_to_add.append(InstallWpeDependencies())
            elif platform == 'gtk':
                steps_to_add.append(InstallGtkDependencies())
            steps_to_add.extend([
                CompileWebKitWithoutChange(retry_build_on_failure=True),
                ValidateChange(verifyBugClosed=False, addURLs=False),
                RunWebKitTestsRepeatFailuresWithoutChangeRedTree(),
            ])
        else:
            steps_to_add.append(AnalyzeLayoutTestsResultsRedTree())
        if steps_to_add:
            self.build.addStepsAfterCurrentStep(steps_to_add)
        return rc

    @defer.inlineCallbacks
    def runCommand(self, command):
        yield shell.Test.runCommand(self, command)
        logText = self.log_observer.getStdout() + self.log_observer.getStderr()
        logTextJson = self.log_observer_json.getStdout()
        with_change_repeat_failures_results = LayoutTestFailures.results_from_string(logTextJson)
        if with_change_repeat_failures_results:
            self.setProperty('with_change_repeat_failures_results_exceed_failure_limit', with_change_repeat_failures_results.did_exceed_test_failure_limit)
            self.setProperty('with_change_repeat_failures_results_nonflaky_failures', sorted(with_change_repeat_failures_results.failing_tests))
            self.setProperty('with_change_repeat_failures_results_flakies', sorted(with_change_repeat_failures_results.flaky_tests))
            if with_change_repeat_failures_results.failing_tests:
                self._addToLog(self.test_failures_log_name, '\n'.join(with_change_repeat_failures_results.failing_tests))
        command_timedout = self._did_command_timed_out(self.log_observer.getHeaders())
        self.setProperty('with_change_repeat_failures_timedout', command_timedout)
        self._parseRunWebKitTestsOutput(logText)

    def start(self):
        # buildbot messages about timeout reached appear on the header stream of BufferLog
        return super().start(BufferLogObserverClass=BufferLogHeaderObserver)


class RunWebKitTestsRepeatFailuresWithoutChangeRedTree(RunWebKitTestsRedTree):
    name = 'layout-tests-repeat-failures-without-change'
    NUM_REPEATS_PER_TEST = 10
    EXIT_AFTER_FAILURES = None
    MAX_SECONDS_STEP_RUN = 10800  # 3h

    def __init__(self, **kwargs):
        super().__init__(maxTime=self.MAX_SECONDS_STEP_RUN, **kwargs)

    def setLayoutTestCommand(self):
        super().setLayoutTestCommand()
        # On the repeat steps we don't enable coredump generation (makes the run much slower if there are crashes)
        self.setCommand([arg for arg in self.command if arg != '--enable-core-dumps-nolimit'])
        with_change_nonflaky_failures = set(self.getProperty('with_change_repeat_failures_results_nonflaky_failures', []))
        first_run_failures = set(self.getProperty('first_run_failures', []))
        with_change_repeat_failures_timedout = self.getProperty('with_change_repeat_failures_timedout', False)
        failures_to_repeat = first_run_failures if with_change_repeat_failures_timedout else with_change_nonflaky_failures
        # Pass '--skipped=always' to ensure that any test passed via command line arguments
        # is skipped anyways if is marked as such on the Expectation files or if is marked
        # as failure (since we are passing also '--skip-failing-tests'). That way we ensure
        # to report the case of a change removing an expectation that still fails with it.
        self.setCommand(self.command + ['--fully-parallel', '--repeat-each=%s' % self.NUM_REPEATS_PER_TEST, '--skipped=always'] + sorted(failures_to_repeat))

    def evaluateCommand(self, cmd):
        rc = self.evaluateResult(cmd)
        self.setProperty('without_change_repeat_failures_retcode', rc)
        steps_to_add = [
            GenerateS3URL(
                f"{self.getProperty('fullPlatform')}-{self.getProperty('architecture')}-{self.getProperty('configuration')}-{self.name}",
                extension='txt',
                content_type='text/plain',
            ), UploadFileToS3(
                'logs.txt',
                links={self.name: 'Full logs'},
                content_type='text/plain',
            )
        ]
        steps_to_add += [ArchiveTestResults(), UploadTestResults(identifier='repeat-failures-without-change'), ExtractTestResults(identifier='repeat-failures-without-change'), AnalyzeLayoutTestsResultsRedTree()]
        self.build.addStepsAfterCurrentStep(steps_to_add)
        return rc

    @defer.inlineCallbacks
    def runCommand(self, command):
        yield shell.Test.runCommand(self, command)
        logText = self.log_observer.getStdout() + self.log_observer.getStderr()
        logTextJson = self.log_observer_json.getStdout()
        without_change_repeat_failures_results = LayoutTestFailures.results_from_string(logTextJson)
        if without_change_repeat_failures_results:
            self.setProperty('without_change_repeat_failures_results_exceed_failure_limit', without_change_repeat_failures_results.did_exceed_test_failure_limit)
            self.setProperty('without_change_repeat_failures_results_nonflaky_failures', sorted(without_change_repeat_failures_results.failing_tests))
            self.setProperty('without_change_repeat_failures_results_flakies', sorted(without_change_repeat_failures_results.flaky_tests))
            if without_change_repeat_failures_results.failing_tests:
                self._addToLog(self.test_failures_log_name, '\n'.join(without_change_repeat_failures_results.failing_tests))
        command_timedout = self._did_command_timed_out(self.log_observer.getHeaders())
        self.setProperty('without_change_repeat_failures_timedout', command_timedout)
        self._parseRunWebKitTestsOutput(logText)

    def start(self):
        # buildbot messages about timeout reached appear on the header stream of BufferLog
        return super().start(BufferLogObserverClass=BufferLogHeaderObserver)


class RunWebKitTestsWithoutChangeRedTree(RunWebKitTestsWithoutChange):
    EXIT_AFTER_FAILURES = 500

    def evaluateCommand(self, cmd):
        rc = shell.Test.evaluateCommand(self, cmd)
        steps_to_add = [
            GenerateS3URL(
                f"{self.getProperty('fullPlatform')}-{self.getProperty('architecture')}-{self.getProperty('configuration')}-{self.name}",
                extension='txt',
                content_type='text/plain',
            ), UploadFileToS3(
                'logs.txt',
                links={self.name: 'Full logs'},
                content_type='text/plain',
            )
        ]
        steps_to_add += [ArchiveTestResults(), UploadTestResults(identifier='clean-tree'), ExtractTestResults(identifier='clean-tree'), AnalyzeLayoutTestsResultsRedTree()]
        self.build.addStepsAfterCurrentStep(steps_to_add)
        self.setProperty('clean_tree_run_status', rc)
        return rc


class AnalyzeLayoutTestsResultsRedTree(AnalyzeLayoutTestsResults):
    MAX_RETRY = 3

    def report_success(self):
        self.build.results = SUCCESS
        self.descriptionDone = 'Passed layout tests'
        message = ''
        self.setProperty('build_summary', message)
        return SUCCESS

    def report_warning(self, message):
        self.build.results = WARNINGS
        self.descriptionDone = message
        self.setProperty('build_summary', message)
        return WARNINGS

    def report_infrastructure_issue_and_maybe_retry_build(self, message):
        retry_count = int(self.getProperty('retry_count', 0))
        if retry_count >= self.MAX_RETRY:
            message += '\nReached the maximum number of retries ({}). Unable to determine if change is bad or there is a pre-existent infrastructure issue.'.format(self.MAX_RETRY)
            self.send_email_for_infrastructure_issue(message)
            return self.report_warning(message)
        message += "\nRetrying build [retry count is {} of {}]".format(retry_count, self.MAX_RETRY)
        self.setProperty('retry_count', retry_count + 1)
        self.send_email_for_infrastructure_issue(message)
        return self.retry_build(message='Unexpected infrastructure issue: {}'.format(message))

    def send_email_for_pre_existent_failures(self, test_names):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            number_failures = len(test_names)
            pluralSuffix = 's' if number_failures > 1 else ''

            email_subject = 'Info about {} pre-existent failure{} at {}'.format(number_failures, pluralSuffix, builder_name)
            email_text = 'Info about pre-existent (non-flaky) test failure{} at EWS:\n'.format(pluralSuffix)
            email_text += '  - Build : {}\n'.format(build_url)
            email_text += '  - Builder : {}\n'.format(builder_name)
            email_text += '  - Worker : {}\n'.format(worker_name)
            for test_name in sorted(test_names):
                test_url = '{}{}'.format(LAYOUT_TESTS_URL, test_name)
                history_url = '{}?suite=layout-tests&test={}'.format(RESULTS_DB_URL, test_name)
                email_text += '\n- <a href="{}">{}</a> (<a href="{}">test history</a>)'.format(test_url, test_name, history_url)
            send_email_to_bot_watchers(email_subject, email_text, builder_name, 'preexisting-{}'.format(test_name))
        except Exception as e:
            print('Error in sending email for flaky failure: {}'.format(e))

    def send_email_for_flaky_failures_and_steps(self, test_names_steps_dict):
        try:
            builder_name = self.getProperty('buildername', '')
            worker_name = self.getProperty('workername', '')
            build_url = '{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, self.build._builderid, self.build.number)
            number_failures = len(test_names_steps_dict)
            pluralSuffix = 's' if number_failures > 1 else ''
            email_subject = 'Info about {} flaky failure{} at {}'.format(number_failures, pluralSuffix, builder_name)
            email_text = 'Info about {} flaky test failure{} at EWS:\n'.format(number_failures, pluralSuffix)
            email_text += '  - Build : {}\n'.format(build_url)
            email_text += '  - Builder : {}\n'.format(builder_name)
            email_text += '  - Worker : {}\n'.format(worker_name)
            flaky_number = 0
            for test_name in test_names_steps_dict:
                flaky_number += 1
                test_url = '{}{}'.format(LAYOUT_TESTS_URL, test_name)
                history_url = '{}?suite=layout-tests&test={}'.format(RESULTS_DB_URL, test_name)
                number_steps_flaky = len(test_names_steps_dict[test_name])
                pluralstepSuffix = 's' if number_steps_flaky > 1 else ''
                step_names_str = '"{}"'.format('", "'.join(test_names_steps_dict[test_name]))
                email_text += '\nFlaky #{}\n  - Test name: <a href="{}">{}</a>\n  - Flaky on step{}: {}\n  - History: {}\n'.format(flaky_number, test_url, test_name, pluralstepSuffix, step_names_str, history_url)
            send_email_to_bot_watchers(email_subject, email_text, builder_name, 'flaky-{}'.format(worker_name))
        except Exception as e:
            print('Error in sending email for flaky failure: {}'.format(e))

    @defer.inlineCallbacks
    def run(self):
        # Run with change, running the whole layout test suite
        first_results_exceed_failure_limit = self.getProperty('first_results_exceed_failure_limit', False)
        first_run_failures = set(self.getProperty('first_run_failures', []))
        first_run_flakies = set(self.getProperty('first_run_flakies', []))

        # Run with change, running first_run_failures 10 times each test
        with_change_repeat_failures_results_exceed_failure_limit = self.getProperty('with_change_repeat_failures_results_exceed_failure_limit', False)
        with_change_repeat_failures_results_nonflaky_failures = set(self.getProperty('with_change_repeat_failures_results_nonflaky_failures', []))
        with_change_repeat_failures_results_flakies = set(self.getProperty('with_change_repeat_failures_results_flakies', []))
        with_change_repeat_failures_timedout = self.getProperty('with_change_repeat_failures_timedout', False)

        # Run without change, running with_change_repeat_failures_results_nonflaky_failures 10 times each test
        without_change_repeat_failures_results_exceed_failure_limit = self.getProperty('without_change_repeat_failures_results_exceed_failure_limit', False)
        without_change_repeat_failures_results_nonflaky_failures = set(self.getProperty('without_change_repeat_failures_results_nonflaky_failures', []))
        without_change_repeat_failures_results_flakies = set(self.getProperty('without_change_repeat_failures_results_flakies', []))
        without_change_repeat_failures_timedout = self.getProperty('without_change_repeat_failures_timedout', False)

        # If we've made it here that means that the first_run failed (non-zero status) but we don't have a list of failures or flakies. That is not expected.
        if (not first_run_failures) and (not first_run_flakies):
            # If we are not on the last retry, then try to retry the whole testing with the hope it was a random infrastructure error.
            retry_count = int(self.getProperty('retry_count', 0))
            if retry_count < self.MAX_RETRY:
                return defer.returnValue(self.report_infrastructure_issue_and_maybe_retry_build('The layout-test run with change generated no list of results and exited with error, retrying with the hope it was a random infrastructure error.'))
            # Otherwise (last retry) report and error or a warning, since we already gave it enough retries for the issue to not be caused by a random infrastructure error.
            # The clean tree run that only happens when the first run gives an error code without generating a list of failures or flakies.
            clean_tree_run_failures = set(self.getProperty('clean_tree_run_failures', []))
            clean_tree_run_flakies = set(self.getProperty('clean_tree_run_flakies', []))
            clean_tree_run_status = self.getProperty('clean_tree_run_status', FAILURE)
            # If the clean-tree run generated some results then we assume this change broke the script run-webkit-tests or something like that.
            if (clean_tree_run_status in [SUCCESS, WARNINGS]) or clean_tree_run_failures or clean_tree_run_flakies:
                rc = yield self.report_failure(set(), first_results_exceed_failure_limit)
                return defer.returnValue(rc)
            # This will end the testing as retry_count will be now self.MAX_RETRY and a warning will be reported.
            return defer.returnValue(self.report_infrastructure_issue_and_maybe_retry_build('The layout-test run with change generated no list of results and exited with error, and the clean_tree without change run did the same thing.'))

        if with_change_repeat_failures_results_exceed_failure_limit or without_change_repeat_failures_results_exceed_failure_limit:
            return defer.returnValue(self.report_infrastructure_issue_and_maybe_retry_build('One of the steps for retrying the failed tests has exited early, but this steps should run without "--exit-after-n-failures" switch, so they should not exit early.'))

        if without_change_repeat_failures_timedout:
            return defer.returnValue(self.report_infrastructure_issue_and_maybe_retry_build('The step "layout-tests-repeat-failures-without-change" was interrumped because it reached the timeout.'))

        if with_change_repeat_failures_timedout:
            # The change is causing the step 'layout-tests-repeat-failures-with-change' to timeout, likely the change is adding many failures or long timeouts needing lot of time to test the repeats.
            # Report the tests that failed on the first run as we don't have the information of the ones that failed on 'layout-tests-repeat-failures-with-change' because it was interrupted due to the timeout.
            # There is no point in repeating this run, it would happen the same on next runs and consume lot of time.
            likely_new_non_flaky_failures = first_run_failures - without_change_repeat_failures_results_nonflaky_failures.union(without_change_repeat_failures_results_flakies)
            self.send_email_for_infrastructure_issue('The step "layout-tests-repeat-failures-with-change" reached the timeout but the step "layout-tests-repeat-failures-without-change" ended. Not trying to repeat this. Reporting {} failures from the first run.'.format(len(likely_new_non_flaky_failures)))
            rc = yield self.report_failure(likely_new_non_flaky_failures, first_results_exceed_failure_limit)
            return defer.returnValue(rc)

        # The checks below need to be after the timeout ones (above) because when a timeout is trigerred no results will be generated for the step.
        # The step with_change_repeat_failures generated no list of failures or flakies, which should only happen when the return code of the step is SUCESS or WARNINGS.
        if not with_change_repeat_failures_results_nonflaky_failures and not with_change_repeat_failures_results_flakies:
            with_change_repeat_failures_retcode = self.getProperty('with_change_repeat_failures_retcode', FAILURE)
            if with_change_repeat_failures_retcode not in [SUCCESS, WARNINGS]:
                return defer.returnValue(self.report_infrastructure_issue_and_maybe_retry_build('The step "layout-tests-repeat-failures" failed to generate any list of failures or flakies and returned an error code.'))

        # Check the same for the step without_change_repeat_failures
        if not without_change_repeat_failures_results_nonflaky_failures and not without_change_repeat_failures_results_flakies:
            without_change_repeat_failures_retcode = self.getProperty('without_change_repeat_failures_retcode', FAILURE)
            if without_change_repeat_failures_retcode not in [SUCCESS, WARNINGS]:
                return defer.returnValue(self.report_infrastructure_issue_and_maybe_retry_build('The step "layout-tests-repeat-failures-without-change" failed to generate any list of failures or flakies and returned an error code.'))

        # Warn EWS bot watchers about flakies so they can garden those. Include the step where the flaky was found in the e-mail to know if it was found with change or without it.
        # Due to the way this class works most of the flakies are filtered on the step with change even when those were pre-existent issues (so this is also useful for bot watchers).
        all_flaky_failures = first_run_flakies.union(with_change_repeat_failures_results_flakies).union(without_change_repeat_failures_results_flakies)
        flaky_steps_dict = {}
        for flaky_failure in all_flaky_failures:
            step_names = []
            if flaky_failure in without_change_repeat_failures_results_flakies:
                step_names.append('layout-tests-repeat-failures-without-change')
            if flaky_failure in with_change_repeat_failures_results_flakies:
                step_names.append('layout-tests-repeat-failures (with change)')
            if flaky_failure in first_run_flakies:
                step_names.append('layout-tests (with change)')
            flaky_steps_dict[flaky_failure] = step_names
        self.send_email_for_flaky_failures_and_steps(flaky_steps_dict)

        # Warn EWS bot watchers about pre-existent non-flaky failures (if any), but send only one e-mail with all the tests to avoid sending too much e-mails.
        pre_existent_non_flaky_failures = without_change_repeat_failures_results_nonflaky_failures - all_flaky_failures
        if pre_existent_non_flaky_failures:
            self.send_email_for_pre_existent_failures(pre_existent_non_flaky_failures)

        # Finally check if there are new consistent (non-flaky) failures caused by the change and warn the change author stetting the status for the build.
        new_non_flaky_failures = with_change_repeat_failures_results_nonflaky_failures - without_change_repeat_failures_results_nonflaky_failures.union(without_change_repeat_failures_results_flakies)
        if new_non_flaky_failures:
            rc = yield self.report_failure(new_non_flaky_failures, first_results_exceed_failure_limit)
            return defer.returnValue(rc)

        return defer.returnValue(self.report_success())


class ArchiveBuiltProduct(shell.ShellCommandNewStyle):
    command = ['python3', 'Tools/CISupport/built-product-archive',
               WithProperties('--platform=%(fullPlatform)s'), WithProperties('--%(configuration)s'), 'archive']
    name = 'archive-built-product'
    description = ['archiving built product']
    descriptionDone = ['Archived built product']
    haltOnFailure = True

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, **kwargs)


class UploadBuiltProduct(transfer.FileUpload):
    name = 'upload-built-product'
    workersrc = WithProperties('WebKitBuild/%(configuration)s.zip')
    masterdest = WithProperties('public_html/archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(change_id)s.zip')
    descriptionDone = ['Uploaded built product']
    haltOnFailure = True

    def __init__(self, **kwargs):
        kwargs['workersrc'] = self.workersrc
        kwargs['masterdest'] = self.masterdest
        kwargs['mode'] = 0o0644
        kwargs['blocksize'] = 1024 * 256
        super().__init__(**kwargs)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to upload built product'}
        return super().getResultSummary()


class UploadFileToS3(shell.ShellCommandNewStyle, AddToLogMixin):
    name = 'upload-file-to-s3'
    descriptionDone = name
    haltOnFailure = True
    flunkOnFailure = True

    def __init__(self, file, links=None, content_type=None, **kwargs):
        super().__init__(timeout=31 * 60, logEnviron=False, **kwargs)
        self.file = file
        self.links = links or dict()
        self.content_type = content_type

    def getLastBuildStepByName(self, name):
        for step in reversed(self.build.executedSteps):
            if name in step.name:
                return step
        return None

    @defer.inlineCallbacks
    def run(self):
        s3url = self.build.s3url
        if not s3url:
            rc = FAILURE
            yield self._addToLog('stdio', f'Failed to get s3url: {s3url}')
            return defer.returnValue(rc)

        self.env = dict(UPLOAD_URL=s3url)

        self.command = [
            'python3', 'Tools/Scripts/upload-file-to-url',
            '--filename', self.file,
        ]
        if self.content_type:
            self.command += ['--content-type', self.content_type]

        rc = yield super().run()

        if rc in [SUCCESS, WARNINGS] and getattr(self.build, 's3_archives', None):
            for step_name, message in self.links.items():
                step = self.getLastBuildStepByName(step_name)
                if not step:
                    continue
                step.addURL(message, self.build.s3_archives[-1])

        return defer.returnValue(rc)

    def doStepIf(self, step):
        return CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES + TESTING_ENVIRONMENT_HOSTNAMES

    def getResultSummary(self):
        if self.results == FAILURE:
            return {'step': f'Failed to upload {self.file} to S3. Please inform an admin.'}
        if self.results == SKIPPED:
            return {'step': 'Skipped upload to S3'}
        if self.results in [SUCCESS, WARNINGS]:
            return {'step': f'Uploaded {self.file} to S3'}
        return super().getResultSummary()


class GenerateS3URL(master.MasterShellCommandNewStyle):
    name = 'generate-s3-url'
    descriptionDone = ['Generated S3 URL']
    haltOnFailure = False
    flunkOnFailure = False

    def __init__(self, identifier, extension='zip', content_type=None, **kwargs):
        self.identifier = identifier
        self.extension = extension
        kwargs['command'] = [
            'python3', '../Shared/generate-s3-url',
            '--change-id', WithProperties('%(change_id)s'),
            '--identifier', self.identifier,
        ]
        if extension:
            kwargs['command'] += ['--extension', extension]
        if content_type:
            kwargs['command'] += ['--content-type', content_type]
        super().__init__(logEnviron=False, **kwargs)

    @defer.inlineCallbacks
    def run(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

        rc = yield super().run()

        self.build.s3url = ''
        if not getattr(self.build, 's3_archives', None):
            self.build.s3_archives = []

        log_text = self.log_observer.getStdout() + self.log_observer.getStderr()
        match = re.search(r'S3 URL: (?P<url>[^\s]+)', log_text)
        # Sample log: S3 URL: https://s3-us-west-2.amazonaws.com/ews-archives.webkit.org/ios-simulator-12-x86_64-release/123456.zip

        build_url = f'{self.master.config.buildbotURL}#/builders/{self.build._builderid}/builds/{self.build.number}'
        if match:
            self.build.s3url = match.group('url')
            self.build.s3_archives.append(S3URL + f"{S3_BUCKET}/{self.identifier}/{self.getProperty('change_id')}.{self.extension}")
            defer.returnValue(rc)
        else:
            print(f'build: {build_url}, logs for GenerateS3URL:\n{log_text}')
            defer.returnValue(FAILURE)

    def hideStepIf(self, results, step):
        return results == SUCCESS

    def doStepIf(self, step):
        return CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES + TESTING_ENVIRONMENT_HOSTNAMES

    def getResultSummary(self):
        if self.results == FAILURE:
            return {'step': 'Failed to generate S3 URL'}
        return super().getResultSummary()


class TransferToS3(master.MasterShellCommandNewStyle):
    name = 'transfer-to-s3'
    description = ['transferring to s3']
    descriptionDone = ['Transferred archive to S3']
    archive = WithProperties('public_html/archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(change_id)s.zip')
    identifier = WithProperties('%(fullPlatform)s-%(architecture)s-%(configuration)s')
    change_id = WithProperties('%(change_id)s')
    command = ['python3', '../Shared/transfer-archive-to-s3', '--change-id', change_id, '--identifier', identifier, '--archive', archive]
    haltOnFailure = False
    flunkOnFailure = False

    def __init__(self, **kwargs):
        kwargs['command'] = self.command
        super().__init__(logEnviron=False, **kwargs)

    @defer.inlineCallbacks
    def run(self):
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

        rc = yield super().run()

        log_text = self.log_observer.getStdout() + self.log_observer.getStderr()
        match = re.search(r'S3 URL: (?P<url>[^\s]+)', log_text)
        # Sample log: S3 URL: https://s3-us-west-2.amazonaws.com/ews-archives.webkit.org/ios-simulator-12-x86_64-release/123456.zip
        if match:
            self.addURL('uploaded archive', match.group('url'))

        defer.returnValue(rc)

    def doStepIf(self, step):
        return CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES + TESTING_ENVIRONMENT_HOSTNAMES

    def hideStepIf(self, results, step):
        return results == SUCCESS and self.getProperty('sensitive', False)

    def getResultSummary(self):
        if self.results == FAILURE:
            return {'step': 'Failed to transfer archive to S3'}
        return super().getResultSummary()


class DownloadBuiltProduct(shell.ShellCommand):
    command = [
        'python3', 'Tools/CISupport/download-built-product',
        WithProperties('--%(configuration)s'),
        WithProperties(S3URL + S3_BUCKET + '/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(change_id)s.zip'),
    ]
    name = 'download-built-product'
    description = ['downloading built product']
    descriptionDone = ['Downloaded built product']
    haltOnFailure = True
    flunkOnFailure = True

    def getResultSummary(self):
        if self.results not in [SUCCESS, SKIPPED]:
            return {'step': 'Failed to download built product from S3'}
        return super().getResultSummary()

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, **kwargs)

    def start(self):
        # Only try to download from S3 on the official deployments <https://webkit.org/b/230006>
        if CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES + TESTING_ENVIRONMENT_HOSTNAMES:
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
    mastersrc = WithProperties('public_html/archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(change_id)s.zip')
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
        super().__init__(**kwargs)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to download built product from build master'}
        return super().getResultSummary()


class ExtractBuiltProduct(shell.ShellCommandNewStyle):
    command = ['python3', 'Tools/CISupport/built-product-archive',
               WithProperties('--platform=%(fullPlatform)s'), WithProperties('--%(configuration)s'), 'extract']
    name = 'extract-built-product'
    description = ['extracting built product']
    descriptionDone = ['Extracted built product']
    haltOnFailure = True
    flunkOnFailure = True

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, **kwargs)


class RunAPITests(shell.TestNewStyle, AddToLogMixin, ShellMixin):
    name = 'run-api-tests'
    description = ['api tests running']
    descriptionDone = ['api-tests']
    jsonFileName = 'api_test_results.json'
    logfiles = {'json': jsonFileName}
    test_failures_log_name = 'test-failures'
    results_db_log_name = 'results-db'
    suffix = 'first_run'
    command = ['python3', 'Tools/Scripts/run-api-tests', '--no-build',
               WithProperties('--%(configuration)s'), '--verbose', '--json-output={0}'.format(jsonFileName)]
    failedTestsFormatString = '%d api test%s failed or timed out'
    failedTestCount = 0
    cancelled_due_to_huge_logs = False
    line_count = 0
    THRESHOLD_FOR_EXCESSIVE_LOGS_API_TESTS = 100000
    MSG_FOR_EXCESSIVE_LOGS_API_TEST = f'Stopped due to excessive logging, limit: {THRESHOLD_FOR_EXCESSIVE_LOGS_API_TESTS}'

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, timeout=3 * 60 * 60, **kwargs)
        self.failing_tests_filtered = []
        self.preexisting_failures_in_results_db = []
        self.steps_to_add = []

    @defer.inlineCallbacks
    def run(self):
        self.log_observer_json = logobserver.BufferLogObserver()
        self.addLogObserver('json', self.log_observer_json)

        self.log_observer = ParseByLineLogObserver(self.parseOutputLine)
        self.addLogObserver('stdio', self.log_observer)

        platform = self.getProperty('platform')
        if platform in ['gtk', 'wpe']:
            self.command = ['python3', f'Tools/Scripts/run-{platform}-tests',
                           '--{0}'.format(self.getProperty('configuration')),
                           '--json-output={0}'.format(self.jsonFileName)]
        else:
            self.command = self.command + customBuildFlag(platform, self.getProperty('fullPlatform'))
        self.command = self.shell_command(' '.join(self.command) + ' > logs.txt 2>&1 ; grep "Ran " logs.txt')

        rc = yield super().run()

        if self.failedTestCount:
            rc = FAILURE

        yield self.analyze_failures_using_results_db()

        self.steps_to_add += [
            GenerateS3URL(
                f"{self.getProperty('fullPlatform')}-{self.getProperty('architecture')}-{self.getProperty('configuration')}-{self.name}",
                extension='txt',
                content_type='text/plain',
            ), UploadFileToS3(
                'logs.txt',
                links={self.name: 'Full logs'},
                content_type='text/plain',
            )
        ]

        if rc in [SUCCESS, WARNINGS]:
            message = 'Passed API tests'
            if self.name == ReRunAPITests.name:
                first_results_failing_tests = self.getProperty('first_run_failures', [])
                flaky_failures_string = ', '.join(first_results_failing_tests)
                pluralSuffix = 's' if len(first_results_failing_tests) > 1 else ''
                message = f'Found flaky test{pluralSuffix}: {flaky_failures_string}'

            self.descriptionDone = message
            if self.name != RunAPITestsWithoutChange.name:
                self.build.results = SUCCESS
                self.setProperty('build_summary', message)
        elif (self.name != RunAPITestsWithoutChange.name and self.preexisting_failures_in_results_db and len(self.failing_tests_filtered) == 0):
            # This means all the tests which failed in this run were also failing or flaky in results database
            message = f"Ignored pre-existing failure: {', '.join(self.preexisting_failures_in_results_db)}"
            self.descriptionDone = message
            self.build.results = SUCCESS
            self.setProperty('build_summary', message)
        else:
            self.doOnFailure()

        self.build.addStepsAfterCurrentStep(self.steps_to_add)
        defer.returnValue(rc)

    def parseOutputLine(self, line):
        self.line_count += 1
        if self.line_count == self.THRESHOLD_FOR_EXCESSIVE_LOGS_API_TESTS:
            self.handleExcessiveLogging()
            return

        match = re.search(r'Ran (?P<ran>\d+) tests of (?P<total>\d+) with (?P<passed>\d+) successful', line)
        if match:
            self.failedTestCount = int(match.group('ran')) - int(match.group('passed'))

    def handleExcessiveLogging(self):
        build_url = f'{self.master.config.buildbotURL}#/builders/{self.build._builderid}/builds/{self.build.number}'
        print(f'\n{self.MSG_FOR_EXCESSIVE_LOGS_API_TEST}, {build_url}\n')
        self.cancelled_due_to_huge_logs = True
        self.build.stopBuild(reason=self.MSG_FOR_EXCESSIVE_LOGS_API_TEST, results=FAILURE)
        self.build.buildFinished([self.MSG_FOR_EXCESSIVE_LOGS_API_TEST], FAILURE)

    def getResultSummary(self):
        if self.cancelled_due_to_huge_logs:
            return {'step': MSG_FOR_EXCESSIVE_LOGS, 'build': MSG_FOR_EXCESSIVE_LOGS}

        status = self.name
        if self.results != SUCCESS:
            if self.failedTestCount:
                self.failedTestPluralSuffix = '' if self.failedTestCount == 1 else 's'
                status = self.failedTestsFormatString % (self.failedTestCount, self.failedTestPluralSuffix)
            else:
                status += ' ({})'.format(Results[self.results])

        return {'step': status}

    @defer.inlineCallbacks
    def analyze_failures_using_results_db(self):
        logTextJson = self.log_observer_json.getStdout()

        failures = self.parse_api_failures_from_string(logTextJson)
        self.setProperty(f'{self.suffix}_failures', sorted(failures))
        if failures:
            yield self._addToLog(self.test_failures_log_name, '\n'.join(failures))
            yield self.filter_api_test_failures_using_results_db(failures)
            self.setProperty(f'{self.suffix}_failures_filtered', sorted(self.failing_tests_filtered))
            self.setProperty(f'results-db_{self.suffix}_pre_existing', sorted(self.preexisting_failures_in_results_db))

    def doOnFailure(self):
        self.steps_to_add += [
            ValidateChange(verifyBugClosed=False, addURLs=False),
            KillOldProcesses(),
            ReRunAPITests(),
        ]

    def parse_api_failures_from_string(self, string):
        if not string:
            return []
        try:
            # Workaround for https://github.com/buildbot/buildbot/issues/4906
            string = ''.join(string.splitlines())
            result = json.loads(string)
        except json.decoder.JSONDecodeError:
            # Retry after ensuring that json has proper ending
            print(f'Encountered error while parsing api-tests json, retrying after ensuring that json has proper ending.')
            if string[-1] != '}':
                string += '}'
            try:
                result = json.loads(string)
            except Exception as e:
                print(f'ERROR: unable to parse data, exception: {e}')
                return []
        except Exception as e:
            print(f'ERROR: unexcepted error while parsing data: {e}')
            return []

        failures = ([failure.get('name') for failure in result.get('Timedout', [])] +
                    [failure.get('name') for failure in result.get('Crashed', [])] +
                    [failure.get('name') for failure in result.get('Failed', [])])
        return failures

    @defer.inlineCallbacks
    def filter_api_test_failures_using_results_db(self, failing_tests):
        self.failing_tests_filtered = failing_tests.copy()
        identifier = self.getProperty('identifier', None)
        platform = self.getProperty('platform', None)
        configuration = {}
        if platform:
            configuration['platform'] = platform
        style = self.getProperty('configuration', None)
        if style and style in ['debug', 'release']:
            configuration['style'] = style

        yield self._addToLog(self.results_db_log_name, f'Checking Results database for failing tests. Identifier: {identifier}, configuration: {configuration}')
        has_commit = False
        if failing_tests and identifier:
            has_commit = yield ResultsDatabase.has_commit(commit=identifier)
            if not has_commit:
                yield self._addToLog(self.results_db_log_name, f"'{identifier}' could not be found on the results database, falling back to tip-of-tree\n")

        for test in failing_tests:
            data = yield ResultsDatabase.is_test_pre_existing_failure(
                test, configuration=configuration,
                commit=identifier if has_commit else None,
                suite='api-tests',
            )
            yield self._addToLog(self.results_db_log_name, f"\n{test}: pass_rate: {data['pass_rate']}, pre-existing-failure={data['is_existing_failure']}\nResponse from results-db: {data['raw_data']}\n{data['logs']}")
            if data['is_existing_failure']:
                self.preexisting_failures_in_results_db.append(test)
                self.failing_tests_filtered.remove(test)
            else:
                # Optimization to skip consulting results-db for every failure if we encounter any new failure,
                # since until there is atleast one failure which is not pre-existing, we will anayways have to continue with retry logic.
                break


class ReRunAPITests(RunAPITests):
    name = 're-run-api-tests'
    suffix = 'second_run'

    def doOnFailure(self):
        self.steps_to_add += [RevertAppliedChanges(), CleanWorkingDirectory(), ValidateChange(verifyBugClosed=False, addURLs=False)]
        platform = self.getProperty('platform')
        if platform == 'wpe':
            self.steps_to_add.append(InstallWpeDependencies())
        elif platform == 'gtk':
            self.steps_to_add.append(InstallGtkDependencies())
        self.steps_to_add.append(CompileWebKitWithoutChange(retry_build_on_failure=True))
        self.steps_to_add.append(ValidateChange(verifyBugClosed=False, addURLs=False))
        self.steps_to_add.append(KillOldProcesses())
        self.steps_to_add.append(RunAPITestsWithoutChange())
        self.steps_to_add.append(AnalyzeAPITestsResults())


class RunAPITestsWithoutChange(RunAPITests):
    name = 'run-api-tests-without-change'

    def doOnFailure(self):
        pass

    def analyze_failures_using_results_db(self):
        pass


class AnalyzeAPITestsResults(buildstep.BuildStep, AddToLogMixin):
    name = 'analyze-api-tests-results'
    description = ['analyze-api-test-results']
    descriptionDone = ['analyze-api-tests-results']
    NUM_FAILURES_TO_DISPLAY = 10

    @defer.inlineCallbacks
    def run(self):
        self.results = {}
        yield self.getTestsResults(RunAPITests.name)
        yield self.getTestsResults(ReRunAPITests.name)
        yield self.getTestsResults(RunAPITestsWithoutChange.name)
        result = yield self.analyzeResults()

        defer.returnValue(result)

    @defer.inlineCallbacks
    def analyzeResults(self):
        if not self.results or len(self.results) == 0:
            yield self._addToLog('stderr', 'Unable to parse API test results: {}'.format(self.results))
            self.build.buildFinished(['Unable to parse API test results'], RETRY)
            defer.returnValue(RETRY)
            return

        first_run_results = self.results.get(RunAPITests.name)
        second_run_results = self.results.get(ReRunAPITests.name)
        clean_tree_results = self.results.get(RunAPITestsWithoutChange.name)

        if not (first_run_results and second_run_results):
            self.build.buildFinished(['Unable to parse API test results'], RETRY)
            defer.returnValue(RETRY)
            return

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

        yield self._addToLog('stderr', '\nFailures in API Test first run: {}'.format(list(first_run_failures)[:self.NUM_FAILURES_TO_DISPLAY]))
        yield self._addToLog('stderr', '\nFailures in API Test second run: {}'.format(list(second_run_failures)[:self.NUM_FAILURES_TO_DISPLAY]))
        yield self._addToLog('stderr', '\nFlaky Tests: {}'.format(flaky_failures_string))
        yield self._addToLog('stderr', '\nFailures in API Test on clean tree: {}'.format(clean_tree_failures_string))

        if new_failures:
            yield self._addToLog('stderr', '\nNew failures: {}\n'.format(new_failures_string))
            self.build.results = FAILURE
            pluralSuffix = 's' if len(new_failures) > 1 else ''
            message = 'Found {} new API test failure{}: {}'.format(len(new_failures), pluralSuffix, new_failures_string)
            if len(new_failures) > self.NUM_FAILURES_TO_DISPLAY:
                message += ' ...'
            self.descriptionDone = message
            self.build.buildFinished([message], FAILURE)
            defer.returnValue(FAILURE)
        else:
            yield self._addToLog('stderr', '\nNo new failures\n')
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
            defer.returnValue(SUCCESS)

    def getBuildStepByName(self, name):
        for step in self.build.executedSteps:
            if step.name == name:
                return step
        return None

    @defer.inlineCallbacks
    def getTestsResults(self, name):
        step = self.getBuildStepByName(name)
        if not step:
            yield self._addToLog('stderr', 'ERROR: step not found: {}'.format(step))
            defer.returnValue(None)
            return

        logs = yield self.master.db.logs.getLogs(step.stepid)
        log = next((log for log in logs if log['name'] == 'json'), None)
        if not log:
            yield self._addToLog('stderr', 'ERROR: log for step not found: {}'.format(step))
            defer.returnValue(None)
            return

        lastline = int(max(0, log['num_lines'] - 1))
        logLines = yield self.master.db.logs.getLogLines(log['id'], 0, lastline)
        if log['type'] == 's':
            logLines = ''.join([line[1:] for line in logLines.splitlines()])

        try:
            self.results[name] = json.loads(logLines)
            defer.returnValue(self.results[name])
        except Exception as ex:
            yield self._addToLog('stderr', 'ERROR: unable to parse data, exception: {}'.format(ex))

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


class ArchiveTestResults(shell.ShellCommandNewStyle):
    command = ['python3', 'Tools/CISupport/test-result-archive',
               Interpolate('--platform=%(prop:platform)s'), Interpolate('--%(prop:configuration)s'), 'archive']
    name = 'archive-test-results'
    description = ['archiving test results']
    descriptionDone = ['Archived test results']
    haltOnFailure = True

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, **kwargs)


class UploadTestResults(transfer.FileUpload):
    name = 'upload-test-results'
    descriptionDone = ['Uploaded test results']
    workersrc = 'layout-test-results.zip'
    haltOnFailure = True

    def __init__(self, identifier='', **kwargs):
        if identifier and not identifier.startswith('-'):
            identifier = '-{}'.format(identifier)
        kwargs['workersrc'] = self.workersrc
        kwargs['masterdest'] = Interpolate('public_html/results/%(prop:buildername)s/%(prop:change_id)s-%(prop:buildnumber)s{}.zip'.format(identifier))
        kwargs['mode'] = 0o0644
        kwargs['blocksize'] = 1024 * 256
        super().__init__(**kwargs)


class ExtractTestResults(master.MasterShellCommandNewStyle):
    name = 'extract-test-results'
    descriptionDone = ['Extracted test results']
    renderables = ['resultDirectory', 'zipFile']
    haltOnFailure = False
    flunkOnFailure = False

    def __init__(self, identifier=''):
        if identifier and not identifier.startswith('-'):
            identifier = '-{}'.format(identifier)

        self.zipFile = Interpolate('public_html/results/%(prop:buildername)s/%(prop:change_id)s-%(prop:buildnumber)s{}.zip'.format(identifier))
        self.resultDirectory = Interpolate('public_html/results/%(prop:buildername)s/%(prop:change_id)s-%(prop:buildnumber)s{}'.format(identifier))
        self.command = ['unzip', '-q', '-o', self.zipFile, '-d', self.resultDirectory]

        super().__init__(command=self.command, logEnviron=False)

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

    @defer.inlineCallbacks
    def run(self):
        rc = yield super().run()
        self.addCustomURLs()
        defer.returnValue(rc)


class PrintConfiguration(steps.ShellSequence):
    name = 'configuration'
    description = ['configuration']
    haltOnFailure = False
    flunkOnFailure = False
    warnOnFailure = False
    logEnviron = False
    command_list_generic = [['hostname']]
    command_list_apple = [['df', '-hl'], ['date'], ['sw_vers'], ['system_profiler', 'SPSoftwareDataType', 'SPHardwareDataType'], ['/bin/sh', '-c', 'echo TimezoneVers: $(cat /usr/share/zoneinfo/+VERSION)'], ['xcodebuild', '-sdk', '-version']]
    command_list_linux = [['df', '-hl', '--exclude-type=fuse.portal'], ['date'], ['uname', '-a'], ['uptime']]

    def __init__(self, **kwargs):
        super().__init__(timeout=60, **kwargs)
        self.commands = []
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

    @defer.inlineCallbacks
    def run(self):
        command_list = list(self.command_list_generic)
        platform = self.getProperty('platform', '*')
        if platform != 'jsc-only':
            platform = platform.split('-')[0]
        if platform in ('mac', 'ios', 'visionos', 'tvos', 'watchos', '*'):
            command_list.extend(self.command_list_apple)
        elif platform in ('gtk', 'wpe', 'jsc-only'):
            command_list.extend(self.command_list_linux)

        for command in command_list:
            self.commands.append(util.ShellArg(command=command, logname='stdio'))
        rc = yield super().run()
        logs = self.log_observer.getStdout() + self.log_observer.getStderr()
        self.parseAndValidate(logs)
        defer.returnValue(rc)

    def convert_build_to_os_name(self, build):
        if not build:
            return 'Unknown'

        build_to_name_mapping = {
            '15': 'Sequoia',
            '14': 'Sonoma',
            '13': 'Ventura'
        }

        for key, value in build_to_name_mapping.items():
            if build.startswith(key):
                return value
        return 'Unknown'

    def parseAndValidate(self, logText):
        os_version, os_name, xcode_version = '', '', ''
        match = re.search('ProductVersion:[ \t]*(.+?)\n', logText)
        if match:
            os_version = match.group(1).strip()
            os_name = self.convert_build_to_os_name(os_version)

        xcode_re = sdk_re = 'Xcode[ \t]+?([0-9.]+?)\n'
        match = re.search(xcode_re, logText)
        if match:
            xcode_version = match.group(1).strip()

        match = re.search('BuildVersion:[ \t]*(.+?)\n', logText)
        if match:
            build_version = match.group(1).strip()
            self.setProperty('build_version', build_version)

        self.setProperty('os_version', os_version)
        self.setProperty('os_name', os_name)
        self.setProperty('xcode_version', xcode_version)
        os_version_builder = self.getProperty('os_version_builder', '')
        xcode_version_builder = self.getProperty('xcode_version_builder', '')
        os_major_version_mismatch = os_version and os_version_builder and (os_version.split('.')[:2] != os_version_builder.split('.')[:2])
        xcode_version_mismatch = xcode_version and xcode_version_builder and (xcode_version != xcode_version_builder)

        if os_major_version_mismatch or xcode_version_mismatch:
            message = f'Error: OS/SDK version mismatch, please inform an admin.'
            detailed_message = message + f' Builder: OS={os_version_builder}, Xcode={xcode_version_builder}; Tester: OS={os_version}, Xcode={xcode_version}'
            print(f'\n{detailed_message}')
            self.build.stopBuild(reason=detailed_message, results=FAILURE)
            self.build.buildFinished([message], FAILURE)

    def getResultSummary(self):
        if self.results not in [SUCCESS, WARNINGS, EXCEPTION]:
            return {'step': 'Failed to print configuration'}

        configuration = 'Printed configuration'
        os_version = self.getProperty('os_version', '')
        if os_version:
            os_name = self.convert_build_to_os_name(os_version)
            configuration = 'OS: {} ({})'.format(os_name, os_version)

        xcode_version = self.getProperty('xcode_version', '')
        if xcode_version:
            configuration += ', Xcode: {}'.format(xcode_version)
        return {'step': configuration}


# FIXME: We should be able to remove this step once abandoning patch workflows
class CleanGitRepo(steps.ShellSequence, ShellMixin):
    name = 'clean-up-git-repo'
    haltOnFailure = False
    flunkOnFailure = False
    logEnviron = False

    def __init__(self, default_branch=DEFAULT_BRANCH, remote=DEFAULT_REMOTE, **kwargs):
        super().__init__(timeout=5 * 60, **kwargs)
        self.default_branch = default_branch
        self.git_remote = remote

    def run(self):
        self.commands = []
        if self.getProperty('platform', '*') == 'win':
            self.commands.append(util.ShellArg(
                command=self.shell_command(r'del .git\gc.log || {}'.format(self.shell_exit_0())),
                logname='stdio',
            ))
        else:
            self.commands.append(util.ShellArg(
                command=self.shell_command('rm -f .git/gc.log || {}'.format(self.shell_exit_0())),
                logname='stdio',
            ))

        for command in [
            self.shell_command('git rebase --abort || {}'.format(self.shell_exit_0())),
            self.shell_command('git am --abort || {}'.format(self.shell_exit_0())),
            self.shell_command('git cherry-pick --abort || {}'.format(self.shell_exit_0())),
            ['git', 'clean', '-f', '-d'],  # Remove any left-over layout test results, added files, etc.
            ['git', 'checkout', '{}/{}'.format(self.git_remote, self.default_branch), '-f'],  # Checkout branch from specific remote
            ['git', 'branch', '-D', self.default_branch],  # Delete any local cache of the specified branch
            ['git', 'branch', self.default_branch],  # Create local instance of branch from remote, but don't track it
            self.shell_command("git branch | grep -v ' {}$' | grep -v 'HEAD detached at' | xargs git branch -D || {}".format(self.default_branch, self.shell_exit_0())),
            self.shell_command("git remote | grep -v '{}$' | xargs -L 1 git remote rm || {}".format(self.git_remote, self.shell_exit_0())),
            ['git', 'prune'],
        ]:
            self.commands.append(util.ShellArg(command=command, logname='stdio'))
        return super().run()

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Encountered some issues during cleanup'}
        return {'step': 'Cleaned up git repository'}


class ApplyWatchList(shell.ShellCommandNewStyle):
    name = 'apply-watch-list'
    description = ['applying watchilist']
    descriptionDone = ['Applied WatchList']
    bug_id = WithProperties('%(bug_id)s')
    command = ['python3', 'Tools/Scripts/webkit-patch', 'apply-watchlist-local', bug_id]
    haltOnFailure = True
    flunkOnFailure = True

    def __init__(self, **kwargs):
        super().__init__(timeout=2 * 60, logEnviron=False, **kwargs)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to apply watchlist'}
        return super().getResultSummary()


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
        previous_build_summary = self.getProperty('build_summary', '')
        if RunWebKitTestsInStressMode.FAILURE_MSG_IN_STRESS_MODE in previous_build_summary:
            self.build.results = FAILURE
        elif any(s in previous_build_summary for s in ('Committed ', '@', 'Passed', 'Ignored pre-existing failure')):
            self.build.results = SUCCESS
        self.build.buildFinished([build_summary], self.build.results)
        return defer.succeed(None)


class PushCommitToWebKitRepo(shell.ShellCommand):
    name = 'push-commit-to-webkit-repo'
    descriptionDone = ['Pushed commit to WebKit repository']
    haltOnFailure = False
    MAX_RETRY = 2
    HASH_RE = re.compile(r'\s+[0-9a-f]+\.\.+(?P<hash>[0-9a-f]+)\s+')

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, timeout=300, **kwargs)

    def start(self, BufferLogObserverClass=logobserver.BufferLogObserver):
        head_ref = self.getProperty('github.base.ref', 'main')
        remote = self.getProperty('remote', '?')
        self.command = ['git', 'push', remote, f'HEAD:{head_ref}']

        username, access_token = GitHub.credentials(user=GitHub.user_for_queue(self.getProperty('buildername', '')))
        self.workerEnvironment['GIT_USER'] = username
        self.workerEnvironment['GIT_PASSWORD'] = access_token

        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)
        return super().start()

    def evaluateCommand(self, cmd):
        rc = shell.ShellCommand.evaluateCommand(self, cmd)
        if rc == SUCCESS:
            log_text = self.log_observer.getStdout() + self.log_observer.getStderr()
            landed_hash = self.hash_from_commit_text(log_text)
            if landed_hash:
                self.setProperty('landed_hash', landed_hash)

            steps_to_add = [
                DetermineLandedIdentifier(),
                LeaveComment(),
                RemoveFlagsOnPatch(), RemoveLabelsFromPullRequest(),
                CloseBug(),
            ]
            self.build.addStepsAfterCurrentStep(steps_to_add)

        else:
            retry_count = int(self.getProperty('retry_count', 0))
            if retry_count < self.MAX_RETRY:
                self.setProperty('retry_count', retry_count + 1)
                if self.getProperty('github.number', ''):
                    self.build.addStepsAfterCurrentStep([
                        CleanGitRepo(),
                        CheckOutSource(),
                        FetchBranches(),
                        UpdateWorkingDirectory(),
                        ShowIdentifier(),
                        CheckOutPullRequest(),
                        AddReviewerToCommitMessage(),
                        Canonicalize(),
                        ValidateChange(verifyMergeQueue=True, verifyNoDraftForMergeQueue=True, verifyObsolete=False, enableSkipEWSLabel=False),
                        PushPullRequestBranch(),
                        UpdatePullRequest(),
                        PushCommitToWebKitRepo(),
                    ])
                else:
                    self.build.addStepsAfterCurrentStep([
                        CleanGitRepo(),
                        CheckOutSource(),
                        FetchBranches(),
                        UpdateWorkingDirectory(),
                        ShowIdentifier(),
                        ApplyPatch(),
                        AddReviewerToCommitMessage(),
                        Canonicalize(),
                        ValidateChange(addURLs=False, verifycqplus=True),
                        PushCommitToWebKitRepo(),
                    ])
                return rc

            if self.getProperty('github.number', ''):
                self.setProperty('comment_text', 'merge-queue failed to commit PR to repository. To retry, remove any blocking labels and re-apply merge-queue label')
            else:
                patch_id = self.getProperty('patch_id', '')
                self.setProperty('comment_text', f'commit-queue failed to commit attachment {patch_id} to WebKit repository. To retry, please set cq+ flag again.')

            self.setProperty('build_finish_summary', 'Failed to commit to WebKit repository')
            self.build.addStepsAfterCurrentStep([LeaveComment(), SetCommitQueueMinusFlagOnPatch(), BlockPullRequest()])
        return rc

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to push commit to Webkit repository'}
        return shell.ShellCommand.getResultSummary(self)

    def doStepIf(self, step):
        return CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES

    def hash_from_commit_text(self, commit_text):
        match = self.HASH_RE.search(commit_text)
        if match:
            return match.group('hash')
        return None


class DetermineLandedIdentifier(shell.ShellCommandNewStyle):
    name = 'determine-landed-identifier'
    descriptionDone = ['Determined landed identifier']
    command = ['/bin/sh', '-c', "git log -1 --no-decorate | grep 'Canonical link: https://commits\\.webkit\\.org/'"]
    CANONICAL_LINK_RE = re.compile(r'\ACanonical link: https://commits\.webkit\.org/(?P<identifier>\d+.?\d*@\S+)\Z')
    haltOnFailure = False

    def __init__(self, **kwargs):
        self.identifier = None
        super().__init__(logEnviron=False, timeout=300, **kwargs)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': f'Identifier: {self.identifier}'}
        if self.results == FAILURE:
            return {'step': 'Failed to determine identifier'}
        return super().getResultSummary()

    @defer.inlineCallbacks
    def run(self, BufferLogObserverClass=logobserver.BufferLogObserver):
        self.log_observer = BufferLogObserverClass(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

        rc = yield super().run()

        loglines = self.log_observer.getStdout().splitlines()

        for line in loglines:
            if not line:
                continue
            match = self.CANONICAL_LINK_RE.match(line[4:])
            if match:
                self.identifier = match.group('identifier')
                break

        landed_hash = self.getProperty('landed_hash')
        if not self.identifier:
            yield task.deferLater(reactor, 60, lambda: None)  # It takes time for commits.webkit.org to digest commits
            self.identifier = yield self.identifier_for_hash(landed_hash)
            if self.identifier and '@' in self.identifier:
                rc = SUCCESS
            else:
                rc = FAILURE

        self.setProperty('comment_text', self.comment_text_for_bug(landed_hash, self.identifier))
        commit_summary = f'Committed {self.identifier or landed_hash}'
        self.descriptionDone = commit_summary
        self.setProperty('build_summary', commit_summary)
        self.addURL(self.identifier, self.url_for_identifier(self.identifier))

        defer.returnValue(rc)

    def url_for_identifier(self, identifier):
        return '{}{}'.format(COMMITS_INFO_URL, identifier)

    @defer.inlineCallbacks
    def identifier_for_hash(self, hash):
        try:
            response = yield TwistedAdditions.request(
                url=f'{COMMITS_INFO_URL}{hash}/json', logger=print,
            )
            if response and response.status_code == 200:
                defer.returnValue(response.json().get('identifier', hash).replace('@trunk', '@main'))
                return
            elif response:
                print(f'Non-200 status code received from {COMMITS_INFO_URL}: {response.status_code}')
                print(response.text)
                defer.returnValue(hash)
                return
        except json.decoder.JSONDecodeError:
            print(f'Response from {COMMITS_INFO_URL} was not JSON')
            print(response.text)
        defer.returnValue(hash)

    def comment_text_for_bug(self, hash=None, identifier=None):
        identifier_str = identifier if identifier and '@' in identifier else '?'
        comment = '{} {} ({}): <{}>'.format(
            'Test gardening commit' if self.getProperty('is_test_gardening') else 'Committed',
            identifier_str, hash, self.url_for_identifier(identifier),
        )

        patch_id = self.getProperty('patch_id', '')
        if patch_id:
            comment += f'\n\nAll reviewed patches have been landed. Closing bug and clearing flags on attachment {patch_id}.'
        pr_number = self.getProperty('github.number', '')
        if pr_number:
            comment += f'\n\nReviewed commits have been landed. Closing PR #{pr_number} and removing active labels.'
        return comment


class CheckStatusOnEWSQueues(buildstep.BuildStep, BugzillaMixin):
    name = 'check-status-on-other-ewses'
    descriptionDone = ['Checked change status on other queues']

    @defer.inlineCallbacks
    def get_change_status(self, change_id, queue):
        url = '{}status/{}/'.format(EWS_URL, change_id)
        try:
            response = yield TwistedAdditions.request(url, logger=lambda content: self._addToLog('stdio', content))
            if response.status_code != 200:
                yield self._addToLog('stdio', 'Failed to access {} with status code: {}\n'.format(url, response.status_code))
                defer.returnValue(-1)
                return
            queue_data = response.json().get(queue, None)
            if queue_data:
                defer.returnValue(queue_data.get('state', None))
            else:
                defer.returnValue(-1)
        except Exception as e:
            yield self._addToLog('stdio', f'Failed to access {url}\n    {e}\n')
            defer.returnValue(-1)

    @defer.inlineCallbacks
    def run(self):
        change_id = self.getProperty('github.head.sha', self.getProperty('patch_id', ''))
        change_status_on_mac_wk2 = yield self.get_change_status(change_id, 'mac-wk2')
        if change_status_on_mac_wk2 == SUCCESS:
            self.setProperty('passed_mac_wk2', True)
        defer.returnValue(SUCCESS)


class ValidateRemote(shell.ShellCommand):
    name = 'validate-remote'
    haltOnFailure = False
    flunkOnFailure = True

    def __init__(self, **kwargs):
        self.summary = ''
        super().__init__(logEnviron=False, **kwargs)

    def start(self, BufferLogObserverClass=logobserver.BufferLogObserver):
        base_ref = self.getProperty('github.base.ref', f'{DEFAULT_REMOTE}/{DEFAULT_BRANCH}')
        remote = self.getProperty('remote', DEFAULT_REMOTE)

        self.command = [
            'git', 'merge-base', '--is-ancestor',
            f'remotes/{remote}/{base_ref}',
            f'remotes/{DEFAULT_REMOTE}/{base_ref}',
        ]

        return super().start()

    def getResultSummary(self):
        if self.results in (FAILURE, SUCCESS):
            return {'step': self.summary}
        return super().getResultSummary()

    def evaluateCommand(self, cmd):
        base_ref = self.getProperty('github.base.ref', f'{DEFAULT_REMOTE}/{DEFAULT_BRANCH}')
        rc = super().evaluateCommand(cmd)

        if rc == SUCCESS:
            self.summary = f"Cannot land on '{base_ref}', it is owned by '{GITHUB_PROJECTS[0]}'"
            self.setProperty(
                'comment_text',
                f"{self.summary}, blocking PR #{self.getProperty('github.number')}.\n"
                f"Make a pull request against '{GITHUB_PROJECTS[0]}' to land this change."
            )
            self.setProperty('build_finish_summary', self.summary)
            self.build.addStepsAfterCurrentStep([LeaveComment(), BlockPullRequest()])
            return FAILURE

        if rc == FAILURE:
            self.summary = f"Verified '{GITHUB_PROJECTS[0]}' does not own '{base_ref}'"
            return SUCCESS

        return rc

    def doStepIf(self, step):
        if not self.getProperty('github.number'):
            return False
        remote = self.getProperty('remote', None)
        if not remote:
            return False
        return remote != DEFAULT_REMOTE

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


# There are cases where we have a branch alias tracking a more traditional static branch.
# We want contributors to be able to land changes on the branch alias instead of the possibly
# changing branch.
class MapBranchAlias(shell.ShellCommand):
    name = 'map-branch-alias'
    haltOnFailure = False
    flunkOnFailure = True
    DEV_BRANCHES = re.compile(r'.*[(eng)(dev)(bug)]/.+')
    PROD_BRANCHES = re.compile(r'\S+-[\d+\.]+-branch')

    def __init__(self, **kwargs):
        self.summary = ''
        super().__init__(logEnviron=False, timeout=60, **kwargs)

    def start(self, BufferLogObserverClass=logobserver.BufferLogObserver):
        base_ref = self.getProperty('github.base.ref', DEFAULT_BRANCH)
        remote = self.getProperty('remote', DEFAULT_REMOTE)

        self.command = ['git', 'branch', '-a', '--contains', f'remotes/{remote}/{base_ref}']

        self.log_observer = BufferLogObserverClass(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

        return super().start()

    def getResultSummary(self):
        if self.results in (FAILURE, SUCCESS):
            return {'step': self.summary}
        return super().getResultSummary()

    def evaluateCommand(self, cmd):
        remote = self.getProperty('remote', DEFAULT_REMOTE)
        branch = self.getProperty('github.base.ref', DEFAULT_BRANCH)
        rc = super().evaluateCommand(cmd)

        if rc == FAILURE:
            self.summary = f"Failed to query checkout for aliases of '{branch}'"
            return FAILURE
        elif rc != SUCCESS:
            return rc

        aliases = set()
        log_text = self.log_observer.getStdout()
        for line in log_text.splitlines():
            line = line.lstrip().rstrip()
            if not line.startswith(f'remotes/{remote}'):
                continue
            candidate = line.split('/', 2)[-1]
            if self.DEV_BRANCHES.match(candidate):
                continue
            aliases.add(candidate)

        if DEFAULT_BRANCH in aliases:
            branch = DEFAULT_BRANCH
        if branch != DEFAULT_BRANCH and self.DEV_BRANCHES.match(branch) and aliases:
            branch = next(iter(aliases))
        if branch != DEFAULT_BRANCH and not self.PROD_BRANCHES.match(branch):
            for alias in aliases:
                if self.PROD_BRANCHES.match(alias):
                    branch = alias
                    break

        self.summary = f"'{branch}' is the prevailing alias"
        self.setProperty('github.base.ref', branch)
        return rc

    def doStepIf(self, step):
        if not self.getProperty('github.number'):
            return False
        return self.getProperty('github.base.ref', DEFAULT_BRANCH) != DEFAULT_BRANCH

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


class ValidateSquashed(shell.ShellCommand):
    name = 'validate-squashed'
    haltOnFailure = False
    flunkOnFailure = True

    def __init__(self, **kwargs):
        self.summary = ''
        super().__init__(logEnviron=False, **kwargs)

    def start(self, BufferLogObserverClass=logobserver.BufferLogObserver):
        base_ref = self.getProperty('github.base.ref', f'{DEFAULT_REMOTE}/{DEFAULT_BRANCH}')
        head_ref = self.getProperty('github.head.ref', 'HEAD')
        self.command = ['git', 'log', '--format=format:"%H"', head_ref, f'^{base_ref}', f'--max-count={MAX_COMMITS_IN_PR_SERIES + 1}']

        self.log_observer = BufferLogObserverClass(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

        return shell.ShellCommand.start(self)

    def getResultSummary(self):
        return {'step': self.summary}

    def evaluateCommand(self, cmd):
        rc = shell.ShellCommand.evaluateCommand(self, cmd)

        pr_number = self.getProperty('github.number')
        patch_id = self.getProperty('patch_id')

        if rc != SUCCESS:
            self.summary = 'Failed to check if commit is squashed'
            comment = self.summary
            if pr_number:
                comment = f"{self.summary}, please re-add `Merge-Queue` to PR #{pr_number} to land it."
            elif patch_id:
                comment = f"{self.summary}, please add cq+ to attachment {patch_id} to land it."

            self.setProperty('build_finish_summary', self.summary)
            self.setProperty('comment_text', comment)
            self.build.addStepsAfterCurrentStep([
                LeaveComment(),
                BlockPullRequest() if pr_number else SetCommitQueueMinusFlagOnPatch(),
            ])
            return rc

        log_text = self.log_observer.getStdout()
        commit_count = len(log_text.splitlines())
        self.setProperty('commit_count', commit_count)

        classification = self.getProperty('classification', [])
        if not isinstance(classification, list):
            classification = []

        if ['Cherry-pick'] == classification:
            if commit_count > 0 and commit_count < MAX_COMMITS_IN_PR_SERIES:
                self.summary = 'Commit sequence is entirely cherry-picks'
                return SUCCESS

            self.summary = 'Too many commits in a pull-request'
            comment = 'Policy allows for multiple cherry-picks to be landed simultaneously ' \
                      f'but there is a limit of {MAX_COMMITS_IN_PR_SERIES}, blocking PR #{pr_number} because it has {commit_count} commits. ' \
                      'Please break this change into multiple pull requests.'
        else:
            if commit_count == 1:
                self.summary = 'Verified commit is squashed'
                return SUCCESS

            self.summary = 'Can only land squashed commits'
            comment = 'This change contains multiple commits which are not squashed together'
            if pr_number:
                comment = f"{comment}, blocking PR #{pr_number}"
            elif patch_id:
                comment = f"{comment}, rejecting attachment {patch_id} from commit queue"
            comment += '. Please squash the commits to land.'

        self.setProperty('comment_text', comment)
        self.setProperty('build_finish_summary', self.summary)
        self.build.addStepsAfterCurrentStep([
            LeaveComment(),
            BlockPullRequest() if pr_number else SetCommitQueueMinusFlagOnPatch(),
        ])
        return FAILURE


class AddReviewerMixin(object):
    NOBODY_SED = 's/NOBODY (OO*PP*S!*)/{}/g'

    @defer.inlineCallbacks
    def gitCommitEnvironment(self):
        owners = self.getProperty('owners', [])
        if not owners:
            return dict(
                GIT_COMMITTER_NAME='EWS',
                GIT_COMMITTER_EMAIL=FROM_EMAIL,
                FILTER_BRANCH_SQUELCH_WARNING='1',
            )

        contributors, _ = yield Contributors.load()
        return dict(
            GIT_COMMITTER_NAME=contributors.get(owners[0].lower(), {}).get('name', 'EWS'),
            GIT_COMMITTER_EMAIL=contributors.get(owners[0].lower(), {}).get('email', FROM_EMAIL),
            FILTER_BRANCH_SQUELCH_WARNING='1',
        )

    def reviewers(self):
        reviewers = self.getProperty('valid_reviewers', [])
        if len(reviewers) == 1:
            return reviewers[0]
        if reviewers:
            conjunction = f'{"," if len(reviewers) > 2 else ""} and '
            return f'{", ".join(reviewers[:-1])}{conjunction}{reviewers[-1]}'
        return 'NOBODY (OOPS!)'


class AddReviewerToCommitMessage(shell.ShellCommand, AddReviewerMixin):
    name = 'add-reviewer-to-commit-message'
    haltOnFailure = True

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, timeout=60, **kwargs)

    @defer.inlineCallbacks
    def run(self, BufferLogObserverClass=logobserver.BufferLogObserver):
        base_ref = self.getProperty('github.base.ref', f'{DEFAULT_REMOTE}/{DEFAULT_BRANCH}')
        head_ref = self.getProperty('github.head.ref', 'HEAD')

        gmtoffset = int(time.localtime().tm_gmtoff * 100 / (60 * 60))
        date = f'{int(time.time())} {gmtoffset}'
        self.command = [
            'git', 'filter-branch', '-f',
            '--env-filter', f"GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}'",
            '--msg-filter', f'sed "{self.NOBODY_SED.format(self.reviewers())}"',
            f'{head_ref}...{base_ref}',
        ]

        commit_environment = yield self.gitCommitEnvironment()
        for key, value in commit_environment.items():
            self.workerEnvironment[key] = value

        rc = yield super().run()
        defer.returnValue(rc)

    def getResultSummary(self):
        if self.results == FAILURE:
            return {'step': 'Failed to apply reviewers'}
        if self.results == SUCCESS:
            return {'step': f'Reviewed by {self.reviewers()}'}
        return super().getResultSummary()

    def doStepIf(self, step):
        classification = self.getProperty('classification', [])
        if not isinstance(classification, list):
            classification = []
        return self.getProperty('valid_reviewers') and ['Cherry-pick'] != classification

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


class ValidateCommitMessage(steps.ShellSequence, ShellMixin, AddToLogMixin):
    name = 'validate-commit-message'
    haltOnFailure = False
    flunkOnFailure = True
    OOPS_RE = 'OO*PP*S!'
    REVIEWED_STRINGS = (
        'Reviewed by',
        'Rubber-stamped by',
        'Rubber stamped by',
        'Unreviewed',
        'Versioning.',
    )
    RE_CHANGELOG = br'^(\+\+\+)\s+(.*/ChangeLog.*)'
    BY_RE = re.compile(r'.+\s+by\s+(.+)$')
    SPLIT_RE = re.compile(r'\s+and\s+|,\s*and\s*|,\s*')

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, timeout=60, **kwargs)
        self.contributors = {}

    @classmethod
    def extract_reviewers(cls, text):
        reviewers = set()
        stripped_text = ''
        for line in text.splitlines():
            match = cls.BY_RE.match(line)
            if not match:
                stripped_text += line + '\n'
                continue
            for person in cls.SPLIT_RE.split(match.group(1)):
                if person and not cls.SPLIT_RE.match(person):
                    reviewers.add(person.rstrip('.'))
        return reviewers, stripped_text

    def is_reviewer(self, name):
        contributor = self.contributors.get(name)
        return contributor and contributor['status'] == 'reviewer'

    def _files(self):
        sourcestamp = self.build.getSourceStamp(self.getProperty('codebase', ''))
        if sourcestamp and sourcestamp.changes:
            return sourcestamp.changes[0].files
        if sourcestamp and sourcestamp.patch:
            files = []
            for line in sourcestamp.patch[1].splitlines():
                match = re.search(self.RE_CHANGELOG, line)
                if match:
                    files.append(match.group(1).decode('utf-8'))
            return files
        return []

    @defer.inlineCallbacks
    def run(self, BufferLogObserverClass=logobserver.BufferLogObserver):
        base_ref = self.getProperty('github.base.ref', f'{DEFAULT_REMOTE}/{DEFAULT_BRANCH}')
        head_ref = self.getProperty('github.head.ref', 'HEAD')
        valid_reviewers = self.getProperty('valid_reviewers', [])
        invalid_reviewers = self.getProperty('invalid_reviewers', [])
        reviewer_error_msg = '' if valid_reviewers else ' and no valid reviewer found'
        invalid_msg = ' and {} are not reviewers' if len(invalid_reviewers) > 1 else ' and {} is not a reviewer'
        if invalid_reviewers:
            reviewer_error_msg = invalid_msg.format(', '.join(invalid_reviewers))

        self.commands = []
        commands = [
            f"git log {head_ref} ^{base_ref} | grep -q '{self.OOPS_RE}' && echo 'Commit message contains (OOPS!){reviewer_error_msg}' || test $? -eq 1",
            "git log {} ^{} | grep -q '\\({}\\)' || echo 'No reviewer information in commit message'".format(
                head_ref, base_ref,
                '\\|'.join(self.REVIEWED_STRINGS),
            ), "git log {} ^{} | grep '\\({}\\)' || true".format(
                head_ref, base_ref,
                '\\|'.join(self.REVIEWED_STRINGS[:3]),
            ),
        ]
        for command in commands:
            self.commands.append(util.ShellArg(command=self.shell_command(command), logname='stdio', haltOnFailure=True))

        self.log_observer = BufferLogObserverClass(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

        rc = yield super().run()

        if rc == SKIPPED:
            self.summary = 'Patches have no commit message'
            defer.returnValue(rc)
            return

        self.contributors, errors = yield Contributors.load(use_network=True)
        for error in errors:
            self._addToLog('stdio', error)
        self._addToLog('stdio', '\n')

        reviewers, log_text = self.extract_reviewers(self.log_observer.getStdout())
        log_text = log_text.rstrip()
        author = self.getProperty('author', '')

        self.summary = ''
        if any(os.path.basename(file).startswith('ChangeLog') for file in self._files()):
            self.summary = 'ChangeLog modified, WebKit only allows commit messages'
            rc = FAILURE
        elif log_text:
            self.summary = log_text.split('\n')[0]  # Display the first error if there are multiple
            rc = FAILURE

        if rc == SUCCESS:
            if reviewers and not self.contributors:
                self.summary = "Failed to load contributors.json, can't validate reviewers"
            elif reviewers and any([not self.is_reviewer(reviewer) for reviewer in reviewers]):
                self.summary = "'{}' is not a reviewer, still continuing"
                for reviewer in reviewers:
                    if not self.is_reviewer(reviewer):
                        self.summary = self.summary.format(reviewer)
                        break
            elif reviewers and author and any([author.startswith(reviewer) for reviewer in reviewers]):
                self.summary = f"'{author}' cannot review their own change"
                rc = FAILURE
            else:
                self.summary = 'Validated commit message'
        elif not self.summary:
            self.summary = 'Error parsing commit message'
            rc = FAILURE

        if rc == FAILURE:
            build_url = f'{self.master.config.buildbotURL}#/builders/{self.build._builderid}/builds/{self.build.number}'
            url_to_show = f'[Build #{self.getProperty("buildnumber", "")}]({build_url})'
            self.setProperty('comment_text', f"{self.summary}, blocking PR #{self.getProperty('github.number')}. Details: {url_to_show}")
            self.setProperty('build_finish_summary', 'Commit message validation failed')
            self.build.addStepsAfterCurrentStep([LeaveComment(), SetCommitQueueMinusFlagOnPatch(), BlockPullRequest()])
        defer.returnValue(rc)

    def getResultSummary(self):
        if self.results in (SUCCESS, FAILURE):
            return {'step': self.summary}
        return super().getResultSummary()


class Canonicalize(steps.ShellSequence, ShellMixin, AddToLogMixin):
    name = 'canonicalize-commit'
    description = ['canonicalize-commit']
    descriptionDone = ['Canonicalize Commit']
    haltOnFailure = True

    def __init__(self, rebase_enabled=True, **kwargs):
        super().__init__(logEnviron=False, timeout=300, **kwargs)
        self.rebase_enabled = rebase_enabled
        self.contributors = {}

    def number_commits_to_canonicalize(self):
        commit_count = self.getProperty('commit_count', 1)
        if not self.rebase_enabled:
            commit_count += 2
        return commit_count

    @defer.inlineCallbacks
    def run(self):
        self.commands = []
        self.contributors, errors = yield Contributors.load(use_network=True)
        for error in errors:
            yield self._addToLog('stdio', error)

        base_ref = self.getProperty('github.base.ref', DEFAULT_BRANCH)
        head_ref = self.getProperty('github.head.ref', None)
        remote = self.getProperty('remote', 'origin')

        commands = [self.shell_command('rm .git/identifiers.json || {}'.format(self.shell_exit_0()))]
        if self.rebase_enabled:
            commands += [['git', 'pull', remote, base_ref, '--rebase']]
            if head_ref:
                commands += [['git', 'branch', '-f', base_ref, head_ref]]
            commands += [['git', 'checkout', base_ref]]
        commands.append(['python3', 'Tools/Scripts/git-webkit', 'canonicalize', '-n', str(self.number_commits_to_canonicalize())])

        if self.getProperty('github.number', ''):
            committer = (self.getProperty('owners', []) or [''])[0]
        else:
            committer = self.getProperty('patch_committer', '').lower()

        contributor = self.contributors.get(committer.lower()) if committer else {}
        committer_name = contributor.get('name', committer or 'WebKit Commit Queue')
        committer_email = contributor.get('email', committer or FROM_EMAIL)

        gmtoffset = int(time.localtime().tm_gmtoff * 100 / (60 * 60))
        date = f'{int(time.time())} {gmtoffset}'
        commands.append([
            'git', 'filter-branch', '-f',
            '--env-filter', f"GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}';GIT_COMMITTER_NAME='{committer_name}';GIT_COMMITTER_EMAIL='{committer_email}'",
            f"HEAD...HEAD~{self.getProperty('commit_count', 1)}",
        ])

        username, access_token = GitHub.credentials(user=GitHub.user_for_queue(self.getProperty('buildername', '')))
        self.env = dict(
            GIT_USER=username,
            GIT_PASSWORD=access_token,
            FILTER_BRANCH_SQUELCH_WARNING='1',
        )

        for command in commands:
            self.commands.append(util.ShellArg(command=command, logname='stdio', haltOnFailure=True))
        rc = yield super().run()
        defer.returnValue(rc)

    def getResultSummary(self):
        commit_pluralized = "commit" if self.number_commits_to_canonicalize() == 1 else "commits"
        if self.results == SUCCESS:
            return {'step': f'Canonicalized {commit_pluralized}'}
        if self.results == FAILURE:
            return {'step': f'Failed to canonicalize {commit_pluralized}'}
        return super().getResultSummary()


class PushPullRequestBranch(shell.ShellCommandNewStyle):
    name = 'push-pull-request-branch'
    haltOnFailure = True

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, timeout=300, **kwargs)

    def run(self, BufferLogObserverClass=logobserver.BufferLogObserver):
        remote, repo_name = self.getProperty('github.head.repo.full_name', DEFAULT_REMOTE).split('/', 1)
        if '-' in repo_name:
            remote = f"{remote}-{repo_name.split('-', 1)[-1]}"
        head_ref = self.getProperty('github.head.ref')
        self.command = ['git', 'push', '-f', remote, f'HEAD:{head_ref}']

        username, access_token = GitHub.credentials(user=GitHub.user_for_queue(self.getProperty('buildername', '')))
        self.env['GIT_USER'] = username
        self.env['GIT_PASSWORD'] = access_token

        return super().run()

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': 'Pushed to pull request branch'}
        if self.results == FAILURE:
            return {'step': 'Failed to push to pull request branch'}
        return super().getResultSummary()

    def doStepIf(self, step):
        return CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES and self.getProperty('github.number') and self.getProperty('github.head.ref') and self.getProperty('github.head.repo.full_name')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


class UpdatePullRequest(shell.ShellCommandNewStyle, GitHubMixin, AddToLogMixin):
    name = 'update-pull-request'
    haltOnFailure = False
    command = ['git', 'log', '--no-decorate']
    ESCAPE_TABLE = {
        '"': '&quot;',
        "'": '&apos;',
        '>': '&gt;',
        '<': '&lt;',
        '&': '&amp;',
    }
    BUGS_RE = [
        re.compile(r'\Awebkit.org/b/(?P<id>\d+)\Z'),
        re.compile(r'\Ahttps?://webkit.org/b/(?P<id>\d+)\Z'),
        re.compile(r'\Awebkit.org/b/(?P<id>\d+)\Z'),
        re.compile(r'\A<?bugs.webkit.org/show_bug.cgi\?id=(?P<id>\d+)>?\Z'),
        re.compile(r'\A<?https?://bugs.webkit.org/show_bug.cgi\?id=(?P<id>\d+)>?\Z'),
    ]

    @classmethod
    def escape_html(cls, message):
        message = ''.join(cls.ESCAPE_TABLE.get(c, c) for c in message)
        message = re.sub(r'(https?://[^\s<>,:;]+?)(?=[\s<>,:;]|(&gt))', r'<a href="\1">\1</a>', message)
        return re.sub(r'rdar://([^\s<>,:;]+?)(?=[\s<>,:;().]|(&gt))', r'<a href="https://rdar.apple.com/\1">rdar://\1</a>', message)

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, timeout=300, **kwargs)

    def getResultSummary(self):
        if self.results == SUCCESS:
            return {'step': 'Updated pull request'}
        if self.results == FAILURE:
            return {'step': 'Failed to update pull request'}
        return super().getResultSummary()

    @classmethod
    def bug_id_from_log(cls, lines):
        for line in lines:
            for word in line.split():
                for candidate in cls.BUGS_RE:
                    match = candidate.match(word)
                    if match:
                        return match.group('id')
        return None

    @classmethod
    def is_test_gardening(cls, lines):
        for line in lines:
            if line.lstrip().lower().startswith('unreviewed test gardening'):
                return True
        return False

    @defer.inlineCallbacks
    def run(self, BufferLogObserverClass=logobserver.BufferLogObserver):
        self.log_observer = BufferLogObserverClass(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

        title = None
        description = None
        rc = 0

        if not self.setProperty('sensitive', False):
            self.command.append('-{}'.format(self.getProperty('commit_count', 1)))
            rc = yield super().run()

            loglines = self.log_observer.getStdout().splitlines()

            title = loglines[4][4:].rstrip()
            description = ''
            index = 0
            while index < len(loglines):
                line = loglines[index]
                if line.startswith('commit'):
                    if description:
                        description = description[:-1] + f'</pre>\n{70 * "-"}\n'
                    description += f'#### {line.split()[1]}\n<pre>\n'
                    index += 4
                else:
                    description += self.escape_html(line[4:] + '\n')
                    index += 1
            if description:
                description += '</pre>\n'

            bug_id = self.bug_id_from_log(loglines)
            if bug_id:
                self.setProperty('bug_id', bug_id)
            self.setProperty('is_test_gardening', self.is_test_gardening(loglines))

        user = self.getProperty('github.head.user.login', '')
        head = self.getProperty('github.head.ref', '')

        did_update_pr = yield self.update_pr(
            self.getProperty('github.number'),
            title=title,
            description=description,
            base=self.getProperty('github.base.ref', ''),
            head=f"{user}:{head}" if user and head else None,
            repository_url=self.getProperty('repository', ''),
        )
        if not did_update_pr:
            return defer.returnValue(FAILURE)
        return defer.returnValue(rc)

    def doStepIf(self, step):
        return CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES and self.getProperty('github.number')

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)


# FIXME: Share static analyzer steps with build-webkit-org since they have a lot of similarities
class ScanBuild(steps.ShellSequence, ShellMixin):
    name = "scan-build"
    description = ["scanning with static analyzer"]
    descriptionDone = ["scanned with static analyzer"]
    flunkOnFailure = True
    analyzeFailed = False
    bugs = 0
    output_directory = SCAN_BUILD_OUTPUT_DIR

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, timeout=2 * 60 * 60, **kwargs)
        self.commandFailed = False

    @defer.inlineCallbacks
    def run(self):
        self.commands = []
        build_command = f"Tools/Scripts/build-and-analyze --output-dir {os.path.join(self.getProperty('builddir'), f'build/{self.output_directory}')} --configuration {self.build.getProperty('configuration')} "
        build_command += f"--only-smart-pointers --analyzer-path={os.path.join(self.getProperty('builddir'), 'llvm-project/build/bin/clang')} "
        build_command += '--scan-build-path=../llvm-project/clang/tools/scan-build/bin/scan-build --sdkroot=macosx --preprocessor-additions=CLANG_WEBKIT_BRANCH=1 '
        build_command += '2>&1 | python3 Tools/Scripts/filter-test-logs scan-build --output build-log.txt'

        for command in [
            self.shell_command(f"/bin/rm -rf {os.path.join(self.getProperty('builddir'), f'build/{self.output_directory}')}"),
            self.shell_command(build_command)
        ]:
            self.commands.append(util.ShellArg(command=command, logname='stdio'))

        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('stdio', self.log_observer)

        rc = yield super().run()

        log_text = self.log_observer.getStdout()
        index = log_text.rfind('Total issue count:')
        if index != -1:
            total_issue_count_regex = re.compile(r'^Total issue count: (\d+)$', re.MULTILINE)
            match = total_issue_count_regex.search(log_text[index:])
            if match:
                self.bugs += int(match.group(1))

        f_index = log_text.rfind('ANALYZE SUCCEEDED')
        if f_index == -1:
            self.analyzeFailed = True
            rc = FAILURE

        steps_to_add = [
            GenerateS3URL(
                f"{self.getProperty('fullPlatform')}-{self.getProperty('architecture')}-{self.getProperty('configuration')}-{self.name}",
                extension='txt',
                content_type='text/plain',
            ), UploadFileToS3(
                'build-log.txt',
                links={self.name: 'Full build log'},
                content_type='text/plain',
            )
        ]

        if rc == SUCCESS:
            steps_to_add += self.addResultsSteps()
        self.build.addStepsAfterCurrentStep(steps_to_add)

        defer.returnValue(rc)

    def addResultsSteps(self):
        return [ParseStaticAnalyzerResults(), FindUnexpectedStaticAnalyzerResults(expectations=True)]

    def getResultSummary(self):
        status = ''
        if self.analyzeFailed or self.commandFailed:
            status += 'Failed to build and analyze WebKit'
        if self.results == SUCCESS:
            status += f'Found {self.bugs} issues'
        return {'step': status}


class ScanBuildWithoutChange(ScanBuild):
    name = 'scan-build-without-change'
    output_directory = SCAN_BUILD_OUTPUT_DIR + '-baseline'

    def addResultsSteps(self):
        return [ParseStaticAnalyzerResultsWithoutChange(), FindUnexpectedStaticAnalyzerResults(expectations=False)]


class ParseStaticAnalyzerResults(shell.ShellCommandNewStyle):
    name = 'parse-static-analyzer-results'
    description = ['parsing static analyzer results']
    descriptionDone = ['parsed static analyzer results']
    haltOnFailure = True
    result_message = ''
    output_dir = 'new'
    scan_build_output = SCAN_BUILD_OUTPUT_DIR

    def __init__(self, baseline=False, **kwargs):
        self.baseline = baseline  # True if built without PR changes applied
        super().__init__(logEnviron=False, **kwargs)

    @defer.inlineCallbacks
    def run(self):
        self.command = ['python3', 'Tools/Scripts/generate-dirty-files']
        self.command += [os.path.join(self.getProperty('builddir'), f'build/{self.scan_build_output}')]
        self.command += ['--output-dir', os.path.join(self.getProperty('builddir'), f'build/{self.output_dir}')]
        self.command += ['--build-dir', os.path.join(self.getProperty('builddir'), 'build')]

        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('stdio', self.log_observer)

        rc = yield super().run()
        if rc != SUCCESS:
            return defer.returnValue(rc)

        log_text = self.log_observer.getStdout()
        index = log_text.rfind('Total')
        self.result_message = log_text[index:].strip()

        return defer.returnValue(rc)

    def evaluateCommand(self, cmd):
        if cmd.rc != 0:
            self.commandFailed = True
            return FAILURE
        return SUCCESS

    def getResultSummary(self):
        status = ''
        if self.result_message:
            status += f' Issues: {self.result_message}'
        if self.results != SUCCESS:
            status += f' ({Results[self.results]})'

        return {u'step': status}


class ParseStaticAnalyzerResultsWithoutChange(ParseStaticAnalyzerResults):
    name = 'parse-static-analyzer-results-without-change'
    output_dir = 'baseline'
    scan_build_output = SCAN_BUILD_OUTPUT_DIR + '-baseline'


class FindUnexpectedStaticAnalyzerResults(shell.ShellCommandNewStyle):
    name = 'find-unexpected-static-analyzer-results'
    description = ['finding unexpected static analyzer results']
    descriptionDone = ['found unexpected static analyzer results']
    result_message = ''

    def __init__(self, expectations=False, **kwargs):
        self.expectations = expectations  # If true, results will be compared against checked-in expectations. Otherwise, they're compared against a previous run.
        super().__init__(logEnviron=False, **kwargs)

    @defer.inlineCallbacks
    def run(self):
        api_key = os.getenv(RESULTS_SERVER_API_KEY)
        if api_key:
            self.env[RESULTS_SERVER_API_KEY] = api_key
        else:
            self._addToLog('stdio', 'No API key for {} found'.format(RESULTS_DB_URL))

        self.command = ['python3', 'Tools/Scripts/compare-static-analysis-results', os.path.join(self.getProperty('builddir'), 'build/new')]
        self.command += ['--build-output', SCAN_BUILD_OUTPUT_DIR]
        if not self.expectations:
            self.command += ['--archived-dir', os.path.join(self.getProperty('builddir'), 'build/baseline')]
            self.command += ['--scan-build-path', '../llvm-project/clang/tools/scan-build/bin/scan-build']  # Only generate results page on the second comparison
            self.command += ['--delete-results']
            if CURRENT_HOSTNAME in EWS_BUILD_HOSTNAMES and self.getProperty('github.base.ref', DEFAULT_BRANCH) == DEFAULT_BRANCH:
                self.command += [
                    '--builder-name', self.getProperty('buildername', ''),
                    '--build-number', self.getProperty('buildnumber', ''),
                    '--buildbot-worker', self.getProperty('workername', ''),
                    '--buildbot-master', CURRENT_HOSTNAME,
                    '--report', RESULTS_DB_URL,
                    '--architecture', self.getProperty('architecture', ''),
                    '--platform', self.getProperty('platform', ''),
                    '--version', self.getProperty('os_version', ''),
                    '--version-name', self.getProperty('os_name', ''),
                    '--style', self.getProperty('configuration', ''),
                    '--sdk', self.getProperty('build_version', '')
                ]
        else:
            self.command += ['--check-expectations']

        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('stdio', self.log_observer)

        rc = yield super().run()
        if rc != SUCCESS:
            return defer.returnValue(rc)

        self.createResultMessage()

        unexpected_results = self.getProperty('unexpected_failing_files', 0) or self.getProperty('unexpected_new_issues', 0) or self.getProperty('unexpected_passing_files', 0)
        if self.expectations and unexpected_results:
            # If there are unexpected results, rebuild without changes to verify causation
            self.build.addStepsAfterCurrentStep([ValidateChange(verifyBugClosed=False, addURLs=False), RevertAppliedChanges(exclude=['new*', 'scan-build-output*']), ScanBuildWithoutChange()])
        elif unexpected_results:
            # Only save the results if there are failures and it is not the first run
            self.build.addStepsAfterCurrentStep([ArchiveStaticAnalyzerResults(), UploadStaticAnalyzerResults(), ExtractStaticAnalyzerTestResults(), DisplaySaferCPPResults()])
        return defer.returnValue(rc)

    def createResultMessage(self):
        log_text = self.log_observer.getStdout()
        match = re.search(r'^Total (new issues|unexpected issues): (\d+)', log_text, re.MULTILINE)
        if match:
            self.result_message += f"{match.group(2)} new issue{'s' if int(match.group(2)) > 1 else ''} "
            self.setProperty('unexpected_new_issues', int(match.group(2)))
        else:
            self.setProperty('unexpected_new_issues', 0)

        match = re.search(r'^Total (new files|unexpected failing files): (\d+)', log_text, re.MULTILINE)
        if match:
            self.result_message += f"{match.group(2)} failing file{'s' if int(match.group(2)) > 1 else ''} "
            self.setProperty('unexpected_failing_files', int(match.group(2)))
        else:
            self.setProperty('unexpected_failing_files', 0)

        match = re.search(r'^Total (fixed files|unexpected passing files): (\d+)', log_text, re.MULTILINE)
        if match:
            self.result_message += f"{match.group(2)} fixed file{'s' if int(match.group(2)) > 1 else ''}"
            self.setProperty('unexpected_passing_files', int(match.group(2)))
        else:
            self.setProperty('unexpected_passing_files', 0)

    def getResultSummary(self):
        status = ''
        if self.result_message:
            status = f'{self.result_message}'
        elif self.results == SUCCESS:
            status = 'Found no unexpected results'
        if self.results != SUCCESS:
            status += f' ({Results[self.results]})'

        return {u'step': status}


class DisplaySaferCPPResults(buildstep.BuildStep, AddToLogMixin):
    name = 'display-safer-cpp-results'
    resultDirectory = ''
    NUM_TO_DISPLAY = 10
    UPDATE_COMMAND = 'Tools/Scripts/update-safer-cpp-expectations -p {project}'
    CHECKER_ARGS = '--{checker} {files}'

    def __init___(self, **kwargs):
        super().__init__(logEnviron=False, **kwargs)

    @defer.inlineCallbacks
    def run(self):
        commands_for_comment = set()
        num_issues = self.getProperty('unexpected_new_issues', 0)
        self.resultDirectory = f"public_html/results/{self.getProperty('buildername')}/{self.getProperty('change_id')}-{self.getProperty('buildnumber')}"
        unexpected_results_data = self.loadResultsData(os.path.join(self.resultDirectory, SCAN_BUILD_OUTPUT_DIR, 'unexpected_results.json'))
        is_log = yield self.getFilesPerProject(unexpected_results_data, 'passes', commands_for_comment)
        is_log += yield self.getFilesPerProject(unexpected_results_data, 'failures', commands_for_comment)
        if num_issues:
            if not is_log:
                pluralSuffix = 's' if num_issues > 1 else ''
                yield self._addToLog('stdio', f'Ignored {num_issues} pre-existing failure{pluralSuffix}')
            self.addURL("View failures", self.resultDirectoryURL() + SCAN_BUILD_OUTPUT_DIR + "/new-results.html")
        self.createComment(commands_for_comment)
        if self.getProperty('unexpected_failing_files', 0):
            return defer.returnValue(FAILURE)
        return defer.returnValue(SUCCESS)

    def loadResultsData(self, unexpected_results_path):
        with open(unexpected_results_path) as f:
            unexpected_results_data = json.load(f)
        return unexpected_results_data

    @defer.inlineCallbacks
    def getFilesPerProject(self, unexpected_results_data, type, commands_for_comment):
        total_file_list = set()
        is_log = 0
        for project, data in unexpected_results_data[type].items():
            command = self.UPDATE_COMMAND.format(project=project)
            log_content = ''
            for checker, files in data.items():
                if files:
                    total_file_list.update(files)
                    file_str = '\n'.join(files)
                    log_content += f'=> {checker}\n\n{file_str}\n\n'
                    command += ' ' + self.CHECKER_ARGS.format(checker=checker, files=' '.join(files))
            if log_content:
                yield self._addToLog(f'{project}-unexpected-{type}', log_content)
                is_log += 1
                if type == 'passes':
                    commands_for_comment.add(command)
        self.setProperty(f'{type}', list(total_file_list))
        return defer.returnValue(is_log)

    def createComment(self, commands_for_comment):
        num_failures = self.getProperty('unexpected_failing_files', 0)
        num_passes = self.getProperty('unexpected_passing_files', 0)
        num_issues = self.getProperty('unexpected_new_issues', 0)

        if not num_failures and not num_passes:
            return

        results_link = self.resultDirectoryURL() + SCAN_BUILD_OUTPUT_DIR + "/new-results.html"
        build_link = f'{self.master.config.buildbotURL}#/builders/{self.build._builderid}/builds/{self.build.number}'
        formatted_build_link = f'[#{self.getProperty("buildnumber", "")}]({build_link})'
        comment = f'### Safer C++ Build {formatted_build_link}\n'

        if num_failures:
            pluralSuffix = 's' if num_issues > 1 else ''
            comment += f":x: Found [{num_issues} new failure{pluralSuffix}]({results_link}). "
            comment += 'Please address these issues before landing. See [WebKit Guidelines for Safer C++ Programming](https://github.com/WebKit/WebKit/wiki/Safer-CPP-Guidelines).\n(cc @rniwa)\n'
        if num_passes:
            pluralSuffix = 's' if num_passes > 1 else ''
            pluralCommand = 's' if len(commands_for_comment) > 1 else ''
            comment += f'\n:warning: Found {num_passes} fixed file{pluralSuffix}! Please update expectations in `Source/[WebKit/WebCore]/SaferCPPExpectations` by running the following command{pluralCommand} and update your {self.change_type}:\n'
            comment += '\n'.join([f"- `{c}`" for c in commands_for_comment])

        self.setProperty('comment_text', comment)
        # FIXME: Add merging blocked upon failure after initial deployment period
        self.build.addStepsAfterCurrentStep([LeaveComment(), SetBuildSummary()])

    @property
    def change_type(self):
        if self.getProperty('github.number', False):
            return 'pull request'
        return 'patch'

    def doStepIf(self, step):
        return self.getProperty('unexpected_failing_files', 0) or self.getProperty('unexpected_passing_files', 0) or self.getProperty('unexpected_new_issues', 0)

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)

    def resultDirectoryURL(self):
        return f"{S3_RESULTS_URL}{self.resultDirectory.replace('public_html/results/', '') + '/'}"

    def getResultSummary(self):
        num_failures = self.getProperty('unexpected_failing_files', 0)
        num_passes = self.getProperty('unexpected_passing_files', 0)
        num_issues = self.getProperty('unexpected_new_issues', 0)
        failing_files = (", ").join(self.getProperty('failures', [])[:self.NUM_TO_DISPLAY])
        passing_files = (", ").join(self.getProperty('passes', [])[:self.NUM_TO_DISPLAY])
        results_summary = ''

        if num_failures:
            pluralSuffix = 's' if num_issues > 1 else ''
            results_summary = f'Found {num_issues} new failure{pluralSuffix} in {failing_files}'
            if num_failures > self.NUM_TO_DISPLAY:
                results_summary += ' ...'
            self.setProperty('build_finish_summary', results_summary)
        elif num_issues:
            pluralSuffix = 's' if num_issues > 1 else ''
            # FIXME: Display which files are being ignored
            results_summary = f'Ignored {num_issues} pre-existing failure{pluralSuffix}'
            self.setProperty('build_summary', results_summary)
        elif num_passes:
            pluralSuffix = 's' if num_passes > 1 else ''
            results_summary = f'Found {num_passes} fixed file{pluralSuffix}: {passing_files}'
            if num_passes > self.NUM_TO_DISPLAY:
                results_summary += ' ...'
            self.setProperty('build_summary', results_summary)

        if num_passes and num_failures:
            pluralSuffix = 's' if num_passes > 1 else ''
            results_summary += f' and found {num_passes} fixed file{pluralSuffix}: {passing_files}'
            if num_passes > self.NUM_TO_DISPLAY:
                results_summary += ' ...'

        return {'step': results_summary}


class ArchiveStaticAnalyzerResults(shell.ShellCommandNewStyle):
    command = ["Tools/Scripts/generate-static-analysis-archive", "--id-string", WithProperties("Build #%(buildnumber)s"), "--output-root", SCAN_BUILD_OUTPUT_DIR, "--destination", STATIC_ANALYSIS_ARCHIVE_PATH]
    name = "archive-static-analyzer-results"
    description = ["archiving static analyzer results"]
    descriptionDone = ["archived static analyzer results"]
    haltOnFailure = True

    def __init__(self, **kwargs):
        super().__init__(logEnviron=False, **kwargs)


class UploadStaticAnalyzerResults(UploadTestResults):
    name = "upload-static-analyzer-results"
    workersrc = STATIC_ANALYSIS_ARCHIVE_PATH
    haltOnFailure = True


class ExtractStaticAnalyzerTestResults(ExtractTestResults):
    name = 'extract-static-analyzer-test-results'

    def addCustomURLs(self):
        pass
