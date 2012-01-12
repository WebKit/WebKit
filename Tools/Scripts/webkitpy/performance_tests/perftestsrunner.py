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
    _test_directories_for_chromium_style_tests = ['inspector']

    def __init__(self, regular_output=sys.stderr, buildbot_output=sys.stdout, args=None):
        self._buildbot_output = buildbot_output
        self._options, self._args = self._parse_args(args)
        self._host = Host()
        self._host._initialize_scm()
        self._port = self._host.port_factory.get(self._options.platform, self._options)
        self._printer = printing.Printer(self._port, self._options, regular_output, buildbot_output, configure_logging=False)
        self._webkit_base_dir_len = len(self._port.webkit_base())
        self._base_path = self._host.filesystem.join(self._port.webkit_base(), self._perf_tests_base_dir)

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

    def _collect_tests(self):
        """Return the list of tests found."""

        def _is_test_file(filesystem, dirname, filename):
            return filename.endswith('.html')

        return find_files.find(self._host.filesystem, self._base_path, paths=self._args, file_filter=_is_test_file)

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
            tests = self._collect_tests()
            unexpected = self._run_tests_set(tests, self._port)
        finally:
            self._printer.cleanup()

        return unexpected

    def _print_status(self, tests, expected, unexpected):
        if len(tests) == expected + unexpected:
            status = "Ran %d tests" % len(tests)
        else:
            status = "Running %d of %d tests" % (expected + unexpected + 1, len(tests))
        if unexpected:
            status += " (%d didn't run)" % unexpected
        self._printer.write(status)

    def _run_tests_set(self, tests, port):
        result_count = len(tests)
        expected = 0
        unexpected = 0
        driver_need_restart = False
        driver = None

        for test in tests:
            if driver_need_restart:
                _log.debug("%s killing driver" % test)
                driver.stop()
                driver = None
            if not driver:
                driver = port.create_driver(worker_number=1)

            relative_test_path = self._host.filesystem.relpath(test, self._base_path)
            self._printer.write('Running %s (%d of %d)' % (relative_test_path, expected + unexpected + 1, len(tests)))

            is_chromium_style = self._host.filesystem.split(relative_test_path)[0] in self._test_directories_for_chromium_style_tests
            test_failed, driver_need_restart = self._run_single_test(test, driver, is_chromium_style)
            if test_failed:
                unexpected = unexpected + 1
            else:
                expected = expected + 1

            self._printer.write('')

        if driver:
            driver.stop()

        return unexpected

    _inspector_result_regex = re.compile('^RESULT .*$')

    def _process_chromium_style_test_result(self, test, output):
        test_failed = False
        got_a_result = False
        for line in re.split('\n', output.text):
            if self._inspector_result_regex.match(line):
                self._buildbot_output.write("%s\n" % line)
                got_a_result = True
            elif not len(line) == 0:
                test_failed = True
                self._printer.write("%s" % line)
        return test_failed or not got_a_result

    _lines_to_ignore_in_parser_result = [
        re.compile(r'^Running \d+ times$'),
        re.compile(r'^Ignoring warm-up '),
        re.compile(r'^\d+$'),
    ]

    def _should_ignore_line_in_parser_test_result(self, line):
        if not line:
            return True
        for regex in self._lines_to_ignore_in_parser_result:
            if regex.match(line):
                return True
        return False

    def _process_parser_test_result(self, test, output):
        got_a_result = False
        test_failed = False
        filesystem = self._host.filesystem
        category, test_name = filesystem.split(filesystem.relpath(test, self._base_path))
        test_name = filesystem.splitext(test_name)[0]
        results = {}
        keys = ['avg', 'median', 'stdev', 'min', 'max']
        score_regex = re.compile(r'^(' + r'|'.join(keys) + r')\s+([0-9\.]+)')
        for line in re.split('\n', output.text):
            score = score_regex.match(line)
            if score:
                results[score.group(1)] = score.group(2)
                continue

            if not self._should_ignore_line_in_parser_test_result(line):
                test_failed = True
                self._printer.write("%s" % line)

        if test_failed or set(keys) != set(results.keys()):
            return True
        self._buildbot_output.write('RESULT %s: %s= %s ms\n' % (category, test_name, results['avg']))
        self._buildbot_output.write(', '.join(['%s= %s ms' % (key, results[key]) for key in keys[1:]]) + '\n')
        return False

    def _run_single_test(self, test, driver, is_chromium_style):
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
            if is_chromium_style:
                test_failed = self._process_chromium_style_test_result(test, output)
            else:
                test_failed = self._process_parser_test_result(test, output)

        if len(output.error):
            self._printer.write('error:\n%s' % output.error)
            test_failed = True

        if test_failed:
            self._printer.write('FAILED')

        return test_failed, driver_need_restart
