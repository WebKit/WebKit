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


import logging
import time

from webkitpy.layout_tests.port import base
from webkitpy.layout_tests.layout_package import test_failures
from webkitpy.layout_tests.layout_package.test_results import TestResult


_log = logging.getLogger(__name__)


def run_single_test(port, options, test_input, driver, worker_name, test_types):
    # FIXME: Pull this into TestShellThread._run().
    runner = SingleTestRunner(options, port, driver, test_input, worker_name, test_types)
    return runner.run()


class ExpectedDriverOutput:
    """Groups information about an expected driver output."""
    def __init__(self, text, image, image_hash):
        self.text = text
        self.image = image
        self.image_hash = image_hash


class SingleTestRunner:

    def __init__(self, options, port, driver, test_input, worker_name, test_types):
        self._options = options
        self._port = port
        self._driver = driver
        self._filename = test_input.filename
        self._timeout = test_input.timeout
        self._worker_name = worker_name
        self._test_types = test_types
        self._testname = port.relative_test_filename(test_input.filename)

    def _expected_driver_output(self):
        return ExpectedDriverOutput(self._port.expected_text(self._filename),
                                    self._port.expected_image(self._filename),
                                    self._port.expected_checksum(self._filename))

    def _should_fetch_expected_checksum(self):
        return (self._options.pixel_tests and
                not (self._options.new_baseline or self._options.reset_results))

    def _driver_input(self):
        # The image hash is used to avoid doing an image dump if the
        # checksums match, so it should be set to a blank value if we
        # are generating a new baseline.  (Otherwise, an image from a
        # previous run will be copied into the baseline."""
        image_hash = None
        if self._should_fetch_expected_checksum():
            image_hash = self._port.expected_checksum(self._filename)
        return base.DriverInput(self._filename, self._timeout, image_hash)

    def run(self):
        if self._options.new_baseline or self._options.reset_results:
            return self._run_rebaseline()
        return self._run_compare_test()

    def _run_compare_test(self):
        driver_output = self._driver.run_test(self._driver_input())
        return self._process_output(driver_output)

    def _run_rebaseline(self):
        driver_output = self._driver.run_test(self._driver_input())
        failures = self._handle_error(driver_output)
        # FIXME: It the test crashed or timed out, it might be bettter to avoid
        # to write new baselines.
        self._save_baselines(driver_output)
        return TestResult(self._filename, failures, driver_output.test_time)

    def _save_baselines(self, driver_output):
        # Although all test_shell/DumpRenderTree output should be utf-8,
        # we do not ever decode it inside run-webkit-tests.  For some tests
        # DumpRenderTree may not output utf-8 text (e.g. webarchives).
        self._save_baseline_data(driver_output.text, ".txt",
                                 generate_new_baseline=self._options.new_baseline)
        if self._options.pixel_tests and driver_output.image_hash:
            self._save_baseline_data(driver_output.image, ".png",
                                     generate_new_baseline=self._options.new_baseline)
            self._save_baseline_data(driver_output.image_hash, ".checksum",
                                     generate_new_baseline=self._options.new_baseline)

    def _save_baseline_data(self, data, modifier, generate_new_baseline=True):
        """Saves a new baseline file into the port's baseline directory.

        The file will be named simply "<test>-expected<modifier>", suitable for
        use as the expected results in a later run.

        Args:
          data: result to be saved as the new baseline
          modifier: type of the result file, e.g. ".txt" or ".png"
          generate_new_baseline: whether to enerate a new, platform-specific
            baseline, or update the existing one
        """

        port = self._port
        fs = port._filesystem
        if generate_new_baseline:
            relative_dir = fs.dirname(self._testname)
            baseline_path = port.baseline_path()
            output_dir = fs.join(baseline_path, relative_dir)
            output_file = fs.basename(fs.splitext(self._filename)[0] +
                "-expected" + modifier)
            fs.maybe_make_directory(output_dir)
            output_path = fs.join(output_dir, output_file)
            _log.debug('writing new baseline result "%s"' % (output_path))
        else:
            output_path = port.expected_filename(self._filename, modifier)
            _log.debug('resetting baseline result "%s"' % output_path)

        port.update_baseline(output_path, data)

    def _handle_error(self, driver_output):
        failures = []
        fs = self._port._filesystem
        if driver_output.timeout:
            failures.append(test_failures.FailureTimeout())
        if driver_output.crash:
            failures.append(test_failures.FailureCrash())
            _log.debug("%s Stacktrace for %s:\n%s" % (self._worker_name, self._testname,
                                                      driver_output.error))
            stack_filename = fs.join(self._options.results_directory, self._testname)
            stack_filename = fs.splitext(stack_filename)[0] + "-stack.txt"
            fs.maybe_make_directory(fs.dirname(stack_filename))
            fs.write_text_file(stack_filename, driver_output.error)
        elif driver_output.error:
            _log.debug("%s %s output stderr lines:\n%s" % (self._worker_name, self._testname,
                                                           driver_output.error))
        return failures

    def _process_output(self, driver_output):
        """Receives the output from a DumpRenderTree process, subjects it to a
        number of tests, and returns a list of failure types the test produced.
        Args:
          driver_output: a DriverOutput object containing the output from the driver

        Returns: a TestResult object
        """
        fs = self._port._filesystem
        failures = self._handle_error(driver_output)
        expected_driver_output = self._expected_driver_output()

        # Check the output and save the results.
        start_time = time.time()
        time_for_diffs = {}
        for test_type in self._test_types:
            start_diff_time = time.time()
            new_failures = test_type.compare_output(
                self._port, self._filename, self._options, driver_output,
                expected_driver_output)
            # Don't add any more failures if we already have a crash, so we don't
            # double-report those tests. We do double-report for timeouts since
            # we still want to see the text and image output.
            if not driver_output.crash:
                failures.extend(new_failures)
            time_for_diffs[test_type.__class__.__name__] = (
                time.time() - start_diff_time)

        total_time_for_all_diffs = time.time() - start_diff_time
        return TestResult(self._filename, failures, driver_output.test_time,
                          total_time_for_all_diffs, time_for_diffs)
