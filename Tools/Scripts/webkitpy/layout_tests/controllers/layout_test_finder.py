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
import json
import logging
import re

from webkitpy.common import find_files
from webkitpy.layout_tests.models import test_expectations
from webkitpy.port.base import Port


_log = logging.getLogger(__name__)


# When collecting test cases, we include any file with these extensions.
_supported_test_extensions = set(['.html', '.shtml', '.xml', '.xhtml', '.pl', '.py', '.htm', '.php', '.svg', '.mht', '.xht'])


# If any changes are made here be sure to update the isUsedInReftest method in old-run-webkit-tests as well.
def _is_reference_html_file(filesystem, dirname, filename):
    if filename.startswith('ref-') or filename.startswith('notref-'):
        return True
    filename_wihout_ext, ext = filesystem.splitext(filename)
    # FIXME: _supported_reference_extensions should be here, https://bugs.webkit.org/show_bug.cgi?id=220421
    if ext not in Port._supported_reference_extensions:
        return False
    for suffix in ['-expected', '-expected-mismatch', '-ref', '-notref']:
        if filename_wihout_ext.endswith(suffix):
            return True
    return False


def _has_supported_extension(filesystem, filename):
    """Return true if filename is one of the file extensions we want to run a test on."""
    extension = filesystem.splitext(filename)[1]
    return extension in _supported_test_extensions


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
        self._w3c_resource_files = None

    def find_tests(self, options, args, device_type=None):
        paths = self._strip_test_dir_prefixes(args)
        if options and options.test_list:
            paths += self._strip_test_dir_prefixes(self._read_test_names_from_file(options.test_list, self._port.TEST_PATH_SEPARATOR))
        test_files = self.find_tests_by_path(paths, device_type=device_type)
        return (paths, test_files)

    def find_tests_by_path(self, paths, device_type=None):
        """Return the list of tests found. Both generic and platform-specific tests matching paths should be returned."""
        expanded_paths = self._expanded_paths(paths, device_type=device_type)
        return self._real_tests(expanded_paths)

    def _expanded_paths(self, paths, device_type=None):
        expanded_paths = []
        fs = self._port._filesystem
        all_platform_dirs = [path for path in fs.glob(fs.join(self._port.layout_tests_dir(), 'platform', '*')) if fs.isdir(path)]
        for path in paths:
            expanded_paths.append(path)
            if self._port.test_isdir(path) and not path.startswith('platform') and not fs.isabs(path):
                for platform_dir in all_platform_dirs:
                    if fs.isdir(fs.join(platform_dir, path)) and platform_dir in self._port.baseline_search_path(device_type=device_type):
                        expanded_paths.append(self._port.relative_test_filename(fs.join(platform_dir, path)))

        return expanded_paths

    def _real_tests(self, paths):
        # When collecting test cases, skip these directories
        skipped_directories = set(['.svn', '_svn', 'resources', 'support', 'script-tests', 'tools', 'reference', 'reftest'])
        files = find_files.find(self._port._filesystem, self._port.layout_tests_dir(), paths, skipped_directories, self._is_test_file, self._port.test_key)
        return [self._port.relative_test_filename(f) for f in files]

    def _is_test_file(self, filesystem, dirname, filename):
        if not _has_supported_extension(filesystem, filename):
            return False
        if _is_reference_html_file(filesystem, dirname, filename):
            return False
        if self._is_w3c_resource_file(filesystem, dirname, filename):
            return False
        # Special case for websocket tooling
        if filename.endswith('_wsh.py'):
            return False
        return True

    def _is_w3c_resource_file(self, filesystem, dirname, filename):
        path = filesystem.join(dirname, filename)
        w3c_path = filesystem.join(self._port.layout_tests_dir(), "imported", "w3c")
        if w3c_path not in path:
            return False

        if not self._w3c_resource_files:
            filepath = filesystem.join(w3c_path, "resources", "resource-files.json")
            json_data = filesystem.read_text_file(filepath)
            self._w3c_resource_files = json.loads(json_data)

        _, extension = filesystem.splitext(filename)
        if extension == '.py':
            return True

        subpath = path[len(w3c_path) + 1:].replace('\\', '/')
        if subpath in self._w3c_resource_files["files"]:
            return True
        for dirpath in self._w3c_resource_files["directories"]:
            if dirpath in subpath:
                return True
        return False

    def find_touched_tests(self, new_or_modified_paths, apply_skip_expectations=True):
        potential_test_paths = []
        for test_file in new_or_modified_paths:
            if not test_file.startswith(self.LAYOUT_TESTS_DIRECTORY):
                continue

            test_file = self._strip_test_dir_prefix(test_file)
            test_paths = self._port.potential_test_names_from_expected_file(test_file)
            if test_paths:
                potential_test_paths.extend(test_paths)
            else:
                potential_test_paths.append(test_file)

        if not potential_test_paths:
            return None

        tests = self.find_tests_by_path(list(set(potential_test_paths)))
        if not apply_skip_expectations:
            return tests

        expectations = test_expectations.TestExpectations(self._port, tests, force_expectations_pass=False)
        expectations.parse_all_expectations()
        tests_to_skip = self.skip_tests(potential_test_paths, tests, expectations, None)
        return [test for test in tests if test not in tests_to_skip]

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
        for filename in filenames:
            try:
                if test_path_separator != fs.sep:
                    filename = filename.replace(test_path_separator, fs.sep)
                file_contents = fs.read_text_file(filename).split('\n')
                for line in file_contents:
                    line = self._strip_comments(line)
                    if line:
                        tests.append(line)
            except IOError as e:
                if e.errno == errno.ENOENT:
                    _log.critical('')
                    _log.critical('--test-list file "{}" not found'.format(filenames))
                raise
        return tests

    @staticmethod
    def _strip_comments(line):
        commentIndex = line.find('//')
        if commentIndex == -1:
            commentIndex = len(line)

        line = re.sub(r'\s+', ' ', line[:commentIndex].strip())
        if line == '':
            return None
        else:
            return line

    def skip_tests(self, paths, all_tests_list, expectations, http_tests):
        all_tests = set(all_tests_list)

        tests_to_skip = expectations.model().get_tests_with_result_type(test_expectations.SKIP)
        if self._options.skip_failing_tests:
            tests_to_skip.update(expectations.model().get_tests_with_result_type(test_expectations.FAIL))
            tests_to_skip.update(expectations.model().get_tests_with_result_type(test_expectations.FLAKY))

        if self._options.skipped == 'only':
            tests_to_skip = all_tests - tests_to_skip
        elif self._options.skipped == 'ignore':
            tests_to_skip = set()

        # unless of course we don't want to run the HTTP tests :)
        if not self._options.http:
            tests_to_skip.update(set(http_tests))

        return tests_to_skip

    def split_into_chunks(self, test_names):
        """split into a list to run and a set to skip, based on --run-chunk and --run-part."""
        if not self._options.run_chunk and not self._options.run_part:
            return test_names, set()

        # If the user specifies they just want to run a subset of the tests,
        # just grab a subset of the non-skipped tests.
        chunk_value = self._options.run_chunk or self._options.run_part
        try:
            (chunk_num, chunk_len) = chunk_value.split(":")
            chunk_num = int(chunk_num)
            assert(chunk_num >= 0)
            test_size = int(chunk_len)
            assert(test_size > 0)
        except AssertionError:
            _log.critical("invalid chunk '%s'" % chunk_value)
            return (None, None)

        # Get the number of tests
        num_tests = len(test_names)

        # Get the start offset of the slice.
        if self._options.run_chunk:
            chunk_len = test_size
            # In this case chunk_num can be really large. We need
            # to make the worker fit in the current number of tests.
            slice_start = (chunk_num * chunk_len) % num_tests
        else:
            # Validate the data.
            assert(test_size <= num_tests)
            assert(chunk_num <= test_size)

            # To count the chunk_len, and make sure we don't skip
            # some tests, we round to the next value that fits exactly
            # all the parts.
            rounded_tests = num_tests
            if rounded_tests % test_size != 0:
                rounded_tests = (num_tests + test_size - (num_tests % test_size))

            chunk_len = rounded_tests // test_size
            slice_start = chunk_len * (chunk_num - 1)
            # It does not mind if we go over test_size.

        # Get the end offset of the slice.
        slice_end = min(num_tests, slice_start + chunk_len)

        tests_to_run = test_names[slice_start:slice_end]

        _log.debug('chunk slice [%d:%d] of %d is %d tests' % (slice_start, slice_end, num_tests, (slice_end - slice_start)))

        # If we reached the end and we don't have enough tests, we run some
        # from the beginning.
        if slice_end - slice_start < chunk_len:
            extra = chunk_len - (slice_end - slice_start)
            _log.debug('   last chunk is partial, appending [0:%d]' % extra)
            tests_to_run.extend(test_names[0:extra])

        return (tests_to_run, set(test_names) - set(tests_to_run))
