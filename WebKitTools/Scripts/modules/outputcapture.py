# Copyright (c) 2009, Google Inc. All rights reserved.
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
#     * Neither the name of Google Inc. nor the names of its
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
#
# Class for unittest support.  Used for capturing stderr/stdout.

import sys
from StringIO import StringIO

class OutputCapture(object):
    def __init__(self):
        self.saved_outputs = dict()

    def _capture_output_with_name(self, output_name):
        self.saved_outputs[output_name] = getattr(sys, output_name)
        setattr(sys, output_name, StringIO())

    def _restore_output_with_name(self, output_name):
        captured_output = getattr(sys, output_name).getvalue()
        setattr(sys, output_name, self.saved_outputs[output_name])
        del self.saved_outputs[output_name]
        return captured_output

    def capture_output(self):
        self._capture_output_with_name("stdout")
        self._capture_output_with_name("stderr")

    def restore_output(self):
        return (self._restore_output_with_name("stdout"), self._restore_output_with_name("stderr"))

    def assert_outputs(self, testcase, function, args=[], kwargs={}, expected_stdout="", expected_stderr=""):
        self.capture_output()
        return_value = function(*args, **kwargs)
        (stdout_string, stderr_string) = self.restore_output()
        testcase.assertEqual(stdout_string, expected_stdout)
        testcase.assertEqual(stderr_string, expected_stderr)
        # This is a little strange, but I don't know where else to return this information.
        return return_value
