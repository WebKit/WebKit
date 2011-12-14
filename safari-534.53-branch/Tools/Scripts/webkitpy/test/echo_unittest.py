# Copyright (C) 2010 Apple Inc. All rights reserved.
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

import os.path
import sys
import unittest

from webkitpy.common.system import executive, outputcapture
from webkitpy.test import echo


class EchoTest(outputcapture.OutputCaptureTestCaseBase):
    def test_basic(self):
        echo.main(['foo', 'bar', 'baz'])
        self.assertStdout('foo bar baz\n')

    def test_no_newline(self):
        echo.main(['-n', 'foo', 'bar', 'baz'])
        self.assertStdout('foo bar baz')

    def test_unicode(self):
        echo.main([u'WebKit \u2661', 'Tor Arne', u'Vestb\u00F8!'])
        self.assertStdout(u'WebKit \u2661 Tor Arne Vestb\u00F8!\n')

    def test_argument_order(self):
        echo.main(['foo', '-n', 'bar'])
        self.assertStdout('foo -n bar\n')

    def test_empty_arguments(self):
        old_argv = sys.argv
        sys.argv = ['echo.py', 'foo', 'bar', 'baz']
        echo.main([])
        self.assertStdout('\n')
        sys.argv = old_argv

    def test_no_arguments(self):
        old_argv = sys.argv
        sys.argv = ['echo.py', 'foo', 'bar', 'baz']
        echo.main()
        self.assertStdout('foo bar baz\n')
        sys.argv = old_argv

    def test_as_command(self):
        output = executive.Executive().run_command(echo.command_arguments('foo', 'bar', 'baz'))
        self.assertEqual(output, 'foo bar baz\n')
