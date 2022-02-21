# Copyright (C) 2022 Apple Inc. All rights reserved.
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
import os
import time

from .base import Base

from webkitbugspy import User, Issue
from webkitcorepy import string_utils
from webkitcorepy.mocks import ContextStack


class AppleDirectoryUserEntry(object):
    def __init__(self, user):
        self.first_name = lambda: user.name.split(' ', 1)[0]
        self.last_name = lambda: user.name.split(' ', 1)[1]
        self.email = lambda: user.email
        self.dsid = lambda: user.username


class AppleDirectoryQuery(object):
    def __init__(self, parent):
        self.parent = parent

    def user_entry_for_dsid(self, dsid):
        return self.user_entry_for_attribute_value('dsid', dsid)

    def user_entry_for_attribute_value(self, name, value):
        if name not in ('cn', 'dsid', 'mail', 'uid'):
            raise ValueError("'{}' is not a valid user attribute value".format(name))
        found = self.parent.users.get(value)
        if not found:
            return None
        if name == 'cn' and value != found.name:
            return None
        if name == 'dsid' and value != found.username:
            return None
        if name in ('mail', 'uid') and value not in found.emails:
            return None
        return AppleDirectoryUserEntry(found)


class RadarModel(object):
    class Person(object):
        def __init__(self, user):
            if isinstance(user, dict):
                self.firstName = user.get('firstName')
                self.lastName = user.get('lastName')
                self.email = user.get('email')
                self.dsid = user.get('dsid')
            else:
                self.firstName = user.name.split(' ', 1)[0]
                self.lastName = user.name.split(' ', 1)[1]
                self.email = user.email
                self.dsid = user.username

    class CCMembership(object):
        def __init__(self, user):
            self.person = RadarModel.Person(user)

    class CollectionProperty(object):
        def __init__(self, model, *properties):
            self.model = model
            self._properties = list(properties)

        def items(self, type=None):
            for property in self._properties:
                yield property

        def add(self, item):
            from datetime import datetime, timedelta

            username = self.model.client.authentication_strategy.username()
            if username:
                by = RadarModel.CommentAuthor(self.model.client.parent.users['{}@APPLECONNECT.APPLE.COM'.format(username)])
            else:
                by = None

            self._properties.append(Radar.DiagnosisEntry(
                text=item.text,
                addedAt=datetime.utcfromtimestamp(int(time.time())),
                addedBy=by,
            ))

    class DescriptionEntry(object):
        def __init__(self, text):
            self.text = text

    class CommentAuthor(object):
        def __init__(self, user):
            self.name = user.name
            self.email = user.email

    def __init__(self, client, issue):
        from datetime import datetime, timedelta

        self.client = client
        self._issue = issue
        self.title = issue['title']
        self.id = issue['id']
        self.createdAt = datetime.utcfromtimestamp(issue['timestamp'] - timedelta(hours=7).seconds)
        self.assignee = self.Person(Radar.transform_user(issue['assignee']))
        self.description = self.CollectionProperty(self, self.DescriptionEntry(issue['description']))
        self.state = 'Analyze' if issue['opened'] else 'Verify'
        self.substate = 'Investigate' if issue['opened'] else None
        self.milestone = '?'
        self.priority = 2
        self.resolution = 'Unresolved' if issue['opened'] else 'Software Changed'
        self.originator = self.Person(Radar.transform_user(issue['creator']))
        self.diagnosis = self.CollectionProperty(self, *[
            Radar.DiagnosisEntry(
                text=comment.content,
                addedAt=datetime.utcfromtimestamp(comment.timestamp - timedelta(hours=7).seconds),
                addedBy=self.CommentAuthor(Radar.transform_user(comment.user)),
            ) for comment in issue.get('comments', [])
        ])
        self.cc_memberships = self.CollectionProperty(self, *[
            self.CCMembership(Radar.transform_user(watcher)) for watcher in issue.get('watchers', [])
        ])

    def related_radars(self):
        for reference in self._issue.get('references', []):
            ref = self.client.radar_for_id(reference)
            if ref:
                yield ref

    def commit_changes(self):
        self.client.parent.issues[self.id]['comments'] = [
            Issue.Comment(
                user=self.client.parent.users[entry.addedBy.email],
                timestamp=int(calendar.timegm(entry.addedAt.timetuple())),
                content=entry.text,
            ) for entry in self.diagnosis.items()
        ]
        self.client.parent.issues[self.id]['assignee'] = self.client.parent.users[self.assignee.dsid]
        self.client.parent.issues[self.id]['opened'] = self.state not in ('Verify', 'Closed')


class RadarClient(object):
    def __init__(self, parent, authentication_strategy):
        self.parent = parent
        self.authentication_strategy = authentication_strategy

    def radar_for_id(self, problem_id):
        found = self.parent.issues.get(problem_id)
        if not found:
            return None
        return RadarModel(self, found)


class Radar(Base, ContextStack):
    top = None

    Person = RadarModel.Person

    class AuthenticationStrategySystemAccount(object):
        def __init__(self, username, __, ___, ____):
            self._username = username

        def username(self):
            return self._username

    class AuthenticationStrategySPNego(object):
        def username(self):
            return os.environ.get('RADAR_USERNAME')

    class ClientSystemIdentifier(object):
        def __init__(self, name, version):
            pass

    class DiagnosisEntry(object):
        def __init__(self, text=None, addedAt=None, addedBy=None):
            self.text = text
            self.addedAt = addedAt
            self.addedBy = addedBy

    @classmethod
    def transform_user(cls, user):
        return User(
            name=user.name,
            username=sum(bytearray(string_utils.encode(user.email))) % 1000,
            emails=user.emails + [user.email.split('@')[0] + '@APPLECONNECT.APPLE.COM'],
        )

    def __init__(self, users=None, issues=None):
        Base.__init__(self, users=users, issues=issues)
        ContextStack.__init__(self, Radar)

        self.users = User.Mapping()
        for name in sorted([user.name for user in users or []]):
            self.users.add(self.transform_user(users[name]))

        self.issues = {}
        for issue in issues or []:
            self.add(issue)

        self.AppleDirectoryQuery = AppleDirectoryQuery(self)
        self.RadarClient = lambda authentication_strategy, client_system_identifier: RadarClient(self, authentication_strategy)

        from mock import patch
        self.patches.append(patch('webkitbugspy.radar.Tracker.radarclient', new=lambda s=None: self))


class NoRadar(ContextStack):
    top = None

    def __init__(self):
        super(NoRadar, self).__init__(NoRadar)

        from mock import patch
        self.patches.append(patch('webkitbugspy.radar.Tracker.radarclient', new=lambda s=None: None))
