# Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

"""Line-based expectation file format (TestExpectations).

This is the primary format used by WebKit layout tests and API tests.
Each non-blank, non-comment line describes one expectation.
"""

from typing import List

from webkitexpectationspy.expectations import Expectation
from webkitexpectationspy.parser import ExpectationParser, ParseWarning


class ExpectationFile:
    def __init__(self, filename, raw_content, parsed_expectations, parse_warnings):
        self._filename = filename
        self._raw_content = raw_content
        self._expectations = parsed_expectations
        self._warnings = parse_warnings

    @classmethod
    def parse(cls, filename, content, suite=None):
        parser = ExpectationParser(suite=suite)
        results = parser.parse(filename, content)

        expectations = []
        warnings = []
        for exp, line_warnings in results:
            warnings.extend(line_warnings)
            if exp is not None:
                expectations.append(exp)

        return cls(filename, content, expectations, warnings)

    def serialize(self) -> str:
        lines = []
        for exp in self._expectations:
            lines.append(repr(exp))
        return '\n'.join(lines)

    def expectations(self) -> List[Expectation]:
        return self._expectations

    def warnings(self) -> List[ParseWarning]:
        return self._warnings

    @property
    def filename(self) -> str:
        return self._filename

    @property
    def raw_content(self) -> str:
        return self._raw_content
