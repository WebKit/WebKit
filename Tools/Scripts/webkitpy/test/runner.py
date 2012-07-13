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
import time
import unittest


_log = logging.getLogger(__name__)


class Runner(object):
    def __init__(self, printer, options, loader):
        self.options = options
        self.printer = printer
        self.loader = loader

    def all_test_names(self, suite):
        names = []
        if hasattr(suite, '_tests'):
            for t in suite._tests:
                names.extend(self.all_test_names(t))
        else:
            names.append(self.printer.test_name(suite))
        return names

    def run(self, suite):
        run_start_time = time.time()
        all_test_names = self.all_test_names(suite)
        result = unittest.TestResult()
        stop = run_start_time
        for test_name in all_test_names:
            self.printer.print_started_test(test_name)
            num_failures = len(result.failures)
            num_errors = len(result.errors)

            start = time.time()
            # FIXME: it's kinda lame that we re-load the test suites for each
            # test, and this may slow things down, but this makes implementing
            # the logging easy and will also allow us to parallelize nicely.
            self.loader.loadTestsFromName(test_name, None).run(result)
            stop = time.time()

            err = None
            failure = None
            if len(result.failures) > num_failures:
                failure = result.failures[num_failures][1]
            elif len(result.errors) > num_errors:
                err = result.errors[num_errors][1]
            self.printer.print_finished_test(result, test_name, stop - start, failure, err)

        self.printer.print_result(result, stop - run_start_time)

        return result
