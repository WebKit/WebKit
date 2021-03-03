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

import calendar
import json
import re

from datetime import datetime
from webkitflaskpy.util import FlaskJSONEncoder


class Commit(object):
    TIMESTAMP_TO_UUID_MULTIPLIER = 100
    MAX_KEY_LENGTH = 128

    REVISION_RE = re.compile(r'[0-9]+$')
    HASH_RE = re.compile(r'[a-fA-F0-9]+$')

    @classmethod
    def from_json(cls, data):
        data = data if isinstance(data, dict) else json.loads(data)
        return cls(
            repository_id=data.get('repository_id'),
            branch=data.get('branch'),
            id=data.get('id'),
            revision=data.get('revision'),
            hash=data.get('hash'),
            timestamp=data.get('timestamp'),
            order=data.get('order'),
            committer=data.get('committer'),
            message=data.get('message'),
        )

    def __init__(self, repository_id, branch, id, timestamp=None, order=None, committer=None, message=None, revision=None, hash=None):
        for argument in [('repository_id', repository_id), ('branch', branch), ('id', id), ('timestamp', timestamp)]:
            if argument[1] is None:
                raise ValueError(f'{argument[0]} is not defined for commit')

        self.repository_id = str(repository_id)
        if not re.match(r'^[a-zA-Z?]+$', self.repository_id) or len(self.repository_id) > self.MAX_KEY_LENGTH:
            raise ValueError(f"'{self.repository_id}' is an invalid repository id")

        self.branch = str(branch)
        if not re.match(r'^[a-zA-Z0-9-.?/]+$', self.branch) or len(self.branch) > self.MAX_KEY_LENGTH:
            raise ValueError(f"'{self.branch}' is an invalid branch name")

        # An id is either a git commit or SVN revision.
        self.id = str(id)
        if not re.match(r'^[a-fA-F0-9?]+$', self.id) or len(self.id) > 40:
            raise ValueError(f"'{self.id}' is an invalid commit id")

        self.revision = revision
        self.hash = hash
        if self.REVISION_RE.match(self.id):
            if not self.revision:
                self.revision = self.id
        elif self.HASH_RE.match(self.id):
            if not self.hash:
                self.hash = self.id

        if self.revision:
            try:
                self.revision = int(self.revision)
            except ValueError:
                raise ValueError(f"'{self.revision}' is not an integer")
        if self.hash:
            self.hash = self.hash.lower()

        if isinstance(timestamp, datetime):
            self.timestamp = timestamp
        else:
            self.timestamp = datetime.utcfromtimestamp(int(timestamp))
        self.order = int(order) if order else 0

        self.committer = committer if committer else None
        self.message = message if message else None

    def timestamp_as_epoch(self):
        return calendar.timegm(self.timestamp.timetuple())

    @property
    def uuid(self):
        # Rebase-based workflows in git can cause commits to have the same commit timestamp. To uniquely recognize them,
        # multiply each timestamp by the `TIMESTAMP_TO_UUID_MULTIPLIER`.
        return self.timestamp_as_epoch() * self.TIMESTAMP_TO_UUID_MULTIPLIER + self.order

    def __eq__(self, other):
        return (isinstance(other, type(self)) and (self.repository_id, self.branch, self.id) == (other.repository_id, other.branch, other.id))

    def __ne__(self, other):
        return not (self == other)

    def __lt__(self, other):
        if self.timestamp < other.timestamp:
            return True
        if self.timestamp > other.timestamp:
            return False
        return self.order < other.order

    def __le__(self, other):
        return self < other or self == other

    def __gt__(self, other):
        if self.timestamp > other.timestamp:
            return True
        if self.timestamp < other.timestamp:
            return False
        return self.order > other.order

    def __ge__(self, other):
        return self > other or self == other

    def __hash__(self):
        return hash(self.repository_id) ^ hash(self.branch) ^ hash(self.id)

    def to_json(self, pretty_print=False):
        return json.dumps(self, cls=self.Encoder, sort_keys=pretty_print, indent=4 if pretty_print else None)

    class Encoder(FlaskJSONEncoder):

        def default(self, obj):
            if isinstance(obj, Commit):
                result = {}
                for key, value in obj.__dict__.items():
                    if value is None:
                        continue
                    result[key] = value
                result['timestamp'] = obj.timestamp_as_epoch()
                return result
            return super(Commit.Encoder, self).default(obj)
