# Copyright (C) 2020 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import time
import threading
import unittest

from webkitcorepy import OutputCapture, Timeout, mocks


class TimeoutTests(unittest.TestCase):

    def asssertIsClose(self, a, b, percentage=1):
        percentage = percentage * .01
        if abs(a - b) > min(abs(percentage * a), abs(percentage * b)):
            self.assertEqual(a, b)

    def test_current_timeout(self):
        self.assertEqual(None, Timeout.current())
        with Timeout(1) as tmp:
            self.assertEqual(tmp.data, Timeout.current())
        self.assertEqual(None, Timeout.current())

    def test_invalid_timeout(self):
        self.assertRaises(ValueError, Timeout, 0)

    def test_timeout_data(self):
        tmp = Timeout(1)
        self.assertEqual(None, tmp.data)
        with tmp:
            self.assertNotEqual(None, tmp.data)
            self.assertEqual(threading.current_thread().ident, tmp.data.thread_id)
            self.assertTrue(time.time() + 1 >= tmp.data.alarm_time)
        self.assertEqual(None, tmp.data)

    def test_difference(self):
        with mocks.Time:
            with Timeout(1):
                self.asssertIsClose(Timeout.difference(), 1)

    def test_deadline(self):
        with mocks.Time:
            with Timeout(1):
                self.asssertIsClose(Timeout.deadline(), time.time() + 1)

    def test_check(self):
        with mocks.Time, OutputCapture() as capturer:
            with Timeout(1):
                Timeout.check()
                self.assertRaises(Timeout.Exception, time.sleep, 1)

        self.assertEqual(capturer.webkitcorepy.log.getvalue(), 'Request to sleep 1 second exceeded the current timeout threshold\n')

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
        with mocks.Time, OutputCapture() as capturer:
            with Timeout(2):
                time.sleep(1)

        self.assertEqual(capturer.webkitcorepy.log.getvalue(), '')

    def test_basic_timeout(self):
        with mocks.Time, OutputCapture() as capturer, self.assertRaises(Timeout.Exception):
            with Timeout(1):
                time.sleep(2)

        self.assertEqual(capturer.webkitcorepy.log.getvalue(), 'Request to sleep 2 seconds exceeded the current timeout threshold\n')

    def test_exception_constructor_timeout(self):
        with mocks.Time, OutputCapture() as capturer, self.assertRaises(RuntimeError):
            with Timeout(1, RuntimeError('This should be raised')):
                time.sleep(2)

        self.assertEqual(capturer.webkitcorepy.log.getvalue(), 'Request to sleep 2 seconds exceeded the current timeout threshold\n')

    def test_nested_inner_timeout(self):
        with mocks.Time, OutputCapture() as capturer, self.assertRaises(Timeout.Exception):
            with Timeout(3, RuntimeError("This shouldn't be raised")):
                with Timeout(1):
                    time.sleep(2)

        self.assertEqual(capturer.webkitcorepy.log.getvalue(), 'Request to sleep 2 seconds exceeded the current timeout threshold\n')

    def test_nested_outer_timeout(self):
        with mocks.Time, OutputCapture() as capturer, self.assertRaises(Timeout.Exception):
            with Timeout(1):
                with Timeout(3, RuntimeError("This shouldn't be raised")):
                    time.sleep(2)

        self.assertEqual(capturer.webkitcorepy.log.getvalue(), 'Request to sleep 2 seconds exceeded the current timeout threshold\n')

    def test_single_trigger(self):
        with mocks.Time, OutputCapture():
            with Timeout(1):
                with self.assertRaises(Timeout.Exception):
                    time.sleep(2)
                time.sleep(2)  # The timeout has already been triggered, so it shouldn't be re-triggered.
