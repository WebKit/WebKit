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

import json
import os
import sys
import re

from webkitcorepy import CallByNeed, string_utils


class CommitClassifier(object):
    class LineFilter(object):
        DEFAULT_FUZZ_RATIO = 90

        @classmethod
        def fuzzy(cls, string, ratio=None):
            try:
                from rapidfuzz import fuzz
            except ModuleNotFoundError:
                return re.compile(string)

            ratio = cls.DEFAULT_FUZZ_RATIO if not ratio else ratio
            return lambda x: fuzz.partial_ratio(string, x) >= ratio

        def __init__(self, value):
            if isinstance(value, str) or isinstance(value, string_utils.unicode):
                self.description = value
                self.do = lambda x: re.search(value, x)
            elif isinstance(value, dict) and 'value' in value:
                self.description = 'fuzz({}, {}%)'.format(value['value'], value.get('ratio', self.DEFAULT_FUZZ_RATIO))
                self.do = self.fuzzy(value['value'], ratio=value.get('ratio'))
            else:
                raise ValueError("'{}' not a valid header filter".format(value))

        def __repr__(self):
            return self.description

        def __call__(self, string):
            return bool(self.do(string))

    class CommitClass(object):
        @classmethod
        def filter_header(cls, header):
            if isinstance(header, str):
                return re.compile(header)
            return None

        def __init__(self, name, pickable=True, headers=None, trailers=None, paths=None, **kwargs):
            self.name = name
            self.pickable = pickable
            self.headers = [CommitClassifier.LineFilter(header) for header in headers or []]
            self.trailers = [CommitClassifier.LineFilter(trailer) for trailer in trailers or []]
            self.paths = [re.compile(r'^{}'.format(path)) for path in (paths or [])]

            if not self.headers and not self.trailers and not self.paths:
                raise ValueError('A CommitClass must not match all commits')

            for argument, _ in kwargs.items():
                sys.stderr.write('{} is not a valid member in CommitClassifier.CommitClass\n'.format(argument))

        def __repr__(self):
            description = '{}(\n'.format(self.name)
            description += '    pickable = {}\n'.format(self.pickable)
            if self.headers:
                description += '    headers = [\n'
                for header in self.headers:
                    description += '        {}\n'.format(header)
                description += '    ]\n'
            if self.paths:
                description += '    paths = [\n'
                for path in self.paths:
                    description += '        {}\n'.format(path.pattern)
                description += '    ]\n'
            description += ')'
            return description

    @classmethod
    def load(cls, file):
        result = cls()
        contents = json.load(file)
        for commit_class in contents:
            result.classes.append(cls.CommitClass(**commit_class))
        return result

    def __init__(self, classes=None):
        self.classes = classes or []

    def classify(self, commit, repository=None):
        header = commit.message.splitlines()[0]
        trailers = commit.trailers
        paths_for = CallByNeed(
            callback=lambda: repository.files_changed(commit.hash or str(commit)),
            type=list,
        )

        for klass in self.classes:
            matching_header = bool(klass.headers and header)
            matching_trailer = bool(klass.trailers and trailers)
            can_exclude = matching_header or matching_trailer or bool(klass.paths and paths_for.value)
            if not can_exclude:
                continue

            matches_header = klass.headers and header and any([f(header) for f in klass.headers])
            matches_trailers = klass.trailers and trailers and any([any([f(trailer) for f in klass.trailers]) for trailer in trailers])
            if (matching_header or matching_trailer) and not matches_header and not matches_trailers:
                continue

            if klass.paths and paths_for.value and not all([
                any([c.match(path) for c in klass.paths]) for path in paths_for.value if not path.endswith('ChangeLog')
            ]):
                continue
            return klass
        return None
