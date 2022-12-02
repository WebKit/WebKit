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

import json
import re

from .user import User

from webkitcorepy import decorators, string_utils


class Tracker(object):
    REFERENCE_RE = re.compile(r'((https|http|rdar|radar)://[^\s><,\'\"}{\]\[)(]*[^\s><,\'\"}{\]\[)(\.\?])')

    _trackers = []

    class Encoder(json.JSONEncoder):
        def default(self, obj):
            if isinstance(obj, dict):
                return {key: self.default(value) for key, value in obj.items()}
            if isinstance(obj, list):
                return [self.default(value) for value in obj]
            if not isinstance(obj, Tracker):
                return super(Tracker.Encoder, self).default(obj)
            return obj.Encoder.default(obj)

    class Redaction(object):
        def __init__(self, redacted=False, reason=None):
            self.redacted = redacted
            self.reason = reason

        def __bool__(self):
            return self.redacted

        def __nonzero__(self):
            return self.redacted

        def __repr__(self):
            if not self.redacted:
                return 'is not redacted'
            if self.reason:
                return '{} and is thus redacted'.format(self.reason)
            return 'is redacted for an unknown reason'

        def __str__(self):
            return self.__repr__()

        def __eq__(self, other):
            if isinstance(other, str):
                return str(self) == other
            elif isinstance(other, bool):
                return self.redacted == other
            elif isinstance(other, Tracker.Redaction):
                return self.redacted == other.redacted and self.reason == other.reason
            return False

        def __ne__(self, other):
            return not self.__eq__(other)

    @classmethod
    def from_json(cls, data):
        from . import bugzilla, github, radar

        data = data if isinstance(data, (dict, list, tuple)) else json.loads(data)
        if isinstance(data, (list, tuple)):
            return [cls.from_json(datum) for datum in data]

        if data.get('type') not in ('bugzilla', 'github', 'radar'):
            raise TypeError("'{}' is not a recognized tracker type".format(data.get('type')))

        unpacked = dict(
            redact=data.get('redact'),
        )
        if data.get('type') in ('bugzilla', 'github'):
            unpacked['url'] = data.get('url')
            unpacked['res'] = [re.compile(r) for r in data.get('res', [])]
        if data.get('type') == 'bugzilla':
            unpacked['radar_importer'] = data.get('radar_importer')

        if data.get('type') == 'radar':
            unpacked['project'] = data.get('project', None)
            unpacked['projects'] = data.get('projects', [])
            unpacked['project'] = data.get('project', None)

        return dict(
            bugzilla=bugzilla.Tracker,
            github=github.Tracker,
            radar=radar.Tracker,
        )[data['type']](**unpacked)

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

    def __init__(self, users=None, redact=None):
        self.users = users or User.Mapping()
        if redact is None:
            self._redact = {re.compile('.*'): False}
        elif isinstance(redact, dict):
            self._redact = {}
            for key, value in redact.items():
                if not isinstance(key, string_utils.basestring):
                    raise ValueError("'{}' is not a string, only strings allowed in redaction mapping".format(key))
                self._redact[re.compile(key)] = bool(value)
        else:
            raise ValueError("Expected redaction mapping to be of type dict, got '{}'".format(type(redact)))

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

    @decorators.Memoize()
    def me(self):
        raise NotImplementedError()

    def issue(self, id):
        raise NotImplementedError()

    def populate(self, issue, member=None):
        raise NotImplementedError()

    def set(self, issue, **properties):
        raise NotImplementedError()

    def add_comment(self, issue, text):
        raise NotImplementedError()

    @property
    def projects(self):
        raise NotImplementedError()

    def create(self, title, description, **kwargs):
        raise NotImplementedError()

    def cc_radar(self, issue, block=False, timeout=None, radar=None):
        raise NotImplementedError()
