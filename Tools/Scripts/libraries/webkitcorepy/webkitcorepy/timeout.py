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

import bisect
import collections
import math
import os
import signal
import time
import threading

from webkitcorepy import log, string_utils


ORIGINAL_SLEEP = time.sleep


class Timeout(object):
    _process_to_timeout_map = collections.defaultdict(list)

    class Data(object):
        def __init__(self, alarm_time, handler):
            self.alarm_time = alarm_time
            self.handler = handler
            self.thread_id = threading.current_thread().ident
            self.triggered = False

        def __lt__(self, other):
            if not other:
                return False
            if not isinstance(other, Timeout.Data):
                raise ValueError('Expected {} in comparison, received {}').format(Timeout.Data, type(other))
            return self.alarm_time < other.alarm_time

    class Exception(Exception):
        pass

    class DisableAlarm(object):
        def __init__(self, patch=True):
            if patch:
                # Allows mock to be managed via autoinstall
                from mock import patch
                self._patch = patch('time.sleep', new=ORIGINAL_SLEEP)
            else:
                self._patch = None

        def __enter__(self):
            signal.alarm(0)
            if self._patch:
                self._patch.__enter__()

        def __exit__(self, exc_type, exc_value, traceback):
            if self._patch:
                self._patch.__exit__(exc_type, exc_value, traceback)

            # We don't rebind the alarm if an exception is already in flight
            if exc_type is None:
                Timeout.bind()

    @classmethod
    def default_handler(cls, signum, frame):
        raise cls.Exception('Timeout alarm was triggered')

    @classmethod
    def current(cls):
        for element in cls._process_to_timeout_map[os.getpid()]:
            if element.triggered:
                continue
            if element.thread_id != threading.current_thread().ident:
                log.critical('Using both alarms and threading in the same process, this is unsupported')
                raise ValueError('Timeout originates from a different thread')
            return element
        return None

    @classmethod
    def deadline(cls):
        current = cls.current()
        return current.alarm_time if current else None

    @classmethod
    def difference(cls, current_time=None):
        current = cls.current()
        if not current:
            return None
        current_time = current_time if current_time else time.time()
        return current.alarm_time - current_time if current.alarm_time - current_time > 0 else 0

    @classmethod
    def check(cls, current_time=None):
        if cls.difference(current_time=current_time) != 0:
            return
        current = cls.current()
        current.triggered = True
        cls.bind()
        current.handler(signal.SIGALRM, None)

    @classmethod
    def bind(cls):
        current = cls.current()
        if not current:
            signal.alarm(0)
            return

        def handler(signum, frame):
            assert signum == signal.SIGALRM
            if current.thread_id != threading.current_thread().ident:
                log.critical('Using both alarms and threading in the same process, this is unsupported')
                raise ValueError('Timeout originates from a different thread')
            current.triggered = True
            Timeout.bind()
            current.handler(signum, frame)

        current_time = time.time()
        cls.check(current_time=current_time)
        signal.signal(signal.SIGALRM, handler)
        signal.alarm(int(math.ceil(current.alarm_time - current_time)))

    @classmethod
    def sleep(cls, seconds):
        difference = cls.difference()
        if difference is not None and seconds >= difference:
            log.error('Request to sleep {} exceeded the current timeout threshold'.format(string_utils.pluralize(seconds, 'second')))
            current = cls.current()
            current.triggered = True
            cls.bind()
            current.handler(signal.SIGALRM, None)
        return ORIGINAL_SLEEP(seconds)

    def __init__(self, seconds=1, handler=None, patch=True):
        if seconds <= 0:
            raise ValueError('Timeouts must be positive')

        if isinstance(handler, BaseException):
            exception = handler

            def exception_handler(signum, frame):
                raise exception

            handler = exception_handler

        self._timeout = seconds
        self._handler = handler if handler else self.default_handler
        self.data = None

        if patch:
            # Allows mock to be managed via autoinstall
            from mock import patch
            self._patch = patch('time.sleep', new=self.sleep)
        else:
            self._patch = None

    def __enter__(self):
        with self.DisableAlarm():
            self.data = self.Data(time.time() + self._timeout, self._handler)
            bisect.insort(self._process_to_timeout_map[os.getpid()], self.data)

        if self._patch:
            self._patch.__enter__()
        return self

    def __exit__(self, *args, **kwargs):
        if self._patch:
            self._patch.__exit__(*args, **kwargs)

        with self.DisableAlarm():
            if not self._process_to_timeout_map[os.getpid()]:
                raise RuntimeError('No timeout registered')
            self._process_to_timeout_map[os.getpid()].remove(self.data)
            self.data = None
