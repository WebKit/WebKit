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

import errno
import logging
import os
import shutil

from layout_package import test_failures
from test_types import test_type_base

# Cache whether we have the image_diff executable available.
_compare_available = True
_compare_msg_printed = False


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

        try:
            shutil.copyfile(source_image, dest_image)
        except IOError, e:
            # A missing expected PNG has already been recorded as an error.
            if errno.ENOENT != e.errno:
                raise

    def _save_baseline_files(self, filename, png_path, checksum):
        """Saves new baselines for the PNG and checksum.

        Args:
          filename: test filename
          png_path: path to the actual PNG result file
          checksum: value of the actual checksum result
        """
        png_file = open(png_path, "rb")
        png_data = png_file.read()
        png_file.close()
        self._save_baseline_data(filename, png_data, ".png")
        self._save_baseline_data(filename, checksum, ".checksum")

    def _create_image_diff(self, port, filename, target):
        """Creates the visual diff of the expected/actual PNGs.

        Args:
          filename: the name of the test
          target: Debug or Release
        """
        diff_filename = self.output_filename(filename,
          self.FILENAME_SUFFIX_COMPARE)
        actual_filename = self.output_filename(filename,
          self.FILENAME_SUFFIX_ACTUAL + '.png')
        expected_filename = self.output_filename(filename,
          self.FILENAME_SUFFIX_EXPECTED + '.png')

        try:
            _compare_available = True
            result = port.diff_image(actual_filename, expected_filename,
                                     diff_filename)
        except ValueError:
            _compare_available = False

        global _compare_msg_printed
        if not _compare_available and not _compare_msg_printed:
            _compare_msg_printed = True
            print('image_diff not found. Make sure you have a ' + target +
                  ' build of the image_diff executable.')

        return result

    def compare_output(self, port, filename, output, test_args, target):
        """Implementation of CompareOutput that checks the output image and
        checksum against the expected files from the LayoutTest directory.
        """
        failures = []

        # If we didn't produce a hash file, this test must be text-only.
        if test_args.hash is None:
            return failures

        # If we're generating a new baseline, we pass.
        if test_args.new_baseline:
            self._save_baseline_files(filename, test_args.png_path,
                                    test_args.hash)
            return failures

        # Compare hashes.
        expected_hash_file = self._port.expected_filename(filename,
                                                          '.checksum')
        expected_png_file = self._port.expected_filename(filename, '.png')

        if test_args.show_sources:
            logging.debug('Using %s' % expected_hash_file)
            logging.debug('Using %s' % expected_png_file)

        try:
            expected_hash = open(expected_hash_file, "r").read()
        except IOError, e:
            if errno.ENOENT != e.errno:
                raise
            expected_hash = ''


        if not os.path.isfile(expected_png_file):
            # Report a missing expected PNG file.
            self.write_output_files(port, filename, '', '.checksum',
                                    test_args.hash, expected_hash,
                                    diff=False, wdiff=False)
            self._copy_output_png(filename, test_args.png_path, '-actual.png')
            failures.append(test_failures.FailureMissingImage(self))
            return failures
        elif test_args.hash == expected_hash:
            # Hash matched (no diff needed, okay to return).
            return failures


        self.write_output_files(port, filename, '', '.checksum',
                                test_args.hash, expected_hash,
                                diff=False, wdiff=False)
        self._copy_output_png(filename, test_args.png_path, '-actual.png')
        self._copy_output_png(filename, expected_png_file, '-expected.png')

        # Even though we only use result in one codepath below but we
        # still need to call CreateImageDiff for other codepaths.
        result = self._create_image_diff(port, filename, target)
        if expected_hash == '':
            failures.append(test_failures.FailureMissingImageHash(self))
        elif test_args.hash != expected_hash:
            # Hashes don't match, so see if the images match. If they do, then
            # the hash is wrong.
            if result == 0:
                failures.append(test_failures.FailureImageHashIncorrect(self))
            else:
                failures.append(test_failures.FailureImageHashMismatch(self))

        return failures

    def diff_files(self, port, file1, file2):
        """Diff two image files.

        Args:
          file1, file2: full paths of the files to compare.

        Returns:
          True if two files are different.
          False otherwise.
        """

        try:
            result = port.diff_image(file1, file2)
        except ValueError, e:
            return True

        return result == 1
