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

from modules.grammar import pluralize
from modules.logging import log

class Command(object):
    name = None
    # show_in_main_help = False # Subclasses must define show_in_main_help, we leave it out here to enforce that.
    def __init__(self, help_text, argument_names=None, options=None, requires_local_commits=False):
        self.help_text = help_text
        self.argument_names = argument_names
        self.required_arguments = self._parse_required_arguments(argument_names)
        self.options = options
        self.option_parser = HelpPrintingOptionParser(usage=SUPPRESS_USAGE, add_help_option=False, option_list=self.options)
        self.requires_local_commits = requires_local_commits
        self.tool = None

    # The tool calls bind_to_tool on each Command after adding it to its list.
    def bind_to_tool(self, tool):
        # Command instances can only be bound to one tool at a time.
        if self.tool and tool != self.tool:
            raise Exception("Command already bound to tool!")
        self.tool = tool

    @staticmethod
    def _parse_required_arguments(argument_names):
        required_args = []
        if not argument_names:
            return required_args
        split_args = argument_names.split(" ")
        for argument in split_args:
            if argument[0] == '[':
                # For now our parser is rather dumb.  Do some minimal validation that
                # we haven't confused it.
                if argument[-1] != ']':
                    raise Exception("Failure to parse argument string %s.  Argument %s is missing ending ]" % (argument_names, argument))
            else:
                required_args.append(argument)
        return required_args

    def name_with_arguments(self):
        usage_string = self.name
        if self.options:
            usage_string += " [options]"
        if self.argument_names:
            usage_string += " " + self.argument_names
        return usage_string

    def parse_args(self, args):
        return self.option_parser.parse_args(args)

    def check_arguments_and_execute(self, args_after_command_name, tool):
        (command_options, command_args) = self.parse_args(args_after_command_name)

        if len(command_args) < len(self.required_arguments):
            log("%s required, %s provided.  Provided: %s  Required: %s\nSee '%s help %s' for usage." % (
                pluralize("argument", len(self.required_arguments)),
                pluralize("argument", len(command_args)),
                "'%s'" % " ".join(command_args),
                " ".join(self.required_arguments),
                tool.name(),
                self.name))
            return 1
        return self.execute(command_options, command_args, tool) or 0

    def standalone_help(self):
        help_text = self.name_with_arguments().ljust(len(self.name_with_arguments()) + 3) + self.help_text + "\n"
        help_text += self.option_parser.format_option_help(IndentedHelpFormatter())
        return help_text

    def execute(self, options, args, tool):
        raise NotImplementedError, "subclasses must implement"


class HelpPrintingOptionParser(OptionParser):
    def __init__(self, epilog_method=None, *args, **kwargs):
        self.epilog_method = epilog_method
        OptionParser.__init__(self, *args, **kwargs)

    def error(self, msg):
        self.print_usage(sys.stderr)
        error_message = "%s: error: %s\n" % (self.get_prog_name(), msg)
        error_message += "\nType \"%s --help\" to see usage.\n" % self.get_prog_name()
        self.exit(1, error_message)

    # We override format_epilog to avoid the default formatting which would paragraph-wrap the epilog
    # and also to allow us to compute the epilog lazily instead of in the constructor (allowing it to be context sensitive).
    def format_epilog(self, epilog):
        if self.epilog_method:
            return "\n%s\n" % self.epilog_method()
        return ""


class HelpCommand(Command):
    name = "help"
    show_in_main_help = False

    def __init__(self):
        options = [
            make_option("-a", "--all-commands", action="store_true", dest="show_all_commands", help="Print all available commands"),
        ]
        Command.__init__(self, "Display information about this program or its subcommands", "[COMMAND]", options=options)
        self.show_all_commands = False # A hack used to pass --all-commands to _help_epilog even though it's called by the OptionParser.

    def _help_epilog(self):
        # Only show commands which are relevant to this checkout's SCM system.  Might this be confusing to some users?
        if self.show_all_commands:
            epilog = "All %prog commands:\n"
            relevant_commands = self.tool.commands[:]
        else:
            epilog = "Common %prog commands:\n"
            relevant_commands = filter(self.tool.should_show_in_main_help, self.tool.commands)
        longest_name_length = max(map(lambda command: len(command.name), relevant_commands))
        relevant_commands.sort(lambda a, b: cmp(a.name, b.name))
        command_help_texts = map(lambda command: "   %s   %s\n" % (command.name.ljust(longest_name_length), command.help_text), relevant_commands)
        epilog += "%s\n" % "".join(command_help_texts)
        epilog += "See '%prog help --all-commands' to list all commands.\n"
        epilog += "See '%prog help COMMAND' for more information on a specific command.\n"
        return self.tool.global_option_parser.expand_prog_name(epilog)

    def execute(self, options, args, tool):
        if args:
            command = self.tool.command_by_name(args[0])
            if command:
                print command.standalone_help()
                return 0

        self.show_all_commands = options.show_all_commands
        tool.global_option_parser.print_help()
        return 0


class MultiCommandTool(object):
    def __init__(self, name=None, commands=None):
        # Allow the unit tests to disable command auto-discovery.
        self.commands = commands or [cls() for cls in self._find_all_commands() if cls.name]
        self.help_command = self.command_by_name(HelpCommand.name)
        # Require a help command, even if the manual test list doesn't include one.
        if not self.help_command:
            self.help_command = HelpCommand()
            self.commands.append(self.help_command)
        for command in self.commands:
            command.bind_to_tool(self)
        self.global_option_parser = HelpPrintingOptionParser(epilog_method=self.help_command._help_epilog, prog=name, usage=self._usage_line())

    @classmethod
    def _add_all_subclasses(cls, class_to_crawl, seen_classes):
        for subclass in class_to_crawl.__subclasses__():
            if subclass not in seen_classes:
                seen_classes.add(subclass)
                cls._add_all_subclasses(subclass, seen_classes)

    @classmethod
    def _find_all_commands(cls):
        commands = set()
        cls._add_all_subclasses(Command, commands)
        return sorted(commands)

    @staticmethod
    def _usage_line():
        return "Usage: %prog [options] COMMAND [ARGS]"

    def name(self):
        return self.global_option_parser.get_prog_name()

    def handle_global_args(self, args):
        (options, args) = self.global_option_parser.parse_args(args)
        # We should never hit this because _split_args splits at the first arg without a leading "-".
        if args:
            self.global_option_parser.error("Extra arguments before command: %s" % args)

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
            if command_name == command.name:
                return command
        return None

    def path(self):
        raise NotImplementedError, "subclasses must implement"

    def should_show_in_main_help(self, command):
        return command.show_in_main_help

    def should_execute_command(self, command):
        raise NotImplementedError, "subclasses must implement"

    def main(self, argv=sys.argv):
        (global_args, command_name, args_after_command_name) = self._split_args(argv[1:])

        # Handle --help, etc:
        self.handle_global_args(global_args)

        command = self.command_by_name(command_name) or self.help_command
        if not command:
            self.global_option_parser.error("%s is not a recognized command" % command_name)

        (should_execute, failure_reason) = self.should_execute_command(command)
        if not should_execute:
            log(failure_reason)
            return 0

        return command.check_arguments_and_execute(args_after_command_name, self)
