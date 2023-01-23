# Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json
import re
import time

from .base import Base

from webkitbugspy import User, Issue, github
from webkitcorepy import mocks


class GitHub(Base, mocks.Requests):
    top = None
    DEFAULT_LABELS = {
        'bug': dict(color='d73a4a', description="Something isn't working"),
        'documentation': dict(color='0075ca', description='Improvements or additions to documentation'),
        'duplicate': dict(color='cfd3d7', description='This issue or pull request already exists'),
        'enhancement': dict(color='a2eeef', description='New feature or request'),
        'good first issue': dict(color='7057ff', description='Good for newcomers'),
        'help wanted': dict(color='008672', description='Extra attention is needed'),
        'invalid': dict(color='e4e669', description="This doesn't seem right"),
        'question': dict(color='d876e3', description='Further information is requested'),
        'wontfix': dict(color='fefefe', description='This will not be worked on'),
    }

    @classmethod
    def transform_user(cls, user):
        return User(
            name=user.name,
            username=user.email.split('@')[0],
            emails=user.emails,
        )

    @classmethod
    def time_string(cls, timestamp):
        from datetime import datetime, timedelta
        return datetime.utcfromtimestamp(timestamp - timedelta(hours=7).seconds).strftime('%Y-%m-%dT%H:%M:%SZ')

    def __init__(self, hostname='github.example.com/WebKit/WebKit', users=None, issues=None, environment=None, projects=None, labels=None):
        hostname, repo = hostname.split('/', 1)
        self.api_remote = 'api.{hostname}/repos/{repo}'.format(hostname=hostname, repo=repo)

        Base.__init__(self, users=users, issues=issues, projects=projects)
        mocks.Requests.__init__(self, hostname, 'api.{}'.format(hostname))

        prefix = self.hosts[0].replace('.', '_').upper()
        if not environment:
            self.users.create(username='username')
        self._environment = environment or mocks.Environment(**{
            '{}_USERNAME'.format(prefix): 'username',
            '{}_TOKEN'.format(prefix): 'token',
        })

        self.labels = labels or self.DEFAULT_LABELS

    def __enter__(self):
        self._environment.__enter__()
        return super(GitHub, self).__enter__()

    def __exit__(self, *args, **kwargs):
        result = super(GitHub, self).__exit__(*args, **kwargs)
        self._environment.__exit__(*args, **kwargs)
        return result

    def _user(self, url, username):
        user = self.users.get(username)
        if not user:
            return mocks.Response.create404(url)
        return mocks.Response.fromJson(dict(
            name=user.name,
            login=user.username,
            email=user.email,
        ), url=url)

    def _labels_for_issue(self, issue):
        ref_labels = self._labels(None).json()
        labels = issue.get('labels', [])

        component = issue.get('component')
        version = issue.get('version')
        for label in labels:
            if label.get('name') == component:
                component = None
            if label.get('name') == version:
                version = None
        for ref in ref_labels:
            if component and ref.get('name') == component:
                labels.append(ref)
            if version and ref.get('name') == version:
                labels.append(ref)
        return labels

    def _issue(self, url, id, data=None):
        if id not in self.issues:
            return mocks.Response(
                url=url,
                headers={'Content-Type': 'text/json'},
                status_code=404,
                text=json.dumps(dict(message="Not Found")),
            )
        issue = self.issues[id]
        if data:
            if data.get('state') == 'opened':
                issue['opened'] = True
            if data.get('state') == 'closed':
                issue['opened'] = False

            if data.get('labels', None) is not None:
                issue['labels'] = []
                issue['component'] = None
                issue['version'] = None
            for label in self._labels(None).json():
                if label.get('name') in data.get('labels', []):
                    issue['labels'].append(label)

        return mocks.Response.fromJson(dict(
            title=issue['title'],
            body=issue['description'],
            user=dict(login=self.users[issue['creator'].name].username),
            created_at=self.time_string(issue['timestamp']),
            state='opened' if issue['opened'] else 'closed',
            labels=self._labels_for_issue(issue),
            milestone=dict(title=issue['milestone']) if issue.get('milestone') else None,
            assignee=dict(login=self.users[issue['assignee'].name].username) if issue['assignee'] else None,
            assignees=[dict(login=self.users[user.name].username) for user in issue.get('watchers', [])],
        ))

    def _comments(self, url, id):
        if id not in self.issues:
            return mocks.Response(
                url=url,
                headers={'Content-Type': 'text/json'},
                status_code=404,
                text=json.dumps(dict(message="Not Found")),
            )
        issue = self.issues[id]
        return mocks.Response.fromJson([
            dict(
                body=comment.content,
                created_at=self.time_string(comment.timestamp),
                user=dict(login=self.users[comment.user.name].username),

            ) for comment in issue['comments']
        ])

    def _post_comment(self, url, id, credentials, data):
        user = self.users.get(credentials.username) if credentials else None
        body = data.get('body')

        if not user or id not in self.issues:
            return mocks.Response.create404(url=url)
        if not body:
            return mocks.Response(status_code=400, url=url)

        timestamp = time.time()
        self.issues[id]['comments'].append(
            Issue.Comment(user=user, timestamp=int(timestamp), content=body),
        )

        return mocks.Response.fromJson(
            url=url, data=dict(
                body=body,
                user=dict(login=user.username),
                updated_at=self.time_string(timestamp),
                created_at=self.time_string(timestamp),
            ),
        )

    def _timelines(self, url, id):
        if id not in self.issues:
            return mocks.Response(
                url=url,
                headers={'Content-Type': 'text/json'},
                status_code=404,
                text=json.dumps(dict(message="Not Found")),
            )
        issue = self.issues[id]
        return mocks.Response.fromJson([
            dict(
                body=comment.content,
                created_at=self.time_string(comment.timestamp),
                actor=dict(login=self.users[comment.user.name].username),
            ) for comment in issue['comments']
        ] + [
            dict(
                event='cross-referenced',
                source=dict(issue=dict(number=reference)),
            ) for reference in issue.get('references', [])
        ], url=url)

    def _labels(self, url):
        result = [dict(
            name=name,
            color=details['color'],
            description=details['description'],
        ) for name, details in self.labels.items()]

        for project in self.projects.values():
            for name in project.get('versions', []):
                result.append(dict(
                    name=name,
                    color=github.Tracker.DEFAULT_VERSION_COLOR,
                ))
            for name, details in project.get('components', {}).items():
                result.append(dict(
                    name=name,
                    description=details['description'],
                    color=github.Tracker.DEFAULT_COMPONENT_COLOR,
                ))

        return mocks.Response.fromJson(list(sorted(result, key=lambda v: v['name'])), url=url)

    def _create(self, url, credentials, data):
        user = self.users.get(credentials.username) if credentials else None
        assignee = self.users.get(data['assignee']) if 'assignee' in data else None

        if not user:
            return mocks.Response.create404(url=url)
        if not all((data.get('title'), data.get('body'))):
            return mocks.Response(status_code=400, url=url)

        id = 1
        while id in self.issues.keys():
            id += 1

        labels = []
        for label in self._labels(None).json():
            if label.get('name') in data.get('labels', []):
                labels.append(label)

        issue = dict(
            id=id,
            title=data['title'],
            timestamp=int(time.time()),
            opened=True,
            creator=user,
            assignee=assignee,
            description=data['body'],
            labels=labels,
            comments=[], watchers=[user, assignee] if assignee else [user],
        )
        self.issues[id] = issue

        return mocks.Response.fromJson(dict(
            number=id,
            title=issue['title'],
            body=issue['description'],
            user=dict(login=self.users[issue['creator'].name].username),
            created_at=self.time_string(issue['timestamp']),
            state='opened' if issue['opened'] else 'closed',
            milestone=None,
            labels=self._labels_for_issue(issue),
            assignee=dict(login=self.users[issue['assignee'].name].username) if issue['assignee'] else None,
            assignees=[dict(login=self.users[user.name].username) for user in issue.get('watchers', [])],
        ), url=url)

    def _add_assignees(self, url, id, credentials, data):
        if id not in self.issues:
            return mocks.Response(
                url=url,
                headers={'Content-Type': 'text/json'},
                status_code=404,
                text=json.dumps(dict(message="Not Found")),
            )
        issue = self.issues[id]

        if len(data.get('assignees', [])) == 0 or not all(assignee in self.users for assignee in data['assignees']):
            return mocks.Response(status_code=400, url=url)

        issue['assignee'] = self.users[data['assignees'][0]]
        issue['assignees'] = data['assignees']
        self.issues[id] = issue

        return mocks.Response.fromJson(dict(
            title=issue['title'],
            body=issue['description'],
            user=dict(login=self.users[issue['creator'].name].username),
            created_at=self.time_string(issue['timestamp']),
            state='opened' if issue['opened'] else 'closed',
            labels=self._labels_for_issue(issue),
            assignee=dict(login=issue['assignee'].username),
            assignees=[dict(login=assignee) for assignee in issue['assignees']],
        ))

    def request(self, method, url, data=None, params=None, auth=None, json=None, **kwargs):
        if not url.startswith('http://') and not url.startswith('https://'):
            return mocks.Response.create404(url)

        stripped_url = url.split('://')[-1]

        match = re.match(r'{}/users/(?P<username>\S+)$'.format(self.hosts[1]), stripped_url)
        if match and method == 'GET':
            return self._user(url, match.group('username'))

        match = re.match(r'{}/user$'.format(self.hosts[1]), stripped_url)
        if match and method == 'GET':
            user = self.users.get(auth.username) if auth else None
            if not user:
                return mocks.Response.create404(url=url)
            return self._user(url, user.username)

        match = re.match(r'{}/issues/(?P<id>\d+)$'.format(self.api_remote), stripped_url)
        if match and method in ('GET', 'PATCH'):
            return self._issue(url, int(match.group('id')), data=json if method == 'PATCH' else None)

        match = re.match(r'{}/issues/(?P<id>\d+)/comments$'.format(self.api_remote), stripped_url)
        if match and method == 'GET':
            return self._comments(url, int(match.group('id')))
        if match and method == 'POST':
            return self._post_comment(url, int(match.group('id')), auth, json)

        match = re.match(r'{}/issues/(?P<id>\d+)/timeline$'.format(self.api_remote), stripped_url)
        if match and method == 'GET':
            return self._timelines(url, int(match.group('id')))

        match = re.match(r'{}/issues/(?P<id>\d+)/labels$'.format(self.api_remote), stripped_url)
        if match and method == 'PUT':
            return self._issue(url, int(match.group('id')), json)

        match = re.match(r'{}/labels$'.format(self.api_remote), stripped_url)
        if match and method == 'GET':
            return self._labels(url)

        match = re.match(r'{}/issues$'.format(self.api_remote), stripped_url)
        if match and method == 'POST':
            return self._create(url, auth, json)

        match = re.match(r'{}/issues/(?P<id>\d+)/assignees$'.format(self.api_remote), stripped_url)
        if match and method == 'POST':
            return self._add_assignees(url, int(match.group('id')), auth, json)

        return mocks.Response.create404(url)
