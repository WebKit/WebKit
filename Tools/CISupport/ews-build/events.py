# Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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

import calendar
import datetime
import json
import os
import re
import time
import twisted

from base64 import b64encode
from buildbot.process.results import SUCCESS, FAILURE, CANCELLED, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.util import httpclientservice, service
from buildbot.www.hooks.github import GitHubEventHandler
from rapidfuzz import fuzz
from .steps import GitHub
from twisted.internet import defer, reactor, task
from twisted.internet.defer import succeed
from twisted.python import log

from .twisted_additions import TwistedAdditions
from .utils import load_password, get_custom_suffix

custom_suffix = get_custom_suffix()


class Events(service.BuildbotService):

    EVENT_SERVER_ENDPOINT = load_password('EVENT_SERVER_ENDPOINT', default=f'https://ews.webkit{custom_suffix}.org/results/')
    MAX_GITHUB_DESCRIPTION = 140
    STEPS_TO_REPORT = [
        'analyze-api-tests-results', 'analyze-compile-webkit-results', 'analyze-jsc-tests-results',
        'analyze-layout-tests-results', 'configuration', 'checkout-pull-request', 'apply-patch',
        'compile-webkit', 'compile-webkit-without-change', 'compile-jsc', 'compile-jsc-without-change',
        'layout-tests', 'layout-tests-repeat-failures', 're-run-layout-tests',
        'run-layout-tests-without-change', 'layout-tests-repeat-failures-without-change',
        'run-layout-tests-in-stress-mode', 'run-layout-tests-in-guard-malloc-stress-mode',
        'run-api-tests', 'run-api-tests-without-change', 're-run-api-tests',
        'scan-build', 'find-unexpected-results', 'display-safer-cpp-results',
        'jscore-test', 'jscore-test-without-change',
        'add-reviewer-to-commit-message', 'commit-patch', 'push-commit-to-webkit-repo', 'canonicalize-commit',
        'build-webkit-org-unit-tests', 'buildbot-check-config', 'buildbot-check-config-for-build-webkit', 'buildbot-check-config-for-ews',
        'ews-unit-tests', 'resultsdbpy-unit-tests',
        'upload-built-product', 'upload-test-results',
        'bindings-tests', 'check-webkit-style',
        'webkitperl-tests', 're-run-webkitperl-tests', 'webkitpy-tests'
    ]
    QUEUES_TO_SKIP_REPORTING = ['__Janitor', 'Safe-Merge-Queue']

    def __init__(self, master_hostname, type_prefix='', name='Events'):
        """
        Initialize the Events Plugin. Sends data to event server on specific buildbot events.
        :param type_prefix: [optional] prefix we want to add to the 'type' field on the json we send
         to event server. (i.e. ews-build, where 'ews-' is the prefix.
        :return: Events Object
        """
        service.BuildbotService.__init__(self, name=name)

        if type_prefix and not type_prefix.endswith("-"):
            type_prefix += "-"
        self.type_prefix = type_prefix
        self.master_hostname = master_hostname

    def sendDataToEWS(self, data):
        if load_password('EWS_API_KEY'):
            data['EWS_API_KEY'] = load_password('EWS_API_KEY')

        TwistedAdditions.request(
            url=self.EVENT_SERVER_ENDPOINT,
            type=b'POST',
            headers={'Content-Type': ['application/json']},
            json=data,
        )

    def sendDataToGitHub(self, repository, sha, data, user=None):
        username, access_token = GitHub.credentials(user=user)

        data['description'] = data.get('description', '')
        if len(data['description']) > self.MAX_GITHUB_DESCRIPTION:
            data['description'] = '{}...'.format(data['description'][:self.MAX_GITHUB_DESCRIPTION - 3])

        auth_header = b64encode('{}:{}'.format(username, access_token).encode('utf-8')).decode('utf-8')

        TwistedAdditions.request(
            url=GitHub.commit_status_url(sha, repository),
            type=b'POST',
            headers={
                'Authorization': ['Basic {}'.format(auth_header)],
                'User-Agent': ['python-twisted/{}'.format(twisted.__version__)],
                'Accept': ['application/vnd.github.v3+json'],
                'Content-Type': ['application/json'],
            }, json=data,
        )

    def getBuilderName(self, build):
        if not (build and 'properties' in build):
            return ''

        return build.get('properties').get('buildername', [''])[0]

    def extractProperty(self, build, property_name):
        if not (build and 'properties' in build and property_name in build['properties']):
            return None

        return build.get('properties').get(property_name)[0]

    @defer.inlineCallbacks
    def buildStarted(self, key, build):
        if not build.get('properties'):
            build['properties'] = yield self.master.db.builds.getBuildProperties(build.get('buildid'))

        if self.getBuilderName(build) in self.QUEUES_TO_SKIP_REPORTING:
            return

        builder = yield self.master.db.builders.getBuilder(build.get('builderid'))
        # Handle both buildbot 2.x (dict) and 4.x (model object)
        if isinstance(builder, dict):
            builder_display_name = builder.get('description', '')
        else:
            builder_display_name = builder.description

        data = {
            "type": self.type_prefix + "build",
            "status": "started",
            "hostname": self.master_hostname,
            "change_id": self.extractProperty(build, 'github.head.sha') or self.extractProperty(build, 'patch_id'),
            "pr_author": self.extractProperty(build, 'github.head.user.login'),
            "pr_number": self.extractProperty(build, 'github.number') or -1,
            "pr_project": self.extractProperty(build, 'project') or '',
            "build_id": build.get('buildid'),
            "builder_id": build.get('builderid'),
            "number": build.get('number'),
            "result": build.get('results'),
            "started_at": build.get('started_at'),
            "complete_at": build.get('complete_at'),
            "state_string": build.get('state_string'),
            "builder_name": self.getBuilderName(build),
            "builder_display_name": builder_display_name,
        }

        self.sendDataToEWS(data)

    def buildFinishedGitHub(self, build):
        sha = self.extractProperty(build, 'github.head.sha')
        repository = self.extractProperty(build, 'repository')

        if not sha or not repository:
            print('Pull request number defined, but sha is {} and repository {}, which are invalid'.format(sha, repository))
            print('Not reporting build result to GitHub')
            return

        data_to_send = dict(
            owner=(self.extractProperty(build, 'owners') or [None])[0],
            repo=(self.extractProperty(build, 'github.head.repo.full_name') or '').split('/')[-1],
            sha=sha,
            target_url='{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, build.get('builderid'), build.get('number')),
            state={
                SUCCESS: 'success',
                WARNINGS: 'success',
                SKIPPED: 'success',
                RETRY: 'pending',
                FAILURE: 'failure'
            }.get(build.get('results'), 'error'),
            description=build.get('state_string'),
            context=build['description'] + custom_suffix,
        )
        self.sendDataToGitHub(repository, sha, data_to_send, user=GitHub.user_for_queue(self.extractProperty(build, 'buildername')))

    @defer.inlineCallbacks
    def buildFinished(self, key, build):
        if not build.get('properties'):
            build['properties'] = yield self.master.db.builds.getBuildProperties(build.get('buildid'))

        if self.getBuilderName(build) in self.QUEUES_TO_SKIP_REPORTING:
            return

        builder = yield self.master.db.builders.getBuilder(build.get('builderid'))
        # Handle both buildbot 2.x (dict) and 4.x (model object)
        if isinstance(builder, dict):
            build['description'] = builder.get('description', '?')
        else:
            build['description'] = builder.description

        if self.extractProperty(build, 'github.number') and (custom_suffix == ''):
            self.buildFinishedGitHub(build)

        data = {
            "type": self.type_prefix + "build",
            "status": "finished",
            "hostname": self.master_hostname,
            "change_id": self.extractProperty(build, 'github.head.sha') or self.extractProperty(build, 'patch_id'),
            "pr_author": self.extractProperty(build, 'github.head.user.login'),
            "pr_number": self.extractProperty(build, 'github.number') or -1,
            "pr_project": self.extractProperty(build, 'project') or '',
            "build_id": build.get('buildid'),
            "builder_id": build.get('builderid'),
            "number": build.get('number'),
            "result": build.get('results'),
            "started_at": build.get('started_at'),
            "complete_at": build.get('complete_at'),
            "state_string": build.get('state_string'),
            "builder_name": self.getBuilderName(build),
            "builder_display_name": builder.get('description', '') if isinstance(builder, dict) else builder.description,
        }

        self.sendDataToEWS(data)

    def stepStarted(self, key, step):
        if step.get('name') not in self.STEPS_TO_REPORT:
            return
        state_string = step.get('state_string')
        if state_string == 'pending':
            state_string = 'Running {}'.format(step.get('name'))

        data = {
            "type": self.type_prefix + "step",
            "status": "started",
            "hostname": self.master_hostname,
            "step_id": step.get('stepid'),
            "build_id": step.get('buildid'),
            "result": step.get('results'),
            "state_string": state_string,
            "started_at": step.get('started_at'),
            "complete_at": step.get('complete_at'),
        }

        self.sendDataToEWS(data)

    def stepFinished(self, key, step):
        if step.get('name') not in self.STEPS_TO_REPORT:
            return
        data = {
            "type": self.type_prefix + "step",
            "status": "finished",
            "hostname": self.master_hostname,
            "step_id": step.get('stepid'),
            "build_id": step.get('buildid'),
            "result": step.get('results'),
            "state_string": step.get('state_string'),
            "started_at": step.get('started_at'),
            "complete_at": step.get('complete_at'),
        }

        self.sendDataToEWS(data)

    @defer.inlineCallbacks
    def startService(self):
        yield service.BuildbotService.startService(self)

        startConsuming = self.master.mq.startConsuming

        self._buildStartedConsumer = yield startConsuming(self.buildStarted, ('builds', None, 'new'))
        self._buildCompleteConsumer = yield startConsuming(self.buildFinished, ('builds', None, 'finished'))
        self._stepStartedConsumer = yield startConsuming(self.stepStarted, ('steps', None, 'started'))
        self._stepFinishedConsumer = yield startConsuming(self.stepFinished, ('steps', None, 'finished'))

    def stopService(self):
        self._buildStartedConsumer.stopConsuming()
        self._buildCompleteConsumer.stopConsuming()
        self._stepStartedConsumer.stopConsuming()
        self._stepFinishedConsumer.stopConsuming()


class logging_disabled(object):
    def __init__(self):
        self.saved_logger = None

    def log_disabled(*args, **kwargs):
        pass

    def __enter__(self):
        self.saved_logger = log.msg
        log.msg = self.log_disabled

    def __exit__(self, type, value, tb):
        log.msg = self.saved_logger


# Based off of webkitscmpy's CommitClassifier, although modified to not raise exceptions
# when provided invalid commit classes.
class CommitClassifier(object):
    class LineFilter(object):
        DEFAULT_FUZZ_RATIO = 90

        @classmethod
        def fuzzy(cls, string, ratio=None):
            ratio = cls.DEFAULT_FUZZ_RATIO if not ratio else ratio
            return lambda x: fuzz.partial_ratio(string, x) >= ratio

        def __init__(self, value):
            self.description = value
            if isinstance(value, str):
                self.do = lambda x: re.search(value, x)
            elif isinstance(value, dict) and 'value' in value:
                self.description = 'fuzz({}, {}%)'.format(value['value'], value.get('ratio', self.DEFAULT_FUZZ_RATIO))
                self.do = self.fuzzy(value['value'], ratio=value.get('ratio'))
            else:
                self.do = None

        def __repr__(self):
            return self.description

        def __call__(self, string):
            return bool(self.do(string)) if self.do else False

    def __init__(self, name=None, pickable=True, headers=None, trailers=None, paths=None, **kwargs):
        self.name = name or '?'
        self.pickable = pickable
        self.headers = [self.LineFilter(header) for header in headers or []]
        self.trailers = [self.LineFilter(trailer) for trailer in trailers or []]
        self.paths = [re.compile(r'^{}'.format(path)) for path in (paths or [])]

        if not self.headers and not self.trailers and not self.paths:
            log.msg(f'Commit class {self.name} matches all commits')

        for argument, _ in kwargs.items():
            log.msg(f'{argument} is not a valid member of CommitClassifier')

    def matches(self, header, trailers, files):
        if not self.headers and not self.trailers and not self.paths:
            return False

        matching_header = bool(self.headers and header)
        matching_trailer = bool(self.trailers and trailers)

        matches_header = self.headers and header and any([f(header) for f in self.headers])
        matches_trailers = self.trailers and trailers and any([any([f(trailer) for f in self.trailers]) for trailer in trailers])

        if (matching_header or matching_trailer) and not matches_header and not matches_trailers:
            return False

        if self.paths and not files:
            return False
        if self.paths and not all([
            any([p.match(path) for p in self.paths]) for path in files
        ]):
            return False
        return True


class GitHubEventHandlerNoEdits(GitHubEventHandler):
    OPEN_STATES = ('open',)
    PUBLIC_REPOS = ('WebKit/WebKit',)
    SENSATIVE_FIELDS = ('github.title',)
    LABEL_PROCESS_DELAY = 10
    ACCOUNTS_TO_IGNORE = ('webkit-early-warning-system', 'webkit-commit-queue')
    TRAILER_RE = re.compile(r'^(?P<key>[^:()\t\/*]+): (?P<value>.+)')

    _commit_classes = []
    _last_commit_classes_refresh = 0
    COMMIT_CLASSES_REFRESH = 60 * 60 * 24  # Refresh our commit-classes once a day
    COMMIT_CLASSES_URL = f'https://raw.githubusercontent.com/{PUBLIC_REPOS[0]}/main/metadata/commit_classes.json'
    DUPE_DETECTOR = {}

    # Because buildbot is single-threaded, we can just place event+hash strings in a process-global
    # record. We create 60 second buckets so that retrying the same event on the same hash will work
    # manually, but when GitHub duplicates events within a few seconds, we ignore the second event.
    @classmethod
    def is_duped(cls, value, bucket=60):
        bucket = int(time.time() // bucket)
        next_bucket = bucket + 1

        for key in list(cls.DUPE_DETECTOR.keys()):
            if key not in (bucket, next_bucket):
                del cls.DUPE_DETECTOR[key]

        for key in [bucket, next_bucket]:
            if key not in cls.DUPE_DETECTOR:
                cls.DUPE_DETECTOR[key] = set()
            if value in cls.DUPE_DETECTOR[key]:
                return True
            cls.DUPE_DETECTOR[key].add(value)
        return False

    @classmethod
    def file_with_status_sign(cls, info):
        if info.get('status') in ('removed', 'renamed'):
            return '--- {}'.format(info['filename'])
        return '+++ {}'.format(info['filename'])

    @classmethod
    @defer.inlineCallbacks
    def commit_classes(cls):
        current_time = time.time()
        if cls._last_commit_classes_refresh + cls.COMMIT_CLASSES_REFRESH > time.time():
            return defer.returnValue(cls._commit_classes)

        try:
            response = yield TwistedAdditions.request(
                url=cls.COMMIT_CLASSES_URL,
                type=b'GET',
                timeout=60,
                logger=log.msg,
            )
            if not response or response.status_code // 100 != 2:
                log.msg(f'Failed to fetch metadata/commit_classes.json from network with status code {response.status_code}')
            else:
                cls._commit_classes = [CommitClassifier(**node) for node in response.json()]
                cls._last_commit_classes_refresh = current_time
        except Exception as e:
            log.msg('Exception while fetching metadata/commit_classes.json from network')
            log.msg(f'    {e}')

        return defer.returnValue(cls._commit_classes)

    @classmethod
    @defer.inlineCallbacks
    def classifiy(cls, message, files):
        classes = yield cls.commit_classes()
        if not classes:
            return defer.returnValue(None)

        lines = message.splitlines()
        header = lines[0]
        trailers = []
        for line in reversed(lines):
            if not cls.TRAILER_RE.match(line):
                break
            trailers.append(line)

        for klass in classes:
            if klass.matches(header, trailers, files):
                return defer.returnValue(klass.name)
        return defer.returnValue(None)

    def _get_payload(self, request):
        # Disable excessive logging inside _get_payload()
        with logging_disabled():
            return super()._get_payload(request)

    def _get_commit_msg(self, repo, sha):
        # Used by buildbot to skip commits based on their commit message.
        # even though we want to grab commit messages for each commit in the PR,
        # we excplicitly _do not_ want to use this functionality, hence why we're
        # disabling it.
        return ''

    @defer.inlineCallbacks
    def _get_commit_messages(self, repo, head, count):
        if not head or not count:
            return defer.returnValue([])

        username, access_token = GitHub.credentials()
        auth_header = b64encode('{}:{}'.format(username, access_token).encode('utf-8')).decode('utf-8')

        response = yield TwistedAdditions.request(
            url="{}/repos/{}/commits".format(self.github_api_endpoint, repo),
            type=b'GET',
            timeout=60,
            params=dict(
                per_page=100,
                sha=head,
            ), headers=dict(
                Authorization=['Basic {}'.format(auth_header)],
                Accept=['application/vnd.github.v3+json'],
            ), logger=log.msg,
        )
        if not response or response.status_code // 100 != 2:
            return defer.returnValue([])

        result = []
        data = response.json()
        for i in range(min(count, len(data))):
            message = (data[i].get('commit') or {}).get('message')
            if message:
                result.append(message)
        return defer.returnValue(result)

    @defer.inlineCallbacks
    def _get_pr_files(self, repo, number):
        # Heavy modification of https://github.com/buildbot/buildbot/blob/v2.10.5/master/buildbot/www/hooks/github.py
        PER_PAGE_LIMIT = 100  # GitHub will list a maximum of 100 files in a single response
        NUM_PAGE_LIMIT = 30  # GitHub stops returning files in a PR after 3000 files

        username, access_token = GitHub.credentials()
        auth_header = b64encode('{}:{}'.format(username, access_token).encode('utf-8')).decode('utf-8')

        page = 1
        files = []
        while page < NUM_PAGE_LIMIT:
            response = yield TwistedAdditions.request(
                url="{}/repos/{}/pulls/{}/files".format(self.github_api_endpoint, repo, number),
                type=b'GET',
                params=dict(
                    per_page=PER_PAGE_LIMIT,
                    page=page,
                ), headers=dict(
                    Authorization=['Basic {}'.format(auth_header)],
                    Accept=['application/vnd.github.v3+json'],
                ), logger=log.msg,
            )
            if not response or response.status_code // 100 != 2:
                break
            data = response.json()
            files += [self.file_with_status_sign(f) for f in data]
            if len(data) < PER_PAGE_LIMIT:
                break
            page += 1

        if not files:
            log.msg('Failed fetching files for PR #{}: response code {}'.format(number, response.status_code if response else '?'))
        return defer.returnValue(files)

    def extractProperties(self, payload):
        result = super().extractProperties(payload)
        if payload.get('base', {}).get('repo', {}).get('full_name') not in self.PUBLIC_REPOS:
            for field in self.SENSATIVE_FIELDS:
                if field in result:
                    del result[field]
        return result

    @defer.inlineCallbacks
    def handle_pull_request(self, payload, event):
        pr_number = payload['number']
        action = payload.get('action')
        state = payload.get('pull_request', {}).get('state')
        labels = [label.get('name') for label in payload.get('pull_request', {}).get('labels', [])]
        sender = payload.get('sender', {}).get('login', '')
        head_sha = payload.get('pull_request', {}).get('head', {}).get('sha')

        if state not in self.OPEN_STATES:
            log.msg("PR #{} is '{}', which triggers nothing".format(pr_number, state))
            return defer.returnValue(([], 'git'))

        if sender in self.ACCOUNTS_TO_IGNORE:
            log.msg(f"PR #{pr_number} ({head_sha}) was updated by '{sender}', ignoring it")
            return defer.returnValue(([], 'git'))

        if custom_suffix != '' and pr_number % 10 != 0:
            # To trigger testing environment on every PR, please comment out this if block and restart buildbot
            log.msg(f'Ignoring PR {pr_number} ({head_sha}) on testing environment.')
            return defer.returnValue(([], 'git'))

        if action == 'labeled' and GitHub.UNSAFE_MERGE_QUEUE_LABEL in labels:
            log.msg(f'PR #{pr_number} ({head_sha}) was labeled for unsafe-merge-queue')
            # 'labeled' is usually an ignored action, override it to force build
            payload['action'] = 'synchronize'
            event = 'unsafe_merge_queue'
        elif action == 'labeled' and GitHub.MERGE_QUEUE_LABEL in labels:
            log.msg(f'PR #{pr_number} ({head_sha}) was labeled for merge-queue')
            # 'labeled' is usually an ignored action, override it to force build
            payload['action'] = 'synchronize'
            event = 'merge_queue'

        result = yield super().handle_pull_request(payload, event)
        changes = result[0]

        # Ignore pull-request events that don't include changes (such as "assigning" a pull-request)
        if not changes or 'properties' not in changes[0]:
            return defer.returnValue(result)

        # We received this exact event in the last 60 seconds, ignore it
        if self.is_duped(f'{event}-{head_sha}'):
            log.msg(f'Discarding duplicate for PR #{pr_number} with hash: {head_sha}')
            return defer.returnValue(([], 'git'))

        log.msg(f'Handling PR #{pr_number} with hash: {head_sha}')
        if event in ('unsafe_merge_queue', 'merge_queue'):
            yield task.deferLater(reactor, self.LABEL_PROCESS_DELAY, lambda: None)

        change = changes[0]

        repo_full_name = payload['repository']['full_name']
        head_sha = payload['pull_request']['head']['sha']
        number_commits = payload['pull_request']['commits']
        messages = yield self._get_commit_messages(repo_full_name, head_sha, number_commits)

        raw_files = [file.split(' ', 1)[-1] for file in change.get('files') or []]

        classes = set()
        for message in messages:
            classification = yield self.classifiy(message, raw_files)
            classes.add(classification or 'Unclassified')
        change['properties'].update({
            'classification': list(classes),
            # Only track acionable labels, since bug category labels may reveal information about security bugs
            'github_labels': [label for label in labels if label in GitHub.LABELS],
        })

        return defer.returnValue(([change], result[1]))
