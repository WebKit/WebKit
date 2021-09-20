# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
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
import sys
import time

if sys.platform.startswith('win'):
    import msvcrt
else:
    import fcntl


class FileLock(object):

    def __init__(self, path, timeout=20):
        self.path = path
        self.timeout = timeout
        self._descriptor = None

    @property
    def acquired(self):
        return bool(self._descriptor)

    def acquire(self):
        if self._descriptor:
            return True

        descriptor = os.open(self.path, os.O_TRUNC | os.O_CREAT)
        start_time = time.time()
        while True:
            try:
                if sys.platform.startswith('win'):
                    msvcrt.locking(descriptor, msvcrt.LK_NBLCK, 32)
                else:
                    fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
                self._descriptor = descriptor
                return True

            except IOError:
                if time.time() - start_time > self.timeout:
                    os.close(descriptor)
                    self._descriptor = None
                    return False
                time.sleep(0.01)

    def release(self):
        try:
            if self._descriptor:
                if sys.platform.startswith('win'):
                    msvcrt.locking(self._descriptor, msvcrt.LK_UNLCK, 32)
                else:
                    fcntl.flock(self._descriptor, fcntl.LOCK_UN)
                os.close(self._descriptor)
                self._descriptor = None
            os.unlink(self.path)
        except (IOError, OSError):
            pass

    def __enter__(self):
        self.acquire()
        return self

    def __exit__(self, *args, **kwargs):
        self.release()
