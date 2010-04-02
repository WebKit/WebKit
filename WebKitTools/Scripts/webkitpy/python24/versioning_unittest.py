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

"""Contains unit tests for versioning.py."""

import logging
import unittest

from webkitpy.common.system.logtesting import LogTesting
from webkitpy.python24.versioning import check_version
from webkitpy.python24.versioning import compare_version

class MockSys(object):

    """A mock sys module for passing to version-checking methods."""

    def __init__(self, current_version):
        """Create an instance.

        current_version: A version string with major, minor, and micro
                         version parts.

        """
        version_info = current_version.split(".")
        version_info = map(int, version_info)

        self.version = current_version + " Version details."
        self.version_info = version_info


class CompareVersionTest(unittest.TestCase):

    """Tests compare_version()."""

    def _mock_sys(self, current_version):
        return MockSys(current_version)

    def test_default_minimum_version(self):
        """Test the configured minimum version that webkitpy supports."""
        (comparison, current_version, min_version) = compare_version()
        self.assertEquals(min_version, "2.5")

    def compare_version(self, target_version, current_version=None):
        """Call compare_version()."""
        if current_version is None:
            current_version = "2.5.3"
        mock_sys = self._mock_sys(current_version)
        return compare_version(mock_sys, target_version)

    def compare(self, target_version, current_version=None):
        """Call compare_version(), and return the comparison."""
        return self.compare_version(target_version, current_version)[0]

    def test_returned_current_version(self):
        """Test the current_version return value."""
        current_version = self.compare_version("2.5")[1]
        self.assertEquals(current_version, "2.5.3")

    def test_returned_target_version(self):
        """Test the current_version return value."""
        target_version = self.compare_version("2.5")[2]
        self.assertEquals(target_version, "2.5")

    def test_target_version_major(self):
        """Test major version for target."""
        self.assertEquals(-1, self.compare("3"))
        self.assertEquals(0, self.compare("2"))
        self.assertEquals(1, self.compare("2", "3.0.0"))

    def test_target_version_minor(self):
        """Test minor version for target."""
        self.assertEquals(-1, self.compare("2.6"))
        self.assertEquals(0, self.compare("2.5"))
        self.assertEquals(1, self.compare("2.4"))

    def test_target_version_micro(self):
        """Test minor version for target."""
        self.assertEquals(-1, self.compare("2.5.4"))
        self.assertEquals(0, self.compare("2.5.3"))
        self.assertEquals(1, self.compare("2.5.2"))


class CheckVersionTest(unittest.TestCase):

    """Tests check_version()."""

    def setUp(self):
        self._log = LogTesting.setUp(self)

    def tearDown(self):
        self._log.tearDown()

    def _check_version(self, minimum_version):
        """Call check_version()."""
        mock_sys = MockSys("2.5.3")
        return check_version(sysmodule=mock_sys, target_version=minimum_version)

    def test_true_return_value(self):
        """Test the configured minimum version that webkitpy supports."""
        is_current = self._check_version("2.4")
        self.assertEquals(True, is_current)
        self._log.assertMessages([])  # No warning was logged.

    def test_false_return_value(self):
        """Test the configured minimum version that webkitpy supports."""
        is_current = self._check_version("2.6")
        self.assertEquals(False, is_current)
        expected_message = ('WARNING: WebKit Python scripts do not support '
                            'your current Python version (2.5.3).  '
                            'The minimum supported version is 2.6.\n  '
                            'See the following page to upgrade your Python '
                            'version:\n\n    '
                            'http://trac.webkit.org/wiki/PythonGuidelines\n\n')
        self._log.assertMessages([expected_message])

