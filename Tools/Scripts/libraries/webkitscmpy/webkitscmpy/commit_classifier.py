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

from __future__ import annotations

import json
import os
import sys
import re
from typing import IO, Any, Callable, TYPE_CHECKING

from webkitcorepy import CallByNeed, string_utils

if TYPE_CHECKING:
    from webkitscmpy import Commit
    from webkitscmpy.scm_base import ScmBase


class CommitClassifier(object):
    class LineFilter(object):
        DEFAULT_FUZZ_RATIO = 90

        @classmethod
        def fuzzy(cls, string: str, ratio: int | None = None) -> Callable[[str], Any]:
            try:
                from rapidfuzz import fuzz
            except ModuleNotFoundError:
                return lambda x: re.compile(string).match(x)

            ratio = cls.DEFAULT_FUZZ_RATIO if not ratio else ratio
            return lambda x: fuzz.partial_ratio(string, x) >= ratio

        def __init__(self, value: dict[str, Any] | str) -> None:
            self.do: Callable[[str], Any]
            if isinstance(value, str) or isinstance(value, string_utils.unicode):
                self.description = value
                pattern = value
                self.do = lambda x: re.search(pattern, x)
            elif isinstance(value, dict) and 'value' in value:
                self.description = 'fuzz({}, {}%)'.format(value['value'], value.get('ratio', self.DEFAULT_FUZZ_RATIO))
                self.do = self.fuzzy(value['value'], ratio=value.get('ratio'))
            else:
                raise ValueError("'{}' not a valid header filter".format(value))

        def __repr__(self) -> str:
            return self.description

        def __call__(self, string: str) -> bool:
            return bool(self.do(string))

    class CommitClass(object):
        @classmethod
        def filter_header(cls, header: object) -> re.Pattern[str] | None:
            if isinstance(header, str):
                return re.compile(header)
            return None

        def __init__(
            self, name: str, pickable: bool = True, headers: list[dict[str, Any] | str] | None = None, contents: list[dict[str, Any] | str] | None = None,
            trailers: list[dict[str, Any] | str] | None = None, paths: list[str] | None = None, **kwargs: Any,
        ) -> None:
            self.name = name
            self.pickable = pickable
            self.headers = [CommitClassifier.LineFilter(header) for header in headers or []]
            self.contents = [CommitClassifier.LineFilter(content) for content in contents or []]
            self.trailers = [CommitClassifier.LineFilter(trailer) for trailer in trailers or []]
            self.paths = [re.compile(r'^{}'.format(path)) for path in (paths or [])]

            if not self.headers and not self.trailers and not self.paths:
                raise ValueError('A CommitClass must not match all commits')

            for argument, _ in kwargs.items():
                sys.stderr.write('{} is not a valid member in CommitClassifier.CommitClass\n'.format(argument))

        def __repr__(self) -> str:
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
    def load(cls, file: IO[str]) -> CommitClassifier:
        result = cls()
        contents = json.load(file)
        for commit_class in contents:
            result.classes.append(cls.CommitClass(**commit_class))
        return result

    def __init__(self, classes: list[CommitClassifier.CommitClass] | None = None) -> None:
        self.classes = classes or []

    def classify(self, commit: Commit, repository: ScmBase | None = None) -> CommitClassifier.CommitClass | None:
        lines = (commit.message or '').splitlines()
        header = lines[0] if lines else ''
        trailers = commit.trailers
        contents = commit.message
        paths_for: CallByNeed[list[str]] = CallByNeed(
            callback=lambda: repository.files_changed(commit.hash or str(commit)) if repository else [],
            type=list,
        )

        for klass in self.classes:
            matching_header = bool(klass.headers and header)
            matching_trailer = bool(klass.trailers and trailers)
            matching_content = bool(klass.contents and contents)
            can_exclude = matching_header or matching_trailer or bool(klass.paths and paths_for.value) or matching_content
            if not can_exclude:
                continue

            matches_header = klass.headers and header and any([f(header) for f in klass.headers])
            matches_trailers = klass.trailers and trailers and any([any([f(trailer) for f in klass.trailers]) for trailer in trailers])
            matches_content = klass.contents and contents and any([f(contents) for f in klass.contents])
            if (matching_header or matching_trailer or matching_content) and not matches_header and not matches_trailers and not matches_content:
                continue

            if klass.paths and paths_for.value and not all([
                any([c.match(path) for c in klass.paths]) for path in paths_for.value if not path.endswith('ChangeLog')
            ]):
                continue
            return klass
        return None
