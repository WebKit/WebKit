# Copyright (C) 2012 Google Inc. All rights reserved.
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

import errno
import logging
import re

from .layout_test_finder import (
    LayoutTestFinder as LayoutTestFinder_New,
    skipped_test_suffixes,
)

_log = logging.getLogger(__name__)


def _is_reference_html_file(filesystem, dirname, filename):
    return filename.startswith(("ref-", "notref-")) or filename.endswith(
        skipped_test_suffixes
    )


class LayoutTestFinder(object):
    """Finds LayoutTests

    We consider any file will a given set of extensions as tests, except for
    those which appear to be references (-expected.html, etc.). Notably this
    means that a test _doesn't_ need to have any associated -expected.* file
    (in those cases, we will report the missing result).
    """

    def __init__(self, port, options):
        # FIXME: we should minimize/eliminate usage of the port, https://bugs.webkit.org/show_bug.cgi?id=220421
        self._port = port
        self._options = options
        self._filesystem = self._port.host.filesystem
        self.LAYOUT_TESTS_DIRECTORY = 'LayoutTests'
        self._test_list_expectations = []

    def find_tests(self, options, args, device_type=None, with_expectations=False):
        paths = self._strip_test_dir_prefixes(args)
        if options and options.test_list:
            test_names, expectations = self._read_test_names_from_file(options.test_list, self._port.TEST_PATH_SEPARATOR)
            paths += self._strip_test_dir_prefixes(test_names)
            self._test_list_expectations = expectations
        tests = self.find_tests_by_path(paths, device_type=device_type, with_expectations=with_expectations)
        return (paths, tests)

    def find_tests_for_specified_files(self, options, args):
        """Find tests for explicitly specified individual test files only.

        This filters out directories and unexpanded glob patterns (containing *, ?, [, ]).
        Use this when you want to find tests that were explicitly specified by the user
        as individual files (either typed directly or expanded by the shell).
        """
        paths = self._strip_test_dir_prefixes(args)
        if options and options.test_list:
            test_names, _ = self._read_test_names_from_file(options.test_list, self._port.TEST_PATH_SEPARATOR)
            paths += self._strip_test_dir_prefixes(test_names)

        layout_tests_dir = self._port.layout_tests_dir()
        paths = [p for p in paths
                 if not any(c in p for c in ['*', '?', '[', ']'])
                 and not self._filesystem.isdir(self._filesystem.join(layout_tests_dir, p))]

        if not paths:
            return ([], [])

        tests = self.find_tests_by_path(paths)
        return (paths, tests)

    def find_tests_by_path(self, paths, device_type=None, with_expectations=False):
        """Return the list of tests found. Both generic and platform-specific tests matching paths should be returned."""
        finder = LayoutTestFinder_New(
            self._port.host.filesystem,
            self._port.layout_tests_dir(),
            self._port.baseline_search_path(device_type),
        )

        return list(finder.get_tests(paths))

    def _is_test_file(self, filesystem, dirname, filename):
        finder = LayoutTestFinder_New(
            self._port.host.filesystem,
            self._port.layout_tests_dir(),
            self._port.baseline_search_path(),
        )
        return finder.is_test_file(
            dirname, filename
        ) and not self._is_w3c_resource_file(filesystem, dirname, filename)

    def _is_w3c_resource_file(self, filesystem, dirname, filename):
        finder = LayoutTestFinder_New(
            self._port.host.filesystem,
            self._port.layout_tests_dir(),
            self._port.baseline_search_path(),
        )

        if dirname:
            dirname = self._filesystem.relpath(dirname, self._port.layout_tests_dir())

        w3c_path = filesystem.join("imported", "w3c")
        if dirname.startswith(w3c_path) and filename.endswith(".py"):
            # For legacy compatibility handle this here.
            return True

        if filename in finder.w3c_support_files.get(dirname, set()):
            return True

        prev = None
        head, tail = self._filesystem.split(dirname)
        while head and head != prev:
            if tail in finder.w3c_support_dirs.get(head, set()):
                return True
            prev = head
            head, tail = self._filesystem.split(head)

        return False

    def _strip_test_dir_prefixes(self, paths):
        return [self._strip_test_dir_prefix(path) for path in paths if path]

    def _strip_test_dir_prefix(self, path):
        # Handle both "LayoutTests/foo/bar.html" and "LayoutTests\foo\bar.html" if
        # the filesystem uses '\\' as a directory separator.
        if path.startswith(self.LAYOUT_TESTS_DIRECTORY + self._port.TEST_PATH_SEPARATOR):
            return path[len(self.LAYOUT_TESTS_DIRECTORY + self._port.TEST_PATH_SEPARATOR):]
        if path.startswith(self.LAYOUT_TESTS_DIRECTORY + self._filesystem.sep):
            return path[len(self.LAYOUT_TESTS_DIRECTORY + self._filesystem.sep):]
        return path

    def _read_test_names_from_file(self, filenames, test_path_separator):
        fs = self._filesystem
        tests = []
        expectations = []
        for filename in filenames:
            if test_path_separator != fs.sep:
                filename = filename.replace(test_path_separator, fs.sep)
            file_contents = fs.read_text_file(filename).split('\n')
            for line_number, line in enumerate(file_contents, 1):
                content = self._strip_test_list_comments(line)
                if not content:
                    continue

                if '[' in content:
                    # Line has inline expectations (TestExpectations syntax)
                    expectations.append((filename, line_number, content))
                    test_name = content[:content.index('[')].strip()
                    if test_name:
                        tests.append(test_name)
                else:
                    tests.append(content)
        return tests, expectations

    def get_test_list_expectations(self):
        return self._test_list_expectations

    def get_test_list_skip_paths(self):
        skip_paths = set()
        for _filename, _line_number, raw_line in self._test_list_expectations:
            match = re.search(r'\[([^\]]+)\]', raw_line)
            if match and 'SKIP' in match.group(1).upper().split():
                test_name = raw_line[:raw_line.index('[')].strip()
                if test_name:
                    skip_paths.add(test_name)
        return skip_paths

    @staticmethod
    def _strip_test_list_comments(line):
        # Strip // comments by finding the position and truncating.
        comment_index = line.find('//')
        if comment_index != -1:
            line = line[:comment_index]
        # Strip # comments.
        comment_index = line.find('#')
        if comment_index != -1:
            line = line[:comment_index]
        line = re.sub(r'\s+', ' ', line.strip())
        return line if line else None
