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
import os
import re
from collections import deque

from webkitcorepy import string_utils

from webkitpy.common import read_checksum_from_png
from webkitpy.layout_tests.controllers import test_result_writer
from webkitpy.layout_tests.models import test_expectations, test_failures
from webkitpy.layout_tests.models.test_results import TestResult
from webkitpy.port.driver import DriverInput, DriverOutput
from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup as Parser

_log = logging.getLogger(__name__)

_render_tree_dump_pattern = re.compile(br"^layer at \(\d+,\d+\) size \d+x\d+\n")


def run_single_test(port, options, results_directory, worker_name, driver, test_input, stop_when_done):
    runner = SingleTestRunner(port, options, results_directory, worker_name, driver, test_input, stop_when_done)
    return runner.run()


class SingleTestRunner(object):
    def __init__(self, port, options, results_directory, worker_name, driver, test_input, stop_when_done):
        self._port = port
        self._filesystem = port.host.filesystem
        self._options = options
        self._results_directory = results_directory
        self._driver = driver
        self._worker_name = worker_name
        self._test_input = test_input
        self._stop_when_done = stop_when_done

        if self._reference_files:
            # Detect and report a test which has a wrong combination of expectation files.
            # For example, if 'foo.html' has two expectation files, 'foo-expected.html' and
            # 'foo-expected.txt', we should warn users. One test file must be used exclusively
            # in either layout tests or reftests, but not in both.
            test = test_input.test
            for expected_filename in (
                test.expected_text_path,
                test.expected_image_path,
                test.expected_audio_path,
            ):
                if expected_filename is None:
                    continue

                _log.error('%s is a reftest, but has an unused expectation file. Please remove %s.', self._test_name, expected_filename)

    @property
    def _test_name(self):
        return self._test_input.test_name

    @property
    def _should_run_pixel_test(self):
        return self._test_input.should_run_pixel_test

    @property
    def _should_dump_jsconsolelog_in_stderr(self):
        return self._test_input.should_dump_jsconsolelog_in_stderr

    @property
    def _reference_files(self):
        files = self._test_input.test.reference_files
        if files is not None:
            return list(files)
        else:
            return []

    @property
    def _timeout(self):
        return self._test_input.timeout

    def _expected_driver_output(self):
        test = self._test_input.test
        fs = self._filesystem

        text = None
        image = None
        checksum = None
        audio = None

        if test.expected_text_path is not None:
            # FIXME: DRT output is actually utf-8, but since we don't decode the
            # output from DRT (instead treating it as a binary string), we read the
            # baselines as a binary string, too.
            text = string_utils.decode(
                fs.read_binary_file(test.expected_text_path),
                target_type=str,
            )

            # End-of-line characters are normalized to '\n'.
            text = text.replace("\r\n", "\n")

        if test.expected_image_path is not None:
            with fs.open_binary_file_for_reading(
                test.expected_image_path
            ) as filehandle:
                checksum = read_checksum_from_png.read_checksum(filehandle)
                filehandle.seek(0)
                image = filehandle.read()

        if test.expected_audio_path is not None:
            audio = fs.read_binary_file(test.expected_audio_path)

        return DriverOutput(text, image, checksum, audio)

    def _should_fetch_expected_checksum(self):
        return self._should_run_pixel_test and not (self._options.new_baseline or self._options.reset_results)

    def _driver_input(self):
        # The image hash is used to avoid doing an image dump if the
        # checksums match, so it should be set to a blank value if we
        # are generating a new baseline.  (Otherwise, an image from a
        # previous run will be copied into the baseline."""
        image_hash = None
        if self._should_fetch_expected_checksum():
            expected_image_path = self._test_input.test.expected_image_path
            if expected_image_path is not None:
                with self._filesystem.open_binary_file_for_reading(
                    expected_image_path
                ) as filehandle:
                    image_hash = read_checksum_from_png.read_checksum(filehandle)

        return DriverInput(self._test_name, self._timeout, image_hash, self._should_run_pixel_test, self._should_dump_jsconsolelog_in_stderr, self._options.additional_header)

    def run(self):
        self_comparison_header = self._port.get_option('self_compare_with_header')
        if self_comparison_header:
            return self._run_self_comparison_test(self_comparison_header)
        if self._options.site_isolation:
            comparison_header = 'SiteIsolationEnabled=true runInCrossOriginFrame=true'
            if self._reference_files:
                return self._run_self_comparison_test(comparison_header)
            return self._run_self_comparison_without_reference_test(comparison_header)
        if self._reference_files:
            if self._port.get_option('no_ref_tests') or self._options.reset_results:
                reftest_type = set([reference_file[0] for reference_file in self._reference_files])
                result = TestResult(self._test_input, reftest_type=reftest_type)
                result.type = test_expectations.SKIP
                return result
            return self._run_reftest()
        if self._test_input.test.is_wpt_crash_test:
            return self._run_wpt_crash_test()
        if self._options.reset_results:
            return self._run_rebaseline()
        return self._run_compare_test()

    def _run_compare_test(self):
        driver_output = self._driver.run_test(self._driver_input(), self._stop_when_done)
        expected_driver_output = self._expected_driver_output()

        if self._options.ignore_metrics:
            expected_driver_output.strip_metrics()
            driver_output.strip_metrics()

        patterns = self._port.logging_patterns_to_strip()
        expected_driver_output.strip_patterns(patterns)
        driver_output.strip_patterns(patterns)

        driver_output.strip_text_start_if_needed(self._port.logging_detectors_to_strip_text_start(self._driver_input().test_name))

        driver_output.strip_stderror_patterns(self._port.stderr_patterns_to_strip())

        test_result = self._compare_output(expected_driver_output, driver_output)
        if self._options.new_test_results:
            self._add_missing_baselines(test_result, driver_output)
        test_result_writer.write_test_result(self._filesystem, self._port, self._results_directory, self._test_name, driver_output, expected_driver_output, test_result.failures)
        return test_result

    def _run_wpt_crash_test(self):
        driver_output = self._driver.run_test(self._driver_input(), self._stop_when_done)
        if self._options.ignore_metrics:
            driver_output.strip_metrics()

        patterns = self._port.logging_patterns_to_strip()
        driver_output.strip_patterns(patterns)
        driver_output.strip_text_start_if_needed(self._port.logging_detectors_to_strip_text_start(self._driver_input().test_name))
        driver_output.strip_stderror_patterns(self._port.stderr_patterns_to_strip())

        failures = []
        failures.extend(self._handle_error(driver_output))
        assert(isinstance(failure, test_failures.FailureCrash) or isinstance(failure, test_failures.FailureTimeout) for failure in failures)

        test_result_writer.write_test_result(self._filesystem, self._port, self._results_directory, self._test_name, driver_output, None, failures)
        return TestResult(self._test_input, failures, driver_output.test_time, driver_output.has_stderr(), pid=driver_output.pid)

    def _run_rebaseline(self):
        driver_output = self._driver.run_test(self._driver_input(), self._stop_when_done)
        failures = self._handle_error(driver_output)
        test_result_writer.write_test_result(self._filesystem, self._port, self._results_directory, self._test_name, driver_output, None, failures)
        # FIXME: It the test crashed or timed out, it might be better to avoid
        # to write new baselines.
        self._overwrite_baselines(driver_output)
        return TestResult(self._test_input, failures, driver_output.test_time, driver_output.has_stderr(), pid=driver_output.pid)

    def _add_missing_baselines(self, test_result, driver_output):
        missingImage = test_result.has_failure_matching_types(test_failures.FailureMissingImage, test_failures.FailureMissingImageHash)
        if test_result.has_failure_matching_types(test_failures.FailureMissingResult):
            self._save_baseline_data(driver_output.text, '.txt')
        if test_result.has_failure_matching_types(test_failures.FailureMissingAudio):
            self._save_baseline_data(driver_output.audio, '.wav')
        if missingImage:
            self._save_baseline_data(driver_output.image, '.png')

    def _overwrite_baselines(self, driver_output):
        self._save_baseline_data(driver_output.text, '.txt', rebaselining=True)
        self._save_baseline_data(driver_output.audio, '.wav', rebaselining=True)
        if self._should_run_pixel_test:
            self._save_baseline_data(driver_output.image, '.png', rebaselining=True)

    def _save_baseline_data(self, data, extension, rebaselining=False):
        if data is None:
            return

        port = self._port
        fs = self._filesystem
        device_type = self._driver.host.device_type

        data = string_utils.encode(data, target_type=bytes)

        if self._options.add_platform_exceptions:
            # The most specific baseline path.
            output_dir = fs.join(
                port.baseline_version_dir(device_type=device_type),
                fs.dirname(self._test_name),
            )
        elif self._options.add_baselines_to_platform:
            # Whatever was passed.
            output_dir = fs.join(
                self._options.add_baselines_to_platform,
                fs.dirname(self._test_name),
            )
        elif rebaselining:
            # The directory containing the existing baseline or the generic path.
            output_dir = fs.dirname(
                port.expected_filename(
                    self._test_name,
                    extension,
                    device_type=device_type,
                )
            )
        elif extension == ".png" or (
            extension == ".txt" and _render_tree_dump_pattern.match(data)
        ):
            # The baseline path applying to the Port.
            output_dir = fs.join(
                port.baseline_platform_dir(), fs.dirname(self._test_name)
            )
        else:
            # The directory containing the test.
            output_dir = fs.dirname(port.abspath_for_test(self._test_name))

        assert extension[0] == "."
        output_basename = test_result_writer.TestResultWriter.expected_filename(
            fs.basename(self._test_name), fs, suffix=extension[1:]
        )

        output_path = fs.join(output_dir, output_basename)

        if not self._options.add_redundant_platform_results:
            baseline_search_path = [
                fs.join(p, fs.dirname(self._test_name))
                for p in (
                    port.baseline_search_path(device_type=device_type)
                    + [port.layout_tests_dir()]
                )
            ]
            seen_output_dir = False
            abs_output_dir = fs.abspath(output_dir)
            for path in baseline_search_path:
                if fs.abspath(path) == abs_output_dir:
                    seen_output_dir = True
                    continue

                if not seen_output_dir:
                    continue

                candidate = fs.join(path, output_basename)
                if fs.isfile(candidate):
                    if (
                        fs.getsize(candidate) == len(data)
                        and fs.read_binary_file(candidate) == data
                    ):
                        _log.info(
                            'Skipping writing redundant expected result "{}" due to existing "{}"'.format(
                                port.relative_test_filename(output_path),
                                port.relative_test_filename(candidate),
                            )
                        )

                        if fs.exists(output_path):
                            _log.info(
                                'Removing existing expected result "{}"'.format(
                                    port.relative_test_filename(output_path),
                                )
                            )
                            fs.remove(output_path)
                        return
                    break

        _log.info('Writing new expected result "%s"' % port.relative_test_filename(output_path))
        fs.maybe_make_directory(output_dir)
        fs.write_binary_file(output_path, data)

    def _handle_error(self, driver_output, reference_filename=None):
        """Returns test failures if some unusual errors happen in driver's run.

        Args:
          driver_output: The output from the driver.
          reference_filename: The full path to the reference file which produced the driver_output.
              This arg is optional and should be used only in reftests until we have a better way to know
              which html file is used for producing the driver_output.
        """
        failures = []
        if driver_output.timeout:
            failures.append(test_failures.FailureTimeout(bool(reference_filename)))

        if reference_filename:
            testname = self._port.relative_test_filename(reference_filename)
        else:
            testname = self._test_name

        if driver_output.crash:
            failures.append(test_failures.FailureCrash(bool(reference_filename),
                                                       driver_output.crashed_process_name,
                                                       driver_output.crashed_pid))
            if driver_output.error:
                _log.debug("%s %s crashed, (stderr lines):" % (self._worker_name, testname))
            else:
                _log.debug("%s %s crashed, (no stderr)" % (self._worker_name, testname))
        elif driver_output.error:
            _log.debug("%s %s output stderr lines:" % (self._worker_name, testname))
        for line in driver_output.error.splitlines():
            _log.debug("  {}".format(string_utils.decode(line, target_type=str)))
        return failures

    def _compare_output(self, expected_driver_output, driver_output):
        failures = []
        failures.extend(self._handle_error(driver_output))

        if driver_output.crash:
            # Don't continue any more if we already have a crash.
            # In case of timeouts, we continue since we still want to see the text and image output.
            return TestResult(self._test_input, failures, driver_output.test_time, driver_output.has_stderr(), pid=driver_output.pid)

        failures.extend(self._compare_text(expected_driver_output.text, driver_output.text))
        failures.extend(self._compare_audio(expected_driver_output.audio, driver_output.audio))
        if self._should_run_pixel_test:
            failures.extend(self._compare_image(expected_driver_output, driver_output))
        return TestResult(self._test_input, failures, driver_output.test_time, driver_output.has_stderr(), pid=driver_output.pid)

    def _compare_text(self, expected_text, actual_text):
        failures = []
        if self._options.ignore_render_tree_dump_results and actual_text and _render_tree_dump_pattern.match(actual_text):
            return failures
        if (expected_text and actual_text and
            # Assuming expected_text is already normalized.
            self._port.do_text_results_differ(expected_text, self._get_normalized_output_text(actual_text))):
            failures.append(test_failures.FailureTextMismatch())
        elif actual_text and not expected_text:
            failures.append(test_failures.FailureMissingResult())
        elif not actual_text and expected_text:
            failures.append(test_failures.FailureNoOutput())
        return failures

    def _compare_audio(self, expected_audio, actual_audio):
        failures = []
        if (expected_audio and actual_audio and
            self._port.do_audio_results_differ(expected_audio, actual_audio)):
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

    # FIXME: This function also creates the image diff. Maybe that work should
    # be handled elsewhere?
    def _compare_image(self, expected_driver_output, driver_output):
        failures = []
        # If we didn't produce a hash file, this test must be text-only.
        if driver_output.image_hash is None:
            return failures
        if not expected_driver_output.image:
            failures.append(test_failures.FailureMissingImage())
        elif not expected_driver_output.image_hash:
            failures.append(test_failures.FailureMissingImageHash())
        elif driver_output.image_hash != expected_driver_output.image_hash:
            diff_result = self._port.diff_image(expected_driver_output.image, driver_output.image)
            if not diff_result.passed:
                failures.append(test_failures.FailureImageHashMismatch(diff_result))
                if diff_result.error_string:
                    _log.warning('  %s : %s' % (self._test_name, diff_result.error_string))
                    driver_output.error = (driver_output.error or '') + diff_result.error_string
                driver_output.image_diff = diff_result.diff_image
            else:
                # See https://bugs.webkit.org/show_bug.cgi?id=69444 for why this isn't a full failure.
                _log.warning('  %s -> pixel hash failed (but diff passed)' % self._test_name)
        return failures

    def _run_reftest(self):
        test_output = self._driver.run_test(self._driver_input(), self._stop_when_done)
        test_output.strip_patterns(self._port.logging_patterns_to_strip())
        test_output.strip_text_start_if_needed(self._port.logging_detectors_to_strip_text_start(self._driver_input().test_name))
        test_output.strip_stderror_patterns(self._port.stderr_patterns_to_strip())

        if test_output.image is None:
            # The driver is misbehaving, kill it so the error doesn't propagate to subsequent tests
            self._driver.stop()
            test_result = TestResult(
                self._test_input,
                self._handle_error(test_output) + [test_failures.FailureReftestNoImagesGenerated(self._test_name)],
                test_output.test_time,
                test_output.has_stderr(),
                pid=test_output.pid,
            )
            test_result_writer.write_test_result(self._filesystem, self._port, self._results_directory, self._test_name, test_output, None, test_result.failures)
            return test_result

        total_test_time = 0
        reference_output = None
        test_result = None

        # A reftest can have multiple match references and multiple mismatch references;
        # the test fails if any mismatch matches and all of the matches don't match.
        # To minimize the number of references we have to check, we run all of the mismatches first,
        # then the matches, and short-circuit out as soon as we can.
        # Note that sorting by the expectation sorts "!=" before "==" so this is easy to do.

        putAllMismatchBeforeMatch = sorted
        reference_test_names = []
        for expectation, reference_filename in putAllMismatchBeforeMatch(self._reference_files):
            reference_test_name = self._port.relative_test_filename(reference_filename)
            reference_test_names.append(reference_test_name)
            reference_output = self._driver.run_test(DriverInput(reference_test_name, self._timeout, None, should_run_pixel_test=True), self._stop_when_done)
            reference_output.strip_patterns(self._port.logging_patterns_to_strip())
            reference_output.strip_text_start_if_needed(self._port.logging_detectors_to_strip_text_start(self._driver_input().test_name))
            reference_output.strip_stderror_patterns(self._port.stderr_patterns_to_strip())

            if reference_output.image is None:
                # The driver is misbehaving, kill it so the error doesn't propagate to subsequent tests
                self._driver.stop()

            test_result = self._compare_output_with_reference(reference_output, test_output, reference_filename, expectation == '!=')

            if (expectation == '!=' and test_result.failures) or (expectation == '==' and not test_result.failures):
                break
            total_test_time += test_result.test_run_time

        assert(reference_output)
        test_result_writer.write_test_result(self._filesystem, self._port, self._results_directory, self._test_name, test_output, reference_output, test_result.failures)
        reftest_type = set([reference_file[0] for reference_file in self._reference_files])
        return TestResult(self._test_input, test_result.failures, total_test_time + test_result.test_run_time, test_result.has_stderr, reftest_type=reftest_type, pid=test_result.pid, references=reference_test_names)

    def _run_self_comparison_test(self, header):
        driver_input = self._driver_input()
        driver_input.should_run_pixel_test = True
        driver_input.force_dump_pixels = True

        reference_output = self._driver.run_test(driver_input, self._stop_when_done)
        driver_input.self_comparison_header = header
        test_output = self._driver.run_test(driver_input, self._stop_when_done)

        test_full_path = self._port.abspath_for_test(self._test_name)
        test_result = self._compare_output_with_reference(reference_output, test_output, test_full_path, False, allow_fuzzy_tolerance=False)

        assert(reference_output)
        test_result_writer.write_test_result(self._filesystem, self._port, self._results_directory, self._test_name, test_output, reference_output, test_result.failures)
        return TestResult(self._test_input, test_result.failures, test_result.test_run_time, test_result.has_stderr, pid=test_result.pid)

    def _run_self_comparison_without_reference_test(self, header):
        driver_input = self._driver_input()
        expected_driver_output = self._driver.run_test(driver_input, self._stop_when_done)
        driver_input.self_comparison_header = header
        driver_output = self._driver.run_test(driver_input, self._stop_when_done)

        for output in (expected_driver_output, driver_output):
            if self._options.ignore_metrics:
                output.strip_metrics()
            output.strip_patterns(self._port.logging_patterns_to_strip())
            output.strip_text_start_if_needed(self._port.logging_detectors_to_strip_text_start(driver_input.test_name))
            output.strip_stderror_patterns(self._port.stderr_patterns_to_strip())

        test_result = self._compare_output(expected_driver_output, driver_output)
        test_result_writer.write_test_result(self._filesystem, self._port, self._results_directory, self._test_name, driver_output, expected_driver_output, test_result.failures)
        return test_result

    def _fuzzy_metadata_for_file(self, filename):
        test_doc = Parser(self._filesystem.read_binary_file(filename))
        fuzzy_nodes = test_doc.findAll('meta', attrs={"name": "fuzzy"})
        if not fuzzy_nodes:
            return None

        args = [u"maxDifference", u"totalPixels"]
        result = {}

        # Taken from wpt/tools/manifest/sourcefile.py, and copied to avoid having webkitpy depend on wpt.
        for node in fuzzy_nodes:
            content = node['content']
            key = None
            # from parse_ref_keyed_meta; splits out the optional reference prefix.
            parts = content.rsplit(u":", 1)
            if len(parts) == 1:
                fuzzy_data = parts[0]
            else:
                ref_file = parts[0]
                key = ref_file
                fuzzy_data = parts[1]

            ranges = fuzzy_data.split(u";")
            if len(ranges) != 2:
                raise ValueError("Malformed fuzzy value \"%s\"" % fuzzy_data)

            arg_values = {}  # type: Dict[Text, List[int]]
            positional_args = deque()  # type: Deque[List[int]]

            for range_str_value in ranges:  # type: Text
                name = None  # type: Optional[Text]
                if u"=" in range_str_value:
                    name, range_str_value = [part.strip() for part in range_str_value.split(u"=", 1)]
                    if name not in args:
                        raise ValueError("%s is not a valid fuzzy property" % name)
                    if arg_values.get(name):
                        raise ValueError("Got multiple values for argument %s" % name)

                if u"-" in range_str_value:
                    range_min, range_max = range_str_value.split(u"-")
                else:
                    range_min = range_str_value
                    range_max = range_str_value
                try:
                    range_value = [int(x.strip()) for x in (range_min, range_max)]
                except ValueError:
                    raise ValueError("Fuzzy value %s must be a range of integers" % range_str_value)

                if name is None:
                    positional_args.append(range_value)
                else:
                    arg_values[name] = range_value

            result[key] = []
            for arg_name in args:
                if arg_values.get(arg_name):
                    arg_value = arg_values.pop(arg_name)
                else:
                    arg_value = positional_args.popleft()
                result[key].append(arg_value)
            assert len(arg_values) == 0 and len(positional_args) == 0

        return result

    @staticmethod
    def _relative_reference_path(test_full_path, reference_full_path):
        test_dir = os.path.split(test_full_path)[0]
        reference_dir, reference_file_name = os.path.split(reference_full_path)
        if test_dir == reference_dir:
            return reference_file_name

        relative_path = os.path.relpath(reference_dir, test_dir)
        return os.path.join(relative_path, reference_file_name)

    def _fuzzy_tolerance_for_reference(self, reference_full_path):
        test_full_path = self._port.abspath_for_test(self._test_name)
        fuzzy = self._fuzzy_metadata_for_file(test_full_path)
        if not fuzzy:
            return None

        reference_relative_path = self._relative_reference_path(test_full_path, reference_full_path)
        tolerance = [[0, 0], [0, 0]]
        if reference_relative_path in fuzzy:
            tolerance = fuzzy[reference_relative_path]
        elif None in fuzzy:
            tolerance = fuzzy[None]

        return {'max_difference': tolerance[0], 'total_pixels': tolerance[1]}

    @staticmethod
    def _test_passes_fuzzy_matching(allowed_fuzzy_values, fuzzy_result):
        if not fuzzy_result:
            _log.error('fuzzy_result is {}'.format(fuzzy_result))
            return False
        maxDifferenceMin = allowed_fuzzy_values['max_difference'][0]
        maxDifferenceMax = allowed_fuzzy_values['max_difference'][1]

        totalPixelsMin = allowed_fuzzy_values['total_pixels'][0]
        totalPixelsMax = allowed_fuzzy_values['total_pixels'][1]

        actualMaxDifference = fuzzy_result['max_difference']
        actualTotalPixels = fuzzy_result['total_pixels']

        # https://web-platform-tests.org/writing-tests/reftests.html says "in the range" but isn't precise about whether the upper bound is included.
        return actualMaxDifference >= maxDifferenceMin and actualMaxDifference <= maxDifferenceMax and actualTotalPixels >= totalPixelsMin and actualTotalPixels <= totalPixelsMax

    def _compare_output_with_reference(self, reference_driver_output, actual_driver_output, reference_filename, mismatch, allow_fuzzy_tolerance=True):
        total_test_time = reference_driver_output.test_time + actual_driver_output.test_time
        has_stderr = reference_driver_output.has_stderr() or actual_driver_output.has_stderr()
        failures = []
        failures.extend(self._handle_error(actual_driver_output))
        if failures:
            # Don't continue any more if we already have crash or timeout.
            return TestResult(self._test_input, failures, total_test_time, has_stderr)

        failures.extend(self._handle_error(reference_driver_output, reference_filename=reference_filename))
        if failures:
            return TestResult(self._test_input, failures, total_test_time, has_stderr, pid=actual_driver_output.pid)

        if not reference_driver_output.image_hash and not actual_driver_output.image_hash:
            failures.append(test_failures.FailureReftestNoImagesGenerated(reference_filename))
        elif mismatch:
            fuzzy_tolerance = None
            if allow_fuzzy_tolerance:
                fuzzy_tolerance = self._fuzzy_tolerance_for_reference(reference_filename)
            if fuzzy_tolerance:
                diff_result = self._port.diff_image(reference_driver_output.image, actual_driver_output.image, tolerance=0)
                match_within_tolerance = self._test_passes_fuzzy_matching(fuzzy_tolerance, diff_result.fuzzy_data)
                _log.debug('{} expected mismatch: allowed fuzziness {}, actual difference {}: matched {}'.format(self._test_name, fuzzy_tolerance, diff_result.fuzzy_data, match_within_tolerance))
                if match_within_tolerance:
                    # FIXME: we could store and show the diff image for mismatch failures.
                    failures.append(test_failures.FailureReftestMismatchDidNotOccur(reference_filename))
            else:
                # Calling image_hash is considered unnecessary for expected mismatch ref tests.
                if reference_driver_output.image_hash == actual_driver_output.image_hash:
                    failures.append(test_failures.FailureReftestMismatchDidNotOccur(reference_filename))

        elif reference_driver_output.image_hash != actual_driver_output.image_hash:
            # ImageDiff has a hard coded color distance threshold even though tolerance=0 is specified.
            diff_result = self._port.diff_image(reference_driver_output.image, actual_driver_output.image, tolerance=0)

            fuzzy_tolerance = None
            if allow_fuzzy_tolerance:
                fuzzy_tolerance = self._fuzzy_tolerance_for_reference(reference_filename)
            if fuzzy_tolerance:
                diff_result.passed = self._test_passes_fuzzy_matching(fuzzy_tolerance, diff_result.fuzzy_data)
                _log.debug('{} allowed fuzziness {}, actual difference {}: pass {}'.format(self._test_name, fuzzy_tolerance, diff_result.fuzzy_data, diff_result.passed))

            if not diff_result.passed:
                failures.append(test_failures.FailureReftestMismatch(reference_filename, diff_result))
                if diff_result.error_string:
                    _log.warning('  %s : %s' % (self._test_name, diff_result.error_string))
                    actual_driver_output.error = (actual_driver_output.error or '') + diff_result.error_string

        return TestResult(self._test_input, failures, total_test_time, has_stderr, pid=actual_driver_output.pid)
