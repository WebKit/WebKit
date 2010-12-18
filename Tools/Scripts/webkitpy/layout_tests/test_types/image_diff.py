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

    def _save_baseline_files(self, filename, image, image_hash,
                             generate_new_baseline):
        """Saves new baselines for the PNG and checksum.

        Args:
          filename: test filename
          image: a image output
          image_hash: a checksum of the image
          generate_new_baseline: whether to generate a new, platform-specific
            baseline, or update the existing one
        """
        self._save_baseline_data(filename, image, ".png", encoding=None,
                                 generate_new_baseline=generate_new_baseline)
        self._save_baseline_data(filename, image_hash, ".checksum",
                                 encoding="ascii",
                                 generate_new_baseline=generate_new_baseline)

    def _copy_image(self, filename, actual_image, expected_image):
        self.write_output_files(filename, '.png',
                                output=actual_image, expected=expected_image,
                                encoding=None, print_text_diffs=False)

    def _copy_image_hash(self, filename, actual_image_hash, expected_image_hash):
        self.write_output_files(filename, '.checksum',
                                actual_image_hash, expected_image_hash,
                                encoding="ascii", print_text_diffs=False)

    def _create_diff_image(self, port, filename, actual_image, expected_image):
        """Creates the visual diff of the expected/actual PNGs.

        Returns True if the images are different.
        """
        diff_filename = self.output_filename(filename,
                                             self.FILENAME_SUFFIX_COMPARE)
        return port.diff_image(actual_image, expected_image, diff_filename)

    def compare_output(self, port, filename, test_args, actual_test_output,
                       expected_test_output):
        """Implementation of CompareOutput that checks the output image and
        checksum against the expected files from the LayoutTest directory.
        """
        failures = []

        # If we didn't produce a hash file, this test must be text-only.
        if actual_test_output.image_hash is None:
            return failures

        # If we're generating a new baseline, we pass.
        if test_args.new_baseline or test_args.reset_results:
            self._save_baseline_files(filename, actual_test_output.image,
                                      actual_test_output.image_hash,
                                      test_args.new_baseline)
            return failures

        if not expected_test_output.image:
            # Report a missing expected PNG file.
            self._copy_image(filename, actual_test_output.image, expected_image=None)
            self._copy_image_hash(filename, actual_test_output.image_hash,
                                  expected_test_output.image_hash)
            failures.append(test_failures.FailureMissingImage())
            return failures
        if not expected_test_output.image_hash:
            # Report a missing expected checksum file.
            self._copy_image(filename, actual_test_output.image,
                             expected_test_output.image)
            self._copy_image_hash(filename, actual_test_output.image_hash,
                                  expected_image_hash=None)
            failures.append(test_failures.FailureMissingImageHash())
            return failures

        if actual_test_output.image_hash == expected_test_output.image_hash:
            # Hash matched (no diff needed, okay to return).
            return failures

        self._copy_image(filename, actual_test_output.image,
                         expected_test_output.image)
        self._copy_image_hash(filename, actual_test_output.image_hash,
                              expected_test_output.image_hash)

        # Even though we only use the result in one codepath below but we
        # still need to call CreateImageDiff for other codepaths.
        images_are_different = self._create_diff_image(port, filename,
                                                       actual_test_output.image,
                                                       expected_test_output.image)
        if not images_are_different:
            failures.append(test_failures.FailureImageHashIncorrect())
        else:
            failures.append(test_failures.FailureImageHashMismatch())

        return failures
