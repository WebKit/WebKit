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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Unit tests for common.py."""


import unittest

from common import check_no_carriage_return


# FIXME: The unit tests for the cpp, text, and common processors should
#        share supporting test code. This can include, for example, the
#        mock style error handling code and the code to check that all
#        of a processor's categories are covered by the unit tests.
#        Such shared code can be located in a shared test file, perhaps
#        ilke this one.
class CarriageReturnTest(unittest.TestCase):

    """Tests check_no_carriage_return()."""

    _category = "whitespace/carriage_return"
    _confidence = 1

    def setUp(self):
        self._style_errors = [] # The list of accumulated style errors.

    def _mock_style_error_handler(self, line_number, category, confidence,
                                  message):
        """Append the error information to the list of style errors."""
        error = (line_number, category, confidence, message)
        self._style_errors.append(error)

    def assert_carriage_return(self, line, is_error):
        """Call check_no_carriage_return() and assert the result."""
        line_number = 100
        handle_style_error = self._mock_style_error_handler

        check_no_carriage_return(line, line_number, handle_style_error)

        expected_message = ("One or more unexpected \\r (^M) found; "
                            "better to use only a \\n")

        if is_error:
            expected_errors = [(line_number, self._category, self._confidence,
                                expected_message)]
            self.assertEquals(self._style_errors, expected_errors)
        else:
            self.assertEquals(self._style_errors, [])

    def test_ends_with_carriage(self):
        self.assert_carriage_return("carriage return\r", is_error=True)

    def test_ends_with_nothing(self):
        self.assert_carriage_return("no carriage return", is_error=False)

    def test_ends_with_newline(self):
        self.assert_carriage_return("no carriage return\n", is_error=False)

    def test_ends_with_carriage_newline(self):
        # Check_no_carriage_return only() checks the final character.
        self.assert_carriage_return("carriage\r in a string", is_error=False)

