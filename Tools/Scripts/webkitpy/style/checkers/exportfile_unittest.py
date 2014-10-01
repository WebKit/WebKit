# Copyright (C) 2014 University of Szeged
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

import os
import unittest

from webkitpy.style.checkers.exportfile import ExportFileChecker
from webkitpy.common.system.filesystem import FileSystem

_sorted_file_contents = u""".objc_class_name_WebTextIterator
.objc_class_name_WebUserContentURLPattern
.objc_class_name_WebView
_WebActionButtonKey
_WebActionElementKey
_WebActionFormKey
"""

_non_sorted_file_contents = u""".objc_class_name_WebTextIterator
.objc_class_name_WebUserContentURLPattern
_WebActionElementKey
.objc_class_name_WebView
_WebActionButtonKey
_WebActionFormKey
"""

_parse_error_file_contents = u""".objc_class_name_WebTextIterator.objc_class_name_WebUserContentURLPattern_WebActionElementKey"""


def handle_style_error(mock_error_handler, line_number, category, confidence, message):
        mock_error_handler.had_error = True
        error = (line_number, category, confidence, message)
        mock_error_handler.errors.append(error)


class MockErrorHandler(object):
    def __init__(self, handle_style_error):
        self.turned_off_filtering = False
        self._handle_style_error = handle_style_error

    def turn_off_line_filtering(self):
        self.turned_off_filtering = True

    def __call__(self, line_number, category, confidence, message):
        self._handle_style_error(self, line_number, category, confidence, message)
        return True


class ExportFileTest(unittest.TestCase):

    def setUp(self):
        self._filesystem = FileSystem()
        self._temp_dir = str(self._filesystem.mkdtemp(suffix="exportfiles"))
        self._old_cwd = self._filesystem.getcwd()
        self._filesystem.chdir(self._temp_dir)
        self._filesystem.write_text_file(os.path.join(self._temp_dir, "sorted_file.exp.in"), _sorted_file_contents)
        self._filesystem.write_text_file(os.path.join(self._temp_dir, "non_sorted_file.exp.in"), _non_sorted_file_contents)
        self._filesystem.write_text_file(os.path.join(self._temp_dir, "parse_error_file.exp.in"), _parse_error_file_contents)

    def tearDown(self):
        self._filesystem.rmtree(self._temp_dir)
        self._filesystem.chdir(self._old_cwd)

    def test_sorted(self):
        """ Test sorted file. """

        file_path = os.path.join(self._temp_dir, "sorted_file.exp.in")
        error_handler = MockErrorHandler(handle_style_error)
        error_handler.errors = []
        error_handler.had_error = False
        checker = ExportFileChecker(file_path, error_handler)
        checker.check()
        self.assertFalse(error_handler.had_error)

    def test_non_sorted(self):
        """ Test non sorted file. """

        file_path = os.path.join(self._temp_dir, "non_sorted_file.exp.in")
        error_handler = MockErrorHandler(handle_style_error)
        error_handler.errors = []
        error_handler.had_error = False
        checker = ExportFileChecker(file_path, error_handler)
        checker.check()
        self.assertTrue(error_handler.had_error)
        self.assertEqual(error_handler.errors[0], (0, 'list/order', 5, file_path + " should be sorted, use Tools/Scripts/sort-export-file script"))

    def test_parse_error(self):
        """ Test parse error file. """

        file_path = os.path.join(self._temp_dir, "parse_error_file.exp.in")
        error_handler = MockErrorHandler(handle_style_error)
        error_handler.errors = []
        error_handler.had_error = False
        checker = ExportFileChecker(file_path, error_handler)
        checker.check()
        self.assertTrue(error_handler.had_error)
        self.assertEqual(error_handler.errors[0], (0, 'list/order', 5, "Parse error during processing " + file_path + ", use Tools/Scripts/sort-export-files for details"))
