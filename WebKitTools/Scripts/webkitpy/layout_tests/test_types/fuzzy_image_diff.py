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

"""Compares the image output of a test to the expected image output using
fuzzy matching.
"""

import errno
import logging
import os
import shutil

from layout_package import test_failures
from test_types import test_type_base


class FuzzyImageDiff(test_type_base.TestTypeBase):

    def compare_output(self, filename, output, test_args, target):
        """Implementation of CompareOutput that checks the output image and
        checksum against the expected files from the LayoutTest directory.
        """
        failures = []

        # If we didn't produce a hash file, this test must be text-only.
        if test_args.hash is None:
            return failures

        expected_png_file = self._port.expected_filename(filename, '.png')

        if test_args.show_sources:
            logging.debug('Using %s' % expected_png_file)

        # Also report a missing expected PNG file.
        if not os.path.isfile(expected_png_file):
            failures.append(test_failures.FailureMissingImage(self))

        # Run the fuzzymatcher
        r = port.fuzzy_diff(test_args.png_path, expected_png_file)
        if r != 0:
            failures.append(test_failures.FailureFuzzyFailure(self))

        return failures
