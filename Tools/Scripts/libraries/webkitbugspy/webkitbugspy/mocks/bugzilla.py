# Copyright (C) 2021 Apple Inc. All rights reserved.
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

from .base import Base

from webkitbugspy import User
from webkitcorepy import mocks


class Bugzilla(Base):
    top = None

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

    def __init__(self, hostname='bugs.example.com', users=None, issues=None):
        super(Bugzilla, self).__init__(hostname, users=users, issues=issues)

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

    def _issue(self, url, id):
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
                summary=issue['title'],
                creation_time=self.time_string(issue['timestamp']),
                status='RESOLVED' if issue['opened'] else 'FIXED',
                creator=self.users[issue['creator'].name].username,
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

    def request(self, method, url, data=None, params=None, auth=None, json=None, **kwargs):
        if not url.startswith('http://') and not url.startswith('https://'):
            return mocks.Response.create404(url)

        stripped_url = url.split('://')[-1]

        match = re.match(r'{}/rest/user\?names=(?P<username>\S+)$'.format(self.hosts[0]), stripped_url)
        if match and method == 'GET':
            return self._user(url, match.group('username'))

        match = re.match(r'{}/rest/bug/(?P<id>\d+)$'.format(self.hosts[0]), stripped_url)
        if match and method == 'GET':
            return self._issue(url, int(match.group('id')))

        match = re.match(r'{}/rest/bug/(?P<id>\d+)\?include_fields=see_also$'.format(self.hosts[0]), stripped_url)
        if match and method == 'GET':
            return self._see_also(url, int(match.group('id')))

        match = re.match(r'{}/rest/bug/(?P<id>\d+)/comment$'.format(self.hosts[0]), stripped_url)
        if match and method == 'GET':
            return self._comments(url, int(match.group('id')))

        return mocks.Response.create404(url)
