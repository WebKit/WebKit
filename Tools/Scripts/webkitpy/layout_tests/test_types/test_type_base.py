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

"""Defines the interface TestTypeBase which other test types inherit from.
"""

import cgi
import errno
import logging

_log = logging.getLogger("webkitpy.layout_tests.test_types.test_type_base")


# Python bug workaround.  See the wdiff code in WriteOutputFiles for an
# explanation.
_wdiff_available = True


class TestTypeBase(object):

    def __init__(self, port, root_output_dir):
        """Initialize a TestTypeBase object.

        Args:
          port: object implementing port-specific information and methods
          root_output_dir: The unix style path to the output dir.
        """
        self._root_output_dir = root_output_dir
        self._port = port

    def compare_output(self, port, filename, options, actual_driver_output,
                        expected_driver_output):
        """Method that compares the output from the test with the
        expected value.

        This is an abstract method to be implemented by all sub classes.

        Args:
          port: object implementing port-specific information and methods
          filename: absolute filename to test file
          options: command line argument object from optparse
          actual_driver_output: a DriverOutput object which represents actual test
              output
          expected_driver_output: a ExpectedDriverOutput object which represents a
              expected test output

        Return:
          a list of TestFailure objects, empty if the test passes
        """
        raise NotImplementedError
