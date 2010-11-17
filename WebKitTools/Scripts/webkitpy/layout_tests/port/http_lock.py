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
import logging
import os
import sys
import tempfile
import time

from webkitpy.common.system.executive import Executive
from webkitpy.common.system.file_lock import FileLock


_log = logging.getLogger("webkitpy.layout_tests.port.http_lock")


class HttpLock(object):

    def __init__(self, lock_path, lock_file_prefix="WebKitHttpd.lock.",
                 guard_lock="WebKit.lock"):
        self._lock_path = lock_path
        if not self._lock_path:
            self._lock_path = tempfile.gettempdir()
        self._lock_file_prefix = lock_file_prefix
        self._lock_file_path_prefix = os.path.join(self._lock_path,
                                                   self._lock_file_prefix)
        self._guard_lock_file = os.path.join(self._lock_path, guard_lock)
        self._guard_lock = FileLock(self._guard_lock_file)
        self._process_lock_file_name = ""
        self._executive = Executive()

    def cleanup_http_lock(self):
        """Delete the lock file if exists."""
        if os.path.exists(self._process_lock_file_name):
            _log.debug("Removing lock file: %s" % self._process_lock_file_name)
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
            if not (current_pid and self._executive.check_running_pid(int(current_pid))):
                _log.debug("Removing stuck lock file: %s" % lock_list[0])
                os.unlink(lock_list[0])
                return
        except (IOError, OSError):
            return
        return int(current_pid)

    def _create_lock_file(self):
        """The lock files are used to schedule the running test sessions in first
        come first served order. The guard lock ensures that the lock numbers are
        sequential."""
        if not os.path.exists(self._lock_path):
            _log.debug("Lock directory does not exist: %s" % self._lock_path)
            return False

        if not self._guard_lock.acquire_lock():
            _log.debug("Guard lock timed out!")
            return False

        self._process_lock_file_name = (self._lock_file_path_prefix +
                                        str(self._next_lock_number()))
        _log.debug("Creating lock file: %s" % self._process_lock_file_name)
        lock_file = open(self._process_lock_file_name, 'w')
        lock_file.write(str(os.getpid()))
        lock_file.close()
        self._guard_lock.release_lock()
        return True


    def wait_for_httpd_lock(self):
        """Create a lock file and wait until it's turn comes. If something goes wrong
        it wont do any locking."""
        if not self._create_lock_file():
            _log.debug("Warning, http locking failed!")
            return

        while self._curent_lock_pid() != os.getpid():
            time.sleep(1)
