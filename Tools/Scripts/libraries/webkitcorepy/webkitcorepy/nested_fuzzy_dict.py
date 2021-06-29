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

from webkitcorepy.string_utils import unicode


class NestedFuzzyDict(object):
    @classmethod
    def assert_valid_key(cls, key):
        if not any((isinstance(key, str), isinstance(key, unicode), isinstance(key, bytes))):
            raise ValueError("'{}' is not a valid key for a NestedDict".format(type(key)))

    def __init__(self, primary_size=None, **kwargs):
        self.primary_size = int(primary_size or 6)
        self._data = dict()
        self.update(dict(**kwargs))

    def getitem(self, keyname, value=None):
        self.assert_valid_key(keyname)
        key_a, key_b = keyname[:self.primary_size], keyname[self.primary_size:]
        found = None
        for key, result in self._data.get(key_a, dict()).items():
            if key.startswith(key_b):
                if found:
                    raise KeyError("Multiple values match '{}'".format(keyname))
                found = key_a + key
                value = result
        return found, value

    def __getitem__(self, keyname):
        key, value = self.getitem(keyname)
        if key:
            return value
        raise KeyError(keyname)

    def get(self, keyname, value=None):
        return self.getitem(keyname, value)[1]

    def __setitem__(self, key, value):
        self.assert_valid_key(key)
        self._data.setdefault(key[:self.primary_size], dict())[key[self.primary_size:]] = value

    def __delitem__(self, keyname):
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

    def __contains__(self, keyname):
        key_a, key_b = keyname[:self.primary_size], keyname[self.primary_size:]
        for key, result in self._data.get(key_a, dict()).items():
            if key.startswith(key_b):
                return True
        return False

    def update(self, data):
        for key, value in data.items():
            self[key] = value

    def __len__(self):
        return sum([len(values) for values in self._data.values()])

    def keys(self):
        for key_a, values in self._data.items():
            for key_b in values.keys():
                yield key_a + key_b

    def values(self):
        for values in self._data.values():
            for value in values.values():
                yield value

    def items(self):
        for key_a, values in self._data.items():
            for key_b, value in values.items():
                yield key_a + key_b, value

    def dict(self):
        result = dict()
        for key, value in self.items():
            result[key] = value
        return result

    def __repr__(self):
        return self.dict().__repr__()

    def __str__(self):
        return self.dict().__str__()
