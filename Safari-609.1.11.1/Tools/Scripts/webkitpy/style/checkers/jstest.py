# Copyright (C) 2017 Apple Inc. All rights reserved.
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

"""Supports ensuring equality of js-test-pre.js and related files."""

import re

from webkitpy.common.system.systemhost import SystemHost

ALL_JS_TEST_FUNCTION_FILES = [
    'JSTests/stress/resources/standalone-pre.js',
    'LayoutTests/http/tests/resources/js-test-pre.js',
    'LayoutTests/resources/js-test-pre.js',
    'LayoutTests/resources/js-test.js',
    'LayoutTests/resources/standalone-pre.js',
]

KEEP_JS_TEST_FILES_IN_SYNC = [
    ['LayoutTests/http/tests/resources/js-test-pre.js',
     'LayoutTests/resources/js-test-pre.js'],
    ['LayoutTests/http/tests/resources/js-test-post.js',
     'LayoutTests/resources/js-test-post.js'],
    ['LayoutTests/http/tests/resources/js-test-post-async.js',
     'LayoutTests/resources/js-test-post-async.js'],
]

KEEP_JS_TEST_FUNCTIONS_IN_SYNC = [
    'shouldBe',
    'shouldNotBe',
    'shouldNotThrow',
    'shouldThrow',
]


def map_functions_to_dict(content):
    """Splits multi-line string containing JavaScript source into a dictionary.

    The dictionary uses the function name as a key, and the function source (less the "function " keyword) as the value.

    Args:
      content: A multi-line string containing JavaScript source to be split into individual function definitions.
    """
    functions = re.split('^function\s+', content, flags=re.MULTILINE)
    function_name_regex = re.compile('^(?P<name>\w+)\s*\(', flags=re.MULTILINE)
    result = {}
    for f in functions:
        match = function_name_regex.match(f)
        if match:
            result[match.group('name')] = strip_trailing_blank_lines_and_comments(f)
    return result


def strip_trailing_blank_lines_and_comments(function):
    """Removes blank lines and lines containing only comments from the end of a multi-line string.

    Args:
        function: A multi-line string representing the source for one JavaScript function, less the "function" keyword.
    """
    lines = function.splitlines(True)
    blank_line_regex = re.compile('^\s*$')
    comment_line_regex = re.compile('^\s*//.*$')
    while blank_line_regex.search(lines[-1]) or comment_line_regex.search(lines[-1]):
        del lines[-1]
    return ''.join(lines)


class JSTestChecker(object):
    categories = {'jstest/function_equality', 'jstest/resource_equality'}

    def __init__(self, file_path, handle_style_error):
        self._file_path = file_path
        self._handle_style_error = handle_style_error
        self._handle_style_error.turn_off_line_filtering()
        self._host = SystemHost()
        self._fs = self._host.filesystem

    def check(self, lines):
        """Run all the checks."""
        for file_group in KEEP_JS_TEST_FILES_IN_SYNC:
            if self._file_path in file_group:
                self.check_js_test_files(file_group)

        if self._file_path in ALL_JS_TEST_FUNCTION_FILES:
            self.check_js_test_functions()

    def check_js_test_files(self, file_group):
        """Test that files in 'file_group' are identical."""
        with self._fs.open_binary_file_for_reading(self._file_path) as file_handle:
            baseline_content = file_handle.read()

        other_files = file_group
        other_files.remove(self._file_path)

        for path in other_files:
            with self._fs.open_binary_file_for_reading(path) as file_handle:
                test_content = file_handle.read()
                if baseline_content != test_content:
                    error_message = "Changes should be kept in sync with {0}.".format(path)
                    self._handle_style_error(0, 'jstest/resource_equality', 5, error_message)

    def check_js_test_functions(self):
        """Test that functions in KEEP_JS_TEST_FUNCTIONS_IN_SYNC are identical."""
        with self._fs.open_binary_file_for_reading(self._file_path) as file_handle:
            baseline_content = file_handle.read()
        baseline_function_map = map_functions_to_dict(baseline_content)

        other_files = ALL_JS_TEST_FUNCTION_FILES
        other_files.remove(self._file_path)

        for path in other_files:
            with self._fs.open_binary_file_for_reading(path) as file_handle:
                test_content = file_handle.read()
            test_function_map = map_functions_to_dict(test_content)

            for function_name in KEEP_JS_TEST_FUNCTIONS_IN_SYNC:
                if function_name in baseline_function_map.keys() and function_name in test_function_map.keys():
                    if baseline_function_map[function_name] != test_function_map[function_name]:
                        error_message = "Changes to function {0}() should be kept in sync with {1}.".format(
                            function_name, path)
                        self._handle_style_error(0, 'jstest/function_equality', 5, error_message)
