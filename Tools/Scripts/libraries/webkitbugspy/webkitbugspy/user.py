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

import json
from typing import Any, Iterator, Optional, Sequence, Union

from collections import defaultdict
from webkitcorepy import string_utils


class User(object):
    class Encoder(json.JSONEncoder):

        def default(self, obj: Any) -> Any:
            if isinstance(obj, dict):
                return {key: self.default(value) for key, value in obj.items()}
            if isinstance(obj, list):
                return [self.default(value) for value in obj]
            if not isinstance(obj, User):
                return super(User.Encoder, self).default(obj)

            result: dict[str, Any] = {}
            if obj._name:
                result['name'] = obj._name
            if obj.emails:
                result['emails'] = obj.emails
            if obj.username:
                result['username'] = obj.username

            return result

    # Maps each of a user's names, usernames and emails to that user.
    class Mapping(defaultdict[Union[str, int, None], Optional['User']]):
        def __init__(self) -> None:
            super(User.Mapping, self).__init__(lambda: None)

        def add(self, user: User) -> User:
            return self.create(name=user._name, username=user.username, emails=user.emails)

        def create(self, name: str | None = None, username: int | str | None = None, emails: Sequence[str | None] | None = None) -> User:
            matched_key = None
            user = None
            keys: list[str | int | None] = [name, username, *(emails or [])]
            for key in keys:
                if not key:
                    continue
                candidate = self.get(key)

                if not candidate:
                    continue
                if user and hash(user) != hash(candidate):
                    raise RuntimeError("'{}' matches '{}', but '{}' already matched '{}'".format(
                        key, candidate, matched_key, user,
                    ))
                matched_key = key
                user = candidate

            if user:
                if name:
                    user._name = name
                if username:
                    user.username = username
                for email in emails or []:
                    if email and email not in user.emails:
                        user.emails.append(email)
            else:
                user = User(name=name, username=username, emails=emails)

            for key in keys:
                self[key] = user

            return user

        # Iterating over a mapping yields each of its users once, rather than its keys.
        def __iter__(self) -> Iterator[User]:  # type: ignore[override]
            yielded = set()
            for user in self.values():
                if user is None or hash(user) in yielded:
                    continue
                yielded.add(hash(user))
                yield user

    def __init__(self, name: str | None = None, username: int | str | None = None, emails: Sequence[str | None] | None = None) -> None:
        self._name = name
        self.emails: list[str] = [email for email in emails or [] if email]
        self.username = username
        if not self._name_or_none():
            raise TypeError('Not enough arguments to define user')

    def _name_or_none(self) -> str | None:
        if self._name:
            return self._name
        if self.username and isinstance(self.username, string_utils.basestring):
            return self.username
        if self.email:
            return self.email
        if self.username:
            return str(self.username)
        return None

    @property
    def name(self) -> str:
        name = self._name_or_none()
        # Users can't be created without something to call them by.
        assert name is not None
        return name

    @property
    def email(self) -> str | None:
        if not self.emails:
            return None
        return self.emails[0]

    def __hash__(self) -> int:
        if self.username:
            return hash(self.username)
        if self._name:
            return hash(self._name)
        if self.email:
            return hash(self.email)
        return 0

    def __repr__(self) -> str:
        address = None
        for candidate in [self.username, self.email]:
            if isinstance(candidate, string_utils.basestring) and candidate != self.name:
                address = candidate
                break
        if not address:
            return self.name
        return u'{} <{}>'.format(self.name, address)

    def __cmp__(self, other: object) -> int:
        if isinstance(other, str):
            ref_value = other
        elif isinstance(other, User):
            ref_value = str(other.username or self.name or '')
        elif other is None:
            ref_value = ''
        else:
            raise ValueError('Cannot compare {} with {}'.format(User, type(other)))
        if str(self.username or self.name or '') == str(ref_value):
            return 0
        return 1 if str(self.username or self.name or '') > ref_value else -1

    def __eq__(self, other: object) -> bool:
        return self.__cmp__(other) == 0

    def __ne__(self, other: object) -> bool:
        return self.__cmp__(other) != 0

    def __lt__(self, other: User | str | None) -> bool:
        return self.__cmp__(other) < 0

    def __le__(self, other: User | str | None) -> bool:
        return self.__cmp__(other) <= 0

    def __gt__(self, other: User | str | None) -> bool:
        return self.__cmp__(other) > 0

    def __ge__(self, other: User | str | None) -> bool:
        return self.__cmp__(other) >= 0
