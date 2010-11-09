#!/usr/bin/env python
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""This class helps to lock files exclusively across processes."""

import os
import sys


class FileLock(object):

    def __init__(self, lock_file_path):
        self._lock_file_path = lock_file_path
        self._lock_file_descriptor = None

    def acquire_lock(self):
        self._lock_file_descriptor = os.open(self._lock_file_path, os.O_CREAT)
        if sys.platform in ('darwin', 'linux2', 'cygwin'):
            import fcntl
            lock_flags = fcntl.LOCK_EX | fcntl.LOCK_NB
            fcntl.flock(self._lock_file_descriptor, lock_flags)
        elif sys.platform == 'win32':
            import msvcrt
            lock_flags = msvcrt.LK_NBLCK
            msvcrt.locking(self._lock_file_descriptor, lock_flags, 32)

    def release_lock(self):
        if self._lock_file_descriptor:
            os.close(self._lock_file_descriptor)
            os.unlink(self._lock_file_path)
