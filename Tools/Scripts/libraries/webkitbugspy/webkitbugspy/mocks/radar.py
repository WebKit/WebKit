# Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
import re
import time
from datetime import datetime, timezone
from unittest.mock import patch

from webkitcorepy import string_utils
from webkitcorepy.mocks import ContextStack

from webkitbugspy import Issue, User, radar

from .base import Base


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

    def member_dsid_list_for_group_name(self, name):
        return [user.username for user in self.parent.users]


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
            from datetime import datetime, timedelta, timezone

            username = self.model.client.authentication_strategy.username()
            if username:
                by = RadarModel.CommentAuthor(self.model.client.parent.users['{}@APPLECONNECT.APPLE.COM'.format(username)])
            else:
                by = None

            self._properties.append(Radar.DiagnosisEntry(
                text=item.text,
                addedAt=datetime.fromtimestamp(int(time.time()), timezone.utc),
                addedBy=by,
            ))

    class DescriptionEntry(object):
        def __init__(self, text):
            self.text = text

    class CommentAuthor(object):
        def __init__(self, user):
            self.name = user.name
            self.email = user.email

    class Event(object):
        def __init__(self, name):
            self.name = name

    class Tentpole(object):
        def __init__(self, args):
            self.name = args['name']

    class MilestoneAssociations(object):
        def __init__(self, milestone):
            self.isCategoryRequired = milestone._isCategoryRequired
            self.categories = milestone._categories
            self.isEventRequired = milestone._isEventRequired
            self.events = milestone._events
            self.isTentpoleRequired = milestone._isTentpoleRequired
            self.tentpoles = milestone._tentpoles

    class Keyword(object):
        def __init__(self, name, isClosed=False):
            self.name = name
            self.isClosed = isClosed

    class RadarGroup(object):
        def __init__(self, name):
            self.name = name

    def __init__(self, client, issue, additional_fields=None):
        from datetime import datetime, timedelta, timezone

        additional_fields = additional_fields or []

        self.client = client
        self._issue = issue
        self.title = issue['title']
        self.id = issue['id']
        self.classification = issue.get('classification', 'Other Bug')
        self.createdAt = datetime.fromtimestamp(issue['timestamp'] - timedelta(hours=7).seconds, timezone.utc)
        self.lastModifiedAt = datetime.fromtimestamp(issue['modified' if issue.get('modified') else 'timestamp'] - timedelta(hours=7).seconds, timezone.utc)
        self.assignee = self.Person(Radar.transform_user(issue['assignee']))
        self.description = self.CollectionProperty(self, self.DescriptionEntry(issue['description']))
        if issue.get('state'):
            self.state = issue['state']
        else:
            self.state = 'Analyze' if issue['opened'] else 'Verify'
        self.duplicateOfProblemID = issue['original']['id'] if issue.get('original', None) else None
        self.related = list()
        if issue.get('substate'):
            self.substate = issue['substate']
        else:
            self.substate = 'Investigate' if issue['opened'] else None
        self.priority = 2
        self.resolution = 'Unresolved' if issue['opened'] else 'Software Changed'
        self.originator = self.Person(Radar.transform_user(issue['creator']))
        self.diagnosis = self.CollectionProperty(self, *[
            Radar.DiagnosisEntry(
                text=comment.content,
                addedAt=datetime.fromtimestamp(comment.timestamp - timedelta(hours=7).seconds, timezone.utc),
                addedBy=self.CommentAuthor(Radar.transform_user(comment.user))
            ) for comment in issue.get('comments', [])
        ])
        self.cc_memberships = self.CollectionProperty(self, *[
            self.CCMembership(Radar.transform_user(watcher)) for watcher in issue.get('watchers', [])
        ])

        self.milestone = Radar.Milestone(issue.get('milestone', '?'))
        if self.client.parent.milestones:
            self.milestone = self.client.parent.milestones.get(self.milestone.name, self.milestone)

        category = issue.get('category')
        self.category = Radar.Category(category) if category else None
        event = issue.get('event')
        self.event = RadarModel.Event(event) if event else None
        tentpole = issue.get('tentpole')
        self.tentpole = RadarModel.Tentpole(dict(name=tentpole)) if tentpole else None

        components = []
        if issue.get('project') and issue.get('component') and issue.get('version'):
            components = self.client.find_components(dict(
                name=dict(eq='{} {}'.format(issue['project'], issue['component'])),
                version=dict(eq=issue['version']),
            ))
        if components:
            self.component = components[0]
        else:
            self.component = None

        if 'sourceChanges' in additional_fields:
            self.sourceChanges = issue.get('sourceChanges', None)

    def related_radars(self):
        for reference in self._issue.get('references', []):
            ref = self.client.radar_for_id(reference)
            if ref:
                yield ref

    def keywords(self):
        return [self.client.parent.keywords[keyword] for keyword in self._issue.get('keywords', [])]

    def commit_changes(self):
        self.client.parent.request_count += 1

        # Anything without someone who added it was made by the current user
        for entry in self.diagnosis.items():
            if not entry.addedBy:
                entry.addedBy = self.CommentAuthor(self.client.parent.users[self.client.current_user().email])

        self.client.parent.issues[self.id]['comments'] = [
            Issue.Comment(
                user=self.client.parent.users[entry.addedBy.email],
                timestamp=int(calendar.timegm(entry.addedAt.timetuple())),
                content=entry.text,
            ) for entry in self.diagnosis.items()
        ]
        self.client.parent.issues[self.id]['assignee'] = self.client.parent.users[self.assignee.dsid]
        self.client.parent.issues[self.id]['opened'] = self.state not in ('Verify', 'Closed')
        self.client.parent.issues[self.id]['state'] = self.state
        self.client.parent.issues[self.id]['substate'] = self.substate
        if self.duplicateOfProblemID:
            if self.duplicateOfProblemID not in self.client.parent.issues:
                raise ValueError('{} is not a known radar'.format(self.duplicateOfProblemID))
            self.client.parent.issues[self.id]['original'] = self.client.parent.issues[self.duplicateOfProblemID]

        for key in ('milestone', 'category', 'event', 'tentpole'):
            attribute = getattr(self, key, None)
            if attribute:
                self.client.parent.issues[self.id][key] = attribute.name

        if self.component:
            project = ''
            component = self.component.name
            for candidate in self.client.parent.projects.keys():
                if component.startswith(candidate):
                    project = candidate
                    component = component[len(project):].lstrip()

            self.client.parent.issues[self.id]['project'] = project
            self.client.parent.issues[self.id]['component'] = component
            self.client.parent.issues[self.id]['version'] = self.component.version

        related = list(self.related)
        for r in related:
            if not self.client.parent.issues[self.id].get('related'):
                self.client.parent.issues[self.id]['related'] = list()
            r_dict = {'relationship': r.type, 'related_radar': r.related_radar_id}
            self.client.parent.issues[self.id]['related'].append(r_dict)

            inverse_r_dict = {'relationship': Radar.Relationship.inverse_map[r.type], 'related_radar': self.id}
            if not self.client.parent.issues[r.related_radar_id].get('related'):
                self.client.parent.issues[r.related_radar_id]['related'] = list()
            self.client.parent.issues[r.related_radar_id]['related'].append(inverse_r_dict)

        if getattr(self, 'sourceChanges', None):
            self.client.parent.issues[self.id]['sourceChanges'] = self.sourceChanges

    def milestone_associations(self, milestone=None):
        return RadarModel.MilestoneAssociations(milestone or self.milestone)

    def relationships(self, relationships=None):
        if not relationships:
            if not self.client.parent.issues[self.id].get('related'):
                self.client.parent.issues[self.id]['related'] = list()
            return [Radar.Relationship(r_dict['relationship'], self.client.radar_for_id(self.id), self.client.radar_for_id(r_dict['related_radar'])) for r_dict in self.client.parent.issues[self.id]['related']]
        result = []
        if relationships[0] not in Radar.Relationship.TYPES:
            raise ValueError("Unknown relationship type '{}'".format(r))
        for data in self.client.parent.issues.values():
            if (data.get('original') or {}).get('id') != self.id:
                continue
            result.append(Radar.Relationship(
                Radar.Relationship.TYPE_ORIGINAL_OF,
                self, RadarModel(self.client, data),
            ))
        return result

    def add_relationship(self, relationship):
        self.related.append(relationship)

    def remove_keyword(self, keyword):
        if keyword.name in self._issue.get('keywords') or []:
            self._issue['keywords'].remove(keyword.name)

    def add_keyword(self, keyword):
        self._issue['keywords'] = self._issue.get('keywords') or []
        if keyword.name not in self._issue['keywords']:
            self._issue['keywords'].append(keyword.name)


class RadarClient(object):
    def __init__(self, parent, authentication_strategy):
        self.parent = parent
        self.authentication_strategy = authentication_strategy

    def current_user(self):
        username = self.authentication_strategy.username() or os.environ.get('RADAR_USERNAME')
        user = self.parent.users.get(f'{username}@APPLECONNECT.APPLE.COM') or self.parent.users.get(username)
        if not user:
            return None
        return RadarModel.Person(Radar.transform_user(user))

    def radar_for_id(self, problem_id, additional_fields=None):
        self.parent.request_count += 1

        found = self.parent.issues.get(problem_id)
        if not found:
            raise Radar.exceptions.RadarAccessDeniedResponseException('Unable to access radar')
        return RadarModel(self, found, additional_fields=additional_fields)

    def find_radars(self, query, return_find_results_directly=False):
        self.parent.request_count += 1

        result = []
        for issue in self.parent.issues.values():
            r = RadarModel(self, issue)

            for key, value in query.items():
                if key == 'state':
                    if r.state != value:
                        break
                elif key == 'assignee':
                    if r.assignee.dsid != value:
                        break
                elif issue.get(key, None) != value:
                    break
            else:
                result.append(r)
        return result

    def milestones_for_component(self, component, include_access_groups=False):
        self.parent.request_count += 1
        return list(self.parent.milestones.values())

    def find_components(self, request_data):
        self.parent.request_count += 1

        filters = []
        for key in ('name', 'version'):
            if key in request_data:
                key_data = request_data[key]
                action = list(key_data.keys())[0] if isinstance(key_data, dict) else 'eq'
                key_value = key_data[action] if isinstance(key_data, dict) else key_data
                filter = dict(
                    eq=lambda key=key, key_value=key_value, **kwargs: not kwargs.get(key) or kwargs[key] == key_value,
                    like=lambda key=key, key_value=key_value, **kwargs: not kwargs.get(key) or re.match(key_value.replace('%', '.*'), kwargs[key]),
                ).get(action)
                if not filter:
                    raise ValueError('{} is not a valid Radar filter'.format(action))
                filters.append(filter)

        result = []
        for project, project_details in self.parent.projects.items():
            for component, component_details in project_details.get('components', {}).items():
                component_name = '{} {}'.format(project, component)
                if any(not filter(name=component_name) for filter in filters):
                    continue
                for version in project_details.get('versions', ['All']):
                    if any(not filter(version=version) for filter in filters):
                        continue
                    result.append(Radar.Component(component_name, component_details['description'], version))
        return result

    def create_radar(self, request_data):
        self.parent.request_count += 1

        if not all((
            request_data.get('title'), request_data.get('description'), request_data.get('component'),
            request_data.get('classification'), request_data.get('reproducible'),
        )):
            raise Radar.exceptions.UnsuccessfulResponseException('Not enough parameters defined to create a radar')

        components = self.find_components(request_data['component'])
        if len(components) != 1:
            raise Radar.exceptions.UnsuccessfulResponseException('Provided component is not valid')
        if request_data['classification'] not in radar.Tracker.CLASSIFICATIONS:
            raise Radar.exceptions.UnsuccessfulResponseException("'{}' is not a valid classification".format(request_data['classification']))
        if request_data['reproducible'] not in radar.Tracker.REPRODUCIBILITY:
            raise Radar.exceptions.UnsuccessfulResponseException("'{}' is not a valid reproducibility value".format(request_data['reproducible']))

        project = ''
        component = components[0].name
        for candidate in self.parent.projects.keys():
            if component.startswith(candidate):
                project = candidate
                component = component[len(project):].lstrip()

        id = 1
        while id in self.parent.issues.keys():
            id += 1

        user = self.parent.users[self.current_user().email]
        issue = dict(
            id=id,
            title=request_data['title'],
            timestamp=int(time.time()),
            modified=int(time.time()),
            opened=True,
            creator=user,
            assignee=user,
            description=request_data['description'],
            project=project,
            component=component,
            version=components[0].version,
            comments=[], watchers=[user],
        )
        self.parent.issues[id] = issue

        return self.radar_for_id(id)

    def clone_radar(self, problem_id, reason_text, component=None):
        self.parent.request_count += 1

        original = self.radar_for_id(problem_id)
        if not original:
            raise Radar.exceptions.UnsuccessfulResponseException("'{}' does not match a known radar".format(problem_id))

        if not isinstance(reason_text, str) and not isinstance(reason_text, string_utils.unicode):
            raise ValueError("Expected reason_text to be '{}' not '{}'".format(str, type(reason_text)))
        if not component:
            component = dict(name=original.component.name.strip(), version=original.component.version)
        description = 'Reason for clone:\n{}\n\n<original text - begin>\n\n{}'.format(
            reason_text,
            '\n'.join([desc.text for desc in original.description.items()]),
        )
        return self.create_radar(dict(
            title=original.title,
            description=description,
            component=component,
            classification=original.classification,
            reproducible='Always',
        ))

    def keywords_for_name(self, keyword_name, additional_fields=None):
        self.parent.request_count += 1

        return [
            keyword for name, keyword in self.parent.keywords.items()
            if name.startswith(keyword_name)
        ]


class Radar(Base, ContextStack):
    top = None

    Person = RadarModel.Person
    Tentpole = RadarModel.Tentpole

    class AuthenticationStrategyNarrative(object):
        def __init__(self, __):
            pass

        def username(self):
            return None

    class AuthenticationStrategySystemAccount(object):
        def __init__(self, username, __, ___, ____):
            self._username = username

        def username(self):
            return self._username

    class AuthenticationStrategySystemAccountOAuth(object):
        def __init__(self, __, ___, ____, _____):
            pass

        def username(self):
            return None

    class AuthenticationStrategySPNego(object):
        def username(self):
            return os.environ.get('RADAR_USERNAME')

    class AuthenticationStrategyAppleConnect(object):
        def username(self):
            return None

    class ClientSystemIdentifier(object):
        def __init__(self, name, version):
            pass

    class Component(object):
        def __init__(self, name, description, version):
            self.name = name
            self.description = description
            self.version = version

        def get(self, item, default=None):
            return getattr(self, item, default)

        def __getitem__(self, item):
            return getattr(self, item)

    class DiagnosisEntry(object):
        def __init__(self, text=None, addedAt=None, addedBy=None):
            self.text = text
            self.addedAt = addedAt or datetime.fromtimestamp(int(time.time()), timezone.utc)
            self.addedBy = addedBy

    class Relationship(object):
        TYPE_RELATED_TO = 'related-to'
        TYPE_ORIGINAL_OF = 'original-of'
        TYPE_DUPLICATE_OF = 'duplicate-of'
        TYPE_CLONE_OF = 'clone-of'
        TYPE_CLONED_TO = 'cloned-to'
        TYPE_BLOCKED_BY = 'blocked-by'
        TYPE_BLOCKING = 'blocking'
        TYPE_PARENT_OF = 'parent-of'
        TYPE_SUBTASK_OF = 'subtask-of'
        TYPE_CAUSE_OF = 'cause-of'
        TYPE_CAUSED_BY = 'caused-by'
        TYPES = [TYPE_RELATED_TO, TYPE_BLOCKED_BY, TYPE_BLOCKING, TYPE_PARENT_OF, TYPE_SUBTASK_OF, TYPE_CAUSE_OF, TYPE_CAUSED_BY,
                 TYPE_DUPLICATE_OF, TYPE_ORIGINAL_OF]

        inverse_map = {
            TYPE_RELATED_TO: TYPE_RELATED_TO,
            TYPE_ORIGINAL_OF: TYPE_DUPLICATE_OF,
            TYPE_CLONE_OF: TYPE_CLONED_TO,
            TYPE_BLOCKING: TYPE_BLOCKED_BY,
            TYPE_PARENT_OF: TYPE_SUBTASK_OF,
            TYPE_CAUSE_OF: TYPE_CAUSED_BY,
        }
        inverse_map.update({v: k for k, v in list(inverse_map.items())})

        def __init__(self, type, radar, related_radar=None):
            self.type = type
            self.radar = radar
            self.related_radar = related_radar
            self.related_radar_id = related_radar.id if related_radar else None

    class exceptions(object):
        class UnsuccessfulResponseException(Exception):
            pass

        class RadarAccessDeniedResponseException(Exception):
            pass

    class RetryPolicy(object):
        pass

    class Milestone(object):
        def __init__(
            self, name,
            isClosed=False,
            isRestricted=False, restrictedAccessGroups=None,
            isProtected=False, protectedAccessGroups=None,
            isCategoryRequired=False, categories=None,
            isEventRequired=False, events=None,
            isTentpoleRequired=False, tentpoles=None,
        ):
            self.name = name['name'] if isinstance(name, dict) else name
            self.isClosed = isClosed
            self.isRestricted = isRestricted
            self.restrictedAccessGroups = [RadarModel.RadarGroup(name) for name in restrictedAccessGroups or []]
            self.isProtected = isProtected
            self.protectedAccessGroups = [RadarModel.RadarGroup(name) for name in protectedAccessGroups or []]

            self._isCategoryRequired = isCategoryRequired
            self._categories = [Radar.Category(category) for category in (categories or [])]

            self._isEventRequired = isEventRequired
            self._events = [RadarModel.Event(event) for event in (events or [])]

            self._isTentpoleRequired = isTentpoleRequired
            self._tentpoles = [RadarModel.Tentpole(dict(name=tentpole)) for tentpole in (tentpoles or [])]

    class Category(object):
        def __init__(self, name):
            self.name = name['name'] if isinstance(name, dict) else name

    @classmethod
    def transform_user(cls, user):
        return User(
            name=user.name,
            username=sum(bytearray(string_utils.encode(user.email))) % 1000,
            emails=user.emails + [user.email.split('@')[0] + '@APPLECONNECT.APPLE.COM'],
        )

    def __init__(self, users=None, issues=None, projects=None, milestones=None):
        Base.__init__(self, users=users, issues=issues, projects=projects)
        ContextStack.__init__(self, Radar)

        self.users = User.Mapping()
        for name in sorted([user.name for user in users or []]):
            self.users.add(self.transform_user(users[name]))

        self.keywords = {}
        for issue in issues or []:
            for keyword in issue.get('keywords') or []:
                self.keywords[keyword] = RadarModel.Keyword(keyword)

        self.issues = {}
        for issue in issues or []:
            self.add(issue)
        self.milestones = {}
        for kwargs in milestones or []:
            ms = Radar.Milestone(**kwargs)
            self.milestones[ms.name] = ms

        self.AppleDirectoryQuery = AppleDirectoryQuery(self)
        self.RadarClient = lambda authentication_strategy, client_system_identifier, retry_policy=None: RadarClient(self, authentication_strategy)

        self.patches.append(patch('webkitbugspy.radar.Tracker.radarclient', new=lambda s=None: self))

        self.request_count = 0


class NoRadar(ContextStack):
    top = None

    def __init__(self):
        super(NoRadar, self).__init__(NoRadar)

        self.patches.append(patch('webkitbugspy.radar.Tracker.radarclient', new=lambda s=None: None))
