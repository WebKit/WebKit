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

from __future__ import annotations

import json
import re
from typing import TYPE_CHECKING, Any, ClassVar, Iterable, overload

from .user import User

from webkitcorepy import decorators, string_utils

if TYPE_CHECKING:
    from .issue import Issue


class Tracker(object):
    REFERENCE_RE = re.compile(r'^<?((https|http|rdar|radar)://[^\s><,\'\"}{\]\[)(]*[^\s><,\'\"}{\]\[)(\.\?])>?', re.MULTILINE)

    _trackers: list[Tracker] = []

    # Each kind of tracker names itself.
    NAME: ClassVar[str]

    class Encoder(json.JSONEncoder):
        def default(self, obj: Any) -> Any:
            if isinstance(obj, dict):
                return {key: self.default(value) for key, value in obj.items()}
            if isinstance(obj, list):
                return [self.default(value) for value in obj]
            if not isinstance(obj, Tracker):
                return super(Tracker.Encoder, self).default(obj)
            # Each kind of tracker's Encoder.default is a hybridmethod, so it can be called on the class.
            encoder: Any = obj.Encoder
            return encoder.default(obj)

    class Redaction(object):
        def __init__(self, redacted: bool = False, reason: str | None = None, exemption: bool = False) -> None:
            self.redacted = redacted
            self.reason = reason
            self.exemption = exemption

            if self.exemption and not self.reason:
                raise ValueError('Must define a reason for an redact exemption')

        def __bool__(self) -> bool:
            return self.redacted

        def __nonzero__(self) -> bool:
            return self.redacted

        def __repr__(self) -> str:
            if self.exemption:
                return '{} and is exempt from redaction'.format(self.reason)
            if not self.redacted:
                return 'is not redacted'
            if self.reason:
                return '{} and is thus redacted'.format(self.reason)
            return 'is redacted for an unknown reason'

        def __str__(self) -> str:
            return self.__repr__()

        def __eq__(self, other: object) -> bool:
            if isinstance(other, str):
                return str(self) == other
            elif isinstance(other, bool):
                return self.redacted == other
            elif isinstance(other, Tracker.Redaction):
                return self.redacted == other.redacted and self.exemption == other.exemption and self.reason == other.reason
            return False

        def __ne__(self, other: object) -> bool:
            return not self.__eq__(other)

    @overload
    @classmethod
    def from_json(cls, data: list[Any] | tuple[Any, ...]) -> list[Tracker]:
        ...

    @overload
    @classmethod
    def from_json(cls, data: dict[str, Any]) -> Tracker:
        ...

    # JSON strings may describe a tracker, or a list of them.
    @overload
    @classmethod
    def from_json(cls, data: str) -> Tracker | list[Tracker]:
        ...

    @classmethod
    def from_json(cls, data: str | dict[str, Any] | list[Any] | tuple[Any, ...]) -> Tracker | list[Tracker]:
        from . import bugzilla, github, radar

        decoded: Any = data if isinstance(data, (dict, list, tuple)) else json.loads(data)
        if isinstance(decoded, (list, tuple)):
            trackers: list[Any] = [cls.from_json(datum) for datum in decoded]
            return trackers

        if decoded.get('type') not in ('bugzilla', 'github', 'radar'):
            raise TypeError("'{}' is not a recognized tracker type".format(decoded.get('type')))

        unpacked: dict[str, Any] = dict(
            redact=decoded.get('redact'),
            redact_exemption=decoded.get('redact_exemption'),
            hide_title=decoded.get('hide_title'),
        )
        if decoded.get('type') in ('bugzilla', 'github'):
            unpacked['url'] = decoded.get('url')
            unpacked['res'] = [re.compile(r) for r in decoded.get('res', [])]
        if decoded.get('type') == 'bugzilla':
            unpacked['radar_importer'] = decoded.get('radar_importer')

        if decoded.get('type') == 'radar':
            unpacked['project'] = decoded.get('project', None)
            unpacked['projects'] = decoded.get('projects', [])
            unpacked['project'] = decoded.get('project', None)

        types: dict[str, type[Tracker]] = dict(
            bugzilla=bugzilla.Tracker,
            github=github.Tracker,
            radar=radar.Tracker,
        )
        return types[decoded['type']](**unpacked)

    @classmethod
    def register(cls, tracker: Tracker) -> Tracker:
        if tracker not in cls._trackers:
            setattr(cls, str(type(tracker)).split('.')[-2], tracker)
            cls._trackers.append(tracker)
        return tracker

    @classmethod
    def instance(cls) -> Tracker | None:
        if cls._trackers:
            return cls._trackers[0]
        return None

    def __init__(self, users: User.Mapping | None = None, redact: dict[str, bool] | None = None, hide_title: bool | None = None, redact_exemption: dict[str, bool] | None = None, timeout: float | None = None) -> None:
        self.users = users or User.Mapping()
        self.hide_title = False if hide_title is None else hide_title
        self.timeout: float | None = None

        # Set below, by name.
        self._redact: dict[re.Pattern[str], bool]
        self._redact_exemption: dict[re.Pattern[str], bool]

        for name, rvalue in (('redact', redact), ('redact_exemption', redact_exemption)):
            if rvalue is None:
                setattr(self, '_{}'.format(name), {re.compile('.*'): False})
            elif isinstance(rvalue, dict):
                attribute: dict[re.Pattern[str], bool] = {}
                for key, value in rvalue.items():
                    if not isinstance(key, string_utils.basestring):
                        raise ValueError("'{}' is not a string, only strings allowed in redaction mapping".format(key))
                    attribute[re.compile(key)] = bool(value)
                setattr(self, '_{}'.format(name), attribute)
            else:
                raise ValueError("Expected redaction mapping to be of type dict, got '{}'".format(type(redact)))

    @decorators.hybridmethod
    def from_string(context: Tracker | type[Tracker], string: str) -> Issue | None:
        if not isinstance(context, type):
            raise NotImplementedError()
        for tracker in context._trackers:
            issue = tracker.from_string(string)
            if issue:
                return issue
        return None

    def user(self, name: str | None = None, username: int | str | None = None, email: str | None = None) -> User | None:
        if not name and not username and not email:
            raise TypeError('No name, username or email defined for user')
        for key in [name, username, email]:
            user = self.users.get(key) if key else None
            if user:
                return user
        return None

    def issues_to_ids(self, issues: Iterable[Issue | int | str]) -> list[str]:
        raise NotImplementedError()

    @decorators.Memoize()
    def me(self) -> User | None:
        raise NotImplementedError()

    def issue(self, id: int) -> Issue:
        raise NotImplementedError()

    def populate(self, issue: Issue, member: str | None = None) -> Issue | None:
        raise NotImplementedError()

    # Returns the updated issue, the comment explaining the change, or None if the change failed.
    def set(self, issue: Issue, **properties: Any) -> Issue | Issue.Comment | None:
        raise NotImplementedError()

    def relate(self, issue: Issue, **relations: Any) -> Issue | Issue.Comment | None:
        raise NotImplementedError()

    def unrelate(self, issue: Issue, **relations: Any) -> Issue | Issue.Comment | None:
        raise NotImplementedError()

    def add_comment(self, issue: Issue, text: str) -> Issue.Comment | None:
        raise NotImplementedError()

    @property
    def projects(self) -> dict[str, dict[str, Any]]:
        raise NotImplementedError()

    def create(
        self, title: str, description: str, *,
        project: str | None = None, component: str | None = None, version: str | None = None, assign: bool = True,
    ) -> Issue | None:
        raise NotImplementedError()

    def cc_radar(self, issue: Issue, block: bool = False, timeout: float | None = None, radar: Issue | None = None) -> Issue | None:
        raise NotImplementedError()
