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

"""Compares the image output of a test to the expected image output.

Compares hashes for the generated and expected images. If the output doesn't
match, returns FailureImageHashMismatch and outputs both hashes into the layout
test results directory.
"""

from __future__ import with_statement

import codecs
import errno
import logging
import os
import shutil

from webkitpy.layout_tests.layout_package import test_failures
from webkitpy.layout_tests.test_types import test_type_base

# Cache whether we have the image_diff executable available.
_compare_available = True
_compare_msg_printed = False

_log = logging.getLogger("webkitpy.layout_tests.test_types.image_diff")


class ImageDiff(test_type_base.TestTypeBase):

    def _copy_output_png(self, test_filename, source_image, extension):
        """Copies result files into the output directory with appropriate
        names.

        Args:
          test_filename: the test filename
          source_file: path to the image file (either actual or expected)
          extension: extension to indicate -actual.png or -expected.png
        """
        self._make_output_directory(test_filename)
        dest_image = self.output_filename(test_filename, extension)

        if os.path.exists(source_image):
            shutil.copyfile(source_image, dest_image)

    def _save_baseline_files(self, filename, png_path, checksum,
                             generate_new_baseline):
        """Saves new baselines for the PNG and checksum.

        Args:
          filename: test filename
          png_path: path to the actual PNG result file
          checksum: value of the actual checksum result
          generate_new_baseline: whether to generate a new, platform-specific
            baseline, or update the existing one
        """
        with open(png_path, "rb") as png_file:
            png_data = png_file.read()
        self._save_baseline_data(filename, png_data, ".png", encoding=None,
                                 generate_new_baseline=generate_new_baseline)
        self._save_baseline_data(filename, checksum, ".checksum",
                                 encoding="ascii",
                                 generate_new_baseline=generate_new_baseline)

    def _create_image_diff(self, port, filename, configuration):
        """Creates the visual diff of the expected/actual PNGs.

        Args:
          filename: the name of the test
          configuration: Debug or Release
        Returns True if the files are different, False if they match
        """
        diff_filename = self.output_filename(filename,
          self.FILENAME_SUFFIX_COMPARE)
        actual_filename = self.output_filename(filename,
          self.FILENAME_SUFFIX_ACTUAL + '.png')
        expected_filename = self.output_filename(filename,
          self.FILENAME_SUFFIX_EXPECTED + '.png')

        result = port.diff_image(expected_filename, actual_filename,
                                 diff_filename)
        return result

    def compare_output(self, port, filename, output, test_args, configuration):
        """Implementation of CompareOutput that checks the output image and
        checksum against the expected files from the LayoutTest directory.
        """
        failures = []

        # If we didn't produce a hash file, this test must be text-only.
        if test_args.hash is None:
            return failures

        # If we're generating a new baseline, we pass.
        if test_args.new_baseline or test_args.reset_results:
            self._save_baseline_files(filename, test_args.png_path,
                                      test_args.hash, test_args.new_baseline)
            return failures

        # Compare hashes.
        expected_hash_file = self._port.expected_filename(filename,
                                                          '.checksum')
        expected_png_file = self._port.expected_filename(filename, '.png')

        # FIXME: We repeat this pattern often, we should share code.
        expected_hash = ''
        if os.path.exists(expected_hash_file):
            with codecs.open(expected_hash_file, "r", "ascii") as file:
                expected_hash = file.read()

        if not os.path.isfile(expected_png_file):
            # Report a missing expected PNG file.
            self.write_output_files(port, filename, '.checksum',
                                    test_args.hash, expected_hash,
                                    encoding="ascii",
                                    print_text_diffs=False)
            self._copy_output_png(filename, test_args.png_path, '-actual.png')
            failures.append(test_failures.FailureMissingImage())
            return failures
        elif test_args.hash == expected_hash:
            # Hash matched (no diff needed, okay to return).
            return failures

        self.write_output_files(port, filename, '.checksum',
                                test_args.hash, expected_hash,
                                encoding="ascii",
                                print_text_diffs=False)
        self._copy_output_png(filename, test_args.png_path, '-actual.png')
        self._copy_output_png(filename, expected_png_file, '-expected.png')

        # Even though we only use the result in one codepath below but we
        # still need to call CreateImageDiff for other codepaths.
        images_are_different = self._create_image_diff(port, filename, configuration)
        if expected_hash == '':
            failures.append(test_failures.FailureMissingImageHash())
        elif test_args.hash != expected_hash:
            if images_are_different:
                failures.append(test_failures.FailureImageHashMismatch())
            else:
                failures.append(test_failures.FailureImageHashIncorrect())

        return failures
