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

from __future__ import annotations

import sys
from typing import Any, Callable

from .tracker import Tracker
from .user import User
from datetime import datetime, timezone
from webkitcorepy import string_utils


class Issue(object):
    class Comment(object):
        def __init__(self, user: User | None, timestamp: int | str | None, content: str | None) -> None:
            if user and not isinstance(user, User):
                raise TypeError("Expected 'user' to be of type {}, got '{}'".format(User, user))
            else:
                self.user = user

            if isinstance(timestamp, string_utils.basestring) and timestamp.isdigit():
                timestamp = int(timestamp)
            if timestamp and not isinstance(timestamp, int):
                raise TypeError("Expected 'timestamp' to be of type int, got '{}'".format(timestamp))
            self.timestamp = timestamp if isinstance(timestamp, int) else None

            if content and not isinstance(content, string_utils.basestring):
                raise ValueError("Expected 'content' to be a string, got '{}'".format(content))
            self.content = content

        def __repr__(self) -> str:
            return '({} @ {}) {}'.format(
                self.user,
                datetime.fromtimestamp(self.timestamp, timezone.utc) if self.timestamp else '-',
                self.content,
            )

    class Attachment(object):
        PATCH_SUFFIXES = ('.patch', '.diff')

        def __init__(self, name: str, content_type: str | None = None, contents: bytes | Callable[..., Any] | str | None = None) -> None:
            self.name = name
            self.content_type = content_type
            self._contents = contents

        def __repr__(self) -> str:
            return self.name or '<unnamed attachment>'

        @property
        def is_patch(self) -> bool:
            return bool(self.name) and self.name.lower().endswith(self.PATCH_SUFFIXES)

        def contents(self) -> bytes | str | None:
            '''The attachment's bytes, retrieved and cached on first access. `contents` passed to the
            constructor may be the bytes themselves or a zero-argument callable that fetches them, so
            that a tracker can defer downloading until a caller actually wants the data. Returns None
            if the content could not be retrieved (for example, a locked radar attachment).'''
            contents = self._contents
            if callable(contents):
                contents = self._contents = contents()
            return contents

    # Trackers fill in an issue's properties as they're accessed, and any may remain None if the
    # tracker doesn't know them.
    def __init__(self, id: int | str, tracker: Tracker) -> None:
        self.id = int(id)
        self.tracker = tracker
        self._original: Issue | None = None
        self._duplicates: list[Issue] | None = None
        self._related: dict[str, list[Issue]] | None = None

        self._link: str | None = None
        self._title: str | None = None
        self._timestamp: int | None = None
        self._modified: int | None = None
        self._creator: User | None = None
        self._description: str | None = None
        self._opened: bool | None = None
        self._state: str | None = None
        self._substate: str | None = None
        self._assignee: User | None = None
        self._watchers: list[User] | None = None
        self._comments: list[Issue.Comment] | None = None
        self._references: list[Issue] | None = None
        self._related_links: list[str] | None = None

        self._labels: list[str] | None = None
        self._project: str | None = None
        self._component: str | None = None
        self._version: str | None = None
        self._milestone: str | None = None
        self._keywords: list[str] | None = None
        self._classification: str | None = None

        self._source_changes: list[str] | None = None

        self._attachments: list[Issue.Attachment] | None = None

        self.tracker.populate(self, None)

    def __str__(self) -> str:
        return '{} {}'.format(self.link, self.title)

    @property
    def link(self) -> str:
        if self._link is None:
            self.tracker.populate(self, 'link')
        # Trackers always know an issue's link.
        assert self._link is not None
        return self._link

    @property
    def title(self) -> str | None:
        if self._title is None:
            self.tracker.populate(self, 'title')
        return self._title

    @property
    def timestamp(self) -> int | None:
        if self._timestamp is None:
            self.tracker.populate(self, 'timestamp')
        return self._timestamp

    @property
    def modified(self) -> int | None:
        if self._modified is None:
            self.tracker.populate(self, 'modified')
        return self._modified

    @property
    def creator(self) -> User | None:
        if self._creator is None:
            self.tracker.populate(self, 'creator')
        return self._creator

    @property
    def description(self) -> str | None:
        if self._description is None:
            self.tracker.populate(self, 'description')
        return self._description

    @property
    def state(self) -> str | None:
        if self._state is None:
            self.tracker.populate(self, 'state')
        return self._state

    @property
    def substate(self) -> str | None:
        if self._substate is None:
            self.tracker.populate(self, 'substate')
        return self._substate

    def set_state(self, state: str, substate: str | None = None) -> bool:
        return bool(self.tracker.set(self, state=state, substate=substate))

    @property
    def opened(self) -> bool | None:
        if self._opened is None:
            self.tracker.populate(self, 'opened')
        return self._opened

    def open(self, why: str | None = None) -> bool:
        if self.opened:
            return False
        return bool(self.tracker.set(self, opened=True, why=why))

    def close(self, why: str | None = None, original: Issue | None = None) -> bool:
        if not self.opened:
            return False
        if original and (self.tracker.NAME != original.tracker.NAME or getattr(self.tracker, 'url', None) != getattr(original.tracker, 'url', None)):
            raise ValueError('Cannot dupe {} to {}'.format(self.link, original.link))
        return bool(self.tracker.set(self, opened=False, why=why, original=original))

    @property
    def original(self) -> Issue | None:
        if self._opened is None:
            self.tracker.populate(self, 'opened')
        return self._original

    @property
    def duplicates(self) -> list[Issue] | None:
        if self._duplicates is None:
            self.tracker.populate(self, 'duplicates')
        return self._duplicates

    @property
    def related(self) -> dict[str, list[Issue]] | None:
        if self._related is None:
            self.tracker.populate(self, 'related')
        return self._related

    def relate(self, **relations: Issue | list[Issue] | None) -> Issue | Issue.Comment | None:
        return self.tracker.relate(self, **relations)

    def unrelate(self, **relations: Issue | list[Issue] | None) -> Issue | Issue.Comment | None:
        return self.tracker.unrelate(self, **relations)

    @property
    def assignee(self) -> User | None:
        if self._assignee is None:
            self.tracker.populate(self, 'assignee')
        return self._assignee

    def assign(self, assignee: User | None, why: str | None = None) -> User | None:
        self.tracker.set(self, assignee=assignee, why=why)
        return self.assignee

    @property
    def watchers(self) -> list[User] | None:
        if self._watchers is None:
            self.tracker.populate(self, 'watchers')
        return self._watchers

    @property
    def comments(self) -> list[Issue.Comment]:
        if self._comments is None:
            self.tracker.populate(self, 'comments')
        return self._comments or []

    @property
    def references(self) -> list[Issue]:
        if self._references is None:
            self.tracker.populate(self, 'references')
        return self._references or []

    @property
    def related_links(self) -> list[str] | None:
        if self._related_links is None:
            self.tracker.populate(self, 'see_also')
        return self._related_links

    def add_related_links(self, see_also: list[str]) -> Issue | Issue.Comment | None:
        return self.tracker.set(self, see_also=see_also)

    def add_comment(self, text: str) -> Issue.Comment | None:
        return self.tracker.add_comment(self, text)

    @property
    def labels(self) -> list[str] | None:
        if self._labels is None:
            self.tracker.populate(self, 'labels')
        return self._labels

    def set_labels(self, labels: list[str]) -> Issue | Issue.Comment | None:
        return self.tracker.set(self, labels=labels)

    @property
    def project(self) -> str | None:
        if self._project is None:
            self.tracker.populate(self, 'project')
        return self._project

    @property
    def component(self) -> str | None:
        if self._component is None:
            self.tracker.populate(self, 'component')
        return self._component

    @property
    def version(self) -> str | None:
        if self._version is None:
            self.tracker.populate(self, 'version')
        return self._version

    @property
    def milestone(self) -> str | None:
        if self._milestone is None:
            self.tracker.populate(self, 'milestone')
        return self._milestone or None

    @property
    def keywords(self) -> list[str] | None:
        if self._keywords is None:
            self.tracker.populate(self, 'keywords')
        return self._keywords

    def set_keywords(self, keywords: list[str]) -> Issue | Issue.Comment | None:
        return self.tracker.set(self, keywords=keywords)

    @property
    def classification(self) -> str | None:
        if self._classification is None:
            self.tracker.populate(self, 'classification')
        return self._classification

    @property
    def source_changes(self) -> list[str] | None:
        if self._source_changes is None:
            self.tracker.populate(self, 'source_changes')
        return self._source_changes

    @property
    def attachments(self) -> list[Issue.Attachment] | None:
        '''The issue's attachments, or None if the tracker has no concept of attachments.'''
        if self._attachments is None:
            self.tracker.populate(self, 'attachments')
        return self._attachments

    @property
    def patches(self) -> list[Issue.Attachment] | None:
        '''The subset of `attachments` that look like patches, or None if the tracker has no concept
        of attachments.'''
        attachments = self.attachments
        if attachments is None:
            return None
        return [attachment for attachment in attachments if attachment.is_patch]

    def add_source_change(self, line: str) -> Issue | Issue.Comment | None:
        parts = line.split(', ')
        parts[-1] = parts[-1][:12]
        search_for = ', '.join(parts) if parts[-1] else line

        source_changes = self.source_changes or []
        for change in source_changes:
            if change.startswith(search_for):
                sys.stderr.write("'{}' is already a registered source change\n".format(line))
                return None
        return self.tracker.set(self, source_changes=source_changes + [line])

    @property
    def _redaction_match(self) -> str:
        result = ''
        for member in ('title', 'project', 'component', 'version', 'classification'):
            result += ';{}:{}'.format(member, getattr(self, member, ''))
        return '{};keywords:{}'.format(result, ','.join(self.keywords or []))

    @property
    def redacted(self) -> Tracker.Redaction:
        match_string = self._redaction_match

        for key, value in self.tracker._redact_exemption.items():
            if key.search(match_string) and value:
                return self.tracker.Redaction(
                    redacted=False,
                    exemption=value,
                    reason="is a {}".format(self.tracker.NAME) if key.pattern == '.*' else "matches '{}'".format(key.pattern),
                )

        match_strings: dict[str, str | None] = {self.link: match_string}
        duplicates = self.duplicates or []
        originals = [self.original] if self.original else []
        for related_issue in duplicates + originals:
            related_match_string: str | None = related_issue._redaction_match
            for key, value in self.tracker._redact_exemption.items():
                if related_match_string and key.search(related_match_string) and value:
                    related_match_string = None
                    break
            if related_match_string:
                match_strings[related_issue.link] = related_match_string

        for m_link, m_string in match_strings.items():
            for key, value in self.tracker._redact.items():
                if m_string is not None and key.search(m_string):
                    if key.pattern == '.*':
                        reason = "is a {}".format(self.tracker.NAME)
                    elif m_link != self.link:
                        reason = "is related to {} which matches '{}'".format(m_link, key.pattern)
                    else:
                        reason = "matches '{}'".format(key.pattern)
                    return self.tracker.Redaction(
                        redacted=value,
                        reason=reason,
                    )
        return self.tracker.Redaction(redacted=False)

    def set_component(self, project: str | None = None, component: str | None = None, version: str | None = None) -> Issue | Issue.Comment | None:
        return self.tracker.set(self, project=project, component=component, version=version)

    def cc_radar(self, block: bool = False, timeout: float | None = None, radar: Issue | None = None) -> Issue | None:
        return self.tracker.cc_radar(self, block=block, timeout=timeout, radar=radar)

    def __hash__(self) -> int:
        return hash(self.link)

    def __cmp__(self, other: object) -> int:
        if not isinstance(other, Issue):
            raise ValueError('Cannot compare {} with {}'.format(Issue, type(other)))
        if self.link == other.link:
            return 0
        return 1 if self.link > other.link else -1

    def __eq__(self, other: object) -> bool:
        return self.__cmp__(other) == 0

    def __ne__(self, other: object) -> bool:
        return self.__cmp__(other) != 0

    def __lt__(self, other: Issue) -> bool:
        return self.__cmp__(other) < 0

    def __le__(self, other: Issue) -> bool:
        return self.__cmp__(other) <= 0

    def __gt__(self, other: Issue) -> bool:
        return self.__cmp__(other) > 0

    def __ge__(self, other: Issue) -> bool:
        return self.__cmp__(other) >= 0
