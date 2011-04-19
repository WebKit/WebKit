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
from webkitpy.layout_tests.layout_package import test_result_writer
from webkitpy.layout_tests.layout_package.test_results import TestResult


_log = logging.getLogger(__name__)


def run_single_test(port, options, test_input, driver, worker_name):
    runner = SingleTestRunner(options, port, driver, test_input, worker_name)
    return runner.run()


class SingleTestRunner:

    def __init__(self, options, port, driver, test_input, worker_name):
        self._options = options
        self._port = port
        self._driver = driver
        self._filename = test_input.filename
        self._timeout = test_input.timeout
        self._worker_name = worker_name
        self._testname = port.relative_test_filename(test_input.filename)

        self._is_reftest = False
        self._is_mismatch_reftest = False
        self._reference_filename = None

        fs = port._filesystem
        reftest_expected_filename = port.reftest_expected_filename(self._filename)
        if fs.exists(reftest_expected_filename):
            self._is_reftest = True
            self._reference_filename = reftest_expected_filename

        reftest_expected_mismatch_filename = port.reftest_expected_mismatch_filename(self._filename)
        if fs.exists(reftest_expected_mismatch_filename):
            if self._is_reftest:
                _log.error('It is not allowed that one test file has both'
                           ' expected.html file and expected-mismatch.html file'
                           ' at the same time. Please remove either %s or %s.',
                           reftest_expected_filename, reftest_expected_mismatch_filename)
            else:
                self._is_reftest = True
                self._is_mismatch_reftest = True
                self._reference_filename = reftest_expected_mismatch_filename

        if self._is_reftest:
            # Detect and report a test which has a wrong combination of expectation files.
            # For example, if 'foo.html' has two expectation files, 'foo-expected.html' and
            # 'foo-expected.txt', we should warn users. One test file must be used exclusively
            # in either layout tests or reftests, but not in both.
            for suffix in ('.txt', '.checksum', '.png', '.wav'):
                expected_filename = self._port.expected_filename(self._filename, suffix)
                if fs.exists(expected_filename):
                    _log.error('The reftest (%s) can not have an expectation file (%s).'
                               ' Please remove that file.', self._testname, expected_filename)

    def _expected_driver_output(self):
        return base.DriverOutput(self._port.expected_text(self._filename),
                                 self._port.expected_image(self._filename),
                                 self._port.expected_checksum(self._filename),
                                 self._port.expected_audio(self._filename))

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
            if self._is_reftest:
                # Returns a dummy TestResult. We don't have to rebase for reftests.
                return TestResult(self._filename)
            else:
                return self._run_rebaseline()
        if self._is_reftest:
            return self._run_reftest()
        return self._run_compare_test()

    def _run_compare_test(self):
        driver_output = self._driver.run_test(self._driver_input())
        expected_driver_output = self._expected_driver_output()
        test_result = self._compare_output(driver_output, expected_driver_output)
        test_result_writer.write_test_result(self._port, self._filename,
                                             driver_output, expected_driver_output, test_result.failures)
        return test_result

    def _run_rebaseline(self):
        driver_output = self._driver.run_test(self._driver_input())
        failures = self._handle_error(driver_output)
        test_result_writer.write_test_result(self._port, self._filename,
                                             driver_output, None, failures)
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
        if driver_output.audio:
            self._save_baseline_data(driver_output.audio, '.wav',
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
        assert data is not None
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

    def _handle_error(self, driver_output, reference_filename=None):
        """Returns test failures if some unusual errors happen in driver's run.

        Args:
          driver_output: The output from the driver.
          reference_filename: The full path to the reference file which produced the driver_output.
              This arg is optional and should be used only in reftests until we have a better way to know
              which html file is used for producing the driver_output.
        """
        failures = []
        fs = self._port._filesystem
        if driver_output.timeout:
            failures.append(test_failures.FailureTimeout(bool(reference_filename)))

        if reference_filename:
            testname = self._port.relative_test_filename(reference_filename)
        else:
            testname = self._testname

        if driver_output.crash:
            failures.append(test_failures.FailureCrash(bool(reference_filename)))
            _log.debug("%s Stacktrace for %s:\n%s" % (self._worker_name, testname,
                                                      driver_output.error))
        elif driver_output.error:
            _log.debug("%s %s output stderr lines:\n%s" % (self._worker_name, testname,
                                                           driver_output.error))
        return failures

    def _compare_output(self, driver_output, expected_driver_output):
        failures = []
        failures.extend(self._handle_error(driver_output))

        if driver_output.crash:
            # Don't continue any more if we already have a crash.
            # In case of timeouts, we continue since we still want to see the text and image output.
            return TestResult(self._filename, failures, driver_output.test_time)

        failures.extend(self._compare_text(driver_output.text, expected_driver_output.text))
        failures.extend(self._compare_audio(driver_output.audio, expected_driver_output.audio))
        if self._options.pixel_tests:
            failures.extend(self._compare_image(driver_output, expected_driver_output))
        return TestResult(self._filename, failures, driver_output.test_time)

    def _compare_text(self, actual_text, expected_text):
        failures = []
        if (expected_text and actual_text and
            # Assuming expected_text is already normalized.
            self._port.compare_text(self._get_normalized_output_text(actual_text), expected_text)):
            failures.append(test_failures.FailureTextMismatch())
        elif actual_text and not expected_text:
            failures.append(test_failures.FailureMissingResult())
        return failures

    def _compare_audio(self, actual_audio, expected_audio):
        failures = []
        if (expected_audio and actual_audio and
            self._port.compare_audio(actual_audio, expected_audio)):
            failures.append(test_failures.FailureAudioMismatch())
        elif actual_audio and not expected_audio:
            failures.append(test_failures.FailureMissingAudio())
        return failures

    def _get_normalized_output_text(self, output):
        """Returns the normalized text output, i.e. the output in which
        the end-of-line characters are normalized to "\n"."""
        # Running tests on Windows produces "\r\n".  The "\n" part is helpfully
        # changed to "\r\n" by our system (Python/Cygwin), resulting in
        # "\r\r\n", when, in fact, we wanted to compare the text output with
        # the normalized text expectation files.
        return output.replace("\r\r\n", "\r\n").replace("\r\n", "\n")

    def _compare_image(self, driver_output, expected_driver_outputs):
        failures = []
        # If we didn't produce a hash file, this test must be text-only.
        if driver_output.image_hash is None:
            return failures
        if not expected_driver_outputs.image:
            failures.append(test_failures.FailureMissingImage())
        elif not expected_driver_outputs.image_hash:
            failures.append(test_failures.FailureMissingImageHash())
        elif driver_output.image_hash != expected_driver_outputs.image_hash:
            failures.append(test_failures.FailureImageHashMismatch())
        return failures

    def _run_reftest(self):
        driver_output1 = self._driver.run_test(self._driver_input())
        driver_output2 = self._driver.run_test(
            base.DriverInput(self._reference_filename, self._timeout, driver_output1.image_hash))
        test_result = self._compare_output_with_reference(driver_output1, driver_output2)

        test_result_writer.write_test_result(self._port, self._filename,
                                             driver_output1, driver_output2, test_result.failures)
        return test_result

    def _compare_output_with_reference(self, driver_output1, driver_output2):
        total_test_time = driver_output1.test_time + driver_output2.test_time
        failures = []
        failures.extend(self._handle_error(driver_output1))
        if failures:
            # Don't continue any more if we already have crash or timeout.
            return TestResult(self._filename, failures, total_test_time)
        failures.extend(self._handle_error(driver_output2, reference_filename=self._reference_filename))
        if failures:
            return TestResult(self._filename, failures, total_test_time)

        if self._is_mismatch_reftest:
            if driver_output1.image_hash == driver_output2.image_hash:
                failures.append(test_failures.FailureReftestMismatchDidNotOccur())
        elif driver_output1.image_hash != driver_output2.image_hash:
            failures.append(test_failures.FailureReftestMismatch())
        return TestResult(self._filename, failures, total_test_time)
