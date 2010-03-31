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

"""Unit tests for ospath.py."""

import os
import unittest

from webkitpy.common.system.ospath import relpath


# Make sure the tests in this class are platform independent.
class RelPathTest(unittest.TestCase):

    """Tests relpath()."""

    os_path_abspath = lambda self, path: path

    def _rel_path(self, path, abs_start_path):
        return relpath(path, abs_start_path, self.os_path_abspath)

    def test_same_path(self):
        rel_path = self._rel_path("WebKit", "WebKit")
        self.assertEquals(rel_path, "")

    def test_long_rel_path(self):
        start_path = "WebKit"
        expected_rel_path = os.path.join("test", "Foo.txt")
        path = os.path.join(start_path, expected_rel_path)

        rel_path = self._rel_path(path, start_path)
        self.assertEquals(expected_rel_path, rel_path)

    def test_none_rel_path(self):
        """Test _rel_path() with None return value."""
        start_path = "WebKit"
        path = os.path.join("other_dir", "foo.txt")

        rel_path = self._rel_path(path, start_path)
        self.assertTrue(rel_path is None)

        rel_path = self._rel_path("WebKitTools", "WebKit")
        self.assertTrue(rel_path is None)
