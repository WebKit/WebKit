# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import argparse
import logging
import re
import unittest

from webkitcorepy import OutputCapture
from webkitcorepy.testing.test_runner import TestRunner


class _Test_TestRunner(unittest.TestCase):
    def _test_pass(self):
        pass

    def _test_fail(self):
        self.fail('assertion failed')

    def _test_error(self):
        raise Exception('exception')

    def _test_skip(self):
        self.skipTest('skipped')

    @unittest.expectedFailure
    def _test_expected_failure(self):
        self.fail('expected failure')

    @unittest.expectedFailure
    def _test_unexpected_success(self):
        pass

    def id(self):
        return self._testMethodName


class StubTestRunner(TestRunner):
    def __init__(self, test_names):
        super().__init__('stub')
        self._test_names = test_names

    def tests(self, args=None):
        return self._test_names

    def run_test(self, test):
        result = unittest.TestResult()
        _Test_TestRunner(test).run(result)
        return result


class TestRunnerTest(unittest.TestCase):
    maxDiff = None

    @staticmethod
    def _normalize(stdout):
        return re.sub(r'[0-9]+\.[0-9]+ seconds', '1.111 seconds', stdout)

    def _run(self, test_name):
        runner = StubTestRunner([test_name])
        with OutputCapture() as captured:
            ret = runner.run(argparse.Namespace(log_level=logging.WARNING))
        return ret, self._normalize(captured.stdout.getvalue()), captured.stderr.getvalue()

    def test_passed(self):
        ret, stdout, stderr = self._run('_test_pass')
        self.assertEqual(ret, 0)
        self.assertEqual(stdout, """\
[1/1] _test_pass passed

Ran 1 of 1 tests in 1.111 seconds

SUCCESS
""")
        self.assertEqual(stderr, '')

    def test_failed(self):
        ret, stdout, stderr = self._run('_test_fail')
        self.assertEqual(ret, 1)
        self.assertEqual(stdout, """\
[1/1] _test_fail failed

Ran 1 of 1 tests in 1.111 seconds

FAILED
    1 failure
""")
        self.assertIn('AssertionError: assertion failed', stderr)
        self.assertEqual(stderr.count('AssertionError: assertion failed'), 1)

    def test_errored(self):
        ret, stdout, stderr = self._run('_test_error')
        self.assertEqual(ret, 1)
        self.assertEqual(stdout, """\
[1/1] _test_error erred

Ran 1 of 1 tests in 1.111 seconds

FAILED
    1 error
""")
        self.assertIn('Exception: exception', stderr)
        self.assertEqual(stderr.count('Exception: exception'), 1)

    def test_skipped(self):
        ret, stdout, stderr = self._run('_test_skip')
        self.assertEqual(ret, 0)
        self.assertEqual(stdout, """\
[1/1] _test_skip skipped

Ran 1 of 1 tests in 1.111 seconds

SUCCESS
""")
        self.assertEqual(stderr, '')

    def test_unexpected_success_causes_failure(self):
        ret, stdout, stderr = self._run('_test_unexpected_success')
        self.assertEqual(ret, 1)
        self.assertEqual(stdout, """\
[1/1] _test_unexpected_success unexpectedly passed

Ran 1 of 1 tests in 1.111 seconds

FAILED
    1 unexpected success(es)
""")
        self.assertEqual(stderr, """
UNEXPECTED SUCCESS: _test_unexpected_success
""")

    def test_expected_failure_returns_success(self):
        ret, stdout, stderr = self._run('_test_expected_failure')
        self.assertEqual(ret, 0)
        self.assertEqual(stdout, """\
[1/1] _test_expected_failure expected failure

Ran 1 of 1 tests in 1.111 seconds

SUCCESS
    1 expected failure(s)
""")
        self.assertEqual(stderr, '')
