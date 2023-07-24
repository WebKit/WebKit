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
from webkitpy.layout_tests.models.test import Test
from webkitpy.port.base import Port
from webkitpy.w3c.common import TEMPLATED_TEST_HEADER


_log = logging.getLogger(__name__)


# When collecting test cases, we include any file with these extensions.
_supported_test_extensions = set(['.html', '.shtml', '.xml', '.xhtml', '.pl', '.py', '.htm', '.php', '.svg', '.mht', '.xht'])

_skipped_filename_patterns = set([
    # Special case for WebSocket tooling.
    r'.*_wsh.py',

    # The WebKit1 bot sometimes creates these files during the course of testing.
    # https://webkit.org/b/208477
    r'boot\.xml',
    r'root\.xml'
])


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
        self.http_subdir = 'http' + port.TEST_PATH_SEPARATOR + 'test'
        self.websocket_subdir = 'websocket' + port.TEST_PATH_SEPARATOR
        self.web_platform_test_subdir = port.web_platform_test_server_doc_root()
        self.webkit_specific_web_platform_test_subdir = (
            'http' + port.TEST_PATH_SEPARATOR + 'wpt' + port.TEST_PATH_SEPARATOR
        )

    def find_tests(self, options, args, device_type=None):
        paths = self._strip_test_dir_prefixes(args)
        if options and options.test_list:
            paths += self._strip_test_dir_prefixes(self._read_test_names_from_file(options.test_list, self._port.TEST_PATH_SEPARATOR))
        tests = self.find_tests_by_path(paths, device_type=device_type)
        return (paths, tests)

    def find_tests_by_path(self, paths, device_type=None):
        """Return the list of tests found. Both generic and platform-specific tests matching paths should be returned."""
        return [
            Test(
                test_file,
                is_http_test=self.http_subdir in test_file,
                is_websocket_test=self.websocket_subdir in test_file or (self.http_subdir in test_file and 'websocket' in test_file),
                is_wpt_test=self._is_wpt_test(test_file),
                is_wpt_crash_test=self._is_wpt_crash_test(test_file),
            )
            for test_file in self._real_tests(paths)
        ]

    def _is_wpt_test(self, test_file):
        return self.web_platform_test_subdir in test_file or self.webkit_specific_web_platform_test_subdir in test_file

    def _is_wpt_crash_test(self, test_file):
        if not self._is_wpt_test(test_file):
            return False
        filename, _ = self._filesystem.splitext(test_file)
        crashtests_dirname = self._port.TEST_PATH_SEPARATOR + 'crashtests' + self._port.TEST_PATH_SEPARATOR
        return filename.endswith('-crash') or filename.endswith('-crash.tentative') or crashtests_dirname in test_file

    def _expand_variants(self, files):
        expanded = []
        fs = self._port._filesystem
        for f in files:
            if "web-platform-tests" not in f:
                expanded.append(f)
                continue
            f, variant_separator, passed_variant = f.partition('?')
            opened_file = fs.open_text_file_for_reading(f)
            try:
                first_line = opened_file.readline()
                if not first_line:
                    expanded.append(f)
                    continue

                if first_line.strip() == TEMPLATED_TEST_HEADER:
                    variants = []
                    for line in iter(opened_file.readline, ''):
                        results = re.match(r'<!--\s*META:\s*variant=(\S*)\s*-->', line)
                        if not results:
                            continue
                        variant = results.group(1)
                        if self._is_valid_variant(variant):
                            variants.append(variant)
                    if variants:
                        for variant in variants:
                            if not passed_variant or variant.startswith(variant_separator + passed_variant):
                                expanded.append(f + variant)
                    else:
                        expanded.append(f)
                else:
                    variants = []
                    for line in iter(opened_file.readline, ''):
                        try:
                            line = line.lstrip()
                            if not line.startswith("<meta"):
                                continue
                            if not re.search(r"name=['\"]?variant['\"]?", line):
                                continue
                            start_index = line.find("content=")
                            if start_index < 0:
                                continue
                            start_index += 8
                            end_chars = ()
                            if line[start_index] == '"' or line[start_index] == '\'':
                                end_chars = (line[start_index],)
                                start_index += 1
                            else:
                                end_chars = (' ', '>')
                            end_index = start_index
                            while line[end_index] not in end_chars:
                                end_index += 1
                            variant = line[start_index:end_index]
                            if self._is_valid_variant(variant):
                                variants.append(line[start_index:end_index])
                        except IndexError:
                            continue
                    if len(variants):
                        for variant in variants:
                            if not passed_variant or variant.startswith(variant_separator + passed_variant):
                                expanded.append(f + variant)
                    else:
                        expanded.append(f)
            except UnicodeDecodeError:
                expanded.append(f)
                continue
        return expanded

    def _is_valid_variant(self, variant):
        return variant == "" or (len(variant) > 1 and variant[0] in ("?", "#")) and variant != "?#"

    def _real_tests(self, paths):
        # When collecting test cases, skip these directories
        skipped_directories = set(['.svn', '_svn', 'resources', 'support', 'script-tests', 'tools', 'reference', 'reftest'])
        files = find_files.find(self._port._filesystem, self._port.layout_tests_dir(), paths, skipped_directories, self._is_test_file, self._port.test_key)
        files = self._expand_variants(files)
        return [self._port.relative_test_filename(f) for f in files]

    def _is_test_file(self, filesystem, dirname, filename):
        if not _has_supported_extension(filesystem, filename):
            return False
        if _is_reference_html_file(filesystem, dirname, filename):
            return False
        if self._is_w3c_resource_file(filesystem, dirname, filename):
            return False

        for pattern in _skipped_filename_patterns:
            if re.match(pattern, filename):
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
