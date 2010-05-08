# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import unittest
import webkit
import StringIO


# Before Python 2.7, StringIO does not work with "with"
# http://bugs.python.org/issue1286
class WithAwareStringIO(StringIO.StringIO):
    # Context management protocol
    def __enter__(self):
        if self.closed:
            raise ValueError("I/O operation on closed file")
        return self

    def __exit__(self, exc, value, tb):
        self.close()


class WebKitTest(unittest.TestCase):
    def _assert_configuration(self, file_contents, expected_configuration):
        port = webkit.WebKitPort()
        port._open_configuration_file = lambda: WithAwareStringIO(file_contents)
        self.assertEqual(port.default_configuration(), expected_configuration)

    def test_default_configuration(self):
        self._assert_configuration("", "Release")
        self._assert_configuration("Debug", "Debug")
        self._assert_configuration("Release", "Release")
        self._assert_configuration("Debug\nRelease", "Debug")
        self._assert_configuration("\nDebug", "Release")

        # Make sure we handle missing configuration files too.
        port = webkit.WebKitPort()
        def mock_open():
            raise IOError
        port._open_configuration_file = mock_open
        self.assertEqual(port.default_configuration(), "Release")
