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
import re
import sys
import time

from webkitcorepy import Environment, decorators
from webkitbugspy import Issue, Tracker as GenericTracker, User, name as library_name, version as library_version


class Priority(object):
    SHOW_STOPPER = 1
    EXPECTED = 2
    IMPORTANT = 3
    NICE_TO_HAVE = 4
    NOT_SET = 5


class Tracker(GenericTracker):
    RES = [
        re.compile(r'<?rdar://problem/(?P<id>\d+)>?'),
        re.compile(r'<?radar://problem/(?P<id>\d+)>?'),
        re.compile(r'<?rdar:\/\/(?P<id>\d+)>?'),
        re.compile(r'<?radar:\/\/(?P<id>\d+)>?'),
    ]

    class Encoder(GenericTracker.Encoder):
        @decorators.hybridmethod
        def default(context, obj):
            if isinstance(obj, Tracker):
                return dict(type='radar')
            if isinstance(context, type):
                raise TypeError('Cannot invoke parent class when classmethod')
            return super(Tracker.Encoder, context).default(obj)


    @staticmethod
    def radarclient():
        try:
            import radarclient
            return radarclient
        except ImportError:
            return None

    def __init__(self, users=None, authentication=None):
        super(Tracker, self).__init__(users=users)

        self.library = self.radarclient()
        if self.library:
            self.client = self.library.RadarClient(
                authentication or self.authentication(),
                self.library.ClientSystemIdentifier(library_name, str(library_version)),
            )
        else:
            self.client = None

    def authentication(self):
        username = Environment.instance().get('RADAR_USERNAME')
        password = Environment.instance().get('RADAR_PASSWORD')
        totp_secret = Environment.instance().get('RADAR_TOTP_SECRET')
        totp_id = Environment.instance().get('RADAR_TOTP_ID') or 1

        if username and password and totp_secret and totp_id:
            return self.library.AuthenticationStrategySystemAccount(
                username, password, totp_secret, totp_id,
            )
        return self.library.AuthenticationStrategySPNego()

    def from_string(self, string):
        for regex in self.RES:
            match = regex.match(string)
            if match:
                return self.issue(int(match.group('id')))
        return None

    def user(self, name=None, username=None, email=None):
        user = super(Tracker, self).user(name=name, username=username, email=email)
        if user:
            return user
        if not name or not username or not email:
            found = None
            if isinstance(username, int):
                found = self.library.AppleDirectoryQuery.user_entry_for_dsid(int(username))
            elif username:
                found = self.library.AppleDirectoryQuery.user_entry_for_attribute_value('uid', '{}@APPLECONNECT.APPLE.COM'.format(username))
            elif email:
                found = self.library.AppleDirectoryQuery.user_entry_for_attribute_value('mail', email)
            elif name:
                found = self.library.AppleDirectoryQuery.user_entry_for_attribute_value('cn', name)
            if not found:
                raise RuntimeError("Failed to find '{}'".format(User(
                    name, username, [email],
                )))
            name = '{} {}'.format(found.first_name(), found.last_name())
            username = found.dsid()
            email = found.email()
        return self.users.create(
            name=name,
            username=username,
            emails=[email],
        )

    @decorators.Memoize()
    def me(self):
        username = self.authentication().username()
        return self.user(username=username)

    def issue(self, id):
        return Issue(id=int(id), tracker=self)

    def populate(self, issue, member=None):
        issue._link = '<rdar://{}>'.format(issue.id)
        if (not self.client or not self.library) and member:
            sys.stderr.write('radarclient inaccessible on this machine\n')
            return issue

        if not member:
            return issue

        radar = self.client.radar_for_id(issue.id)
        if not radar:
            sys.stderr.write("Failed to fetch '{}'\n".format(issue.link))
            return issue

        issue._title = radar.title
        issue._timestamp = int(calendar.timegm(radar.createdAt.timetuple()))
        issue._assignee = self.user(
            name='{} {}'.format(radar.assignee.firstName, radar.assignee.lastName),
            username=radar.assignee.dsid,
            email=radar.assignee.email,
        )
        issue._description = '\n'.join([desc.text for desc in radar.description.items()])
        issue._opened = False if radar.state in ('Verify', 'Closed') else True
        issue._creator = self.user(
            name='{} {}'.format(radar.originator.firstName, radar.originator.lastName),
            username=radar.originator.dsid,
            email=radar.originator.email,
        )

        if member == 'watchers':
            issue._watchers = []
            for member in radar.cc_memberships.items():
                if member.person.dsid == radar.originator.dsid:
                    continue
                issue._watchers.append(self.user(
                    name='{} {}'.format(member.person.firstName, member.person.lastName),
                    username=member.person.dsid,
                    email=member.person.email,
                ))

        if member == 'comments':
            issue._comments = []
            for item in radar.diagnosis.items(type='user'):
                issue._comments.append(Issue.Comment(
                    user=self.user(
                        name=item.addedBy.name,
                        email=item.addedBy.email,
                    ), timestamp=int(calendar.timegm(item.addedAt.timetuple())),
                    content=item.text,
                ))

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

            for r in radar.related_radars():
                candidate = self.issue(r.id)
                if candidate.link in refs or candidate.id == issue.id:
                    continue
                issue._references.append(candidate)
                refs.add(candidate.link)

        return issue

    def set(self, issue, assignee=None, opened=None, why=None, **properties):
        if not self.client or not self.library:
            sys.stderr.write('radarclient inaccessible on this machine\n')
            return None
        if properties:
            raise TypeError("'{}' is an invalid property".format(list(properties.keys())[0]))

        radar = self.client.radar_for_id(issue.id)
        if not radar:
            sys.stderr.write("Failed to fetch '{}'\n".format(issue.link))
            return None

        did_change = False

        if assignee:
            if not isinstance(assignee, User):
                raise TypeError("Must assign to '{}', not '{}'".format(User, type(assignee)))
            issue._assignee = self.user(name=assignee.name, username=assignee.username, email=assignee.email)
            radar.assignee = self.library.Person({'dsid': int(issue._assignee.username)})
            did_change = True

        if opened is not None:
            issue._opened = bool(opened)
            if issue._opened:
                radar.state = 'Analyze'
                if radar.milestone is None or radar.priority == Priority.NOT_SET:
                    radar.substate = 'Screen'
                else:
                    radar.substate = 'Investigate'
                radar.resolution = 'Unresolved'
            else:
                radar.state = 'Verify'
                radar.resolution = 'Software Changed'
            did_change = True

        if did_change:
            radar.commit_changes()
        return self.add_comment(issue, why) if why else issue

    def add_comment(self, issue, text):
        if not self.client or not self.library:
            sys.stderr.write('radarclient inaccessible on this machine\n')
            return None

        radar = self.client.radar_for_id(issue.id)
        if not radar:
            sys.stderr.write("Failed to fetch '{}'\n".format(issue.link))
            return None

        comment = self.library.DiagnosisEntry()
        comment.text = text
        radar.diagnosis.add(comment)
        radar.commit_changes()

        result = Issue.Comment(
            user=self.me(),
            timestamp=int(time.time()),
            content=comment.text,
        )
        if not issue._comments:
            self.populate(issue, 'comments')
        issue._comments.append(result)

        return result
