# Copyright (C) 2021 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import annotations

from typing import Generic, Iterator, Mapping, TypeVar, cast

from webkitcorepy.string_utils import unicode

V = TypeVar('V')


class NestedFuzzyDict(Generic[V]):
    @classmethod
    def assert_valid_key(cls, key: object) -> None:
        if not any((isinstance(key, str), isinstance(key, unicode), isinstance(key, bytes))):
            raise ValueError("'{}' is not a valid key for a NestedDict".format(type(key)))

    def __init__(self, primary_size: int | None = None, **kwargs: V) -> None:
        self.primary_size = int(primary_size or 6)
        self._data: dict[str, dict[str, V]] = dict()
        self.update(dict(**kwargs))

    def getitem(self, keyname: str, value: V | None = None) -> tuple[str | None, V | None]:
        self.assert_valid_key(keyname)
        key_a, key_b = keyname[:self.primary_size], keyname[self.primary_size:]
        found: str | None = None
        for key, result in self._data.get(key_a, dict()).items():
            if key.startswith(key_b):
                if found:
                    raise KeyError("Multiple values match '{}'".format(keyname))
                found = key_a + key
                value = result
        return found, value

    def __getitem__(self, keyname: str) -> V:
        key, value = self.getitem(keyname)
        if key:
            return cast(V, value)
        raise KeyError(keyname)

    def get(self, keyname: str, value: V | None = None) -> V | None:
        return self.getitem(keyname, value)[1]

    def __setitem__(self, key: str, value: V) -> None:
        self.assert_valid_key(key)
        self._data.setdefault(key[:self.primary_size], dict())[key[self.primary_size:]] = value

    def __delitem__(self, keyname: str) -> None:
        self.assert_valid_key(keyname)
        key_a, key_b = keyname[:self.primary_size], keyname[self.primary_size:]
        to_remove = []
        for key, result in self._data.get(key_a, dict()).items():
            if key.startswith(key_b):
                to_remove.append(key)
        if not to_remove:
            raise KeyError(keyname)
        for key in to_remove:
            del self._data[key_a][key]
        if not self._data.get(key_a, True):
            del self._data[key_a]

    def __contains__(self, keyname: str) -> bool:
        key_a, key_b = keyname[:self.primary_size], keyname[self.primary_size:]
        for key, result in self._data.get(key_a, dict()).items():
            if key.startswith(key_b):
                return True
        return False

    def update(self, data: Mapping[str, V]) -> None:
        for key, value in data.items():
            self[key] = value

    def __len__(self) -> int:
        return sum([len(values) for values in self._data.values()])

    def keys(self) -> Iterator[str]:
        for key_a, values in self._data.items():
            for key_b in values.keys():
                yield key_a + key_b

    def values(self) -> Iterator[V]:
        for values in self._data.values():
            for value in values.values():
                yield value

    def items(self) -> Iterator[tuple[str, V]]:
        for key_a, values in self._data.items():
            for key_b, value in values.items():
                yield key_a + key_b, value

    def dict(self) -> dict[str, V]:
        result: dict[str, V] = dict()
        for key, value in self.items():
            result[key] = value
        return result

    def __repr__(self) -> str:
        return self.dict().__repr__()

    def __str__(self) -> str:
        return self.dict().__str__()
