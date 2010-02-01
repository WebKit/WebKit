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

"""Unit tests for error_handlers.py."""


import unittest

from .. style_references import parse_patch
from checker import ProcessorOptions
from error_handlers import DefaultStyleErrorHandler
from error_handlers import PatchStyleErrorHandler


class StyleErrorHandlerTestBase(unittest.TestCase):

    def setUp(self):
        self._error_messages = ""
        self._error_count = 0

    def _mock_increment_error_count(self):
        self._error_count += 1

    def _mock_stderr_write(self, message):
        self._error_messages += message


class DefaultStyleErrorHandlerTest(StyleErrorHandlerTestBase):

    """Tests DefaultStyleErrorHandler class."""

    _category = "whitespace/tab"

    def _options(self, output_format):
        return ProcessorOptions(verbosity=3, output_format=output_format)

    def _error_handler(self, options):
        file_path = "foo.h"
        return DefaultStyleErrorHandler(file_path,
                                        options,
                                        self._mock_increment_error_count,
                                        self._mock_stderr_write)

    def _prepare_call(self, output_format="emacs"):
        """Return options after initializing."""
        options = self._options(output_format)

        # Test that count is initialized to zero.
        self.assertEquals(0, self._error_count)
        self.assertEquals("", self._error_messages)

        return options

    def _call_error_handler(self, options, confidence):
        """Handle an error with given confidence."""
        handle_error = self._error_handler(options)

        line_number = 100
        message = "message"

        handle_error(line_number, self._category, confidence, message)

    def test_call_non_reportable(self):
        """Test __call__() method with a non-reportable error."""
        confidence = 1
        options = self._prepare_call()

        # Confirm the error is not reportable.
        self.assertFalse(options.is_reportable(self._category, confidence))

        self._call_error_handler(options, confidence)

        self.assertEquals(0, self._error_count)
        self.assertEquals("", self._error_messages)

    def test_call_reportable_emacs(self):
        """Test __call__() method with a reportable error and emacs format."""
        confidence = 5
        options = self._prepare_call("emacs")

        self._call_error_handler(options, confidence)

        self.assertEquals(1, self._error_count)
        self.assertEquals(self._error_messages,
                          "foo.h:100:  message  [whitespace/tab] [5]\n")

    def test_call_reportable_vs7(self):
        """Test __call__() method with a reportable error and vs7 format."""
        confidence = 5
        options = self._prepare_call("vs7")

        self._call_error_handler(options, confidence)

        self.assertEquals(1, self._error_count)
        self.assertEquals(self._error_messages,
                          "foo.h(100):  message  [whitespace/tab] [5]\n")


class PatchStyleErrorHandlerTest(StyleErrorHandlerTestBase):

    """Tests PatchStyleErrorHandler class."""

    file_path = "__init__.py"

    patch_string = """diff --git a/__init__.py b/__init__.py
index ef65bee..e3db70e 100644
--- a/__init__.py
+++ b/__init__.py
@@ -1 +1,2 @@
 # Required for Python to search this directory for module files
+# New line

"""

    def test_call(self):
        patch_files = parse_patch(self.patch_string)
        diff = patch_files[self.file_path]

        options = ProcessorOptions(verbosity=3)

        handle_error = PatchStyleErrorHandler(diff,
                                              self.file_path,
                                              options,
                                              self._mock_increment_error_count,
                                              self._mock_stderr_write)

        category = "whitespace/tab"
        confidence = 5
        message = "message"

        # Confirm error is reportable.
        self.assertTrue(options.is_reportable(category, confidence))

        # Confirm error count initialized to zero.
        self.assertEquals(0, self._error_count)

        # Test error in unmodified line (error count does not increment).
        handle_error(1, category, confidence, message)
        self.assertEquals(0, self._error_count)

        # Test error in modified line (error count increments).
        handle_error(2, category, confidence, message)
        self.assertEquals(1, self._error_count)

