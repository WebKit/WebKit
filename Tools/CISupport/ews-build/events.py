# Copyright (C) 2019, 2022 Apple Inc. All rights reserved.
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
import time
import twisted

from base64 import b64encode
from buildbot.process.results import SUCCESS, FAILURE, CANCELLED, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.util import httpclientservice, service
from buildbot.www.hooks.github import GitHubEventHandler
from steps import GitHub
from twisted.internet import defer
from twisted.internet import reactor
from twisted.internet.defer import succeed
from twisted.python import log
from twisted.web.client import Agent
from twisted.web.http_headers import Headers
from twisted.web.iweb import IBodyProducer
from zope.interface import implementer

custom_suffix = '-uat' if os.getenv('BUILDBOT_UAT') else ''

@implementer(IBodyProducer)
class JSONProducer(object):
    """
    Perform JSON asynchronously as to not lock the buildbot main event loop
    """

    def __init__(self, data):
        try:
            self.body = json.dumps(data, default=self.json_serialize_datetime).encode('utf-8')
        except TypeError:
            self.body = ''
        self.length = len(self.body)

    def startProducing(self, consumer):
        if self.body:
            consumer.write(self.body)
        return succeed(None)

    def pauseProducing(self):
        pass

    def stopProducing(self):
        pass

    def json_serialize_datetime(self, obj):
        """
        Serializing buildbot dates into UNIX epoch timestamps.
        """
        if isinstance(obj, datetime.datetime):
            return int(calendar.timegm(obj.timetuple()))

        raise TypeError("Type %s not serializable" % type(obj))


class Events(service.BuildbotService):

    EVENT_SERVER_ENDPOINT = 'https://ews.webkit{}.org/results/'.format(custom_suffix).encode()
    MAX_GITHUB_DESCRIPTION = 140
    SHORT_STEPS = (
        'configure-build',
        'validate-change',
        'configuration',
        'clean-up-git-repo',
        'fetch-branch-references',
        'show-identifier',
        'update-working-directory',
        'apply-patch',
        'kill-old-processes',
        'set-build-summary',
    )

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
        if os.getenv('EWS_API_KEY', None):
            data['EWS_API_KEY'] = os.getenv('EWS_API_KEY')
        agent = Agent(reactor)
        body = JSONProducer(data)

        agent.request(b'POST', self.EVENT_SERVER_ENDPOINT, Headers({'Content-Type': ['application/json']}), body)

    def sendDataToGitHub(self, repository, sha, data, user=None):
        username, access_token = GitHub.credentials(user=user)

        data['description'] = data.get('description', '')
        if len(data['description']) > self.MAX_GITHUB_DESCRIPTION:
            data['description'] = '{}...'.format(data['description'][:self.MAX_GITHUB_DESCRIPTION - 3])

        auth_header = b64encode('{}:{}'.format(username, access_token).encode('utf-8')).decode('utf-8')

        agent = Agent(reactor)
        body = JSONProducer(data)
        d = agent.request(b'POST', GitHub.commit_status_url(sha, repository).encode('utf-8'), Headers({
            'Authorization': ['Basic {}'.format(auth_header)],
            'User-Agent': ['python-twisted/{}'.format(twisted.__version__)],
            'Accept': ['application/vnd.github.v3+json'],
            'Content-Type': ['application/json'],
        }), body)

    def getBuilderName(self, build):
        if not (build and 'properties' in build):
            return ''

        return build.get('properties').get('buildername')[0]

    def extractProperty(self, build, property_name):
        if not (build and 'properties' in build and property_name in build['properties']):
            return None

        return build.get('properties').get(property_name)[0]

    @defer.inlineCallbacks
    def buildStarted(self, key, build):
        if not build.get('properties'):
            build['properties'] = yield self.master.db.builds.getBuildProperties(build.get('buildid'))

        builder = yield self.master.db.builders.getBuilder(build.get('builderid'))
        builder_display_name = builder.get('description')

        data = {
            "type": self.type_prefix + "build",
            "status": "started",
            "hostname": self.master_hostname,
            "change_id": self.extractProperty(build, 'github.head.sha') or self.extractProperty(build, 'patch_id'),
            "pr_id": self.extractProperty(build, 'github.number') or -1,
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
        if not build.get('steps'):
            build['steps'] = yield self.master.db.steps.getSteps(build.get('buildid'))

        builder = yield self.master.db.builders.getBuilder(build.get('builderid'))
        build['description'] = builder.get('description', '?')

        if self.extractProperty(build, 'github.number'):
            self.buildFinishedGitHub(build)

        data = {
            "type": self.type_prefix + "build",
            "status": "finished",
            "hostname": self.master_hostname,
            "change_id": self.extractProperty(build, 'github.head.sha') or self.extractProperty(build, 'patch_id'),
            "pr_id": self.extractProperty(build, 'github.number') or -1,
            "pr_project": self.extractProperty(build, 'project') or '',
            "build_id": build.get('buildid'),
            "builder_id": build.get('builderid'),
            "number": build.get('number'),
            "result": build.get('results'),
            "started_at": build.get('started_at'),
            "complete_at": build.get('complete_at'),
            "state_string": build.get('state_string'),
            "builder_name": self.getBuilderName(build),
            "builder_display_name": builder.get('description'),
            "steps": build.get('steps'),
        }

        self.sendDataToEWS(data)

    @defer.inlineCallbacks
    def stepStartedGitHub(self, build, state_string):
        sha = self.extractProperty(build, 'github.head.sha')
        repository = self.extractProperty(build, 'repository')
        if not sha or not repository:
            print('Pull request number defined, but sha is {} and repository {}, which are invalid'.format(sha, repository))
            print('Not reporting step started to GitHub')
            return

        builder = yield self.master.db.builders.getBuilder(build.get('builderid'))

        data_to_send = dict(
            owner=(self.extractProperty(build, 'owners') or [None])[0],
            repo=(self.extractProperty(build, 'github.head.repo.full_name') or '').split('/')[-1],
            sha=sha,
            target_url='{}#/builders/{}/builds/{}'.format(self.master.config.buildbotURL, build.get('builderid'), build.get('number')),
            state={
                SUCCESS: 'pending',
                WARNINGS: 'pending',
                FAILURE: 'failure',
                EXCEPTION: 'error',
            }.get(build.get('results'), 'pending'),
            description=state_string,
            context=builder.get('description', '?') + custom_suffix,
        )
        self.sendDataToGitHub(repository, sha, data_to_send, user=GitHub.user_for_queue(self.extractProperty(build, 'buildername')))

    @defer.inlineCallbacks
    def stepStarted(self, key, step):
        state_string = step.get('state_string')
        if state_string == 'pending':
            state_string = 'Running {}'.format(step.get('name'))

        build = yield self.master.db.builds.getBuild(step.get('buildid'))
        if not build.get('properties'):
            build['properties'] = yield self.master.db.builds.getBuildProperties(step.get('buildid'))

        # We need to force the defered properties to resolve
        if build['properties'].get('github.number') and build.get('step') not in self.SHORT_STEPS:
            self.stepStartedGitHub(build, state_string)

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


class GitHubEventHandlerNoEdits(GitHubEventHandler):
    OPEN_STATES = ('open',)
    PUBLIC_REPOS = ('WebKit/WebKit',)
    SENSATIVE_FIELDS = ('github.title',)
    UNSAFE_MERGE_QUEUE_LABEL = 'unsafe-merge-queue'
    MERGE_QUEUE_LABEL = 'merge-queue'
    LABEL_PROCESS_DELAY = 10
    ACCOUNTS_TO_IGNORE = ('webkit-early-warning-system', 'webkit-commit-queue')

    @classmethod
    def file_with_status_sign(cls, info):
        if info.get('status') in ('removed', 'renamed'):
            return '--- {}'.format(info['filename'])
        return '+++ {}'.format(info['filename'])

    def _get_commit_msg(self, repo, sha):
        return ''

    @defer.inlineCallbacks
    def _get_pr_files(self, repo, number):
        # Copied from https://github.com/buildbot/buildbot/blob/v2.10.5/master/buildbot/www/hooks/github.py to include added/modified/deleted
        headers = {"User-Agent": "Buildbot"}
        if self._token:
            headers["Authorization"] = "token " + self._token

        url = "/repos/{}/pulls/{}/files".format(repo, number)
        http = yield httpclientservice.HTTPClientService.getService(
            self.master,
            self.github_api_endpoint,
            headers=headers,
            debug=self.debug,
            verify=self.verify,
        )
        res = yield http.get(url)
        if 200 <= res.code < 300:
            data = yield res.json()
            return [self.file_with_status_sign(f) for f in data]

        log.msg('Failed fetching PR files: response code {}'.format(res.code))
        return []

    def extractProperties(self, payload):
        result = super(GitHubEventHandlerNoEdits, self).extractProperties(payload)
        if payload.get('base', {}).get('repo', {}).get('full_name') not in self.PUBLIC_REPOS:
            for field in self.SENSATIVE_FIELDS:
                if field in result:
                    del result[field]
        return result

    def handle_pull_request(self, payload, event):
        pr_number = payload['number']
        action = payload.get('action')
        state = payload.get('pull_request', {}).get('state')
        labels = [label.get('name') for label in payload.get('pull_request', {}).get('labels', [])]
        sender = payload.get('sender', {}).get('login', '')

        if state not in self.OPEN_STATES:
            log.msg("PR #{} is '{}', which triggers nothing".format(pr_number, state))
            return ([], 'git')

        if action == 'labeled' and self.UNSAFE_MERGE_QUEUE_LABEL in labels:
            log.msg("PR #{} was labeled for unsafe-merge-queue".format(pr_number))
            # 'labeled' is usually an ignored action, override it to force build
            payload['action'] = 'synchronize'
            time.sleep(self.LABEL_PROCESS_DELAY)
            return super(GitHubEventHandlerNoEdits, self).handle_pull_request(payload, 'unsafe_merge_queue')
        if action == 'labeled' and self.MERGE_QUEUE_LABEL in labels:
            log.msg("PR #{} was labeled for merge-queue".format(pr_number))
            # 'labeled' is usually an ignored action, override it to force build
            payload['action'] = 'synchronize'
            time.sleep(self.LABEL_PROCESS_DELAY)
            return super(GitHubEventHandlerNoEdits, self).handle_pull_request(payload, 'merge_queue')

        if sender in self.ACCOUNTS_TO_IGNORE:
            log.msg(f"PR #{pr_number} was updated by '{sender}', ignore it")
            return ([], 'git')

        return super(GitHubEventHandlerNoEdits, self).handle_pull_request(payload, event)
