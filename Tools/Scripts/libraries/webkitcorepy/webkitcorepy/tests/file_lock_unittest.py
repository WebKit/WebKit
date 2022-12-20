# Copyright (C) 2021 Apple Inc. All rights reserved.
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

import os
import shutil
import tempfile
import time
import unittest

from webkitcorepy import FileLock, mocks


class FileLockTestCase(unittest.TestCase):

    def __init__(self, *args, **kwargs):
        super(FileLockTestCase, self).__init__(*args, **kwargs)
        self.path = None

    def setUp(self):
        self.path = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.path, ignore_errors=True)

    def test_basic(self):
        path = os.path.join(self.path, 'example-{}.lock'.format(os.getpid()))
        lock = FileLock(path, timeout=0)

        self.assertFalse(lock.acquired)
        with lock:
            self.assertTrue(lock.acquired)
        self.assertFalse(lock.acquired)

        with lock:
            self.assertTrue(lock.acquired)
        self.assertFalse(lock.acquired)

    def test_locked(self):
        path = os.path.join(self.path, 'example-{}.lock'.format(os.getpid()))
        lock_a = FileLock(path, timeout=0)
        lock_b = FileLock(path, timeout=0)

        self.assertFalse(lock_a.acquired)
        self.assertFalse(lock_b.acquired)

        with lock_a:
            self.assertTrue(lock_a.acquired)
            self.assertFalse(lock_b.acquired)

            with lock_b:
                self.assertTrue(lock_a.acquired)
                self.assertFalse(lock_b.acquired)

            self.assertTrue(lock_a.acquired)
            self.assertFalse(lock_b.acquired)

        self.assertFalse(lock_a.acquired)
        self.assertFalse(lock_b.acquired)

    def test_locked_timeout(self):
        with mocks.Time:
            path = os.path.join(self.path, 'example-{}.lock'.format(os.getpid()))
            lock_a = FileLock(path, timeout=0)
            lock_b = FileLock(path, timeout=30)

            self.assertFalse(lock_a.acquired)
            self.assertFalse(lock_b.acquired)

            start_time = time.time()
            with lock_a:
                self.assertAlmostEqual(start_time, time.time(), places=1)
                self.assertTrue(lock_a.acquired)
                self.assertFalse(lock_b.acquired)

                with lock_b:
                    self.assertAlmostEqual(start_time + 30, time.time(), places=1)
                    self.assertTrue(lock_a.acquired)
                    self.assertFalse(lock_b.acquired)

                self.assertTrue(lock_a.acquired)
                self.assertFalse(lock_b.acquired)

            self.assertFalse(lock_a.acquired)
            self.assertFalse(lock_b.acquired)

    def test_double(self):
        path = os.path.join(self.path, 'example-{}.lock'.format(os.getpid()))
        lock = FileLock(path, timeout=0)

        self.assertFalse(lock.acquired)
        with lock:
            self.assertTrue(lock.acquired)
            with lock:
                self.assertTrue(lock.acquired)
            self.assertFalse(lock.acquired)
        self.assertFalse(lock.acquired)
