# Copyright (C) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import unittest
from StringIO import StringIO

from modules.commands.queries import *
from modules.mock_bugzillatool import *

class QueryCommandsTest(unittest.TestCase):
    def _capture_output_with_name(output_name):
        self.saved_outputs[output_name] = getattr(sys, output_name)
        setattr(sys, output_name, StringIO.StringIO())

    def _release_output_with_name(output_name):
        captured_output = getattr(sys, output_name).getvalue()
        setattr(sys, output_name, self.saved_outputs[output_name])
        del self.saved_outputs[output_name]
        return captured_output

    def _capture_output(self):
        self._capture_output_with_name("stdout")
        self._capture_output_with_name("stderr")

    def _restore_output(self):
        return (self._release_output_with_name("stdout"), self._release_output_with_name("stderr"))

    def _assert_execute_outputs(self, command, command_args, expected_stdout, expected_stderr = ""):
        self._capture_output()
        command.execute(None, command_args, MockBugzillaTool())
        (stdout_string, stderr_string) = self._restore_output()
        self.assertEqual(stdout_string, expected_stdout)
        self.assertEqual(expected_stderr, expected_stderr)

    def test_bugs_to_commit(self):
        self._assert_execute_outputs(BugsToCommit(), None, "42\n75\n")

    def test_patches_to_commit(self):
        expected_stdout = "http://example.com/197\nhttp://example.com/128\n"
        expected_stderr = "Patches in commit queue:\n"
        self._assert_execute_outputs(PatchesToCommit(), None, expected_stdout, expected_stderr)

    def test_reviewed_patches(self):
        expected_stdout = "http://example.com/197\nhttp://example.com/128\n"
        self._assert_execute_outputs(ReviewedPatches(), [42], expected_stdout)

    def test_tree_status(self):
        expected_stdout = "ok   : Builder1\nok   : Builder2\n"
        self._assert_execute_outputs(TreeStatus(), None, expected_stdout)
