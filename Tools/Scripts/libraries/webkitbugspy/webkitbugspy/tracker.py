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

import re

from .user import User

from webkitcorepy import decorators


class Tracker(object):
    REFERENCE_RE = re.compile(r'((https|http|rdar|radar)://[^\s><,\'\"}{\]\[)(]+[^\s><,\'\"}{\]\[)(\.\?])')

    _trackers = []

    @classmethod
    def register(cls, tracker):
        if tracker not in cls._trackers:
            setattr(cls, str(type(tracker)).split('.')[-2], tracker)
            cls._trackers.append(tracker)
        return tracker

    @classmethod
    def instance(cls):
        if cls._trackers:
            return cls._trackers[0]
        return None

    def __init__(self, users=None):
        self.users = users or User.Mapping()

    @decorators.hybridmethod
    def from_string(context, string):
        if not isinstance(context, type):
            raise NotImplementedError()
        for tracker in context._trackers:
            issue = tracker.from_string(string)
            if issue:
                return issue
        return None

    def user(self, name=None, username=None, email=None):
        if not name and not username and not email:
            raise TypeError('No name, username or email defined for user')
        for key in [name, username, email]:
            if key and self.users.get(key):
                return self.users.get(key)
        return None

    def issue(self, id):
        raise NotImplementedError()

    def populate(self, issue, member=None):
        raise NotImplementedError()
