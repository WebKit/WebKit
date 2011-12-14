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

import StringIO
import os.path
import sys
import unittest

from webkitpy.common.system import executive, outputcapture
from webkitpy.test import cat


class CatTest(outputcapture.OutputCaptureTestCaseBase):
    def assert_cat(self, input):
        saved_stdin = sys.stdin
        sys.stdin = StringIO.StringIO(input)
        cat.main()
        self.assertStdout(input)
        sys.stdin = saved_stdin

    def test_basic(self):
        self.assert_cat('foo bar baz\n')

    def test_no_newline(self):
        self.assert_cat('foo bar baz')

    def test_unicode(self):
        self.assert_cat(u'WebKit \u2661 Tor Arne Vestb\u00F8!')

    def test_as_command(self):
        input = 'foo bar baz\n'
        output = executive.Executive().run_command(cat.command_arguments(), input=input)
        self.assertEqual(input, output)
