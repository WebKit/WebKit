# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import logging
import optparse
import unittest

import webkitpy.style.checker as checker

from webkitpy.common.host import Host
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.logtesting import LogTesting
from webkitpy.style.checker import StyleProcessor
from webkitpy.style.filereader import TextFileReader
from webkitpy.style.main import change_directory

from webkitcorepy import OutputCapture


class ChangeDirectoryTest(unittest.TestCase):
    _original_directory = "/original"
    _checkout_root = "/WebKit"

    def setUp(self):
        self._log = LogTesting.setUp(self)
        self.filesystem = MockFileSystem(dirs=[self._original_directory, self._checkout_root], cwd=self._original_directory)

    def tearDown(self):
        self._log.tearDown()

    def _change_directory(self, paths, checkout_root):
        return change_directory(self.filesystem, paths=paths, checkout_root=checkout_root)

    def _assert_result(self, actual_return_value, expected_return_value,
                       expected_log_messages, expected_current_directory):
        self.assertEqual(actual_return_value, expected_return_value)
        self._log.assertMessages(expected_log_messages)
        self.assertEqual(self.filesystem.getcwd(), expected_current_directory)

    def test_paths_none(self):
        paths = self._change_directory(checkout_root=self._checkout_root, paths=None)
        self._assert_result(paths, None, [], self._checkout_root)

    def test_paths_convertible(self):
        paths = ["/WebKit/foo1.txt", "/WebKit/foo2.txt"]
        paths = self._change_directory(checkout_root=self._checkout_root, paths=paths)
        self._assert_result(paths, ["foo1.txt", "foo2.txt"], [], self._checkout_root)

    def test_with_scm_paths_unconvertible(self):
        paths = ["/WebKit/foo1.txt", "/outside/foo2.txt"]
        paths = self._change_directory(checkout_root=self._checkout_root, paths=paths)
        log_messages = [
"""WARNING: Path-dependent style checks may not work correctly:

  One of the given paths is outside the WebKit checkout of the current
  working directory:

    Path: /outside/foo2.txt
    Checkout root: /WebKit

  Pass only files below the checkout root to ensure correct results.
  See the help documentation for more info.

"""]
        self._assert_result(paths, paths, log_messages, self._original_directory)


class ExpectationLinterInStyleCheckerTest(unittest.TestCase):

    def setUp(self):
        parser = checker.check_webkit_style_parser()
        (_, options) = parser.parse([])
        self._style_checker_configuration = checker.check_webkit_style_configuration(options)

    def _generate_file_reader(self, file_system):
        style_processor = StyleProcessor(self._style_checker_configuration)
        return TextFileReader(file_system, style_processor)

    def _generate_testing_host(self, files={}):
        host = Host()
        expectation_files = files

        host.filesystem = MockFileSystem(dirs=['/mock-checkout/LayoutTests'])
        options = optparse.Values()
        setattr(options, 'layout_tests_dir', '/mock-checkout/LayoutTests')

        all_ports = [host.port_factory.get(name, options=options) for name in host.port_factory.all_port_names()]
        for port in all_ports:
            for path in port.expectations_files():
                if path not in expectation_files:
                    expectation_files[path] = '# Empty expectation file\n'

        expectation_files['/mock-checkout/LayoutTests/css1/test.html'] = 'Test'
        expectation_files['/mock-checkout/LayoutTests/css1/test-expected.txt'] = 'Test Expectation'
        host.filesystem = MockFileSystem(files=expectation_files)
        return host

    def test_no_linter_errors(self):
        host = self._generate_testing_host()

        with OutputCapture(level=logging.ERROR) as captured:
            file_reader = self._generate_file_reader(host.filesystem)
            file_reader.do_association_check('/mock-checkout', host)
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue(), '')

    def test_linter_duplicate_line(self):
        files = {
            '/mock-checkout/LayoutTests/TestExpectations':
            '# TestExpectations\ncss1/test.html [ Failure ]\ncss1/test.html [ Failure ]\n'}
        host = self._generate_testing_host(files)

        with OutputCapture(level=logging.ERROR) as captured:
            file_reader = self._generate_file_reader(host.filesystem)
            file_reader.do_association_check('/mock-checkout', host)
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue(), '')

        with OutputCapture(level=logging.ERROR) as captured:
            file_reader = self._generate_file_reader(host.filesystem)
            file_reader.process_file('/mock-checkout/LayoutTests/TestExpectations', line_numbers=[1, 2, 3])
            file_reader.do_association_check('/mock-checkout', host)
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.root.log.getvalue(),
            '/mock-checkout/LayoutTests/TestExpectations:3:  Duplicate or ambiguous entry lines LayoutTests/TestExpectations:2 and LayoutTests/TestExpectations:3.  [test/expectations] [5]\n',
        )

    def test_linter_duplicate_line_no_edit(self):
        files = {
            '/mock-checkout/LayoutTests/TestExpectations':
            '# TestExpectations\ncss1/test.html [ Failure ]\ncss1/test.html [ Failure ]\n'}
        host = self._generate_testing_host(files)

        with OutputCapture(level=logging.ERROR) as captured:
            file_reader = self._generate_file_reader(host.filesystem)
            file_reader.process_paths(['/mock-checkout/LayoutTests/platform/ios/TestExpectations'])
            file_reader.do_association_check('/mock-checkout', host)
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue(), '')

    def test_linter_duplicate_line_edit_in_file(self):
        files = {
            '/mock-checkout/LayoutTests/TestExpectations':
            '# TestExpectations\ncss1/test.html [ Failure ]\ncss1/test.html [ Failure ]\n'}
        host = self._generate_testing_host(files)

        with OutputCapture(level=logging.ERROR) as captured:
            file_reader = self._generate_file_reader(host.filesystem)
            file_reader.process_file('/mock-checkout/LayoutTests/TestExpectations', line_numbers=[1])
            file_reader.do_association_check('/mock-checkout', host)
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue(), '')

    def test_linter_duplicate_line_only_deletes(self):
        files = {
            '/mock-checkout/LayoutTests/TestExpectations':
                '# TestExpectations\ncss1/test.html [ Failure ]\ncss1/test.html [ Failure ]\n'}
        host = self._generate_testing_host(files)

        with OutputCapture(level=logging.ERROR) as captured:
            file_reader = self._generate_file_reader(host.filesystem)
            file_reader.process_file('/mock-checkout/LayoutTests/TestExpectations')
            file_reader.do_association_check('/mock-checkout', host)
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue(), '')

    def test_linter_added_file_with_error(self):
        files = {
            '/mock-checkout/LayoutTests/TestExpectations':
            '# TestExpectations\ncss1/test.html [ Failure ]\ncss1/test.html [ Failure ]\n'}
        host = self._generate_testing_host(files)

        with OutputCapture(level=logging.ERROR) as captured:
            file_reader = self._generate_file_reader(host.filesystem)
            file_reader.process_file('/mock-checkout/LayoutTests/TestExpectations', line_numbers=[1, 2, 3])
            file_reader.do_association_check('/mock-checkout', host)
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.root.log.getvalue(),
            '/mock-checkout/LayoutTests/TestExpectations:3:  Duplicate or ambiguous entry lines LayoutTests/TestExpectations:2 and LayoutTests/TestExpectations:3.  [test/expectations] [5]\n',
        )

    def test_linter_deleted_file(self):
        files = {
            '/mock-checkout/LayoutTests/TestExpectations':
            '# TestExpectations\ncss1/deleted-test.html [ Failure ]\n'}
        host = self._generate_testing_host(files)

        with OutputCapture(level=logging.ERROR) as captured:
            file_reader = self._generate_file_reader(host.filesystem)
            file_reader.delete_file('/mock-checkout/LayoutTests/css1/deleted-test.html')
            file_reader.do_association_check('/mock-checkout', host)
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.root.log.getvalue(),
            '/mock-checkout/LayoutTests/TestExpectations:2:  Path does not exist.  [test/expectations] [5]\n',
        )

    def test_linter_deleted_file_no_edit(self):
        files = {
            '/mock-checkout/LayoutTests/TestExpectations':
            '# TestExpectations\ncss1/deleted-test.html [ Failure ]\n'}
        host = self._generate_testing_host(files)

        with OutputCapture(level=logging.ERROR) as captured:
            file_reader = self._generate_file_reader(host.filesystem)
            file_reader.delete_file('/mock-checkout/LayoutTests/css1/other-deleted-test.html')
            file_reader.do_association_check('/mock-checkout', host)
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue(), '')
