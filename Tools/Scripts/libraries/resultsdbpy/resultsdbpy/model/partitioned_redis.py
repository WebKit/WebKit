# Copyright (C) 2019 Apple Inc. All rights reserved.
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


# This isn't a perfect implementation, but it covers the cases needed by results databases
class PartitionedRedis(object):

    def __init__(self, redis, partition):
        self._redis = redis
        self._partition = partition
        if not self._partition:
            raise ValueError('KeyedRedis object requires a defined partition key')

    def ping(self):
        return self._redis.ping()

    def get(self, name):
        return self._redis.get(self._partition + ':' + name)

    def set(self, name, value, **kwargs):
        return self._redis.set(self._partition + ':' + name, value, **kwargs)

    def lock(self, name, **kwargs):
        return self._redis.lock(self._partition + ':' + name, **kwargs)

    def delete(self, *names):
        names_to_delete = [self._partition + ':' + name for name in names]
        return self._redis.delete(*names_to_delete)

    def scan_iter(self, match=None, **kwargs):
        for key in self._redis.scan_iter(match=self._partition + ':' + match, **kwargs):
            yield key[len(self._partition) + 1:]
