# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi <rgabor@inf.u-szeged.hu>, University of Szeged
# Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the Google name nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""WebKit implementations of the Port interface."""

import logging
import re
import time

from webkitcorepy import BytesIO, string_utils


_log = logging.getLogger(__name__)


class ImageDiffer(object):
    def __init__(self, port):
        self._port = port
        self._tolerance = None
        self._process = None

    def diff_image(self, expected_contents, actual_contents, tolerance):
        if tolerance != self._tolerance or (self._process and self._process.has_available_stdout()):
            self.stop()
        try:
            assert(expected_contents)
            assert(actual_contents)
            assert(tolerance is not None)

            if not self._process:
                self._start(tolerance)
            # Note that although we are handed 'old', 'new', ImageDiff wants 'new', 'old'.
            buffer = BytesIO()
            buffer.write(string_utils.encode('Content-Length: {}\n'.format(len(actual_contents))))
            buffer.write(actual_contents)
            buffer.write(string_utils.encode('Content-Length: {}\n'.format(len(expected_contents))))
            buffer.write(expected_contents)
            self._process.write(buffer.getvalue())
            return self._read()
        except IOError as exception:
            return (None, 0, "Failed to compute an image diff: %s" % str(exception))

    def _start(self, tolerance):
        command = [self._port._path_to_image_diff(), '--tolerance', str(tolerance)]
        if self._port._should_use_jhbuild():
            command = self._port._jhbuild_wrapper + command
        environment = self._port.setup_environ_for_server('ImageDiff')
        self._process = self._port._server_process_constructor(self._port, 'ImageDiff', command, environment, crash_message='Test marked as failed, ImageDiff crashed')
        self._process.start()
        self._tolerance = tolerance

    def _read(self):
        deadline = time.time() + 2.0
        output = None
        output_image = b''

        while not self._process.timed_out and not self._process.has_crashed():
            output = self._process.read_stdout_line(deadline)
            if self._process.timed_out or self._process.has_crashed() or not output:
                break

            if output.startswith(b'diff'):  # This is the last line ImageDiff prints.
                break

            if output.startswith(b'Content-Length'):
                m = re.match(b'Content-Length: (\d+)', output)
                content_length = int(string_utils.decode(m.group(1), target_type=str))
                output_image = self._process.read_stdout(deadline, content_length)
                output = self._process.read_stdout_line(deadline)
                break

        stderr = string_utils.decode(self._process.pop_all_buffered_stderr(), target_type=str)
        err_str = ''
        if stderr:
            err_str += "ImageDiff produced stderr output:\n" + stderr
        if self._process.timed_out:
            err_str += "ImageDiff timed out\n"
        if self._process.has_crashed():
            err_str += "ImageDiff crashed\n"

        diff_percent = 0
        if output and output.startswith(b'diff'):
            m = re.match(b'diff: (.+)% (passed|failed)', output)
            if m.group(2) == b'passed':
                return (None, 0, None)
            diff_percent = float(string_utils.decode(m.group(1), target_type=str))

        return (output_image, diff_percent, err_str or None)

    def stop(self):
        if self._process:
            self._process.stop()
            self._process = None
