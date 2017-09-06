# Copyright (C) 2017 Apple Inc. All rights reserved.
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

import time
import threading
import unittest

from webkitpy.common.timeout_context import Timeout


class TimeoutContextTests(unittest.TestCase):

    def test_current_timeout(self):
        self.assertEqual(None, Timeout.current())
        with Timeout(1) as tmp:
            self.assertEqual(tmp.data, Timeout.current())
        self.assertEqual(None, Timeout.current())

    def test_invalid_timeout(self):
        self.assertRaises(RuntimeError, Timeout, 0)

    def test_timeout_data(self):
        tmp = Timeout(1)
        self.assertEqual(None, tmp.data)
        with tmp:
            self.assertNotEqual(None, tmp.data)
            self.assertEqual(threading.current_thread().ident, tmp.data.thread_id)
            self.assertTrue(time.time() + 1 >= tmp.data.alarm_time)
        self.assertEqual(None, tmp.data)

    def test_nested_inner_precedence(self):
        tmp_outer = Timeout(2)
        tmp_inner = Timeout(1)
        with tmp_outer:
            self.assertEqual(tmp_outer.data, Timeout.current())
            with tmp_inner:
                self.assertEqual(tmp_inner.data, Timeout.current())
            self.assertEqual(tmp_outer.data, Timeout.current())
        self.assertEqual(None, Timeout.current())

    def test_nested_outer_precedence(self):
        tmp_outer = Timeout(1)
        tmp_inner = Timeout(2)
        with tmp_outer:
            self.assertEqual(tmp_outer.data, Timeout.current())
            with tmp_inner:
                self.assertEqual(tmp_outer.data, Timeout.current())
            self.assertEqual(tmp_outer.data, Timeout.current())
        self.assertEqual(None, Timeout.current())

    def test_no_timeout(self):
        with Timeout(2):
            time.sleep(1)

    def test_basic_timeout(self):
        def should_timeout():
            with Timeout(1):
                time.sleep(2)
        self.assertRaises(RuntimeError, should_timeout)

    def test_exception_constructor_timeout(self):
        def should_timeout():
            with Timeout(1, Exception('This should be raised')):
                time.sleep(2)
        self.assertRaises(Exception, should_timeout)

    def test_nested_inner_timeout(self):
        def should_timeout():
            with Timeout(3, Exception("This shouldn't be raised")):
                with Timeout(1):
                    time.sleep(2)
        self.assertRaises(RuntimeError, should_timeout)

    def test_nested_outer_timeout(self):
        def should_timeout():
            with Timeout(1):
                with Timeout(3, Exception("This shouldn't be raised")):
                    time.sleep(2)
        self.assertRaises(RuntimeError, should_timeout)
