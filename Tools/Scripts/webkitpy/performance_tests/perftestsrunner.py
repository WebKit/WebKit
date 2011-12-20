#!/usr/bin/env python
# Copyright (C) 2011 Google Inc. All rights reserved.
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

"""Run Inspector's perf tests in perf mode."""

import logging
import optparse
import re
import sys

from webkitpy.common import find_files
from webkitpy.common.host import Host
from webkitpy.layout_tests.port.driver import DriverInput
from webkitpy.layout_tests.views import printing

_log = logging.getLogger(__name__)


class PerfTestsRunner(object):
    _perf_tests_base_dir = 'PerformanceTests'
    _result_regex = re.compile('^RESULT .*$')

    def __init__(self, perf_tests_dir, regular_output=sys.stderr, buildbot_output=sys.stdout, args=None):
        self._perf_tests_dir = perf_tests_dir
        self._buildbot_output = buildbot_output
        self._options, self._args = self._parse_args(args)
        self._host = Host()
        self._host._initialize_scm()
        self._port = self._host.port_factory.get(self._options.platform, self._options)
        self._printer = printing.Printer(self._port, self._options, regular_output, buildbot_output, configure_logging=False)
        self._webkit_base_dir_len = len(self._port.webkit_base())

    def _parse_args(self, args=None):
        print_options = printing.print_options()

        perf_option_list = [
            optparse.make_option('--debug', action='store_const', const='Debug', dest="configuration",
                                 help='Set the configuration to Debug'),
            optparse.make_option('--release', action='store_const', const='Release', dest="configuration",
                                 help='Set the configuration to Release'),
            optparse.make_option("--platform",
                                 help="Specify port/platform being tested (i.e. chromium-mac)"),
            optparse.make_option("--build-directory",
                                 help="Path to the directory under which build files are kept (should not include configuration)"),
            optparse.make_option("--time-out-ms", default=30000,
                                 help="Set the timeout for each test"),
            ]

        option_list = (perf_option_list + print_options)
        return optparse.OptionParser(option_list=option_list).parse_args(args)

    def _collect_tests(self, webkit_base, filesystem=None):
        """Return the list of tests found."""

        def _is_test_file(filesystem, dirname, filename):
            return filename.endswith('.html')

        filesystem = filesystem or self._host.filesystem
        base_dir = filesystem.join(webkit_base, self._perf_tests_base_dir, self._perf_tests_dir)
        return find_files.find(filesystem, base_dir, paths=self._args, file_filter=_is_test_file)

    def run(self):
        if self._options.help_printing:
            self._printer.help_printing()
            self._printer.cleanup()
            return 0

        if not self._port.check_build(needs_http=False):
            _log.error("Build not up to date for %s" % self._port._path_to_driver())
            return -1

        # We wrap any parts of the run that are slow or likely to raise exceptions
        # in a try/finally to ensure that we clean up the logging configuration.
        unexpected = -1
        try:
            tests = self._collect_tests(self._port.webkit_base())
            unexpected = self._run_tests_set(tests, self._port)
        finally:
            self._printer.cleanup()

        return unexpected

    def _run_tests_set(self, tests, port):
        result_count = len(tests)
        expected = 0
        unexpected = 0
        self._printer.print_one_line_summary(result_count, 0, 0)
        driver_need_restart = False
        driver = None

        for test in tests:
            if driver_need_restart:
                _log.debug("%s killing driver" % test)
                driver.stop()
                driver = None
            if not driver:
                driver = port.create_driver(worker_number=1)

            test_failed, driver_need_restart = self._run_single_test(test, driver)
            if test_failed:
                unexpected = unexpected + 1
            else:
                expected = expected + 1

            self._printer.print_one_line_summary(result_count, expected, unexpected)

        if driver:
            driver.stop()

        return unexpected

    def _run_single_test(self, test, driver):
        test_failed = False
        driver_need_restart = False
        output = driver.run_test(DriverInput(test, self._options.time_out_ms, None, False))

        if output.text == None:
            test_failed = True
        elif output.timeout:
            self._printer.write('timeout: %s' % test[self._webkit_base_dir_len + 1:])
            test_failed = True
            driver_need_restart = True
        elif output.crash:
            self._printer.write('crash: %s' % test[self._webkit_base_dir_len + 1:])
            driver_need_restart = True
            test_failed = True
        else:
            got_a_result = False
            for line in re.split('\n', output.text):
                if self._result_regex.match(line):
                    self._buildbot_output.write("%s\n" % line)
                    got_a_result = True
                elif not len(line) == 0:
                    test_failed = True
                    self._printer.write("%s" % line)
            test_failed = test_failed or not got_a_result

        if len(output.error):
            self._printer.write('error:\n%s' % output.error)
            test_failed = True

        return test_failed, driver_need_restart
