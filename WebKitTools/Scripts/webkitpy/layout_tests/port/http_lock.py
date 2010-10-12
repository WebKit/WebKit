#!/usr/bin/env python
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
# Copyright (C) 2010 Andras Becsi (abecsi@inf.u-szeged.hu), University of Szeged
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

"""This class helps to block NRWT threads when more NRWTs run
http and websocket tests in a same time."""

import glob
import os
import sys
import tempfile
import time


class HttpLock(object):

    def __init__(self, lock_path, lock_file_prefix="WebKitHttpd.lock.",
                 guard_lock="WebKit.lock"):
        if not lock_path:
            self._lock_path = tempfile.gettempdir()
        self._lock_file_prefix = lock_file_prefix
        self._lock_file_path_prefix = os.path.join(self._lock_path,
                                                   self._lock_file_prefix)
        self._guard_lock_file = os.path.join(self._lock_path, guard_lock)
        self._process_lock_file_name = ""

    def cleanup_http_lock(self):
        """Delete the lock file if exists."""
        if os.path.exists(self._process_lock_file_name):
            os.unlink(self._process_lock_file_name)

    def _extract_lock_number(self, lock_file_name):
        """Return the lock number from lock file."""
        prefix_length = len(self._lock_file_path_prefix)
        return int(lock_file_name[prefix_length:])

    def _lock_file_list(self):
        """Return the list of lock files sequentially."""
        lock_list = glob.glob(self._lock_file_path_prefix + '*')
        lock_list.sort(key=self._extract_lock_number)
        return lock_list

    def _next_lock_number(self):
        """Return the next available lock number."""
        lock_list = self._lock_file_list()
        if not lock_list:
            return 0
        return self._extract_lock_number(lock_list[-1]) + 1

    def _check_pid(self, current_pid):
        """Return True if pid is alive, otherwise return False."""
        try:
            os.kill(current_pid, 0)
        except OSError:
            return False
        else:
            return True

    def _curent_lock_pid(self):
        """Return with the current lock pid. If the lock is not valid
        it deletes the lock file."""
        lock_list = self._lock_file_list()
        if not lock_list:
            return
        try:
            current_lock_file = open(lock_list[0], 'r')
            current_pid = current_lock_file.readline()
            current_lock_file.close()
            if not (current_pid and
              sys.platform in ('darwin', 'linux2') and
              self._check_pid(int(current_pid))):
                os.unlink(lock_list[0])
                return
        except IOError, OSError:
            return
        return int(current_pid)

    def _create_lock_file(self):
        """The lock files are used to schedule the running test sessions in first
        come first served order. The sequential guard lock ensures that the lock
        numbers are sequential."""
        while(True):
            try:
                sequential_guard_lock = os.open(self._guard_lock_file,
                                                os.O_CREAT | os.O_NONBLOCK | os.O_EXCL)

                self._process_lock_file_name = (self._lock_file_path_prefix +
                                                str(self._next_lock_number()))
                lock_file = open(self._process_lock_file_name, 'w')
                lock_file.write(str(os.getpid()))
                lock_file.close()
                os.close(sequential_guard_lock)
                os.unlink(self._guard_lock_file)
                break
            except OSError:
                pass

    def wait_for_httpd_lock(self):
        """Create a lock file and wait until it's turn comes."""
        self._create_lock_file()
        while self._curent_lock_pid() != os.getpid():
            time.sleep(1)
