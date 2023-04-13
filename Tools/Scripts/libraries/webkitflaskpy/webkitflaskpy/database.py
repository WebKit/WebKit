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

from webkitcorepy import Environment


class Database(object):
    _instance = None
    HOST_ENV = 'REDIS_HOST'
    PASSWORD_ENV = 'REDIS_PASSWORD'
    EXPIRATION_ENV = 'REDIS_DEFAULT_EXPIRATION'
    DEFAULT_EXPIRATION = 60 * 60 * 24

    @classmethod
    def instance(cls, **kwargs):
        if not cls._instance:
            cls._instance = cls(**kwargs)
        return cls._instance

    def __init__(self, host=None, password=None, partition=None, default_expiration=None):
        self.partition = partition or ''
        if default_expiration is None:
            self.default_expiration = int(Environment.instance().get(self.EXPIRATION_ENV) or self.DEFAULT_EXPIRATION)
        else:
            self.default_expiration = int(default_expiration)

        self.host = host or Environment.instance().get(self.HOST_ENV)
        self.password = password or Environment.instance().get(self.PASSWORD_ENV)

        if self.host:
            import redis
            self._redis = redis.StrictRedis(host=self.host, password=self.password)
        else:
            import fakeredis
            self._redis = fakeredis.FakeStrictRedis()

    def ping(self):
        return self._redis.ping()

    def get(self, name):
        return self._redis.get(self.partition + name)

    def set(self, name, value, **kwargs):
        if 'ex' not in kwargs and self.default_expiration:
            kwargs['ex'] = self.default_expiration
        return self._redis.set(self.partition + name, value, **kwargs)

    def lock(self, name, **kwargs):
        return self._redis.lock(self.partition + name, **kwargs)

    def delete(self, *names):
        names_to_delete = [self.partition + name for name in names]
        return self._redis.delete(*names_to_delete)

    def scan_iter(self, match=None, **kwargs):
        for key in self._redis.scan_iter(match=self.partition + (match or '*'), **kwargs):
            yield key[len(self.partition):]
