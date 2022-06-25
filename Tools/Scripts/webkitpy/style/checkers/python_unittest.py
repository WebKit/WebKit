# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Unit tests for python.py."""

import os
import sys
import unittest

from webkitpy.style.checkers.python import PythonChecker


class PythonCheckerTest(unittest.TestCase):

    """Tests the PythonChecker class."""

    def test_init(self):
        """Test __init__() method."""
        def _mock_handle_style_error(self):
            pass

        checker = PythonChecker("foo.txt", _mock_handle_style_error)
        self.assertEqual(checker._file_path, "foo.txt")
        self.assertEqual(checker._handle_style_error,
                          _mock_handle_style_error)

    def test_check(self):
        """Test check() method."""
        errors = []

        def _mock_handle_style_error(line_number, category, confidence, message):
            error = (line_number, category, confidence, message)
            errors.append(error)

        current_dir = os.path.dirname(__file__)
        file_path = os.path.join(current_dir, "python_unittest_input.py")

        checker = PythonChecker(file_path, _mock_handle_style_error)
        checker.check(lines=[])

        # FIXME: https://bugs.webkit.org/show_bug.cgi?id=204133
        expected_errors = [(4, "pep8/W291", 5, "trailing whitespace")]
        if sys.version_info < (3, 0):
            expected_errors.append((4, "pylint/E0602", 5, "Undefined variable 'error'"))

        self.assertEqual(errors, expected_errors)

    def test_pylint_false_positives(self):
        """Test that pylint false positives are suppressed."""
        errors = []

        def _mock_handle_style_error(line_number, category, confidence,
                                     message):
            error = (line_number, category, confidence, message)
            errors.append(error)

        current_dir = os.path.dirname(__file__)
        file_path = os.path.join(current_dir, "python_unittest_falsepositives.py")

        checker = PythonChecker(file_path, _mock_handle_style_error)
        checker.check(lines=[])

        self.assertEqual(errors, [])
