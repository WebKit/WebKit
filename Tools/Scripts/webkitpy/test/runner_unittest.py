# Copyright (C) 2012 Google, Inc.
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
import re
import unittest

from webkitcorepy import StringIO

from webkitpy.tool.mocktool import MockOptions
from webkitpy.test.printer import Printer
from webkitpy.test.runner import Runner


class FakeTestCase(object):
    def __init__(self, name):
        self.name = name
        self.failureException = AssertionError

    def id(self):
        return self.name


class FakeModuleSuite(object):
    def __init__(self, name, result, msg):
        self.name = name
        self.result = result
        self.msg = msg

    def __str__(self):
        return self.name

    def run(self, result):
        tc = FakeTestCase(self.name)
        result.startTest(tc)
        try:
            if self.result == 'F':
                result.addFailure(tc, (None, None, None))
            elif self.result == 'E':
                result.addError(tc, (None, None, None))
            elif self.result == '.':
                result.addSuccess(tc)
            elif self.result == 'x':
                result.addExpectedFailure(tc, (None, None, None))
            elif self.result == 'u':
                result.addUnexpectedSuccess(tc)
            else:
                assert False, f'unreachable: {self.result!r}'
        finally:
            result.stopTest(tc)


class FakeTopSuite(object):
    def __init__(self, tests):
        self._tests = tests


class FakeLoader(object):
    def __init__(self, *test_triples):
        self.triples = test_triples
        self._tests = []
        self._results = {}
        for test_name, result, msg in self.triples:
            self._tests.append(test_name)
            m = re.match(r"(\w+) \(([\w.]+)\)", test_name)
            self._results['%s.%s' % (m.group(2), m.group(1))] = tuple([test_name, result, msg])

    def top_suite(self):
        return FakeTopSuite(self._tests)

    def loadTestsFromName(self, name, _):
        return FakeModuleSuite(*self._results[name])


class _Test_Runner(unittest.TestCase):
    def _test_all_subtests_pass(self):
        for i in range(2):
            with self.subTest(i=i):
                pass

    def _test_one_subtest_fails(self):
        with self.subTest(i=0):
            self.fail('boom')

    def _test_two_subtests_fail(self):
        for i in range(2):
            with self.subTest(i=i):
                self.fail('boom %d' % i)

    def _test_plain_failure(self):
        self.fail('boom')


class StubLoader(object):
    def loadTestsFromName(self, name, _):
        _, method_name = name.rsplit('.', 1)
        return _Test_Runner(method_name)


class RunnerTest(unittest.TestCase):
    def setUp(self):
        # Here we have to jump through a hoop to make sure test-webkitpy doesn't log
        # any messages from these tests :(.
        self.root_logger = logging.getLogger()
        self.log_levels = []
        self.log_handlers = self.root_logger.handlers[:]
        for handler in self.log_handlers:
            self.log_levels.append(handler.level)
            handler.level = logging.CRITICAL

    def tearDown(self):
        for handler in self.log_handlers:
            handler.level = self.log_levels.pop(0)

    def test_run(self, verbose=0, timing=False, child_processes=1, quiet=False):
        options = MockOptions(verbose=verbose, timing=timing, child_processes=child_processes, quiet=quiet, pass_through=False)
        stream = StringIO()
        loader = FakeLoader(('test1 (Foo)', '.', ''),
                            ('test2 (Foo)', 'F', 'test2\nfailed'),
                            ('test3 (Foo)', 'E', 'test3\nerred'))
        runner = Runner(Printer(stream, options), loader)
        runner.run(['Foo.test1', 'Foo.test2', 'Foo.test3'], 1)
        self.assertEqual(len(runner.tests_run), 3)
        self.assertEqual(len(runner.failures), 1)
        self.assertEqual(len(runner.errors), 1)

    def test_run_expected_failures(self):
        options = MockOptions(verbose=0, timing=False, child_processes=1, quiet=False, pass_through=False)
        stream = StringIO()
        loader = FakeLoader(('test1 (Foo)', 'x', ''),
                            ('test2 (Foo)', 'u', ''))
        runner = Runner(Printer(stream, options), loader)
        runner.run(['Foo.test1', 'Foo.test2'], 1)
        self.assertEqual(len(runner.tests_run), 2)
        self.assertEqual(len(runner.failures), 0)
        self.assertEqual(len(runner.errors), 0)
        self.assertEqual(len(runner.expected_failures), 1)
        self.assertEqual(len(runner.unexpected_successes), 1)

    def _run(self, method_name):
        options = MockOptions(verbose=0, timing=False, child_processes=1, quiet=False, pass_through=False)
        stream = StringIO()
        printer = Printer(stream, options)
        runner = Runner(printer, StubLoader())
        runner.run(['_Test_Runner.%s' % method_name], 1)
        return runner

    def test_one_failing_subtest_does_not_crash_the_runner(self):
        runner = self._run('_test_one_subtest_fails')
        self.assertEqual(len(runner.tests_run), 1)
        self.assertEqual(len(runner.failures), 1)
        self.assertEqual(len(runner.errors), 0)

    def test_two_failing_subtests_do_not_crash_the_runner(self):
        runner = self._run('_test_two_subtests_fail')
        self.assertEqual(len(runner.tests_run), 1)
        self.assertEqual(len(runner.failures), 1)
        self.assertEqual(len(runner.errors), 0)

    def test_all_passing_subtests_do_not_crash_the_runner(self):
        runner = self._run('_test_all_subtests_pass')
        self.assertEqual(len(runner.tests_run), 1)
        self.assertEqual(len(runner.failures), 0)
        self.assertEqual(len(runner.errors), 0)

    def test_failing_subtest_failure_message_identifies_the_subtest(self):
        runner = self._run('_test_two_subtests_fail')
        _, failures = runner.failures[0]
        self.assertEqual(failures[0].splitlines()[0],
                         '_test_two_subtests_fail (webkitpy.test.runner_unittest._Test_Runner) (i=0)')
        self.assertEqual(failures[0].splitlines()[-1], 'AssertionError: boom 0')
        self.assertEqual(failures[1].splitlines()[0],
                         '_test_two_subtests_fail (webkitpy.test.runner_unittest._Test_Runner) (i=1)')
        self.assertEqual(failures[1].splitlines()[-1], 'AssertionError: boom 1')

    def test_plain_failure_message_still_includes_the_traceback(self):
        runner = self._run('_test_plain_failure')
        _, failures = runner.failures[0]
        self.assertEqual(failures[0].splitlines()[0],
                         '_test_plain_failure (webkitpy.test.runner_unittest._Test_Runner)')
        self.assertEqual(failures[0].splitlines()[-1], 'AssertionError: boom')
