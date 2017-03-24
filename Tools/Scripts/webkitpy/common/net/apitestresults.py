# Copyright (C) 2017 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging

from webkitpy.common.net.abstracttestresults import AbstractTestResults

_log = logging.getLogger(__name__)


class APITestResults(AbstractTestResults):
    def __init__(self, failures, timeouts):
        self._failures = failures
        self._timeouts = timeouts

        self._tests_not_passed = failures + timeouts

    @classmethod
    def intersection(cls, first, second):
        intersection_failures = list(set(first._failures) & set(second._failures))
        intersection_timeouts = list(set(first._timeouts) & set(second._timeouts))
        return cls(intersection_failures, intersection_timeouts)

    @classmethod
    def results_from_string(cls, string):
        parsed_results = cls.parse_json_string(string)
        if not parsed_results:
            return None

        if 'failures' not in parsed_results or 'timeouts' not in parsed_results:
            return None

        return cls(parsed_results['failures'], parsed_results['timeouts'])

    def equals(self, other):
        return set(self._failures) == set(other._failures) and set(self._timeouts) == set(other._timeouts)

    def is_subset(self, other):
        return set(self._failures) <= set(other._failures) and set(self._timeouts) <= set(other._timeouts)

    def all_passed(self):
        return not self._tests_not_passed

    def failing_tests(self):
        return self._tests_not_passed

    # No defined failure limit for API tests.
    def did_exceed_test_failure_limit(self):
        return False
