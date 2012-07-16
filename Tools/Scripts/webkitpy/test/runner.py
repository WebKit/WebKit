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

"""code to actually run a list of python tests."""

import logging
import re
import time
import unittest

from webkitpy.common import message_pool

_log = logging.getLogger(__name__)


_test_description = re.compile("(\w+) \(([\w.]+)\)")


def _test_name(test):
    m = _test_description.match(str(test))
    return "%s.%s" % (m.group(2), m.group(1))


class Runner(object):
    def __init__(self, printer, options, loader):
        self.options = options
        self.printer = printer
        self.loader = loader
        self.result = unittest.TestResult()
        self.worker_factory = lambda caller: _Worker(caller, self.loader)

    def all_test_names(self, suite):
        names = []
        if hasattr(suite, '_tests'):
            for t in suite._tests:
                names.extend(self.all_test_names(t))
        else:
            names.append(_test_name(suite))
        return names

    def run(self, suite):
        run_start_time = time.time()
        all_test_names = self.all_test_names(suite)
        self.printer.num_tests = len(all_test_names)

        with message_pool.get(self, self.worker_factory, int(self.options.child_processes)) as pool:
            pool.run(('test', test_name) for test_name in all_test_names)

        self.printer.print_result(self.result, time.time() - run_start_time)
        return self.result

    def handle(self, message_name, source, test_name, delay=None, result=None):
        if message_name == 'started_test':
            self.printer.print_started_test(source, test_name)
            return

        self.result.testsRun += 1
        self.result.errors.extend(result.errors)
        self.result.failures.extend(result.failures)
        self.printer.print_finished_test(source, test_name, delay, result.failures, result.errors)


class _Worker(object):
    def __init__(self, caller, loader):
        self._caller = caller
        self._loader = loader

    def handle(self, message_name, source, test_name):
        assert message_name == 'test'
        result = unittest.TestResult()
        start = time.time()
        self._caller.post('started_test', test_name)
        self._loader.loadTestsFromName(test_name, None).run(result)

        # The tests in the TestResult contain file objects and other unpicklable things; we only
        # care about the test name, so we rewrite the result to replace the test with the test name.
        # FIXME: We need an automated test for this, but I don't know how to write an automated
        # test that will fail in this case that doesn't get picked up by test-webkitpy normally :(.
        result.failures = [(_test_name(failure[0]), failure[1]) for failure in result.failures]
        result.errors = [(_test_name(error[0]), error[1]) for error in result.errors]

        self._caller.post('finished_test', test_name, time.time() - start, result)
