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
import sys
import time
import urllib

from .base import Base

from webkitbugspy import User, Issue
from webkitbugspy.mocks.radar import Radar as RadarMock
from webkitcorepy import mocks


class Bugzilla(Base, mocks.Requests):
    top = None
    CREDENTIAL_RE = re.compile(r'\??login=(?P<login>\S+)\&password=(?P<password>\S+)\&?$')

    @classmethod
    def time_string(cls, timestamp):
        from datetime import datetime, timedelta
        return datetime.utcfromtimestamp(timestamp - timedelta(hours=7).seconds).strftime('%Y-%m-%dT%H:%M:%SZ')

    @classmethod
    def transform_user(cls, user):
        return User(
            name=user.name,
            username=user.email,
            emails=user.emails,
        )

    def __init__(self, hostname='bugs.example.com', users=None, issues=None, environment=None, projects=None):
        Base.__init__(self, users=users, issues=issues, projects=projects)
        mocks.Requests.__init__(self, hostname)
        self._comment_count = 1

        prefix = self.hosts[0].replace('.', '_').upper()
        self._environment = environment or mocks.Environment(**{
            '{}_USERNAME'.format(prefix): 'username',
            '{}_PASSWORD'.format(prefix): 'password',
        })

        for issue in self.issues.values():
            self._comment_count += (1 + len(issue.get('comments', [])))

    def __enter__(self):
        self._environment.__enter__()
        return super(Bugzilla, self).__enter__()

    def __exit__(self, *args, **kwargs):
        result = super(Bugzilla, self).__exit__(*args, **kwargs)
        self._environment.__exit__(*args, **kwargs)
        return result

    def _user(self, url, username):
        user = self.users[username]
        if not user:
            return mocks.Response(
                url=url,
                headers={'Content-Type': 'text/json'},
                status_code=404,
                text=json.dumps(dict(
                    code=51,
                    error=True,
                    message="There is no user named '{}'. Either you mis-typed the name or that user has not yet registered for a Bugzilla account.".format(username),
                )),
            )
        return mocks.Response.fromJson(dict(
            users=[dict(
                name=user.username,
                real_name=user.name,
            )],
        ), url=url)

    def _user_for_credentials(self, credentials):
        if not credentials:
            return None
        match = self.CREDENTIAL_RE.match(credentials)
        if not match:
            return None
        return self.users.get(match.group('login'))

    def _issue(self, url, id, credentials=None, data=None):
        user = self._user_for_credentials(credentials)

        if id not in self.issues:
            return mocks.Response(
                url=url,
                headers={'Content-Type': 'text/json'},
                status_code=404,
                text=json.dumps(dict(
                    code=101,
                    error=True,
                    message="Bug #{} does not exist.".format(id),
                )),
            )
        issue = self.issues[id]
        if data:
            if not user:
                return mocks.Response(status_code=401, url=url)

            if self.users.get(data.get('assigned_to')):
                issue['assignee'] = self.users[data['assigned_to']]
            if data.get('status'):
                if not issue['opened'] and data['status'] != 'REOPENED' and not data.get('comment'):
                    return mocks.Response(
                        url=url,
                        headers={'Content-Type': 'text/json'},
                        status_code=400,
                        text=json.dumps(dict(
                            code=32000,
                            error=True,
                            message="You have to specify a comment when changing the Status of a bug from RESOLVED to REOPENED.",
                        )),
                    )
                issue['opened'] = data['status'] == 'REOPENED'
            if data.get('comment'):
                issue['comments'].append(
                    Issue.Comment(user=user, timestamp=int(time.time()), content=data['comment']['body']),
                )
            if data.get('project'):
                issue['product'] = data['project']
            if data.get('component'):
                issue['component'] = data['component']
            if data.get('version'):
                issue['version'] = data['version']

            if not issue.get('watchers', None):
                issue['watchers'] = []
            for candidate in data.get('cc', {}).get('add', []):
                if candidate in [u.emails for u in issue['watchers']]:
                    continue
                candidate = self.users.get(candidate)
                if not candidate:
                    continue
                issue['watchers'].append(candidate)
                if 'Radar' not in candidate.name or 'Bug Importer' not in candidate.name or 'InRadar' in data.get('keywords', {}).get('add', []):
                    continue
                radar_id = issue['id']
                if RadarMock.top:
                    radar_id = RadarMock.top.add(dict(
                        title='{} ({})'.format(issue['description'], issue['id']),
                        timestamp=time.time(),
                        opened=True,
                        creator=candidate,
                        assignee=user,
                        description='From <https://{}/show_bug.cgi?id={}>:\n\n{}'.format(self.hosts[0], issue['id'], issue['description']),
                        project=issue.get('project'),
                        component=issue.get('component'),
                        watchers=[candidate, user],
                        keywords=issue.get('keywords', []),
                    ))
                issue['comments'].append(
                    Issue.Comment(user=candidate, timestamp=int(time.time()), content='<rdar://problem/{}>'.format(radar_id)),
                )

        return mocks.Response.fromJson(dict(
            bugs=[dict(
                id=id,
                summary=issue['title'],
                creation_time=self.time_string(issue['timestamp']),
                status='REOPENED' if issue['opened'] else 'RESOLVED',
                resolution='' if issue['opened'] else 'FIXED',
                creator=self.users[issue['creator'].name].username,
                product=issue.get('project'),
                component=issue.get('component'),
                version=issue.get('version'),
                keywords=issue.get('keywords', []),
                creator_detail=dict(
                    email=issue['creator'].email,
                    name=self.users[issue['creator'].name].username,
                    real_name=issue['creator'].name,
                ), assigned_to=self.users[issue['assignee'].name].username,
                assigned_to_detail=dict(
                    email=issue['assignee'].email,
                    name=self.users[issue['assignee'].name].username,
                    real_name=issue['assignee'].name,
                ), cc=[self.users[user.name].username for user in issue.get('watchers', [])],
                cc_detail=[
                    dict(
                        email=user.email,
                        name=self.users[user.name].username,
                        real_name=user.name,
                    ) for user in issue.get('watchers', [])
                ], see_also=[
                    'https://{}/show_bug.cgi?id={}'.format(self.hosts[0], n) for n in issue.get('references', [])
                ],
            )],
        ), url=url)

    def _see_also(self, url, id):
        if id not in self.issues:
            return mocks.Response(
                url=url,
                headers={'Content-Type': 'text/json'},
                status_code=404,
                text=json.dumps(dict(
                    code=101,
                    error=True,
                    message="Bug #{} does not exist.".format(id),
                )),
            )
        issue = self.issues[id]
        return mocks.Response.fromJson(dict(
            bugs=[dict(
                id=id,
                see_also=[
                    'https://{}/show_bug.cgi?id={}'.format(self.hosts[0], n) for n in issue.get('references', [])
                ],
            )],
        ), url=url)

    def _comments(self, url, id):
        if id not in self.issues:
            return mocks.Response(
                url=url,
                headers={'Content-Type': 'text/json'},
                status_code=404,
                text=json.dumps(dict(
                    code=101,
                    error=True,
                    message="Bug #{} does not exist.".format(id),
                )),
            )

        issue = self.issues[id]
        return mocks.Response.fromJson(dict(
            bugs={str(id): dict(comments=[
                dict(
                    bug_id=id,
                    creator=self.users[issue['creator'].name].username,
                    creation_time=self.time_string(issue['timestamp']),
                    time=self.time_string(issue['timestamp']),
                    text=issue['description'],
                ),
            ] + [
                dict(
                    bug_id=id,
                    creator=self.users[comment.user.name].username,
                    creation_time=self.time_string(comment.timestamp),
                    time=self.time_string(comment.timestamp),
                    text=comment.content,
                ) for comment in issue['comments']
            ])},
        ), url=url)

    def _post_comment(self, url, id, credentials, data):
        comment = data.get('comment', None)
        user = self._user_for_credentials(credentials)

        if not user:
            return mocks.Response(status_code=401, url=url)
        if id not in self.issues:
            return mocks.Response.create404(url=url)
        if not comment:
            return mocks.Response(status_code=400, url=url)

        self.issues[id]['comments'].append(
            Issue.Comment(user=user, timestamp=int(time.time()), content=comment),
        )

        self._comment_count += 1
        return mocks.Response(
            status_code=201,
            text=json.dumps(dict(id=self._comment_count)),
            url=url,
        )

    def _product_details(self, url, id):
        for name, product in self.projects.items():
            if product['id'] != id:
                continue
            return mocks.Response.fromJson(
                dict(products=[dict(
                    name=name,
                    description=product['description'],
                    is_active=True,
                    components=[dict(
                        name=component,
                        description=details['description'],
                        is_active=True,
                    ) for component, details in product['components'].items()],
                    versions=[dict(
                        name=version,
                        is_active=True,
                    ) for version in product['versions']],
                )]), url=url,
            )

        return mocks.Response.fromJson(
            dict(products=[]),
            url=url,
        )

    def _create(self, url, credentials, data):
        user = self._user_for_credentials(credentials)
        assignee = self.users.get(data['assigned_to']) if 'assigned_to' in data else None
        if not user:
            return mocks.Response(status_code=401, url=url)

        if not all((data.get('summary'), data.get('description'), data.get('product'), data.get('component'), data.get('version'))):
            return mocks.Response(
                status_code=400,
                text=json.dumps(dict(message='Failed to create bug')),
                url=url,
            )

        id = 1
        while id in self.issues.keys():
            id += 1

        self.issues[id] = dict(
            id=id,
            title=data['summary'],
            timestamp=int(time.time()),
            opened=True,
            creator=user,
            assignee=assignee,
            description=data['description'],
            project=data.get('product'),
            component=data.get('component'),
            version=data.get('version'),
            keywords=[],
            comments=[], watchers=[user, assignee] if assignee else [user],
        )

        return mocks.Response(
            status_code=200,
            text=json.dumps(dict(id=id)),
            url=url,
        )

    def request(self, method, url, data=None, params=None, auth=None, json=None, **kwargs):
        if not url.startswith('http://') and not url.startswith('https://'):
            return mocks.Response.create404(url)

        if sys.version_info >= (3, 0):
            stripped_url = urllib.parse.unquote(url).split('://')[-1]
        else:
            stripped_url = urllib.unquote(url).split('://')[-1]

        match = re.match(r'{}/rest/user\?(?P<credentials>login=\S+\&password=\S+\&)?names=(?P<username>\S+)$'.format(self.hosts[0]), stripped_url)
        if match and method == 'GET':
            return self._user(url, match.group('username'))

        match = re.match(r'{}/rest/bug/(?P<id>\d+)(?P<credentials>\?login=\S+\&password=\S+)?$'.format(self.hosts[0]), stripped_url)
        if match and method == 'GET':
            return self._issue(url, int(match.group('id')))
        if match and method == 'PUT':
            return self._issue(url, int(match.group('id')), match.group('credentials'), data=json)

        match = re.match(r'{}/rest/bug/(?P<id>\d+)\?(?P<credentials>login=\S+\&password=\S+\&)?include_fields=see_also$'.format(self.hosts[0]), stripped_url)
        if match and method == 'GET':
            return self._see_also(url, int(match.group('id')))

        match = re.match(r'{}/rest/bug/(?P<id>\d+)/comment(?P<credentials>\?login=\S+\&password=\S+)?$'.format(self.hosts[0]), stripped_url)
        if match and method == 'GET':
            return self._comments(url, int(match.group('id')))
        if match and method == 'POST':
            return self._post_comment(url, int(match.group('id')), match.group('credentials'), json)

        match = re.match(r'{}/rest/product_enterable(?P<credentials>\?login=\S+\&password=\S+)?$'.format(self.hosts[0]), stripped_url)
        if match and method == 'GET':
            return mocks.Response.fromJson(dict(
                ids=[project['id'] for project in self.projects.values()],
            ), url=url)

        match = re.match(r'{}/rest/product/(?P<id>\d+)(?P<credentials>\?login=\S+\&password=\S+)?$'.format(self.hosts[0]), stripped_url)
        if match and method == 'GET':
            return self._product_details(url, int(match.group('id')))

        match = re.match(r'{}/rest/bug(?P<credentials>\?login=\S+\&password=\S+)?$'.format(self.hosts[0]), stripped_url)
        if match and method == 'POST':
            return self._create(url, match.group('credentials'), json)
        return mocks.Response.create404(url)
