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

import sys
import unittest
from multicommandtool import MultiCommandTool, Command
from StringIO import StringIO

from optparse import make_option

class TrivialCommand(Command):
    def __init__(self, **kwargs):
        Command.__init__(self, "help text", **kwargs)

    def execute(self, options, args, tool):
        pass


class CommandTest(unittest.TestCase):
    def test_name_with_arguments(self):
        command_with_args = TrivialCommand(argument_names="ARG1 ARG2")
        self.assertEqual(command_with_args.name_with_arguments("simple"), "simple ARG1 ARG2")

        command_with_args = TrivialCommand(options=[make_option("--my_option")])
        self.assertEqual(command_with_args.name_with_arguments("simple"), "simple [options]")


class TrivialTool(MultiCommandTool):
    def __init__(self, commands):
        MultiCommandTool.__init__(self, commands)

    def should_show_command_help(self, command):
        return True

    def should_execute_command(self, command):
        return True


class MultiCommandToolTest(unittest.TestCase):

    def _capture_stderr(self):
        self.saved_stderr = sys.stderr
        sys.stderr = StringIO()

    def _release_stderr(self):
        string = sys.stderr.getvalue()
        sys.stderr = self.saved_stderr
        self.saved_stderr = None
        return string

    def _assert_split(self, args, expected_split):
        self.assertEqual(MultiCommandTool._split_args(args), expected_split)

    def test_split_args(self):
        # MultiCommandToolTest._split_args returns: (global_args, command, command_args)
        full_args = ["--global-option", "command", "--option", "arg"]
        full_args_expected = (["--global-option"], "command", ["--option", "arg"])
        self._assert_split(full_args, full_args_expected)

        full_args = []
        full_args_expected = ([], None, [])
        self._assert_split(full_args, full_args_expected)

        full_args = ["command", "arg"]
        full_args_expected = ([], "command", ["arg"])
        self._assert_split(full_args, full_args_expected)

    def test_command_by_name(self):
        foo_command = { "name" : "foo_command", "object" :  TrivialCommand() }
        tool = TrivialTool([foo_command])

        self.assertEqual(tool.command_by_name("foo_command"), foo_command)
        self.assertEqual(tool.command_by_name("bar"), None)

    def test_command_help(self):
        command_with_args = TrivialCommand(options=[make_option("--my_option")])
        foo_command = { "name" : "foo_command", "object" :  command_with_args }
        tool = TrivialTool([foo_command])

        self._capture_stderr()
        exit_code = tool.main(["tool", "help", "foo_command"])
        help_text = self._release_stderr()
        expected_subcommand_help = "  foo_command [options]   help text\nOptions:\n  --my_option=MY_OPTION\n\n"
        self.assertEqual(exit_code, 0)
        self.assertEqual(help_text, expected_subcommand_help)


if __name__ == "__main__":
    unittest.main()
