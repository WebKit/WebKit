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


_log = logging.getLogger(__name__)


class TestRunner(object):
    def __init__(self, stream, options, loader):
        self.options = options
        self.stream = stream
        self.loader = loader
        self.test_description = re.compile("(\w+) \(([\w.]+)\)")

    def test_name(self, test):
        m = self.test_description.match(str(test))
        return "%s.%s" % (m.group(2), m.group(1))

    def all_test_names(self, suite):
        names = []
        if hasattr(suite, '_tests'):
            for t in suite._tests:
                names.extend(self.all_test_names(t))
        else:
            names.append(self.test_name(suite))
        return names

    def run(self, suite):
        run_start_time = time.time()
        all_test_names = self.all_test_names(suite)
        result = unittest.TestResult()
        stop = run_start_time
        for test_name in all_test_names:
            if self.options.verbose:
                self.stream.write(test_name)
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
            self.write_result(result, test_name, stop - start, failure, err)

        self.write_summary(result, stop - run_start_time)

        return result

    def write_result(self, result, test_name, test_time, failure=None, err=None):
        timing = ''
        if self.options.timing:
            timing = ' %.4fs' % test_time
        if self.options.verbose:
            if failure:
                msg = ' failed'
            elif err:
                msg = ' erred'
            else:
                msg = ' passed'
            self.stream.write(msg + timing + '\n')
        else:
            if failure:
                msg = 'F'
            elif err:
                msg = 'E'
            else:
                msg = '.'
            self.stream.write(msg)

    def write_summary(self, result, run_time):
        self.stream.write('\n')

        for (test, err) in result.errors:
            self.stream.write("=" * 80 + '\n')
            self.stream.write("ERROR: " + self.test_name(test) + '\n')
            self.stream.write("-" * 80 + '\n')
            for line in err.splitlines():
                self.stream.write(line + '\n')
            self.stream.write('\n')

        for (test, failure) in result.failures:
            self.stream.write("=" * 80 + '\n')
            self.stream.write("FAILURE: " + self.test_name(test) + '\n')
            self.stream.write("-" * 80 + '\n')
            for line in failure.splitlines():
                self.stream.write(line + '\n')
            self.stream.write('\n')

        self.stream.write('-' * 80 + '\n')
        self.stream.write('Ran %d test%s in %.3fs\n' %
            (result.testsRun, result.testsRun != 1 and "s" or "", run_time))

        if result.wasSuccessful():
            self.stream.write('\nOK\n')
        else:
            self.stream.write('FAILED (failures=%d, errors=%d)\n' %
                (len(result.failures), len(result.errors)))
