# Copyright (C) 2010 Google Inc. All rights reserved.
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

from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.models.test import Test
from webkitpy.layout_tests.models.test_input import TestInput


class TestResult(object):
    """Data object containing the results of a single test."""

    def __init__(self, test_input, failures=None, test_run_time=None, has_stderr=False, reftest_type=None, pid=None, references=None):
        # this takes a TestInput, and not a Test, as running the same Test with
        # different input options can result in differing results
        if not isinstance(test_input, TestInput):
            # FIXME: figure out something better
            # Changing all callers will be hard but probably worth it?
            test_input = TestInput(Test(test_input))
        self.test_input = test_input
        self.failures = failures or []
        self.test_run_time = test_run_time or 0  # The time taken to execute the test itself.
        self.has_stderr = has_stderr
        self.reftest_type = sorted(reftest_type or [])
        self.pid = pid
        self.references = references or []

        # FIXME: Setting this in the constructor makes this class hard to mutate.
        self.type = test_failures.determine_result_type(failures)

        # These are set by the worker, not by the driver, so they are not passed to the constructor.
        self.worker_name = ''
        self.shard_name = ''
        self.total_run_time = 0  # The time taken to run the test plus any references, compute diffs, etc.
        self.test_number = None
        self.is_other_crash = False

    @property
    def test_name(self):
        return self.test_input.test_name

    def __eq__(self, other):
        return (self.test_name == other.test_name and
                self.failures == other.failures and
                self.test_run_time == other.test_run_time)

    def __ne__(self, other):
        return not (self == other)

    def __hash__(self):
        return self.test_name.__hash__()

    def __str__(self):
        result = self.test_name
        if len(self.failures):
            result += ' failures: ' + ','.join([failure.message() for failure in self.failures])
        if len(self.reftest_type):
            result += ' reftest_type: ' + ','.join(self.reftest_type)
        return result

    def convert_to_failure(self, failure_result):
        if self.type is failure_result.type:
            return
        self.failures.extend(failure_result.failures)
        self.type = test_failures.determine_result_type(self.failures)

    def has_failure_matching_types(self, *failure_classes):
        for failure in self.failures:
            if type(failure) in failure_classes:
                return True
        return False
