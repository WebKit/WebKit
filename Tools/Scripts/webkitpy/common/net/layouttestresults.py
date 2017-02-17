# Copyright (c) 2010, Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging

from webkitpy.common.net.abstracttestresults import AbstractTestResults
from webkitpy.common.net.resultsjsonparser import ParsedJSONResults
from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup, SoupStrainer
from webkitpy.layout_tests.models import test_results
from webkitpy.layout_tests.models import test_failures

_log = logging.getLogger(__name__)


# FIXME: This should be unified with all the layout test results code in the layout_tests package
# This doesn't belong in common.net, but we don't have a better place for it yet.
def path_for_layout_test(test_name):
    return "LayoutTests/%s" % test_name


# FIXME: This should be unified with ResultsSummary or other NRWT layout tests code
# in the layout_tests package.
# This doesn't belong in common.net, but we don't have a better place for it yet.
class LayoutTestResults(AbstractTestResults):
    @classmethod
    def results_from_string(cls, string):
        if not string:
            return None
        parsed_results = ParsedJSONResults(string)
        return cls(parsed_results.test_results(), parsed_results.did_exceed_test_failure_limit())

    def __init__(self, test_results, did_exceed_test_failure_limit):
        self._unit_test_failures = []
        self._test_results = test_results
        self._did_exceed_test_failure_limit = did_exceed_test_failure_limit

    def did_exceed_test_failure_limit(self):
        return self._did_exceed_test_failure_limit

    def test_results(self):
        return [result for result in self._test_results]

    def results_matching_failure_types(self, failure_types):
        return [result for result in self._test_results if result.has_failure_matching_types(*failure_types)]

    def tests_matching_failure_types(self, failure_types):
        return [result.test_name for result in self.results_matching_failure_types(failure_types)]

    def failing_test_results(self):
        return self.results_matching_failure_types(test_failures.ALL_FAILURE_CLASSES)

    def failing_tests(self):
        return [result.test_name for result in self.failing_test_results()] + self._unit_test_failures

    def add_unit_test_failures(self, unit_test_results):
        self._unit_test_failures = unit_test_results
