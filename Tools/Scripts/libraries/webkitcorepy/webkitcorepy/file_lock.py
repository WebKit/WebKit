# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
# Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

import errno
import os
import re
import sys
import time

from webkitcorepy import string_utils
from webkitcorepy.timeout import Timeout

if sys.platform.startswith('win'):
    import msvcrt


class FileLock(object):
    INTEGER_RE = re.compile(r'^\d+$')
    USE_WINDOWS = sys.platform.startswith('win')
    USE_EXLOCK = not USE_WINDOWS and getattr(os, 'O_EXLOCK', False)

    @classmethod
    def is_process_running(cls, pid):
        if cls.USE_WINDOWS:
            raise RuntimeError('Funciton does not support Windows')
        try:
            os.kill(pid, 0)
            return True
        except OSError as e:
            if e.errno == errno.ESRCH:
                return False
            raise

    def __init__(self, path, timeout=20):
        self.path = path
        self.timeout = timeout
        self._descriptor = None

    @property
    def acquired(self):
        return bool(self._descriptor)

    def acquire(self):
        if self._descriptor:
            raise RuntimeError('Cannot re-enter acquired FileLock')

        if not self.USE_EXLOCK and not self.USE_WINDOWS and os.path.exists(self.path):
            with open(self.path) as file:
                pid = file.readline().strip()
            if self.INTEGER_RE.match(pid) and not self.is_process_running(int(pid)):
                os.unlink(self.path)

        if self.USE_EXLOCK and self.timeout:
            with Timeout(
                seconds=self.timeout, patch=False,
                handler=OSError(errno.EAGAIN, "Resource temporarily unavailable: '{}'".format(self.path)),
            ):
                self._descriptor = os.open(self.path, os.O_CREAT | os.O_WRONLY | os.O_EXLOCK)
            return True

        start_time = time.time()
        while True:
            try:
                if self.USE_WINDOWS:
                    self._descriptor = os.open(self.path, os.O_TRUNC | os.O_CREAT)
                    msvcrt.locking(self._descriptor, msvcrt.LK_NBLCK, 32)
                elif self.USE_EXLOCK:
                    self._descriptor = os.open(self.path, os.O_CREAT | os.O_WRONLY | os.O_EXLOCK | os.O_NONBLOCK)
                else:
                    self._descriptor = os.open(self.path, os.O_CREAT | os.O_WRONLY | os.O_EXCL)
                    os.write(self._descriptor, string_utils.encode(str(os.getpid())))
                return True

            except (IOError, OSError):
                if time.time() - start_time > self.timeout:
                    if self._descriptor:
                        os.close(self._descriptor)
                    self._descriptor = None
                    raise
                time.sleep(0.01)

    def release(self):
        if not self._descriptor:
            raise RuntimeError('Cannot release unclaimed lock')
        try:
            if self.USE_WINDOWS:
                msvcrt.locking(self._descriptor, msvcrt.LK_UNLCK, 32)
            elif not self.USE_EXLOCK:
                os.unlink(self.path)
        finally:
            os.close(self._descriptor)
            self._descriptor = None

    def __enter__(self):
        self.acquire()
        return self

    def __exit__(self, *args, **kwargs):
        if not self._descriptor:
            return
        self.release()
