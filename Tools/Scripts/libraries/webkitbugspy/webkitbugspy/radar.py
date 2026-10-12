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

from __future__ import annotations

import calendar
import functools
import os
import re
import subprocess
import sys
import time
import webkitcorepy
from typing import Any, Callable, TypeVar, cast

from urllib.parse import urlparse

from webkitcorepy import Environment, decorators
from webkitbugspy import Issue, Tracker as GenericTracker, User, name as library_name, version as library_version


F = TypeVar('F', bound=Callable[..., Any])


def handle_access_exception(func: F) -> F:
    '''Report errors from Radar instead of raising them. Wrapped methods return None if access to a
    radar is denied, and exit if Radar fails to respond.'''

    @functools.wraps(func)
    def try_func(self: Tracker, *args: Any, **kwargs: Any) -> Any:
        try:
            return func(self, *args, **kwargs)
        except self.radarclient().exceptions.RadarAccessDeniedResponseException as e:
            sys.stderr.write(f'{e.code} Permission Denied\n')
            sys.stderr.write(f'{e.reason}\n')
        except self.radarclient().exceptions.UnsuccessfulResponseException as e:
            sys.stderr.write(f'{e.reason}\n')
            sys.exit(1)
        return None
    return cast(F, try_func)


class Priority(object):
    SHOW_STOPPER = 1
    EXPECTED = 2
    IMPORTANT = 3
    NICE_TO_HAVE = 4
    NOT_SET = 5


class Tracker(GenericTracker):
    # Used by Tools/Scripts/hooks/prepare-commit-msg get_bugs_string().
    RES = [
        re.compile(r'<?rdar://problem/(?P<id>\d+)>?'),
        re.compile(r'<?radar://problem/(?P<id>\d+)>?'),
        re.compile(r'<?rdar:\/\/(?P<id>\d+)>?'),
        re.compile(r'<?radar:\/\/(?P<id>\d+)>?'),
        re.compile(r'<?https:\/\/rdar\.apple\.com\/(?P<id>\d+)>?'),
    ]

    OTHER_BUG = 'Other Bug'
    SECURITY = 'Security'
    CRASH_HANG_DATA_LOSS = 'Crash/Hang/Data Loss'
    POWER = 'Power'
    PERFORMANCE = 'Performance'
    UI_USABILITY = 'UI/Usability'
    SERIOUS_BUG = 'Serious Bug'
    FEATURE = 'Feature (New)'
    ENHANCEMENT = 'Enhancement'
    TASK = 'Task'

    CLASSIFICATIONS = [
        OTHER_BUG,
        SECURITY,
        CRASH_HANG_DATA_LOSS,
        POWER,
        PERFORMANCE,
        UI_USABILITY,
        SERIOUS_BUG,
        FEATURE,
        ENHANCEMENT,
        TASK
    ]

    RELATIONSHIP_TYPES = ['related-to', 'blocked-by', 'blocking', 'parent-of', 'subtask-of', 'cause-of', 'caused-by', 'duplicate-of', 'original-of', 'clone-of', 'cloned-to']

    ALWAYS = 'Always'
    SOMETIMES = 'Sometimes'
    RARELY = 'Rarely'
    UNABLE = 'Unable'
    DIDNT_TRY = "I Didn't Try"
    NOT_APPLICABLE = 'Not Applicable'

    REPRODUCIBILITY = [NOT_APPLICABLE, ALWAYS, SOMETIMES, RARELY, UNABLE, DIDNT_TRY]
    NAME = 'Radar'

    class Encoder(GenericTracker.Encoder):
        @decorators.hybridmethod
        def default(context: Any, obj: Any) -> Any:
            if isinstance(obj, Tracker):
                return dict(
                    type='radar',
                    projects=obj._projects,
                    hide_title=obj.hide_title,
                )
            if isinstance(context, type):
                raise TypeError('Cannot invoke parent class when classmethod')
            return super(Tracker.Encoder, context).default(obj)

    # radarclient is Apple-internal, and not available everywhere.
    @staticmethod
    def radarclient() -> Any:
        try:
            import radarclient
            return radarclient
        except ImportError:
            return None

    def __init__(
        self, users: User.Mapping | None = None, authentication: Any = None, project: str | None = None, projects: list[str] | None = None,
        redact: dict[str, bool] | None = None, hide_title: bool | None = None, redact_exemption: dict[str, bool] | None = None,
    ) -> None:
        hide_title = True if hide_title is None else hide_title
        super(Tracker, self).__init__(users=users, redact=redact, redact_exemption=redact_exemption, hide_title=hide_title)
        self._projects = [project] if project else (projects or [])

        self._keywords: dict[str, Any] = dict()
        self._invalid_keywords: set[str] = set()

        self.library = self.radarclient()

        self._authentication = authentication
        self._client: Any = None

    @property
    def client(self) -> Any:
        if self._client:
            return self._client

        if self.authentication():
            self._client = self.library.RadarClient(
                self.authentication(), self.library.ClientSystemIdentifier(library_name, str(library_version)),
                retry_policy=self.radarclient().RetryPolicy(),
            )
        return self._client

    def authentication(self) -> Any:
        if self._authentication:
            return self._authentication

        if not self.library:
            return None

        identity = Environment.instance().get('RADAR_IDENTITY')
        username = Environment.instance().get('RADAR_USERNAME')
        password = Environment.instance().get('RADAR_PASSWORD')
        totp_secret = Environment.instance().get('RADAR_TOTP_SECRET')
        totp_id = Environment.instance().get('RADAR_TOTP_ID') or 1

        try:
            if identity and os.path.isdir(identity):
                self._authentication = self.library.AuthenticationStrategyNarrative(identity)
            elif username and password and totp_secret and totp_id:
                self._authentication = self.library.AuthenticationStrategySystemAccountOAuth(
                    username, password, totp_secret, totp_id,
                )
            else:
                self._authentication = self.library.AuthenticationStrategyAppleConnect()
        except Exception:
            sys.stderr.write('No valid authentication session for Radar\n')
        return self._authentication

    @classmethod
    def parse_id(cls, string: str) -> str | list[str] | None:
        """Parse a radar URL string and return the numeric ID(s) as strings.

        Returns a single ID string for one ID, a list of ID strings for
        multiple IDs (ampersand-separated), or None for no match.
        Accepts rdar://, radar://, and https://rdar.apple.com/ URL forms,
        with optional angle brackets.
        """
        value = string.strip()
        if not value:
            return None

        # Strip optional angle brackets
        if value.startswith('<') and value.endswith('>'):
            value = value[1:-1].strip()

        parsed = urlparse(value)

        # https://rdar.apple.com/N form
        if parsed.scheme == 'https' and parsed.netloc == 'rdar.apple.com':
            id_part = parsed.path.lstrip('/')
            if not id_part:
                return None
            parts = id_part.split('&')
            ids = [p for p in parts if p.isdigit()]
            if not ids:
                return None
            if len(ids) == 1:
                return ids[0]
            return ids

        # rdar:// or radar:// forms
        if parsed.scheme not in ('rdar', 'radar'):
            return None

        if parsed.netloc == 'problem':
            id_part = parsed.path.lstrip('/')
        else:
            id_part = parsed.netloc

        if not id_part:
            return None

        parts = id_part.split('&')
        ids = [p for p in parts if p.isdigit()]
        if not ids:
            return None
        if len(ids) == 1:
            return ids[0]
        return ids

    def from_string(self, string: str) -> Issue | None:
        result = type(self).parse_id(string)
        if result is None:
            return None
        if isinstance(result, list):
            result = result[0]
        return self.issue(int(result))

    def user(self, name: str | None = None, username: int | str | None = None, email: str | None = None) -> User:
        user = super(Tracker, self).user(name=name, username=username, email=email)
        if user:
            return user
        if not name or not username or not email:
            found = None
            try:
                if isinstance(username, int):
                    found = self.library.AppleDirectoryQuery.user_entry_for_dsid(int(username))
                elif username:
                    found = self.library.AppleDirectoryQuery.user_entry_for_attribute_value('uid', '{}@APPLECONNECT.APPLE.COM'.format(username))
                elif email:
                    found = self.library.AppleDirectoryQuery.user_entry_for_attribute_value('mail', email)
                elif name:
                    found = self.library.AppleDirectoryQuery.user_entry_for_attribute_value('cn', name)
            except subprocess.CalledProcessError:
                pass
            if not found:
                return self.users.create(
                    # Users without names are named by their DSID.
                    name=name or username,  # type: ignore[arg-type]
                    username=None,
                    emails=[email],
                )
            name = '{} {}'.format(found.first_name(), found.last_name())
            username = found.dsid()
            email = found.email()
        return self.users.create(
            name=name,
            username=username,
            emails=[email],
        )

    @decorators.Memoize()
    def me(self) -> User | None:
        if self.client:
            user = self.client.current_user()
            if user:
                return self.users.create(
                    name='{} {}'.format(user.firstName, user.lastName),
                    username=user.dsid,
                    emails=[user.email],
                )
        return None

    def issue(self, id: int | str) -> Issue:
        return Issue(id=int(id), tracker=self)

    @handle_access_exception
    def populate(self, issue: Issue, member: str | None = None) -> Issue | None:
        issue._link = 'rdar://{}'.format(issue.id)
        issue._labels = []
        issue._related_links = []  # We don't yet have a defined idiom for "related links" in radar
        if member == 'attachments':
            issue._attachments = []
        if (not self.client or not self.library) and member:
            sys.stderr.write('radarclient inaccessible on this machine\n')
            return issue

        if not member or member == 'labels':
            return issue

        additional_fields = []
        if member == 'source_changes':
            additional_fields.append('sourceChanges')
        radar = self.client.radar_for_id(issue.id, additional_fields=additional_fields)
        if not radar:
            sys.stderr.write("Failed to fetch '{}'\n".format(issue.link))
            return issue

        issue._title = radar.title
        issue._timestamp = int(calendar.timegm(radar.createdAt.timetuple()))
        issue._modified = int(calendar.timegm(radar.lastModifiedAt.timetuple()))
        issue._assignee = self.user(
            name='{} {}'.format(radar.assignee.firstName, radar.assignee.lastName),
            username=radar.assignee.dsid,
            email=radar.assignee.email,
        )
        issue._description = '\n'.join([desc.text for desc in radar.description.items()])
        issue._opened = False if radar.state in ('Verify', 'Closed') else True
        issue._state = radar.state
        issue._substate = radar.substate
        if radar.duplicateOfProblemID is not None:
            issue._original = self.issue(radar.duplicateOfProblemID)
        issue._creator = self.user(
            name='{} {}'.format(radar.originator.firstName, radar.originator.lastName),
            username=radar.originator.dsid,
            email=radar.originator.email,
        )
        issue._milestone = radar.milestone.name if radar.milestone else ''

        if member == 'source_changes':
            issue._source_changes = []
            if radar.sourceChanges is not None:
                issue._source_changes = radar.sourceChanges.splitlines()

        if member == 'attachments' and issue._attachments is not None:
            for attachment in radar.attachments.items():
                issue._attachments.append(Issue.Attachment(
                    name=attachment.fileName,
                    contents=lambda attachment=attachment: self._attachment_contents(attachment),
                ))

        if member == 'keywords':
            issue._keywords = [kw.name for kw in (radar.keywords() or [])]

        if member == 'classification':
            issue._classification = radar.classification

        if member == 'watchers':
            issue._watchers = []
            for membership in radar.cc_memberships.items():
                if membership.person.dsid == radar.originator.dsid:
                    continue
                issue._watchers.append(self.user(
                    name='{} {}'.format(membership.person.firstName, membership.person.lastName),
                    username=membership.person.dsid,
                    email=membership.person.email,
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
            refs: set[str] = set()

            for text in [issue.description] + [comment.content for comment in issue.comments]:
                for match in self.REFERENCE_RE.findall(text or ''):
                    candidate = GenericTracker.from_string(match[0]) or self.from_string(match[0])
                    if not candidate or candidate.link in refs or candidate.id == issue.id:
                        continue
                    issue._references.append(candidate)
                    refs.add(candidate.link)

            for r in radar.related_radars():
                related = self.issue(r.id)
                if related.link in refs or related.id == issue.id:
                    continue
                issue._references.append(related)
                refs.add(related.link)

        if radar.component and member in ('project', 'component', 'version'):
            issue._project = ''
            component: str = radar.component.get('name', '')
            issue._version = radar.component.get('version', 'All')
            for project in self._projects:
                if component.startswith(project):
                    issue._project = project
                    component = component[len(project):].lstrip()
                    break
            issue._component = component

        if member == 'duplicates':
            issue._duplicates = []
            for r in radar.relationships([self.radarclient().Relationship.TYPE_ORIGINAL_OF]):
                if r.related_radar:
                    issue._duplicates.append(self.issue(r.related_radar.id))

        if member == 'related':
            issue._related = {r: [] for r in self.RELATIONSHIP_TYPES}
            for r in radar.relationships():
                issue._related[r.type].append(self.issue(r.related_radar_id))

        return issue

    def _attachment_contents(self, attachment: Any) -> bytes | None:
        '''Download an attachment's bytes, or None if it is locked.'''
        try:
            contents: bytes = attachment.content(client=self.client)
            return contents
        except self.radarclient().exceptions.AttachmentLockedException:
            sys.stderr.write("'{}' is locked and cannot be downloaded\n".format(attachment.fileName))
            return None

    @handle_access_exception
    def set(
        self, issue: Issue, assignee: User | None = None, opened: bool | None = None, why: str | None = None,
        project: str | None = None, component: str | None = None, version: str | None = None, original: Issue | None = None,
        keywords: list[str] | None = None, source_changes: list[str] | None = None, state: str | None = None, substate: str | None = None,
        resolution: str | None = None, see_also: list[str] | None = None, **properties: Any,
    ) -> Issue | Issue.Comment | None:
        if not self.client or not self.library:
            sys.stderr.write('radarclient inaccessible on this machine\n')
            return None
        if properties:
            raise TypeError("'{}' is an invalid property".format(list(properties.keys())[0]))

        additional_fields = []
        if source_changes:
            additional_fields.append('sourceChanges')
        radar = self.client.radar_for_id(issue.id, additional_fields=additional_fields)
        if not radar:
            sys.stderr.write("Failed to fetch '{}'\n".format(issue.link))
            return None

        did_change = False

        if assignee:
            if not isinstance(assignee, User):
                raise TypeError("Must assign to '{}', not '{}'".format(User, type(assignee)))
            issue._assignee = self.user(name=assignee.name, username=assignee.username, email=assignee.email)
            # Radar users are identified by DSID.
            assert issue._assignee.username is not None
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
                if original:
                    radar.resolution = 'Duplicate'
                    radar.duplicateOfProblemID = original.id
                    issue._original = original
                else:
                    radar.resolution = 'Software Changed'
            issue._state = radar.state
            issue._substate = radar.substate
            did_change = True

        if state is not None:
            if radar.state == 'Analyze' and state != 'Analyze':
                radar.resolution = resolution or 'Software Changed'
            radar.state = state
            issue._state = state
            did_change = True

        if substate is not None:
            radar.substate = substate
            issue._substate = substate
            did_change = True

        if project or component or version:
            if not project and len(self.projects) == 1:
                project = list(self.projects.keys())[0]
            if not project:
                raise ValueError('No project provided')
            if not self.projects.get(project):
                raise ValueError("'{}' is not a recognized project".format(project))

            components = self.projects.get(project, {}).get('components', {}).keys()
            if not component and len(components) == 1:
                component = list(components)[0]
            if not component or component == '*':
                component = ''
            if component and component not in components:
                raise ValueError("'{}' is not a recognized component of '{}'".format(component, project))

            if component:
                versions = self.projects.get(project, {}).get('components', {}).get(component, {}).get('versions', [])
            else:
                versions = self.projects.get(project, {}).get('versions', [])
            if not version:
                version = versions[0]
            if version not in versions:
                raise ValueError("'{}' is not a recognized version in '{} {}'".format(version, project, component))

            components = self.client.find_components(dict(
                name=dict(eq='{} {}'.format(project, component)),
                version=dict(eq=version),
            ))
            if not components:
                raise ValueError("No components match '{}' with version '{}'".format('{} {}'.format(project, component), version))
            if len(components) > 1:
                raise ValueError("{} components match '{}' with version '{}'".format(len(components), '{} {}'.format(project, component), version))
            radar.component = components[0]
            did_change = True

            issue._project = project
            issue._component = component
            issue._version = version

        if keywords is not None:
            current_keywords = issue.keywords or []
            for keyword in keywords + current_keywords:
                if keyword not in self._invalid_keywords and keyword not in self._keywords:
                    candidates = self.client.keywords_for_name(keyword)
                    for candidate in candidates:
                        self._keywords[candidate.name] = candidate
                if keyword in self._keywords:
                    continue
                self._invalid_keywords.add(keyword)
                raise ValueError("'{}' is not a valid keyword".format(keyword))

            for word in current_keywords:
                if word not in keywords:
                    radar.remove_keyword(self._keywords[word])
            for word in keywords:
                if word not in current_keywords:
                    radar.add_keyword(self._keywords[word])
            did_change = True
            issue._keywords = keywords

        if source_changes:
            did_change = True
            radar.sourceChanges = '\n'.join(source_changes)

        if see_also:
            sys.stderr.write('Radar does not support the see_also field at this time\n')
            return None

        if did_change:
            radar.commit_changes()
        return self.add_comment(issue, why) if why else issue

    @handle_access_exception
    def add_comment(self, issue: Issue, text: str) -> Issue.Comment | None:
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
        if issue._comments is not None:
            issue._comments.append(result)

        return result

    @handle_access_exception
    def create_relationship(self, issue: Issue, issue2: Issue, relationship: str) -> None:
        if relationship not in self.RELATIONSHIP_TYPES:
            sys.stderr.write('{} is not a valid relationship type.'.format(relationship))
            return None

        radar = self.client.radar_for_id(issue.id)
        if not radar:
            sys.stderr.write("Failed to fetch '{}'\n".format(issue.link))
            return None

        radar2 = self.client.radar_for_id(issue2.id)
        if not radar2:
            sys.stderr.write("Failed to fetch '{}'\n".format(issue2.link))
            return None

        if relationship == self.radarclient().Relationship.TYPE_DUPLICATE_OF:
            radar.state = 'Verify'
            radar.resolution = 'Duplicate'
            radar.duplicateOfProblemID = issue2.id
            issue._original = issue2
            radar.commit_changes()
        elif relationship == self.radarclient().Relationship.TYPE_ORIGINAL_OF:
            radar2 = self.client.radar_for_id(issue2.id)
            radar2.state = 'Verify'
            radar2.resolution = 'Duplicate'
            radar2.duplicateOfProblemID = issue.id
            issue2._original = issue
            radar2.commit_changes()
        else:
            new_relationship = self.radarclient().Relationship(relationship, radar, radar2)
            radar.add_relationship(new_relationship)
            radar.commit_changes()

        if not issue._related:
            self.populate(issue, 'related')
        else:
            issue._related[relationship].append(issue2)

        return None

    @handle_access_exception
    def remove_relationship(self, issue: Issue, issue2: Issue, relationship: str) -> Issue | None:
        if relationship not in self.RELATIONSHIP_TYPES:
            sys.stderr.write(f'{relationship} is not a valid relationship type.')
            return None
        if relationship in (
            self.radarclient().Relationship.TYPE_DUPLICATE_OF,
            self.radarclient().Relationship.TYPE_ORIGINAL_OF,
        ):
            raise NotImplementedError(f'Cannot remove a {relationship} relationship')

        radar = self.client.radar_for_id(issue.id)
        if not radar:
            sys.stderr.write(f"Failed to fetch '{issue.link}'\n")
            return None

        # 'delete_relationship' only accepts a relationship the radar already has
        existing = next((
            candidate for candidate in radar.relationships() or []
            if candidate.type == relationship and candidate.related_radar_id == issue2.id
        ), None)
        if not existing:
            return issue

        radar.delete_relationship(existing)
        radar.commit_changes()

        if not issue._related:
            self.populate(issue, 'related')
        else:
            issue._related[relationship] = [
                candidate for candidate in issue._related[relationship] if candidate.id != issue2.id
            ]
        return issue

    @handle_access_exception
    def unrelate(
        self, issue: Issue, related_to: Issue | None = None, blocked_by: Issue | None = None, blocking: Issue | None = None,
        parent_of: Issue | None = None, subtask_of: Issue | None = None, cause_of: Issue | None = None, caused_by: Issue | None = None,
        duplicate_of: Issue | None = None, original_of: Issue | None = None, **relations: Any,
    ) -> Issue | None:
        if relations:
            raise TypeError(f"'{list(relations.keys())[0]}' is an invalid relation")

        for related, relationship in (
            (related_to, self.radarclient().Relationship.TYPE_RELATED_TO),
            (blocked_by, self.radarclient().Relationship.TYPE_BLOCKED_BY),
            (blocking, self.radarclient().Relationship.TYPE_BLOCKING),
            (parent_of, self.radarclient().Relationship.TYPE_PARENT_OF),
            (subtask_of, self.radarclient().Relationship.TYPE_SUBTASK_OF),
            (cause_of, self.radarclient().Relationship.TYPE_CAUSE_OF),
            (caused_by, self.radarclient().Relationship.TYPE_CAUSED_BY),
            (duplicate_of, self.radarclient().Relationship.TYPE_DUPLICATE_OF),
            (original_of, self.radarclient().Relationship.TYPE_ORIGINAL_OF),
        ):
            if related:
                self.remove_relationship(issue, related, relationship)
        return issue

    def relate(
        self, issue: Issue, related_to: Issue | None = None, blocked_by: Issue | None = None, blocking: Issue | None = None,
        parent_of: Issue | None = None, subtask_of: Issue | None = None, cause_of: Issue | None = None, caused_by: Issue | None = None,
        duplicate_of: Issue | None = None, original_of: Issue | None = None, **relations: Any,
    ) -> Issue | None:
        if relations:
            raise TypeError("'{}' is an invalid relation".format(list(relations.keys())[0]))

        if not self.client or not self.library:
            sys.stderr.write('radarclient inaccessible on this machine\n')
            return None

        try:
            if related_to:
                self.create_relationship(issue, related_to, self.radarclient().Relationship.TYPE_RELATED_TO)
            if blocked_by:
                self.create_relationship(issue, blocked_by, self.radarclient().Relationship.TYPE_BLOCKED_BY)
            if blocking:
                self.create_relationship(issue, blocking, self.radarclient().Relationship.TYPE_BLOCKING)
            if parent_of:
                self.create_relationship(issue, parent_of, self.radarclient().Relationship.TYPE_PARENT_OF)
            if subtask_of:
                self.create_relationship(issue, subtask_of, self.radarclient().Relationship.TYPE_SUBTASK_OF)
            if cause_of:
                self.create_relationship(issue, cause_of, self.radarclient().Relationship.TYPE_CAUSE_OF)
            if caused_by:
                self.create_relationship(issue, caused_by, self.radarclient().Relationship.TYPE_CAUSED_BY)
            if duplicate_of:
                self.create_relationship(issue, duplicate_of, self.radarclient().Relationship.TYPE_DUPLICATE_OF)
            if original_of:
                self.create_relationship(issue, original_of, self.radarclient().Relationship.TYPE_ORIGINAL_OF)
        except AttributeError:
            raise AttributeError('Input should be Issue objects.')

        return issue

    @property
    @webkitcorepy.decorators.Memoize()
    @handle_access_exception
    def projects(self) -> dict[str, dict[str, Any]]:
        result: dict[str, dict[str, Any]] = dict()
        for project in self._projects:
            result[project] = dict(
                components=dict(),
                description=None,
                versions=[],
            )
            for component in self.client.find_components(dict(name=dict(like=project + '%'), isClosed=False)):
                name = component.name[(len(project)):].lstrip()
                if not name:
                    if 'All' not in result[project]['versions']:
                        result[project]['description'] = component.description
                    result[project]['versions'].append(component.version)
                    continue

                if name not in result[project]['components']:
                    result[project]['components'][name] = dict(description=None, versions=[])
                if 'All' not in result[project]['versions']:
                    result[project]['components'][name]['description'] = component.description
                result[project]['components'][name]['versions'].append(component.version)
        return result

    @handle_access_exception
    def create(
        self, title: str, description: str,
        project: str | None = None, component: str | None = None, version: str | None = None,
        classification: str | None = None, reproducible: str | None = None,
        assign: bool = True, keywords: list[str] | None = None,
    ) -> Issue | None:
        if not title:
            raise ValueError('Must define title to create bug')
        if not description:
            raise ValueError('Must define description to create bug')

        if not project and len(self.projects) == 1:
            project = list(self.projects.keys())[0]
        if not project:
            project = webkitcorepy.Terminal.choose(
                'What project should the bug be associated with?',
                options=sorted(self.projects.keys()), numbered=True,
            )

        components = sorted(self.projects.get(project, {}).get('components', {}).keys())
        if not component and len(components) == 1:
            component = components[0]
        elif not component and components:
            if self.projects[project]['versions']:
                components = ['*'] + components
            component = webkitcorepy.Terminal.choose(
                "What component in '{}' should the bug be associated with?".format(project),
                options=components, numbered=True, default=('*' if components[0] == '*' else None),
            )
        if not component or component == '*':
            component = ''

        if component:
            versions = self.projects.get(project, {}).get('components', {}).get(component, {}).get('versions', [])
        else:
            versions = self.projects.get(project, {}).get('versions', [])
        if not version and len(versions) == 1:
            version = versions[0]
        elif not version and versions:
            version = webkitcorepy.Terminal.choose(
                "What version of '{}{}' should the bug be associated with?".format(project, (' ' + component) if component else ''),
                options=versions, numbered=True, default=('All' if 'All' in versions else None),
            )
        if not version:
            version = 'All'

        # Don't perform any checks of project, component or version. Radar defines many more projects than this class
        # is aware of, if the caller knows better, trust them.

        classification = classification or self.CLASSIFICATIONS[0]
        if classification not in self.CLASSIFICATIONS:
            raise ValueError("'{}' is not a valid bug classification".format(classification))

        reproducible = reproducible or self.REPRODUCIBILITY[0]
        if reproducible not in self.REPRODUCIBILITY:
            raise ValueError("'{}' is not a valid reproducibility argument".format(classification))

        try:
            name = '{} {}'.format(project, component) if component else project
            response = self.client.create_radar(dict(
                title=title,
                description=description,
                component=dict(name=name, version=version),
                classification=classification,
                reproducible=reproducible,
            ))
        except self.library.exceptions.UnsuccessfulResponseException as e:
            sys.stderr.write('Failed to create radar:\n')
            sys.stderr.write('{}\n'.format(e))
            return None

        result = self.issue(response.id)
        if assign:
            result.assign(self.me())
        return result

    def cc_radar(self, issue: Issue, block: bool = False, timeout: float | None = None, radar: Issue | None = None) -> Issue:
        # cc-ing radar is a no-op for radar
        return issue

    @handle_access_exception
    def clone(
        self, issue: Issue, reason: str,
        project: str | None = None, component: str | None = None, version: str | None = None,
        assign: bool = True,
    ) -> Issue | None:
        if not reason:
            raise ValueError('Reason must be provided for a clone')
        if not self.client or not self.library:
            sys.stderr.write('radarclient inaccessible on this machine\n')
            return None

        project = project or issue.project
        component = component or issue.component
        version = version or issue.version

        try:
            name = '{} {}'.format(project, component) if component else project
            clone = self.client.clone_radar(
                issue.id, reason_text=reason,
                component=dict(name=(name or '').strip(), version=version),
            )
        except self.library.exceptions.UnsuccessfulResponseException as e:
            sys.stderr.write('Failed to clone {}:\n'.format(issue))
            sys.stderr.write('{}\n'.format(e))
            return None

        result = self.issue(clone.id)
        if assign:
            result.assign(self.me())
        return result

    @handle_access_exception
    def search(self, query: dict[str, Any]) -> list[Issue]:
        if not query or len(query) == 0:
            raise ValueError('Query must be provided')

        radars = self.client.find_radars(query, return_find_results_directly=True)
        issues = []
        for radar in radars:
            if radar.id:
                issue = Issue(id=radar.id, tracker=self)
                issues.append(issue)

        return issues
