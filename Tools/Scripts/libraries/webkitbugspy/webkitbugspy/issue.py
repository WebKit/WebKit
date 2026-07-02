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

import sys

from .tracker import Tracker
from .user import User
from datetime import datetime, timezone
from webkitcorepy import string_utils


class Issue(object):
    class Comment(object):
        def __init__(self, user, timestamp, content):
            if user and not isinstance(user, User):
                raise TypeError("Expected 'user' to be of type {}, got '{}'".format(User, user))
            else:
                self.user = user

            if isinstance(timestamp, string_utils.basestring) and timestamp.isdigit():
                timestamp = int(timestamp)
            if timestamp and not isinstance(timestamp, int):
                raise TypeError("Expected 'timestamp' to be of type int, got '{}'".format(timestamp))
            self.timestamp = timestamp

            if content and not isinstance(content, string_utils.basestring):
                raise ValueError("Expected 'content' to be a string, got '{}'".format(content))
            self.content = content

        def __repr__(self):
            return '({} @ {}) {}'.format(
                self.user,
                datetime.fromtimestamp(self.timestamp, timezone.utc) if self.timestamp else '-',
                self.content,
            )

    def __init__(self, id, tracker):
        self.id = int(id)
        self.tracker = tracker
        self._original = None
        self._duplicates = None
        self._related = None

        self._link = None
        self._title = None
        self._timestamp = None
        self._modified = None
        self._creator = None
        self._description = None
        self._opened = None
        self._state = None
        self._substate = None
        self._assignee = None
        self._watchers = None
        self._comments = None
        self._references = None
        self._related_links = None

        self._labels = None
        self._project = None
        self._component = None
        self._version = None
        self._milestone = None
        self._keywords = None
        self._classification = None

        self._source_changes = None

        self.tracker.populate(self, None)

    def __str__(self):
        return '{} {}'.format(self.link, self.title)

    @property
    def link(self):
        if self._link is None:
            self.tracker.populate(self, 'link')
        return self._link

    @property
    def title(self):
        if self._title is None:
            self.tracker.populate(self, 'title')
        return self._title

    @property
    def timestamp(self):
        if self._timestamp is None:
            self.tracker.populate(self, 'timestamp')
        return self._timestamp

    @property
    def modified(self):
        if self._modified is None:
            self.tracker.populate(self, 'modified')
        return self._modified

    @property
    def creator(self):
        if self._creator is None:
            self.tracker.populate(self, 'creator')
        return self._creator

    @property
    def description(self):
        if self._description is None:
            self.tracker.populate(self, 'description')
        return self._description

    @property
    def state(self):
        if self._state is None:
            self.tracker.populate(self, 'state')
        return self._state

    @property
    def substate(self):
        if self._substate is None:
            self.tracker.populate(self, 'substate')
        return self._substate

    def set_state(self, state, substate=None):
        return bool(self.tracker.set(self, state=state, substate=substate))

    @property
    def opened(self):
        if self._opened is None:
            self.tracker.populate(self, 'opened')
        return self._opened

    def open(self, why=None):
        if self.opened:
            return False
        return bool(self.tracker.set(self, opened=True, why=why))

    def close(self, why=None, original=None):
        if not self.opened:
            return False
        if original and (self.tracker.NAME != original.tracker.NAME or getattr(self.tracker, 'url', None) != getattr(original.tracker, 'url', None)):
            raise ValueError('Cannot dupe {} to {}'.format(self.link, original.link))
        return bool(self.tracker.set(self, opened=False, why=why, original=original))

    @property
    def original(self):
        if self._opened is None:
            self.tracker.populate(self, 'opened')
        return self._original

    @property
    def duplicates(self):
        if self._duplicates is None:
            self.tracker.populate(self, 'duplicates')
        return self._duplicates

    @property
    def related(self):
        if self._related is None:
            self.tracker.populate(self, 'related')
        return self._related

    def relate(self, **relations):
        return self.tracker.relate(self, **relations)

    @property
    def assignee(self):
        if self._assignee is None:
            self.tracker.populate(self, 'assignee')
        return self._assignee

    def assign(self, assignee, why=None):
        self.tracker.set(self, assignee=assignee, why=why)
        return self.assignee

    @property
    def watchers(self):
        if self._watchers is None:
            self.tracker.populate(self, 'watchers')
        return self._watchers

    @property
    def comments(self):
        if self._comments is None:
            self.tracker.populate(self, 'comments')
        return self._comments or []

    @property
    def references(self):
        if self._references is None:
            self.tracker.populate(self, 'references')
        return self._references or []

    @property
    def related_links(self):
        if self._related_links is None:
            self.tracker.populate(self, 'see_also')
        return self._related_links

    def add_related_links(self, see_also):
        return self.tracker.set(self, see_also=see_also)

    def add_comment(self, text):
        return self.tracker.add_comment(self, text)

    @property
    def labels(self):
        if self._labels is None:
            self.tracker.populate(self, 'labels')
        return self._labels

    def set_labels(self, labels):
        return self.tracker.set(self, labels=labels)

    @property
    def project(self):
        if self._project is None:
            self.tracker.populate(self, 'project')
        return self._project

    @property
    def component(self):
        if self._component is None:
            self.tracker.populate(self, 'component')
        return self._component

    @property
    def version(self):
        if self._version is None:
            self.tracker.populate(self, 'version')
        return self._version

    @property
    def milestone(self):
        if self._milestone is None:
            self.tracker.populate(self, 'milestone')
        return self._milestone or None

    @property
    def keywords(self):
        if self._keywords is None:
            self.tracker.populate(self, 'keywords')
        return self._keywords

    def set_keywords(self, keywords):
        return self.tracker.set(self, keywords=keywords)

    @property
    def classification(self):
        if self._classification is None:
            self.tracker.populate(self, 'classification')
        return self._classification

    @property
    def source_changes(self):
        if self._source_changes is None:
            self.tracker.populate(self, 'source_changes')
        return self._source_changes

    def add_source_change(self, line):
        parts = line.split(', ')
        parts[-1] = parts[-1][:12]
        search_for = ', '.join(parts) if parts[-1] else line

        for change in self.source_changes:
            if change.startswith(search_for):
                sys.stderr.write("'{}' is already a registered source change\n".format(line))
                return None
        return self.tracker.set(self, source_changes=self.source_changes + [line])

    @property
    def _redaction_match(self):
        result = ''
        for member in ('title', 'project', 'component', 'version', 'classification'):
            result += ';{}:{}'.format(member, getattr(self, member, ''))
        return '{};keywords:{}'.format(result, ','.join(self.keywords or []))

    @property
    def redacted(self):
        match_string = self._redaction_match

        for key, value in self.tracker._redact_exemption.items():
            if key.search(match_string) and value:
                return self.tracker.Redaction(
                    redacted=False,
                    exemption=value,
                    reason="is a {}".format(self.tracker.NAME) if key.pattern == '.*' else "matches '{}'".format(key.pattern),
                )

        match_strings = {self.link: match_string}
        duplicates = self.duplicates or []
        originals = [self.original] if self.original else []
        for related_issue in duplicates + originals:
            match_string = related_issue._redaction_match
            for key, value in self.tracker._redact_exemption.items():
                if key.search(match_string) and value:
                    match_string = None
                    break
            if match_string:
                match_strings[related_issue.link] = match_string

        for m_link, m_string in match_strings.items():
            for key, value in self.tracker._redact.items():
                if key.search(m_string):
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

    def set_component(self, project=None, component=None, version=None):
        return self.tracker.set(self, project=project, component=component, version=version)

    def cc_radar(self, block=False, timeout=None, radar=None):
        return self.tracker.cc_radar(self, block=block, timeout=timeout, radar=radar)

    def __hash__(self):
        return hash(self.link)

    def __cmp__(self, other):
        if not isinstance(other, Issue):
            raise ValueError('Cannot compare {} with {}'.format(Issue, type(other)))
        if self.link == other.link:
            return 0
        return 1 if self.link > other.link else -1

    def __eq__(self, other):
        return self.__cmp__(other) == 0

    def __ne__(self, other):
        return self.__cmp__(other) != 0

    def __lt__(self, other):
        return self.__cmp__(other) < 0

    def __le__(self, other):
        return self.__cmp__(other) <= 0

    def __gt__(self, other):
        return self.__cmp__(other) > 0

    def __ge__(self, other):
        return self.__cmp__(other) >= 0
