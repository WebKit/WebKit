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

"""Unit tests for main.py."""

import os
import unittest

from main import change_directory
from webkitpy.style_references import LogTesting


class ChangeDirectoryTest(unittest.TestCase):

    """Tests change_directory()."""

    _original_directory = "/original"
    _checkout_root = "/WebKit"

    class _MockOs(object):

        """A mock os module for unit testing."""

        def __init__(self, test_case):
            self._test_case = test_case
            self._current_directory = \
                ChangeDirectoryTest._original_directory

        def chdir(self, current_directory):
            self._current_directory = current_directory

        def assertCurrentDirectory(self, expected_directory):
            self._test_case.assertEquals(expected_directory,
                                         self._current_directory)

    def setUp(self):
        self._log = LogTesting.setUp(self)
        self._mock_os = self._MockOs(self)

    def tearDown(self):
        self._log.tearDown()

    # This method is a convenient wrapper for change_working_directory() that
    # passes the mock_os for this unit testing class.
    def _change_directory(self, paths, checkout_root):
        return change_directory(paths=paths,
                                checkout_root=checkout_root,
                                mock_os=self._mock_os)

    def _assert_result(self, actual_return_value, expected_return_value,
                       expected_log_messages, expected_current_directory):
        self.assertEquals(actual_return_value, expected_return_value)
        self._log.assertMessages(expected_log_messages)
        self._mock_os.assertCurrentDirectory(expected_current_directory)

    def test_checkout_root_none_paths_none(self):
        self.assertRaises(Exception, self._change_directory,
                          checkout_root=None, paths=None)
        self._log.assertMessages([])
        self._mock_os.assertCurrentDirectory(self._original_directory)

    def test_checkout_root_none(self):
        paths = self._change_directory(checkout_root=None,
                                       paths=["path1"])
        log_messages = [
"""WARNING: WebKit checkout root not found:
  Path-dependent style checks may not work correctly.
  See the help documentation for more info.
"""]
        self._assert_result(paths, ["path1"], log_messages,
                            self._original_directory)

    def test_paths_none(self):
        paths = self._change_directory(checkout_root=self._checkout_root,
                                       paths=None)
        self._assert_result(paths, None, [], self._checkout_root)

    def test_paths_convertible(self):
        paths=["/WebKit/foo1.txt",
               "/WebKit/foo2.txt"]
        paths = self._change_directory(checkout_root=self._checkout_root,
                                       paths=paths)
        self._assert_result(paths, ["foo1.txt", "foo2.txt"], [],
                            self._checkout_root)

    def test_with_scm_paths_unconvertible(self):
        paths=["/WebKit/foo1.txt",
               "/outside/foo2.txt"]
        paths = self._change_directory(checkout_root=self._checkout_root,
                                       paths=paths)
        log_messages = [
"""WARNING: Path-dependent style checks may not work correctly:

  One of the given paths is outside the WebKit checkout of the current
  working directory:

    Path: /outside/foo2.txt
    Checkout root: /WebKit

  Pass only files below the checkout root to ensure correct results.
  See the help documentation for more info.

"""]
        self._assert_result(paths, paths, log_messages,
                            self._original_directory)
