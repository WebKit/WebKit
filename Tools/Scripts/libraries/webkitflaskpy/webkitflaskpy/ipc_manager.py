# Copyright (C) 2023 Apple Inc. All rights reserved.
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

import hashlib
import json

from webkitcorepy import string_utils
from webkitflaskpy import Database


class IPCManager(object):
    DEFAULT_EXPIRATION = 10 * 60
    DEFAULT_TIMEOUT = 60
    RETRY = 3

    @classmethod
    def encode(cls, data):
        encoded = json.dumps(data, sort_keys=True)
        return hashlib.md5(string_utils.encode(encoded)).hexdigest(), encoded

    def __init__(self, *databases):
        self.databases = databases or [Database()]
        self.callbacks = dict()

    def register(self, name, callback):
        if not isinstance(name, str):
            raise ValueError('Name of registered callback must be a string')
        if not callable(callback):
            raise ValueError('Callback must be callable')
        self.callbacks[name] = callback

    def enqueue(self, name, *args, **kwargs):
        expiration = kwargs.pop('ex', self.DEFAULT_EXPIRATION)
        retry = kwargs.pop('retry', self.RETRY)
        if name not in self.callbacks:
            raise ValueError("'{}' is not a registered callback".format(name))
        digest, encoded = self.encode([name, args, kwargs, retry, expiration])

        for database in self.databases:
            database.set(digest, encoded, ex=expiration)
        return True

    def dequeue(self, key, filter=None, index=None, timeout=None):
        timeout = timeout or self.DEFAULT_TIMEOUT
        if index is None:
            for i in range(len(self.databases)):
                result = self.dequeue(key, index=i, timeout=timeout)
                if result:
                    return result
            return False

        lock = self.databases[index].lock(name='lock_{}'.format(key), timeout=timeout, blocking_timeout=0.05)
        try:
            if not lock.acquire():
                return False
            raw = self.databases[index].get(key)
            if not raw:
                self.databases[index].delete(key)
                return False
            data = json.loads(raw)
            if filter and not filter(data):
                return False
            if len(data) != 5:
                self.databases[index].delete(key)
                return False
            name, args, kwargs, retry, expiration = data
            if name not in self.callbacks:
                self.databases[index].delete(key)
                return False

            if retry > 0:
                _, encoded = self.encode([name, args, kwargs, retry - 1, expiration])
                self.databases[index].set(key, encoded, ex=expiration)
            else:
                self.databases[index].delete(key)
            self.callbacks[name](*args, **kwargs)
            if retry > 0:
                self.databases[index].delete(key)
            return True
        finally:
            if lock.local.token is not None:
                lock.release()

    def dequeue_all(self, filter=None, index=None):
        result = False
        indecies = range(len(self.databases)) if index is None else [index]
        for i in indecies:
            for key in self.databases[i].scan_iter():
                key = key.decode()
                result |= self.dequeue(key, filter=filter, index=i)
        return result

    def tasks(self, filter=None, index=None):
        if index is None:
            for i in range(len(self.databases)):
                for task in self.tasks(filter=filter, index=i):
                    yield task
        else:
            for key in self.databases[index].scan_iter():
                key = key.decode()
                raw = self.databases[index].get(key)
                if not raw:
                    continue
                data = json.loads(raw)[:3]
                if not filter or filter(data):
                    yield (key, data)
