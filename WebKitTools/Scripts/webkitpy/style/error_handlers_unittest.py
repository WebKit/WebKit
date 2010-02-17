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
        self._error_messages = []
        self._error_count = 0

    def _mock_increment_error_count(self):
        self._error_count += 1

    def _mock_stderr_write(self, message):
        self._error_messages.append(message)


class DefaultStyleErrorHandlerTest(StyleErrorHandlerTestBase):

    """Tests DefaultStyleErrorHandler class."""

    _category = "whitespace/tab"
    """The category name for the tests in this class."""

    _file_path = "foo.h"
    """The file path for the tests in this class."""

    def _check_initialized(self):
        """Check that count and error messages are initialized."""
        self.assertEquals(0, self._error_count)
        self.assertEquals(0, len(self._error_messages))

    def _error_handler(self, options):
        return DefaultStyleErrorHandler(file_path=self._file_path,
                                        increment_error_count=
                                            self._mock_increment_error_count,
                                        options=options,
                                        stderr_write=self._mock_stderr_write)

    def _call_error_handler(self, handle_error, confidence):
        """Call the given error handler with a test error."""
        handle_error(line_number=100,
                     category=self._category,
                     confidence=confidence,
                     message="message")

    def test_call_non_reportable(self):
        """Test __call__() method with a non-reportable error."""
        confidence = 1
        options = ProcessorOptions(verbosity=3)
        self._check_initialized()

        # Confirm the error is not reportable.
        self.assertFalse(options.is_reportable(self._category,
                                               confidence,
                                               self._file_path))

        error_handler = self._error_handler(options)
        self._call_error_handler(error_handler, confidence)

        self.assertEquals(0, self._error_count)
        self.assertEquals([], self._error_messages)

    def test_call_reportable_emacs(self):
        """Test __call__() method with a reportable error and emacs format."""
        confidence = 5
        options = ProcessorOptions(verbosity=3, output_format="emacs")
        self._check_initialized()

        error_handler = self._error_handler(options)
        self._call_error_handler(error_handler, confidence)

        self.assertEquals(1, self._error_count)
        self.assertEquals(self._error_messages[-1],
                          "foo.h:100:  message  [whitespace/tab] [5]\n")

    def test_call_reportable_vs7(self):
        """Test __call__() method with a reportable error and vs7 format."""
        confidence = 5
        options = ProcessorOptions(verbosity=3, output_format="vs7")
        self._check_initialized()

        error_handler = self._error_handler(options)
        self._call_error_handler(error_handler, confidence)

        self.assertEquals(1, self._error_count)
        self.assertEquals(self._error_messages[-1],
                          "foo.h(100):  message  [whitespace/tab] [5]\n")

    def test_call_max_reports_per_category(self):
        """Test error report suppression in __call__() method."""
        confidence = 5
        options = ProcessorOptions(verbosity=3,
                                   max_reports_per_category={self._category: 2})
        error_handler = self._error_handler(options)

        self._check_initialized()

        # First call: usual reporting.
        self._call_error_handler(error_handler, confidence)
        self.assertEquals(1, self._error_count)
        self.assertEquals(1, len(self._error_messages))
        self.assertEquals(self._error_messages[-1],
                          "foo.h:100:  message  [whitespace/tab] [5]\n")

        # Second call: suppression message reported.
        self._call_error_handler(error_handler, confidence)
        self.assertEquals(2, self._error_count)
        self.assertEquals(2, len(self._error_messages))
        self.assertEquals(self._error_messages[-1],
                          "foo.h:100:  message  [whitespace/tab] [5]\n"
                          "Suppressing further [%s] reports for this file.\n"
                          % self._category)

        # Third call: no report.
        self._call_error_handler(error_handler, confidence)
        self.assertEquals(3, self._error_count)
        self.assertEquals(2, len(self._error_messages))


class PatchStyleErrorHandlerTest(StyleErrorHandlerTestBase):

    """Tests PatchStyleErrorHandler class."""

    _file_path = "__init__.py"

    _patch_string = """diff --git a/__init__.py b/__init__.py
index ef65bee..e3db70e 100644
--- a/__init__.py
+++ b/__init__.py
@@ -1 +1,2 @@
 # Required for Python to search this directory for module files
+# New line

"""

    def test_call(self):
        patch_files = parse_patch(self._patch_string)
        diff = patch_files[self._file_path]

        options = ProcessorOptions(verbosity=3)

        handle_error = PatchStyleErrorHandler(diff,
                                              self._file_path,
                                              options,
                                              self._mock_increment_error_count,
                                              self._mock_stderr_write)

        category = "whitespace/tab"
        confidence = 5
        message = "message"

        # Confirm error is reportable.
        self.assertTrue(options.is_reportable(category,
                                              confidence,
                                              self._file_path))

        # Confirm error count initialized to zero.
        self.assertEquals(0, self._error_count)

        # Test error in unmodified line (error count does not increment).
        handle_error(1, category, confidence, message)
        self.assertEquals(0, self._error_count)

        # Test error in modified line (error count increments).
        handle_error(2, category, confidence, message)
        self.assertEquals(1, self._error_count)

