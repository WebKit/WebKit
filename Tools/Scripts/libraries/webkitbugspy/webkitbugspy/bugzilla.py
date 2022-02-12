# Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

import calendar
import re
import requests
import sys
import time
import webkitcorepy

from .issue import Issue
from .tracker import Tracker as GenericTracker

from datetime import datetime
from webkitbugspy import User


class Tracker(GenericTracker):
    ROOT_RE = re.compile(r'\Ahttps?://(?P<domain>\S+)\Z')
    RE_TEMPLATES = [
        r'\Ahttps?://{}/show_bug.cgi\?id=(?P<id>\d+)\Z',
        r'\A{}/show_bug.cgi\?id=(?P<id>\d+)\Z',
    ]

    class Encoder(GenericTracker.Encoder):
        @webkitcorepy.decorators.hybridmethod
        def default(context, obj):
            if isinstance(obj, Tracker):
                result = dict(
                    type='bugzilla',
                    url=obj.url,
                )
                if obj._res[len(Tracker.RE_TEMPLATES):]:
                    result['res'] = [compiled.pattern for compiled in obj._res[len(Tracker.RE_TEMPLATES):]]
                return result
            if isinstance(context, type):
                raise TypeError('Cannot invoke parent class when classmethod')
            return super(Tracker.Encoder, context).default(obj)


    def __init__(self, url, users=None, res=None):
        super(Tracker, self).__init__(users=users)

        match = self.ROOT_RE.match(url)
        if not match:
            raise TypeError("'{}' is not a valid bugzilla url".format(url))
        self.url = url
        self._res = [
            re.compile(template.format(match.group('domain')))
            for template in self.RE_TEMPLATES
        ] + (res or [])

    def user(self, name=None, username=None, email=None):
        user = super(Tracker, self).user(name=name, username=username, email=email)
        if user:
            return user
        if name and username and email:
            return self.users.create(
                name=name,
                username=username,
                emails=[email],
            )

        if not username and not email:
            raise RuntimeError("Failed to find username for '{}'".format(name))
        response = requests.get('{url}/rest/user{query}'.format(
            url=self.url,
            query=self._login_arguments(
                required=False,
                query='names={name}'.format(name=username or email or name),
            ),
        ))
        response = response.json().get('users') if response.status_code // 100 == 2 else None
        if not response:
            return self.users.create(
                name=name,
                username=username,
                emails=[email] if email else None,
            )

        return self.users.create(
            name=response[0]['real_name'],
            username=response[0]['name'],
            emails=[response[0]['name']],
        )

    def from_string(self, string):
        for regex in self._res:
            match = regex.match(string)
            if match:
                return self.issue(int(match.group('id')))
        return None

    def credentials(self, required=True):
        return webkitcorepy.credentials(
            url=self.url,
            required=required,
            prompt=self.url.split('//')[-1],
        )

    def _login_arguments(self, required=False, query=None):
        username, password = self.credentials(required=required)
        if not username or not password:
            return '?{}'.format(query) if query else ''
        return '?login={username}&password={password}{query}'.format(
            username=username,
            password=password,
            query='&{}'.format(query) if query else '',
        )

    @webkitcorepy.decorators.Memoize()
    def me(self):
        username, _ = self.credentials(required=True)
        return self.user(username=username)

    def issue(self, id):
        return Issue(id=int(id), tracker=self)

    def populate(self, issue, member=None):
        issue._link = '{}/show_bug.cgi?id={}'.format(self.url, issue.id)

        if member in ['title', 'timestamp', 'creator', 'opened', 'assignee', 'watchers']:
            response = requests.get('{}/rest/bug/{}{}'.format(self.url, issue.id, self._login_arguments(required=False)))
            response = response.json().get('bugs', []) if response.status_code == 200 else None
            if response:
                response = response[0]
                issue._title = response['summary']
                issue._timestamp = int(calendar.timegm(datetime.strptime(response['creation_time'], '%Y-%m-%dT%H:%M:%SZ').timetuple()))
                if response.get('creator_detail'):
                    issue._creator = self.user(
                        name=response['creator_detail'].get('real_name'),
                        username=response['creator_detail'].get('name'),
                        email=response['creator_detail'].get('email'),
                    )
                else:
                    issue._creator = self.user(username=response['creator']) if response.get('creator') else None
                issue._opened = response['status'] != 'RESOLVED'
                if response.get('assigned_to_detail'):
                    issue._assignee = self.user(
                        name=response['assigned_to_detail'].get('real_name'),
                        username=response['assigned_to_detail'].get('name'),
                        email=response['assigned_to_detail'].get('email'),
                    )
                else:
                    issue._assignee = self.user(username=response['assigned_to']) if response.get('assigned_to') else None
                issue._watchers = []
                for name in response.get('cc', []):
                    issue._watchers.append(self.user(username=name))

            else:
                sys.stderr.write("Failed to fetch '{}'\n".format(issue.link))

        if member in ['description', 'comments']:
            response = requests.get('{}/rest/bug/{}/comment{}'.format(self.url, issue.id, self._login_arguments(required=False)))
            if response.status_code == 200:
                response = response.json().get('bugs', {}).get(str(issue.id), {}).get('comments', None)
            else:
                response = None

            if response:
                issue._description = response[0].get('text')
                issue._comments = [
                    Issue.Comment(
                        user=self.user(username=comment['creator']),
                        timestamp=int(calendar.timegm(datetime.strptime(comment['creation_time'], '%Y-%m-%dT%H:%M:%SZ').timetuple())),
                        content=comment['text'],
                    ) for comment in response[1:]
                ]
            else:
                sys.stderr.write("Failed to fetch comments for '{}'\n".format(issue.link))

        if member == 'references':
            issue._references = []
            refs = set()

            for text in [issue.description] + [comment.content for comment in issue.comments]:
                for match in self.REFERENCE_RE.findall(text):
                    candidate = GenericTracker.from_string(match[0]) or self.from_string(match[0])
                    if not candidate or candidate.link in refs or candidate.id == issue.id:
                        continue
                    issue._references.append(candidate)
                    refs.add(candidate.link)

            response = requests.get('{url}/rest/bug/{id}{query}'.format(
                url=self.url, id=issue.id,
                query=self._login_arguments(required=False, query='include_fields=see_also'),
            ))
            response = response.json().get('bugs', []) if response.status_code == 200 else None
            if response:
                for link in response[0].get('see_also', []):
                    candidate = GenericTracker.from_string(link) or self.from_string(link)
                    if not candidate or candidate.link in refs or candidate.id == issue.id:
                        continue
                    issue._references.append(candidate)
                    refs.add(candidate.link)
            else:
                sys.stderr.write("Failed to fetch related issues for '{}'\n".format(issue.link))

        return issue

    def set(self, issue, assignee=None, opened=None, why=None, **properties):
        update_dict = dict()

        if properties:
            raise TypeError("'{}' is an invalid property".format(list(properties.keys())[0]))

        if assignee:
            if not isinstance(assignee, User):
                raise TypeError("Must assign to '{}', not '{}'".format(User, type(assignee)))
            issue._assignee = self.user(name=assignee.name, username=assignee.username, email=assignee.email)
            update_dict['assigned_to'] = issue._assignee.username

        if opened is not None:
            issue._opened = bool(opened)
            if issue._opened:
                update_dict['status'] = 'REOPENED'
                why = why or 'Reopening bug'
            else:
                update_dict['status'] = 'RESOLVED'
                update_dict['resolution'] = 'FIXED'

        if why is not None:
            update_dict['comment'] = dict(body=why)

        if update_dict:
            update_dict['ids'] = [issue.id]
            response = requests.put(
                '{}/rest/bug/{}{}'.format(self.url, issue.id, self._login_arguments(required=True)),
                json=update_dict,
            )
            if response.status_code // 100 != 2:
                if assignee:
                    issue._assignee = None
                if opened is not None:
                    issue._opened = None
                sys.stderr.write("Failed to modify '{}'\n".format(issue))
                return None

        return issue

    def add_comment(self, issue, text):
        response = requests.post(
            '{}/rest/bug/{}/comment{}'.format(self.url, issue.id, self._login_arguments(required=True)),
            json=dict(comment=text),
        )
        if response.status_code // 100 != 2:
            sys.stderr.write("Failed to add comment to '{}'\n".format(issue))
            return None

        result = Issue.Comment(
            user=self.me(),
            timestamp=int(time.time()),
            content=text,
        )
        if not issue._comments:
            self.populate(issue, 'comments')
        issue._comments.append(result)

        return result
