#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""Compares the text output of a test to the expected text output.

If the output doesn't match, returns FailureTextMismatch and outputs the diff
files into the layout test results directory.
"""

from __future__ import with_statement

import codecs
import errno
import logging
import os.path

from webkitpy.layout_tests.layout_package import test_failures
from webkitpy.layout_tests.test_types import test_type_base

_log = logging.getLogger("webkitpy.layout_tests.test_types.text_diff")


class TestTextDiff(test_type_base.TestTypeBase):

    def _get_normalized_output_text(self, output):
        # Some tests produce "\r\n" explicitly.  Our system (Python/Cygwin)
        # helpfully changes the "\n" to "\r\n", resulting in "\r\r\n".
        norm = output.replace("\r\r\n", "\r\n").strip("\r\n").replace(
             "\r\n", "\n")
        return norm + "\n"

    def _get_normalized_expected_text(self, filename):
        """Given the filename of the test, read the expected output from a file
        and normalize the text.  Returns a string with the expected text, or ''
        if the expected output file was not found."""
        return self._port.expected_text(filename)

    def compare_output(self, port, filename, output, test_args, configuration):
        """Implementation of CompareOutput that checks the output text against
        the expected text from the LayoutTest directory."""
        failures = []

        # If we're generating a new baseline, we pass.
        if test_args.new_baseline or test_args.reset_results:
            # Although all test_shell/DumpRenderTree output should be utf-8,
            # we do not ever decode it inside run-webkit-tests.  For some tests
            # DumpRenderTree may not output utf-8 text (e.g. webarchives).
            self._save_baseline_data(filename, output, ".txt", encoding=None,
                                     generate_new_baseline=test_args.new_baseline)
            return failures

        # Normalize text to diff
        output = self._get_normalized_output_text(output)
        expected = self._get_normalized_expected_text(filename)

        # Write output files for new tests, too.
        if port.compare_text(output, expected):
            # Text doesn't match, write output files.
            self.write_output_files(filename, ".txt", output,
                                    expected, encoding=None,
                                    print_text_diffs=True)

            if expected == '':
                failures.append(test_failures.FailureMissingResult())
            else:
                failures.append(test_failures.FailureTextMismatch(True))

        return failures
