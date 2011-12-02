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

from webkitpy.common.system.crashlogs import CrashLogs
from webkitpy.layout_tests.models import test_failures


_log = logging.getLogger(__name__)


def write_test_result(port, test_name, driver_output,
                      expected_driver_output, failures):
    """Write the test result to the result output directory."""
    root_output_dir = port.results_directory()
    writer = TestResultWriter(port, root_output_dir, test_name)
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
        elif isinstance(failure, test_failures.FailureMissingImageHash):
            writer.write_image_files(driver_output.image, expected_driver_output.image)
        elif isinstance(failure, test_failures.FailureImageHashMismatch):
            writer.write_image_files(driver_output.image, expected_driver_output.image)
            writer.write_image_diff_files(driver_output.image_diff)
        elif isinstance(failure, (test_failures.FailureAudioMismatch,
                                  test_failures.FailureMissingAudio)):
            writer.write_audio_files(driver_output.audio, expected_driver_output.audio)
        elif isinstance(failure, test_failures.FailureCrash):
            crashed_driver_output = expected_driver_output if failure.is_reftest else driver_output
            writer.write_crash_report(crashed_driver_output.crashed_process_name, crashed_driver_output.error)
        elif isinstance(failure, test_failures.FailureReftestMismatch):
            writer.write_image_files(driver_output.image, expected_driver_output.image)
            # FIXME: This work should be done earlier in the pipeline (e.g., when we compare images for non-ref tests).
            # FIXME: We should always have 2 images here.
            if driver_output.image and expected_driver_output.image:
                image_diff = port.diff_image(driver_output.image, expected_driver_output.image, tolerance=0)[0]
                if image_diff:
                    writer.write_image_diff_files(image_diff)
                else:
                    _log.warn('Can not get image diff. ImageDiff program might not work correctly.')
            writer.copy_file(failure.reference_filename)
        elif isinstance(failure, test_failures.FailureReftestMismatchDidNotOccur):
            writer.write_image_files(driver_output.image, expected_image=None)
            writer.copy_file(failure.reference_filename)
        else:
            assert isinstance(failure, (test_failures.FailureTimeout,))


class TestResultWriter(object):
    """A class which handles all writing operations to the result directory."""

    # Filename pieces when writing failures to the test results directory.
    FILENAME_SUFFIX_ACTUAL = "-actual"
    FILENAME_SUFFIX_EXPECTED = "-expected"
    FILENAME_SUFFIX_DIFF = "-diff"
    FILENAME_SUFFIX_STDERR = "-stderr"
    FILENAME_SUFFIX_CRASH_LOG = "-crash-log"
    FILENAME_SUFFIX_WDIFF = "-wdiff.html"
    FILENAME_SUFFIX_PRETTY_PATCH = "-pretty-diff.html"
    FILENAME_SUFFIX_IMAGE_DIFF = "-diff.png"
    FILENAME_SUFFIX_IMAGE_DIFFS_HTML = "-diffs.html"

    def __init__(self, port, root_output_dir, test_name):
        self._port = port
        self._root_output_dir = root_output_dir
        self._test_name = test_name

    def _make_output_directory(self):
        """Creates the output directory (if needed) for a given test filename."""
        fs = self._port._filesystem
        output_filename = fs.join(self._root_output_dir, self._test_name)
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
        output_filename = fs.join(self._root_output_dir, self._test_name)
        return fs.splitext(output_filename)[0] + modifier

    def _output_testname(self, modifier):
        fs = self._port._filesystem
        return fs.splitext(fs.basename(self._test_name))[0] + modifier

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
        filename = self.output_filename(self.FILENAME_SUFFIX_STDERR + ".txt")
        fs.maybe_make_directory(fs.dirname(filename))
        fs.write_binary_file(filename, error)

    def write_crash_report(self, crashed_process_name, error):
        fs = self._port._filesystem
        filename = self.output_filename(self.FILENAME_SUFFIX_CRASH_LOG + ".txt")
        fs.maybe_make_directory(fs.dirname(filename))
        # FIXME: We shouldn't be grabbing private members of port.
        crash_logs = CrashLogs(fs)
        log = crash_logs.find_newest_log(crashed_process_name)
        # CrashLogs doesn't support every platform, so we fall back to
        # including the stderr output, which is admittedly somewhat redundant.
        fs.write_text_file(filename, log if log else error)

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

    def write_image_diff_files(self, image_diff):
        diff_filename = self.output_filename(self.FILENAME_SUFFIX_IMAGE_DIFF)
        fs = self._port._filesystem
        fs.write_binary_file(diff_filename, image_diff)

        diffs_html_filename = self.output_filename(self.FILENAME_SUFFIX_IMAGE_DIFFS_HTML)
        # FIXME: old-run-webkit-tests shows the diff percentage as the text contents of the "diff" link.
        # FIXME: old-run-webkit-tests include a link to the test file.
        html = """<!DOCTYPE HTML>
<html>
<head>
<title>%(title)s</title>
<style>.label{font-weight:bold}</style>
</head>
<body>
Difference between images: <a href="%(diff_filename)s">diff</a><br>
<div class=imageText></div>
<div class=imageContainer data-prefix="%(prefix)s">Loading...</div>
<script>
(function() {
    var preloadedImageCount = 0;
    function preloadComplete() {
        ++preloadedImageCount;
        if (preloadedImageCount < 2)
            return;
        toggleImages();
        setInterval(toggleImages, 2000)
    }

    function preloadImage(url) {
        image = new Image();
        image.addEventListener('load', preloadComplete);
        image.src = url;
        return image;
    }

    function toggleImages() {
        if (text.textContent == 'Expected Image') {
            text.textContent = 'Actual Image';
            container.replaceChild(actualImage, container.firstChild);
        } else {
            text.textContent = 'Expected Image';
            container.replaceChild(expectedImage, container.firstChild);
        }
    }

    var text = document.querySelector('.imageText');
    var container = document.querySelector('.imageContainer');
    var actualImage = preloadImage(container.getAttribute('data-prefix') + '-actual.png');
    var expectedImage = preloadImage(container.getAttribute('data-prefix') + '-expected.png');
})();
</script>
</body>
</html>
""" % {
            'title': self._test_name,
            'diff_filename': self._output_testname(self.FILENAME_SUFFIX_IMAGE_DIFF),
            'prefix': self._output_testname(''),
        }
        # FIXME: This seems like a text file, not a binary file.
        self._port._filesystem.write_binary_file(diffs_html_filename, html)

    def copy_file(self, src_filepath):
        fs = self._port._filesystem
        assert fs.exists(src_filepath), 'src_filepath: %s' % src_filepath
        dst_filepath = fs.join(self._root_output_dir, self._port.relative_test_filename(src_filepath))
        self._make_output_directory()
        fs.copyfile(src_filepath, dst_filepath)
