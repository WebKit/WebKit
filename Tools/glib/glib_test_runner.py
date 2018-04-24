# Copyright (C) 2018 Igalia S.L.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.

import errno
import os
import select
import socket
import struct
import subprocess
import sys
from signal import alarm, signal, SIGALRM
from io import BytesIO


(LOG_NONE,
 LOG_ERROR,
 LOG_START_BINARY,
 LOG_LIST_CASE,
 LOG_SKIP_CASE,
 LOG_START_CASE,
 LOG_STOP_CASE,
 LOG_MIN_RESULT,
 LOG_MAX_RESULT,
 LOG_MESSAGE,
 LOG_START_SUITE,
 LOG_STOP_SUITE) = range(12)

(TEST_RUN_SUCCESS,
 TEST_RUN_SKIPPED,
 TEST_RUN_FAILURE,
 TEST_RUN_INCOMPLETE) = range(4)


class TestTimeout(Exception):
    pass


class Message(object):

    def __init__(self, log_type, strings, numbers):
        self.log_type = log_type
        self.strings = strings
        self.numbers = numbers

    @staticmethod
    def create(data):
        if len(data) < 5 * 4:
            return 0, None

        def read_unsigned(bytes, n=1):
            values = struct.unpack('%dI' % n, bytes.read(4 * n))
            return [socket.ntohl(v) for v in values if v is not None]

        def read_double(bytes, n=1):
            return struct.unpack('>%dd' % n, bytes.read(8 * n))

        def read_string(bytes, n=1):
            values = []
            for i in range(n):
                str_len = read_unsigned(bytes)[0]
                values.append(struct.unpack('%ds' % str_len, bytes.read(str_len))[0])
            return values

        bytes = BytesIO(data)
        msg_len = read_unsigned(bytes)[0]
        if len(data) < msg_len:
            return 0, None

        log_type, n_strings, n_numbers, reserved = read_unsigned(bytes, 4)
        if reserved != 0:
            return 0, None

        strings = read_string(bytes, n_strings)
        numbers = read_double(bytes, n_numbers)

        return msg_len, Message(log_type, strings, numbers)


class GLibTestRunner(object):

    def __init__(self, test_binary, timeout, is_slow_function=None, slow_timeout=0):
        self._test_binary = test_binary
        self._timeout = timeout
        if is_slow_function is not None:
            self._is_test_slow = is_slow_function
        else:
            self._is_test_slow = lambda x, y: False
        self._slow_timeout = slow_timeout

        self._stderr_fd = None
        self._subtest = None
        self._subtest_messages = []
        self._results = {}

    def _process_data(self, data):
        retval = []
        msg_len, msg = Message.create(data)
        while msg_len:
            retval.append(msg)
            data = data[msg_len:]
            msg_len, msg = Message.create(data)

        return data, retval

    def _process_message(self, message):
        if message.log_type == LOG_ERROR:
            self._subtest_message(message.strings)
        elif message.log_type == LOG_START_CASE:
            self._subtest_start(message.strings[0])
        elif message.log_type == LOG_STOP_CASE:
            self._subtest_end(message.numbers[0])
        elif message.log_type == LOG_MESSAGE:
            self._subtest_message([message.strings[0]])

    def _read_from_pipe(self, pipe_r):
        data = ''
        read_set = [pipe_r]
        while read_set:
            try:
                rlist, _, _ = select.select(read_set, [], [])
            except select.error, e:
                if e.args[0] == errno.EINTR:
                    continue
                raise

            if pipe_r in rlist:
                buffer = os.read(pipe_r, 4096)
                if not buffer:
                    read_set.remove(pipe_r)

                data += buffer
                data, messages = self._process_data(data)
                for message in messages:
                    self._process_message(message)

    def _read_from_stderr(self, fd):
        data = ''
        read_set = [fd]
        while True:
            try:
                rlist, _, _ = select.select(read_set, [], [], 0)
            except select.error, e:
                if e.args[0] == errno.EINTR:
                    continue
                raise

            if fd not in rlist:
                return data

            buffer = os.read(fd, 4096)
            if not buffer:
                return data

            data += buffer

    @staticmethod
    def _start_timeout(timeout):
        if timeout <= 0:
            return

        def _alarm_handler(signum, frame):
            raise TestTimeout

        signal(SIGALRM, _alarm_handler)
        alarm(timeout)

    @staticmethod
    def _stop_timeout():
        alarm(0)

    def _subtest_start(self, subtest):
        self._subtest = subtest
        message = self._subtest + ':'
        sys.stdout.write('  %-68s' % message)
        sys.stdout.flush()
        timeout = self._timeout
        if self._is_test_slow(os.path.basename(self._test_binary), self._subtest):
            timeout = self._slow_timeout
        self._start_timeout(timeout)

    def _subtest_message(self, message):
        if self._subtest is None:
            sys.stdout.write('%s\n' % '\n'.join(message))
        else:
            self._subtest_messages.extend(message)

    def _subtest_stderr(self, errors):
        ignore_next_line = False
        for line in errors.rstrip('\n').split('\n'):
            if ignore_next_line:
                ignore_next_line = False
                continue
            if line == '**':
                ignore_next_line = True
                continue
            sys.stderr.write('%s\n' % line)
        sys.stderr.flush()

    def _subtest_end(self, result, did_timeout=False):
        self._stop_timeout()
        if did_timeout:
            self._results[self._subtest] = 'TIMEOUT'
        elif result == TEST_RUN_SUCCESS:
            self._results[self._subtest] = 'PASS'
        elif result == TEST_RUN_SKIPPED:
            self._results[self._subtest] = 'SKIP'
        elif not self._subtest_messages:
            self._results[self._subtest] = 'CRASH'
        else:
            self._results[self._subtest] = 'FAIL'

        sys.stdout.write('%s\n' % self._results[self._subtest])
        if self._subtest_messages:
            sys.stdout.write('%s\n' % '\n'.join(self._subtest_messages))
        sys.stdout.flush()

        errors = self._read_from_stderr(self._stderr_fd)
        if errors:
            self._subtest_stderr(errors)

        self._subtest = None
        self._subtest_messages = []

    def run(self, subtests=[], skipped=[], env=None):
        pipe_r, pipe_w = os.pipe()
        command = [self._test_binary, '--quiet', '--keep-going', '--GTestLogFD=%d' % pipe_w]
        if self._results:
            command.append('--GTestSkipCount=%d' % len(self._results))
        for subtest in subtests:
            command.extend(['-p', subtest])
        for skip in skipped:
            command.extend(['-s', skip])

        if not self._results:
            sys.stdout.write('TEST: %s...\n' % self._test_binary)
            sys.stdout.flush()
        p = subprocess.Popen(command, stderr=subprocess.PIPE, env=env)
        self._stderr_fd = p.stderr.fileno()
        os.close(pipe_w)

        need_restart = False

        try:
            self._read_from_pipe(pipe_r)
            p.wait()
        except TestTimeout:
            self._subtest_end(0, did_timeout=True)
            p.terminate()
            need_restart = True
        finally:
            os.close(pipe_r)

        if self._subtest is not None:
            self._subtest_end(256)
            need_restart = True

        self._stderr_fd = None

        if need_restart:
            self.run(subtests, skipped, env)

        return self._results

if __name__ == '__main__':
    for test in sys.argv[1:]:
        runner = GLibTestRunner(test, 5)
        runner.run()
