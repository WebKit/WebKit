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

import logging
import math
import os
import signal
import time
import threading

_log = logging.getLogger(__name__)


class Timeout(object):

    thread_exception = RuntimeError('Timeout originates from a different thread')
    _process_to_timeout_map = {}

    class TimeoutData(object):

        def __init__(self, alarm_time, handler):
            self.alarm_time = alarm_time
            self.handler = handler
            self.thread_id = threading.current_thread().ident

    @staticmethod
    def default_handler(signum, frame):
        raise RuntimeError('Timeout alarm was triggered')

    @staticmethod
    def current():
        result = Timeout._process_to_timeout_map.get(os.getpid(), [])
        if not result:
            return None
        if result[0].thread_id != threading.current_thread().ident:
            _log.critical('Using both alarms and threading in the same process, this is unsupported')
            raise Timeout.thread_exception
        return result[0]

    def __init__(self, seconds=1, handler=None):
        if seconds == 0:
            raise RuntimeError('Cannot have a timeout of 0 seconds')

        if isinstance(handler, BaseException):
            exception = handler

            def exception_handler(signum, frame):
                raise exception

            handler = exception_handler

        self._timeout = seconds
        self._handler = handler if handler else Timeout.default_handler
        self.data = None

    @staticmethod
    def _bind_timeout_data_to_alarm(data):
        def handler(signum, frame):
            assert signum == signal.SIGALRM
            if data.thread_id != threading.current_thread().ident:
                raise Timeout.thread_exception
            data.handler(signum, frame)

        current_time = time.time()
        if data.alarm_time <= current_time:
            handler(signal.SIGALRM, None)

        signal.signal(signal.SIGALRM, handler)
        signal.alarm(int(math.ceil(data.alarm_time - current_time)))

    def __enter__(self):
        signal.alarm(0)  # Imiediatly disable the alarm so we aren't interupted.
        self.data = Timeout.TimeoutData(time.time() + self._timeout, self._handler)
        current_timeout = Timeout.current()

        # Another timeout is more urgent.
        if current_timeout and current_timeout.alarm_time < self.data.alarm_time:
            for i in range(len(Timeout._process_to_timeout_map[os.getpid()]) - 1):
                if self.data.alarm_time < Timeout._process_to_timeout_map[os.getpid()][i + 1].alarm_time:
                    Timeout._process_to_timeout_map[os.getpid()].insert(i, self.data)
                    break
            Timeout._process_to_timeout_map[os.getpid()].append(self.data)

        # This is the most urgent timeout
        else:
            Timeout._process_to_timeout_map[os.getpid()] = [self.data] + Timeout._process_to_timeout_map.get(os.getpid(), [])

        Timeout._bind_timeout_data_to_alarm(Timeout.current())
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        signal.alarm(0)  # Imiediatly disable the alarm so we aren't interupted.

        if not Timeout._process_to_timeout_map[os.getpid()]:
            raise RuntimeError('No timeout registered')
        Timeout._process_to_timeout_map[os.getpid()].remove(self.data)
        self.data = None

        if Timeout._process_to_timeout_map[os.getpid()]:
            Timeout._bind_timeout_data_to_alarm(Timeout.current())
