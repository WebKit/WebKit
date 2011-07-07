#!/usr/bin/python
# Copyright (C) 2010 Google Inc. All rights reserved.
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

import unittest

from webkitpy.common.array_stream import ArrayStream
from webkitpy.layout_tests.views.metered_stream import MeteredStream


class TestMeteredStream(unittest.TestCase):
    def test_regular(self):
        a = ArrayStream()
        m = MeteredStream(stream=a)
        self.assertTrue(a.empty())

        # basic test
        m.write("foo")
        exp = ['foo']
        self.assertEquals(a.get(), exp)

        # now check that a second write() does not overwrite the first.
        m.write("bar")
        exp.append('bar')
        self.assertEquals(a.get(), exp)

        m.update("batter")
        exp.append('batter')
        self.assertEquals(a.get(), exp)

        # The next update() should overwrite the laste update() but not the
        # other text. Note that the cursor is effectively positioned at the
        # end of 'foo', even though we had to erase three more characters.
        m.update("foo")
        exp.append('\b\b\b\b\b\b      \b\b\b\b\b\b')
        exp.append('foo')
        self.assertEquals(a.get(), exp)

        # now check that a write() does overwrite the update
        m.write("foo")
        exp.append('\b\b\b   \b\b\b')
        exp.append('foo')
        self.assertEquals(a.get(), exp)

        # Now test that we only back up to the most recent newline.

        # Note also that we do not back up to erase the most recent write(),
        # i.e., write()s do not get erased.
        a.reset()
        m.update("foo\nbar")
        m.update("baz")
        self.assertEquals(a.get(), ['foo\nbar', '\b\b\b   \b\b\b', 'baz'])


if __name__ == '__main__':
    unittest.main()
