# Copyright (c) 2009, Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
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
# MultiCommandTool provides a framework for writing svn-like/git-like tools
# which are called with the following format:
# tool-name [global options] command-name [command options]

import sys

from optparse import OptionParser, IndentedHelpFormatter, SUPPRESS_USAGE, make_option

from modules.logging import log

class Command:
    def __init__(self, help_text, argument_names=None, options=None, requires_local_commits=False):
        self.help_text = help_text
        self.argument_names = argument_names
        self.options = options
        self.option_parser = HelpPrintingOptionParser(usage=SUPPRESS_USAGE, add_help_option=False, option_list=self.options)
        self.requires_local_commits = requires_local_commits

    def name_with_arguments(self, command_name):
        usage_string = command_name
        if self.options:
            usage_string += " [options]"
        if self.argument_names:
            usage_string += " " + self.argument_names
        return usage_string

    def parse_args(self, args):
        return self.option_parser.parse_args(args)

    def execute(self, options, args, tool):
        raise NotImplementedError, "subclasses must implement"


class NonWrappingEpilogIndentedHelpFormatter(IndentedHelpFormatter):
    # The standard IndentedHelpFormatter paragraph-wraps the epilog, killing our custom formatting.
    def format_epilog(self, epilog):
        if epilog:
            return "\n" + epilog + "\n"
        return ""


class HelpPrintingOptionParser(OptionParser):
    def error(self, msg):
        self.print_usage(sys.stderr)
        error_message = "%s: error: %s\n" % (self.get_prog_name(), msg)
        error_message += "\nType \"" + self.get_prog_name() + " --help\" to see usage.\n"
        self.exit(1, error_message)


class MultiCommandTool:
    def __init__(self, commands):
        self.commands = commands
        # FIXME: Calling self._commands_usage() in the constructor is bad because
        # it calls self.should_show_command_help which is subclass-defined.
        # The subclass will not be fully initialized at this point.
        self.global_option_parser = HelpPrintingOptionParser(usage=self._usage_line(), formatter=NonWrappingEpilogIndentedHelpFormatter(), epilog=self._commands_usage())

    @staticmethod
    def _usage_line():
        return "Usage: %prog [options] command [command-options] [command-arguments]"

    # FIXME: This can all be simplified once Command objects know their own names.
    @staticmethod
    def _name_and_arguments(command):
        return command['object'].name_with_arguments(command["name"])

    def _command_help_formatter(self):
        # Use our own help formatter so as to indent enough.
        formatter = IndentedHelpFormatter()
        formatter.indent()
        formatter.indent()
        return formatter

    @classmethod
    def _help_for_command(cls, command, formatter, longest_name_length):
        help_text = "  " + cls._name_and_arguments(command).ljust(longest_name_length + 3) + command['object'].help_text + "\n"
        help_text += command['object'].option_parser.format_option_help(formatter)
        return help_text

    @classmethod
    def _standalone_help_for_command(cls, command):
        return cls._help_for_command(command, IndentedHelpFormatter(), len(cls._name_and_arguments(command)))

    def _commands_usage(self):
        # Only show commands which are relevant to this checkout.  This might be confusing to some users?
        relevant_commands = filter(self.should_show_command_help, self.commands)
        longest_name_length = max(map(lambda command: len(self._name_and_arguments(command)), relevant_commands))
        command_help_texts = map(lambda command: self._help_for_command(command, self._command_help_formatter(), longest_name_length), relevant_commands)
        return "Commands:\n" + "".join(command_help_texts)

    def handle_global_args(self, args):
        (options, args) = self.global_option_parser.parse_args(args)
        # We should never hit this because _split_args splits at the first arg without a leading "-".
        if args:
            self.global_option_parser.error("Extra arguments before command: " + args)

    @staticmethod
    def _split_args(args):
        # Assume the first argument which doesn't start with "-" is the command name.
        command_index = 0
        for arg in args:
            if arg[0] != "-":
                break
            command_index += 1
        else:
            return (args[:], None, [])

        global_args = args[:command_index]
        command = args[command_index]
        command_args = args[command_index + 1:]
        return (global_args, command, command_args)

    def command_by_name(self, command_name):
        for command in self.commands:
            if command_name == command["name"]:
                return command
        return None

    def should_show_command_help(self, command):
        raise NotImplementedError, "subclasses must implement"

    def should_execute_command(self, command):
        raise NotImplementedError, "subclasses must implement"


    def main(self, argv=sys.argv):
        (global_args, command_name, args_after_command_name) = self._split_args(argv[1:])

        # Handle --help, etc:
        self.handle_global_args(global_args)

        if not command_name:
            self.global_option_parser.error("No command specified")

        if command_name == "help":
            if args_after_command_name:
                command = self.command_by_name(args_after_command_name[0])
                log(self._standalone_help_for_command(command))
            else:
                self.global_option_parser.print_help()
            return 0

        command = self.command_by_name(command_name)
        if not command:
            self.global_option_parser.error(command_name + " is not a recognized command")

        (should_execute, failure_reason) = self.should_execute_command(command)
        if not should_execute:
            log(failure_reason)
            return 0

        command_object = command["object"]
        (command_options, command_args) = command_object.parse_args(args_after_command_name)
        return command_object.execute(command_options, command_args, self)
