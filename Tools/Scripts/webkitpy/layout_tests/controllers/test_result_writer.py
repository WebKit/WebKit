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

import re
import logging

from webkitpy.common.wavediff import WaveDiff


_log = logging.getLogger(__name__)


def write_test_result(filesystem, port, results_directory, test_name, driver_output,
                      expected_driver_output, failures):
    """Write the test result to the result output directory."""
    root_output_dir = results_directory
    writer = TestResultWriter(filesystem, port, root_output_dir, test_name)

    if driver_output.error:
        writer.write_stderr(driver_output.error)

    for failure in failures:
        failure.write_failure(writer, driver_output, expected_driver_output, port)


class TestResultWriter(object):
    """A class which handles all writing operations to the result directory."""

    # Filename pieces when writing failures to the test results directory.
    FILENAME_SUFFIX_ACTUAL = "-actual"
    FILENAME_SUFFIX_EXPECTED = "-expected"
    FILENAME_SUFFIX_DIFF = "-diff"
    FILENAME_SUFFIX_STDERR = "-stderr"
    FILENAME_SUFFIX_CRASH_LOG = "-crash-log"
    FILENAME_SUFFIX_SAMPLE = "-sample"
    FILENAME_SUFFIX_PRETTY_PATCH = "-pretty-diff.html"
    FILENAME_SUFFIX_IMAGE_DIFF = "-diff.png"
    FILENAME_SUFFIX_IMAGE_DIFFS_HTML = "-diffs.html"
    PROCESS_NAME_RE = re.compile(r'(com\.apple|org\.WebKit)\..+')

    @staticmethod
    def expected_filename(test_name, filesystem, port_name=None, suffix='txt'):
        if not port_name:
            return filesystem.splitext(test_name)[0] + TestResultWriter.FILENAME_SUFFIX_EXPECTED + '.' + suffix
        return filesystem.join("platform", port_name, filesystem.splitext(test_name)[0] + TestResultWriter.FILENAME_SUFFIX_EXPECTED + '.' + suffix)

    @staticmethod
    def actual_filename(test_name, filesystem, suffix='txt'):
        return filesystem.splitext(test_name)[0] + TestResultWriter.FILENAME_SUFFIX_ACTUAL + '.' + suffix

    def __init__(self, filesystem, port, root_output_dir, test_name):
        self._filesystem = filesystem
        self._port = port
        self._root_output_dir = root_output_dir
        self._test_name = test_name

    def _make_output_directory(self):
        """Creates the output directory (if needed) for a given test filename."""
        fs = self._filesystem
        output_filename = fs.join(self._root_output_dir, self._test_name)
        fs.maybe_make_directory(fs.dirname(output_filename))

    def output_filename(self, modifier):
        """Returns a filename inside the output dir that contains modifier.

        For example, if test name is "fast/dom/foo.html" and modifier is "-expected.txt",
        the return value is "/<path-to-root-output-dir>/fast/dom/foo-expected.txt".

        Args:
          modifier: a string to replace the extension of filename with

        Return:
          The absolute path to the output filename
        """
        fs = self._filesystem

        # Test names that are acutally process names are treated like they don't have any extension
        if self.PROCESS_NAME_RE.match(fs.basename(self._test_name)):
            ext_parts = (self._test_name, '', '')
        else:
            ext_parts = fs.splitext(self._test_name)
        output_basename = ext_parts[0]
        extension = ext_parts[1]

        # Find the actual file extension while keeping track of URI fragment or query, if any. Only
        # the last extra part will be used for naming the output file, eg if self._test_name is
        # "foo.html?bar#blah" then final output_basename will be "foo_blah" and extension will be
        # ".html".
        extra_part = ''
        for char in ('?', '#'):
            index = extension.find(char)
            if index != -1:
                extension, extra_part = extension[:index], extension[index + 1:]

        if extra_part:
            output_basename += '_' + extra_part
        return fs.join(self._root_output_dir, output_basename) + modifier

    def _write_binary_file(self, path, contents):
        if contents is not None:
            self._make_output_directory()
            self._filesystem.write_binary_file(path, contents)

    def _write_text_file(self, path, contents):
        if contents is not None:
            self._make_output_directory()
            self._filesystem.write_text_file(path, contents)

    def _output_testname(self, modifier):
        fs = self._filesystem
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
        actual_filename = self.output_filename(self.FILENAME_SUFFIX_ACTUAL + file_type)
        expected_filename = self.output_filename(self.FILENAME_SUFFIX_EXPECTED + file_type)

        self._write_binary_file(actual_filename, output)
        self._write_binary_file(expected_filename, expected)

    def write_stderr(self, error):
        filename = self.output_filename(self.FILENAME_SUFFIX_STDERR + ".txt")
        self._write_binary_file(filename, error)

    def write_crash_log(self, crash_log):
        filename = self.output_filename(self.FILENAME_SUFFIX_CRASH_LOG + ".txt")
        self._write_text_file(filename, crash_log)

    def copy_sample_file(self, sample_file):
        filename = self.output_filename(self.FILENAME_SUFFIX_SAMPLE + ".txt")
        self._filesystem.copyfile(sample_file, filename)

    def write_text_files(self, actual_text, expected_text):
        self.write_output_files(".txt", actual_text, expected_text)

    def create_text_diff_and_write_result(self, actual_text, expected_text):
        # FIXME: This function is actually doing the diffs as well as writing results.
        # It might be better to extract code which does 'diff' and make it a separate function.
        if not actual_text or not expected_text:
            return

        file_type = '.txt'
        actual_filename = self.output_filename(self.FILENAME_SUFFIX_ACTUAL + file_type)
        expected_filename = self.output_filename(self.FILENAME_SUFFIX_EXPECTED + file_type)
        # We treat diff output as binary. Diff output may contain multiple files
        # in conflicting encodings.
        diff = self._port.diff_text(expected_text, actual_text, expected_filename, actual_filename)
        diff_filename = self.output_filename(self.FILENAME_SUFFIX_DIFF + file_type)
        self._write_binary_file(diff_filename, diff)

        # Use WebKit's PrettyPatch.rb to get an HTML diff.
        if self._port.pretty_patch.pretty_patch_available():
            pretty_patch = self._port.pretty_patch.pretty_patch_text(diff_filename)
            pretty_patch_filename = self.output_filename(self.FILENAME_SUFFIX_PRETTY_PATCH)
            self._write_binary_file(pretty_patch_filename, pretty_patch)

    def write_audio_files(self, actual_audio, expected_audio):
        self.write_output_files('.wav', actual_audio, expected_audio)

    def create_audio_diff_and_write_result(self, actual_audio, expected_audio):
        diff_filename = self.output_filename(self.FILENAME_SUFFIX_DIFF + '.txt')
        self._write_text_file(diff_filename, WaveDiff(expected_audio, actual_audio).diffText())

    def write_image_files(self, actual_image, expected_image):
        self.write_output_files('.png', actual_image, expected_image)

    def write_image_diff_files(self, image_diff, diff_percent_text=None, fuzzy_data_text=None):
        diff_filename = self.output_filename(self.FILENAME_SUFFIX_IMAGE_DIFF)
        self._write_binary_file(diff_filename, image_diff)

        base_dir = self._port.path_from_webkit_base('LayoutTests', 'fast', 'harness')

        image_diff_template = self._filesystem.join(base_dir, 'image-diff-template.html')
        image_diff_file = ""
        if self._filesystem.exists(image_diff_template):
            image_diff_file = self._filesystem.read_text_file(image_diff_template)

        html = image_diff_file.replace('__TITLE__', self._test_name)
        html = html.replace('__TEST_NAME__', self._test_name)
        html = html.replace('__PREFIX__', self._output_testname(''))

        if not diff_percent_text:
            html = html.replace('__HIDE_DIFF_CLASS__', 'hidden')
        else:
            html = html.replace('__HIDE_DIFF_CLASS__', '')
            html = html.replace('__PIXEL_DIFF__', diff_percent_text)

        if not fuzzy_data_text:
            html = html.replace('__HIDE_FUZZY_CLASS__', 'hidden')
        else:
            html = html.replace('__HIDE_FUZZY_CLASS__', '')
            html = html.replace('__FUZZY_DATA__', fuzzy_data_text)

        diffs_html_filename = self.output_filename(self.FILENAME_SUFFIX_IMAGE_DIFFS_HTML)
        self._filesystem.write_text_file(diffs_html_filename, html)

    def write_reftest(self, src_filepath):
        fs = self._filesystem
        dst_dir = fs.dirname(fs.join(self._root_output_dir, self._test_name))
        dst_filepath = fs.join(dst_dir, fs.basename(src_filepath))
        self._make_output_directory()
        fs.copyfile(src_filepath, dst_filepath)
