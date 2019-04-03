# Copyright (c) 2015, Canon Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/gcor other materials provided with the distribution.
# 3.  Neither the name of Canon Inc. nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import optparse
import unittest

from collections import OrderedDict

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.layout_tests.controllers.layout_test_finder import LayoutTestFinder
from webkitpy.port.test import TestPort


class MockPort(TestPort):
    def __init__(self, host):
        super(MockPort, self).__init__(host)

    def tests(self, paths):
        return [path for path in paths if path.endswith('.html')]


class LayoutTestFinderTests(unittest.TestCase):
    def touched_files(self, touched_files, fs=None):
        host = MockHost()
        if fs:
            host.filesystem = fs
        else:
            fs = host.filesystem
        port = MockPort(host)
        return (fs, LayoutTestFinder(port, optparse.Values({'skipped': 'always', 'skip_failing_tests': False, 'http': True})).find_touched_tests(touched_files))

    def test_touched_test(self):
        paths = ['LayoutTests/test.html', 'LayoutTests/test', 'test2.html', 'Source/test1.html']
        fs, touched_tests = self.touched_files(paths)
        self.assertItemsEqual(touched_tests, ['test.html'])

    def test_expected_touched_test(self):
        paths = ['LayoutTests/test-expected.txt', 'LayoutTests/no-test-expected.txt']
        fs = MockFileSystem()
        fs.write_text_file('/test.checkout/LayoutTests/test.html', 'This is a test')
        fs, touched_tests = self.touched_files(paths, fs)
        self.assertItemsEqual(touched_tests, ['test.html'])

    def test_platform_expected_touched_test(self):
        paths = ['LayoutTests/platform/mock/test-expected.txt', 'LayoutTests/platform/mock/no-test-expected.txt']
        fs = MockFileSystem()
        fs.write_text_file('/test.checkout/LayoutTests/test.html', 'This is a test')
        fs, touched_tests = self.touched_files(paths, fs)
        self.assertItemsEqual(touched_tests, ['test.html'])

    def test_platform_duplicate_touched_test(self):
        paths = ['LayoutTests/test1.html', 'LayoutTests/test1.html', 'LayoutTests/platform/mock1/test2-expected.txt', 'LayoutTests/platform/mock2/test2-expected.txt']
        fs = MockFileSystem()
        fs.write_text_file('/test.checkout/LayoutTests/test2.html', 'This is a test')
        fs, touched_tests = self.touched_files(paths, fs)
        self.assertItemsEqual(touched_tests, ['test1.html', 'test2.html'])

    def test_touched_but_skipped_test(self):
        host = MockHost()
        port = MockPort(host)

        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = 'test1.html [ Skip ]\ntest3.html [ Skip ]\n'
        port.expectations_dict = lambda **kwargs: expectations_dict
        port.test_exists = lambda test: True

        paths = ['LayoutTests/test0.html', 'LayoutTests/test1.html', 'LayoutTests/test2-expected.txt', 'LayoutTests/test3-expected.txt']
        host.filesystem.write_text_file('/test.checkout/LayoutTests/test2.html', 'This is a test to be runned')
        host.filesystem.write_text_file('/test.checkout/LayoutTests/test3.html', 'This is a test to be skipped')

        touched_tests = LayoutTestFinder(port, optparse.Values({'skipped': 'always', 'skip_failing_tests': False, 'http': True})).find_touched_tests(paths)
        self.assertItemsEqual(touched_tests, ['test0.html', 'test2.html'])
