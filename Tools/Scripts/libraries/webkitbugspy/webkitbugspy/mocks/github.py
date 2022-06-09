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
import os
import re

from .base import Base

from webkitbugspy import User
from webkitcorepy import mocks


class GitHub(Base):
    top = None

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

    def __init__(self, hostname='github.example.com/WebKit/WebKit', users=None, issues=None):
        hostname, repo = hostname.split('/', 1)
        self.api_host = 'api.{hostname}/repos/{repo}'.format(hostname=hostname, repo=repo)

        super(GitHub, self).__init__(hostname, 'api.{}'.format(hostname), users=users, issues=issues)

        self._environment = None

    def __enter__(self):
        prefix = self.hosts[0].replace('.', '_').upper()
        username_key = '{}_USERNAME'.format(prefix)
        token_key = '{}_TOKEN'.format(prefix)
        self._environment = {
            username_key: os.environ.get(username_key),
            token_key: os.environ.get(token_key),
        }
        os.environ[username_key] = 'username'
        os.environ[token_key] = 'token'

        return super(GitHub, self).__enter__()

    def __exit__(self, *args, **kwargs):
        result = super(GitHub, self).__exit__(*args, **kwargs)
        for key in self._environment.keys():
            if self._environment[key]:
                os.environ[key] = self._environment[key]
            else:
                del os.environ[key]
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

    def _issue(self, url, id):
        if id not in self.issues:
            return mocks.Response(
                url=url,
                headers={'Content-Type': 'text/json'},
                status_code=404,
                text=json.dumps(dict(message="Not Found")),
            )
        issue = self.issues[id]
        return mocks.Response.fromJson(dict(
            title=issue['title'],
            body=issue['description'],
            user=dict(login=self.users[issue['creator'].name].username),
            created_at=self.time_string(issue['timestamp']),
            state='opened' if issue['opened'] else 'closed',
            assignee=dict(login=self.users[issue['assignee'].name].username),
            assignees=[dict(login=self.users[user.name].username) for user in issue['watchers']],
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
        ])

    def request(self, method, url, data=None, params=None, auth=None, json=None, **kwargs):
        if not url.startswith('http://') and not url.startswith('https://'):
            return mocks.Response.create404(url)

        stripped_url = url.split('://')[-1]

        match = re.match(r'{}/users/(?P<username>\S+)$'.format(self.hosts[1]), stripped_url)
        if match and method == 'GET':
            return self._user(url, match.group('username'))

        match = re.match(r'{}/issues/(?P<id>\d+)$'.format(self.api_host), stripped_url)
        if match and method == 'GET':
            return self._issue(url, int(match.group('id')))

        match = re.match(r'{}/issues/(?P<id>\d+)/comments$'.format(self.api_host), stripped_url)
        if match and method == 'GET':
            return self._comments(url, int(match.group('id')))

        match = re.match(r'{}/issues/(?P<id>\d+)/timeline$'.format(self.api_host), stripped_url)
        if match and method == 'GET':
            return self._timelines(url, int(match.group('id')))

        return mocks.Response.create404(url)
