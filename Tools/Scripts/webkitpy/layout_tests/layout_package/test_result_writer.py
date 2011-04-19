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

from webkitpy.layout_tests.layout_package import test_failures


_log = logging.getLogger(__name__)


def write_test_result(port, filename, driver_output,
                      expected_driver_output, failures):
    """Write the test result to the result output directory."""
    root_output_dir = port.results_directory()
    checksums_mismatch_but_images_are_same = False
    imagehash_mismatch_failure = None
    writer = TestResultWriter(port, root_output_dir, filename)
    if driver_output.error:
        writer.write_stderr(driver_output.error)

    for failure in failures:
        # FIXME: Instead of this long 'if' block, each failure class might
        # have a responsibility for writing a test result.
        if isinstance(failure, (test_failures.FailureMissingResult,
                                test_failures.FailureTextMismatch)):
            writer.write_text_files(driver_output.text, expected_driver_output.text)
            writer.create_text_diff_and_write_result(driver_output.text, expected_driver_output.text)
        elif isinstance(failure, test_failures.FailureMissingImage):
            writer.write_image_files(driver_output.image, expected_image=None)
            writer.write_image_hashes(driver_output.image_hash, expected_driver_output.image_hash)
        elif isinstance(failure, test_failures.FailureMissingImageHash):
            writer.write_image_files(driver_output.image, expected_driver_output.image)
            writer.write_image_hashes(driver_output.image_hash, expected_image_hash=None)
        elif isinstance(failure, test_failures.FailureImageHashMismatch):
            writer.write_image_files(driver_output.image, expected_driver_output.image)
            writer.write_image_hashes(driver_output.image_hash, expected_driver_output.image_hash)
            images_are_different = writer.create_image_diff_and_write_result(
                driver_output.image, expected_driver_output.image)
            if not images_are_different:
                checksums_mismatch_but_images_are_same = True
                imagehash_mismatch_failure = failure
        elif isinstance(failure, (test_failures.FailureAudioMismatch,
                                  test_failures.FailureMissingAudio)):
            writer.write_audio_files(driver_output.audio, expected_driver_output.audio)
        elif isinstance(failure, test_failures.FailureCrash):
            if failure.is_reftest:
                writer.write_crash_report(expected_driver_output.error)
            else:
                writer.write_crash_report(driver_output.error)
        elif isinstance(failure, test_failures.FailureReftestMismatch):
            writer.write_image_files(driver_output.image, expected_driver_output.image)
            writer.create_image_diff_and_write_result(driver_output.image, expected_driver_output.image)
            writer.copy_file(port.reftest_expected_filename(filename), '-expected.html')
        elif isinstance(failure, test_failures.FailureReftestMismatchDidNotOccur):
            writer.write_image_files(driver_output.image, expected_image=None)
            writer.copy_file(port.reftest_expected_mismatch_filename(filename), '-expected-mismatch.html')
        else:
            assert isinstance(failure, (test_failures.FailureTimeout,))

    # FIXME: This is an ugly hack to handle FailureImageHashIncorrect case.
    # Ideally, FailureImageHashIncorrect case should be detected before this
    # function is called. But it requires calling create_diff_image() to detect
    # whether two images are same or not. So we need this hack until we have a better approach.
    if checksums_mismatch_but_images_are_same:
        # Replace FailureImageHashMismatch with FailureImageHashIncorrect.
        failures.remove(imagehash_mismatch_failure)
        failures.append(test_failures.FailureImageHashIncorrect())


class TestResultWriter(object):
    """A class which handles all writing operations to the result directory."""

    # Filename pieces when writing failures to the test results directory.
    FILENAME_SUFFIX_ACTUAL = "-actual"
    FILENAME_SUFFIX_EXPECTED = "-expected"
    FILENAME_SUFFIX_DIFF = "-diff"
    FILENAME_SUFFIX_WDIFF = "-wdiff.html"
    FILENAME_SUFFIX_PRETTY_PATCH = "-pretty-diff.html"
    FILENAME_SUFFIX_IMAGE_DIFF = "-diff.png"

    def __init__(self, port, root_output_dir, filename):
        self._port = port
        self._root_output_dir = root_output_dir
        self._filename = filename
        self._testname = port.relative_test_filename(filename)

    def _make_output_directory(self):
        """Creates the output directory (if needed) for a given test filename."""
        fs = self._port._filesystem
        output_filename = fs.join(self._root_output_dir, self._testname)
        self._port.maybe_make_directory(fs.dirname(output_filename))

    def output_filename(self, modifier):
        """Returns a filename inside the output dir that contains modifier.

        For example, if test name is "fast/dom/foo.html" and modifier is "-expected.txt",
        the return value is "/<path-to-root-output-dir>/fast/dom/foo-expected.txt".

        Args:
          modifier: a string to replace the extension of filename with

        Return:
          The absolute path to the output filename
        """
        fs = self._port._filesystem
        output_filename = fs.join(self._root_output_dir, self._testname)
        return fs.splitext(output_filename)[0] + modifier

    def write_output_files(self, file_type, output, expected):
        """Writes the test output, the expected output in the results directory.

        The full output filename of the actual, for example, will be
          <filename>-actual<file_type>
        For instance,
          my_test-actual.txt

        Args:
          file_type: A string describing the test output file type, e.g. ".txt"
          output: A string containing the test output
          expected: A string containing the expected test output
        """
        self._make_output_directory()
        actual_filename = self.output_filename(self.FILENAME_SUFFIX_ACTUAL + file_type)
        expected_filename = self.output_filename(self.FILENAME_SUFFIX_EXPECTED + file_type)

        fs = self._port._filesystem
        if output is not None:
            fs.write_binary_file(actual_filename, output)
        if expected is not None:
            fs.write_binary_file(expected_filename, expected)

    def write_stderr(self, error):
        fs = self._port._filesystem
        filename = self.output_filename("-stderr.txt")
        fs.maybe_make_directory(fs.dirname(filename))
        fs.write_text_file(filename, error)

    def write_crash_report(self, error):
        """Write crash information."""
        fs = self._port._filesystem
        filename = self.output_filename("-stack.txt")
        fs.maybe_make_directory(fs.dirname(filename))
        fs.write_text_file(filename, error)

    def write_text_files(self, actual_text, expected_text):
        self.write_output_files(".txt", actual_text, expected_text)

    def create_text_diff_and_write_result(self, actual_text, expected_text):
        # FIXME: This function is actually doing the diffs as well as writing results.
        # It might be better to extract code which does 'diff' and make it a separate function.
        if not actual_text or not expected_text:
            return

        self._make_output_directory()
        file_type = '.txt'
        actual_filename = self.output_filename(self.FILENAME_SUFFIX_ACTUAL + file_type)
        expected_filename = self.output_filename(self.FILENAME_SUFFIX_EXPECTED + file_type)
        fs = self._port._filesystem
        # We treat diff output as binary. Diff output may contain multiple files
        # in conflicting encodings.
        diff = self._port.diff_text(expected_text, actual_text, expected_filename, actual_filename)
        diff_filename = self.output_filename(self.FILENAME_SUFFIX_DIFF + file_type)
        fs.write_binary_file(diff_filename, diff)

        # Shell out to wdiff to get colored inline diffs.
        wdiff = self._port.wdiff_text(expected_filename, actual_filename)
        wdiff_filename = self.output_filename(self.FILENAME_SUFFIX_WDIFF)
        fs.write_binary_file(wdiff_filename, wdiff)

        # Use WebKit's PrettyPatch.rb to get an HTML diff.
        pretty_patch = self._port.pretty_patch_text(diff_filename)
        pretty_patch_filename = self.output_filename(self.FILENAME_SUFFIX_PRETTY_PATCH)
        fs.write_binary_file(pretty_patch_filename, pretty_patch)

    def write_audio_files(self, actual_audio, expected_audio):
        self.write_output_files('.wav', actual_audio, expected_audio)

    def write_image_files(self, actual_image, expected_image):
        self.write_output_files('.png', actual_image, expected_image)

    def write_image_hashes(self, actual_image_hash, expected_image_hash):
        self.write_output_files('.checksum', actual_image_hash, expected_image_hash)

    def create_image_diff_and_write_result(self, actual_image, expected_image):
        """Writes the visual diff of the expected/actual PNGs.

        Returns True if the images are different.
        """
        # FIXME: This function is actually doing the diff as well as writing a result.
        # It might be better to extract 'diff' code and make it a separate function.
        # To do so, we have to change port.diff_image() as well.
        diff_filename = self.output_filename(self.FILENAME_SUFFIX_IMAGE_DIFF)
        return self._port.diff_image(actual_image, expected_image, diff_filename)

    def copy_file(self, src_filepath, dst_extension):
        fs = self._port._filesystem
        assert fs.exists(src_filepath), 'src_filepath: %s' % src_filepath
        dst_filename = self.output_filename(dst_extension)
        self._make_output_directory()
        fs.copyfile(src_filepath, dst_filename)
