# Copyright (C) 2021 Apple Inc. All rights reserved.
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

import os


class Environment(object):
    _instance = None

    @classmethod
    def instance(cls, path=None):
        if not cls._instance:
            cls._instance = cls(path=path)
        if path and path != cls._instance.path:
            cls._instance.path = path
        return cls._instance

    def __init__(self, path=None, divider='___'):
        self._mapping = dict()
        self.path = path
        self._divider = divider
        self._paths = set()

    def load(self, *prefixes):
        if not self.path:
            return self
        for file in os.listdir(self.path):
            prefix, key = file.split(self._divider, 1) if self._divider in file else (None, file)
            if prefix and prefix not in prefixes:
                continue
            self._paths.add(os.path.join(self.path, file))
            with open(os.path.join(self.path, file), 'r') as fl:
                self._mapping[key] = fl.read().rstrip('\n')
        return self

    def get(self, key, value=None):
        if key in os.environ:
            return os.environ.get(key, value)
        return self._mapping.get(key, value)

    def secure(self, *extra_paths):
        '''Delete unused environment files in self.path along with the provided extra paths'''

        for file in os.listdir(self.path):
            path = os.path.join(self.path, file)
            if path not in self._paths:
                os.remove(path)
        for path in extra_paths:
            if os.path.isfile(path):
                os.remove(path)
            if os.path.exists(path):
                raise OSError("Failed to delete '{}' when securing credentials".format(path))

    def __getitem__(self, key):
        result = self.get(key)
        if not result:
            raise KeyError(key)
        return result

    def __setitem__(self, key, value):
        os.environ[key] = value

    def keys(self):
        for key in self._mapping.keys():
            yield key
        for key in os.environ.keys():
            yield key

    def values(self):
        for value in self._mapping.values():
            yield value
        for value in os.environ.values():
            yield value

    def items(self):
        for key, value in self._mapping.items():
            yield key, value
        for key, value in os.environ.items():
            yield key, value
