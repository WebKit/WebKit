#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2019 Apple Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Application represents the main operation of the script. It's a singleton,
# created by main() and then invoked to run everything. This class parses the
# command-line options, validates them, creates and invokes BaseGenerators,
# reports the results, and handles the catching and reporting of any
# exceptions/errors.

from __future__ import print_function

import argparse
import itertools
import os
import sys
import textwrap
import traceback

from functools import reduce

import webkitpy.generate_xcfilelists_lib.generators as Generators
import webkitpy.generate_xcfilelists_lib.util as util


EX_GENERAL_ERROR = 1  # General error
EX_ACTION_REQUIRED = 2  # Returned when script determines that the generated .xcfilelist files have changed.


class Application(object):

    __slots__ = (
        "command_file",
        "parser", "cmd_line_args",
        "dispatch", "project_specific_generators",
        "supported_project_tags", "supported_platforms", "supported_configurations")

    # Aliases for platforms. These are handy when using the script from the
    # command line and you don't remember if it's "ios" or "iphoneos, or "tvos"
    # or "appletvos". This list of aliases will let you use any of those.

    platform_aliases = {
        "ios":          "iphoneos",
        "iphone":       "iphoneos",
        "simulator":    "iphonesimulator",
        "sim":          "iphonesimulator",
        "mac":          "macosx",
        "macos":        "macosx",
        "osx":          "macosx",
        "tvos":         "appletvos",
        "tv":           "appletvos",
        "tvsimulator":  "appletvsimulator",
        "watch":        "watchos",
    }

    @util.LogEntryExit
    def __init__(self, command_file):
        self.command_file = os.path.realpath(command_file)
        self.parser = None
        self.cmd_line_args = None

        self.dispatch = {
            "generate":       self._cmd_set_environment_and_generate,
            "generate-xcode": self._cmd_generate_within_xcode,
            "check":          self._cmd_set_environment_and_check,
            "check-xcode":    self._cmd_check_within_xcode,
            "generate-inner": self._cmd_generate_within_xcode_and_return_results_to_caller,
            "help":           self._cmd_help,
        }

        self.project_specific_generators = {
            "JavaScriptCore":   Generators.JavaScriptCoreGenerator,
            "WebCore":          Generators.WebCoreGenerator,
            "WebKit":           Generators.WebKitGenerator,
            "WebKitLegacy":     Generators.WebKitLegacyGenerator,
            "DumpRenderTree":   Generators.DumpRenderTreeGenerator,
            "WebKitTestRunner": Generators.WebKitTestRunnerGenerator,
            "TestWebKitAPI":    Generators.TestWebKitAPIGenerator,
        }

        self.supported_project_tags = None
        self.supported_platforms = None
        self.supported_configurations = None

    @util.LogEntryExit
    def run(self):
        try:
            self._initialize()

            self.parser = self._create_parser()
            self.cmd_line_args = args = self.parser.parse_args()

            if args.help:
                return self._cmd_help(os.EX_OK)

            self._validate_args(args)

            try:
                func = self.dispatch[args.command]
            except KeyError as e:
                raise util.InvalidCommandError(args.command)

            return func()

        except util.InvalidArgumentError as e:
            print("### Invalid argument: {}".format(e))
            return self._cmd_help(os.EX_USAGE)

        except util.InvalidCommandError as e:
            if e.args:
                print("### Invalid command: {}".format(e))
            else:
                print("### Missing command")
            return self._cmd_help(os.EX_USAGE)

        except SystemExit:
            raise

        except BaseException as e:
            traceback.print_exc()
            return os.EX_SOFTWARE

    # Perform some post __init__ initialization. This is performed after
    # __init__ so that we can respond to any information provided in any
    # sub-class's __init__.

    @util.LogEntryExit
    def _initialize(self):
        def collect_attributes(key):
            configurations = set()
            for project_tag in self.project_specific_generators:
                configurations |= set(key(self.project_specific_generators[project_tag]))
            return configurations

        self.supported_project_tags = sorted(list(self.project_specific_generators.keys()))
        self.supported_platforms = sorted(list(collect_attributes(lambda gen_cls: gen_cls.VALID_PLATFORMS)))
        self.supported_configurations = sorted(list(collect_attributes(lambda gen_cls: gen_cls.VALID_CONFIGURATIONS)))

    @util.LogEntryExit
    def _create_parser(self):
        valid_commands = ("generate", "generate-xcode", "check", "check-xcode", "generate-inner", "help")
        valid_commands_prompt = "(" + " | ".join(valid_commands) + ")"

        parser = argparse.ArgumentParser(add_help=False,
                formatter_class=argparse.RawDescriptionHelpFormatter,
                description="""\
Generate or check .xcfilelist files. One of the following commands must be
specified on the command-line:

  generate              Generate a complete and up-to-date set of .xcfilelist
                        files and copy them to their appropriate places in the
                        project directories.
  generate-xcode        Similar to generate, but to be called from within Xcode.
  check                 Generate a complete and up-to-date set of .xcfilelist
                        files and compare them to their counterparts in the
                        project directories.
  check-xcode           Similar to check, but to be called from within Xcode.
  generate-inner        [Used by script internals] Generate an .xcfilelist file
                        for a particular combination of project, platform, and
                        configuration. This operation is performed in the
                        context of an Xcode build in order to inherit the same
                        environment as that build. Once generated, the results
                        are returned to the calling instance of this script.
  help                  Print this text and exit.""")

        parser.add_argument("command", action=util.CheckCommandAction,
                valid_commands=valid_commands, metavar=valid_commands_prompt,
                help="""\
                        The operation to perform.""")

        parser.add_argument("--project", action=util.CheckValidItemAction,
                item_type="project",
                valid_items=self.supported_project_tags,
                dest="project_tags", metavar="<PROJECT>", help="""\
                        Specify which project or projects for which to generate
                        .xcfilelist files or to check. Possible values are
                        ({}). Can be specified more than once. Default is to
                        iterate over all projects.""".format(
                            ", ".join(self.supported_project_tags)))
        parser.add_argument("--platform", action=util.CheckValidItemAction,
                item_type="platform",
                valid_items=self.supported_platforms,
                aliases=self.platform_aliases,
                dest="platforms", metavar="<PLATFORM>", help="""\
                        Specify which platform or platforms for which to
                        generate .xcfilelist files or to check. Possible values
                        are ({}, plus common aliases). Can be specified more
                        than once. Default is to iterate over all platforms,
                        filtered to those platforms that a particular project
                        supports (e.g., you can't specify 'iphoneos' for
                        WebKitTestRunner).""".format(
                            ", ".join(self.supported_platforms)))
        parser.add_argument("--configuration", action=util.CheckValidItemAction,
                item_type="configuration",
                valid_items=self.supported_configurations,
                dest="configurations", metavar="<CONFIGURATION>", help="""\
                        Specify which configuration or configurations for which
                        to generate .xcfilelist files or to check. Possible
                        values are ({}). Can be specified more than once.
                        Default is to iterate over all
                        configurations.""".format(
                            ", ".join(self.supported_configurations)))
        parser.add_argument("--xcode", metavar="<WORKSPACE>", help="""\
                        If the existing build output was created by building
                        with Xcode, specify the path to the workspace that was
                        used.""")
        parser.add_argument("-d", "--debug", action="store_true", help="""\
                        Provide verbose output.""")
        parser.add_argument("--debug-file", help="""\
                        [Used by script internals] Name of the file to which to
                        write debug information. Used when this script
                        sub-launches itself and needs to collect the debug
                        information from the sub-launched instance. Not
                        normally used when this script is invoked from the
                        command-line or Xcode.""")
        parser.add_argument("--pickle-file", help="""\
                        [Used by script internals] Name of the file used to
                        store results to be transported out from the Xcode
                        execution environment out to an outer layer. This
                        parameter is only used with the 'generate-core'
                        command.""")
        parser.add_argument("-q", "--quiet", action="store_true", help="""\
                        Don't print any standard output.""")
        parser.add_argument("-h", "--help", action="store_true", help="""\
                        Print this text and exit.""")

        setattr(parser, "application", self)
        return parser

    @util.LogEntryExit
    def _validate_args(self, args):
        if not self.cmd_line_args.project_tags:
            self.cmd_line_args.project_tags = self.supported_project_tags
        if not self.cmd_line_args.platforms:
            self.cmd_line_args.platforms = self.supported_platforms
        if not self.cmd_line_args.configurations:
            self.cmd_line_args.configurations = self.supported_configurations

        if util.is_running_under_xcode():
            assert len(self.cmd_line_args.project_tags) == 1
            assert len(self.cmd_line_args.platforms) == 1
            assert len(self.cmd_line_args.configurations) == 1

    @util.LogEntryExit
    def _cmd_set_environment_and_generate(self):
        generators = self._do_set_environment_and_generate()
        generators = self._do_merge(generators)
        return self._report_results(generators)

    @util.LogEntryExit
    def _cmd_generate_within_xcode(self):
        generators = self._do_generate()
        generators = self._do_merge(generators)
        return self._report_results(generators)

    @util.LogEntryExit
    def _cmd_set_environment_and_check(self):
        generators = self._do_set_environment_and_generate()
        return self._report_results(generators)

    @util.LogEntryExit
    def _cmd_check_within_xcode(self):
        generators = self._do_generate()
        return self._report_results(generators)

    @util.LogEntryExit
    def _cmd_generate_within_xcode_and_return_results_to_caller(self):
        generators = self._do_generate()
        with open(self.cmd_line_args.pickle_file, "wb") as f:
            for generator in generators:
                generator.pickle_to_file(f)
        return os.EX_OK

    @util.LogEntryExit
    def _cmd_help(self, status=os.EX_OK):
        self.parser.print_help()
        return status

    @util.LogEntryExit
    def _do_set_environment_and_generate(self):
        def core_operation(generator, generators):
            new_generators = generator.set_environment_and_generate()
            generators.extend(new_generators)
            return generators
        return self._do_generate_common(core_operation)

    @util.LogEntryExit
    def _do_generate(self):
        def core_operation(generator, generators):
            generator.generate()
            generators.append(generator)
            return generators
        return self._do_generate_common(core_operation)

    @util.LogEntryExit
    def _do_generate_common(self, core_operation):
        generators = []

        for triple in itertools.product(
                self.cmd_line_args.project_tags,
                self.cmd_line_args.platforms,
                self.cmd_line_args.configurations):
            generator = self.project_specific_generators[triple[0]](self, *triple)
            if not generator.is_valid():
                continue
            self._log_progress("Generating .xcfilelists for {}/{}/{}".format(*triple))
            try:
                generators = core_operation(generator, generators)
            except BaseException as e:
                # TODO: Turn the traceback into a string, and then allow
                # this field to be pickled and printed by the calling
                # context. Right now, pickling raises an exception if it
                # encounters a Traceback object. See BaseGenerator.pickle_to_file.
                (generator.ex_type, generator.ex_value, generator.ex_traceback) = sys.exc_info()
            if generator.has_error():
                sys.exit(self._report_results([generator]))

        return generators

    @util.LogEntryExit
    def _do_merge(self, generators):
        if self._any_have_errors(generators):
            return generators

        for generator in generators:
            if generator.has_action():
                self._log_progress("Merging .xcfilelists for {}/{}/{}".format(*generator.triple))
                generator.merge()

        return generators

    @util.LogEntryExit
    def _report_results(self, generators):
        generators_with_errors = [generator for generator in generators if generator.has_error()]
        if generators_with_errors:
            for generator in generators_with_errors:
                generator.report_error()
            return EX_GENERAL_ERROR

        generators_with_actions = [generator for generator in generators if generator.has_action()]
        if generators_with_actions:
            if self.cmd_line_args.command == "generate" or self.cmd_line_args.command == "generate-xcode":
                self._report_merge_results(generators_with_actions)
            else:
                self._report_remediation_steps(generators_with_actions)
            return EX_ACTION_REQUIRED

        return os.EX_OK

    @util.LogEntryExit
    def _report_merge_results(self, generators):
        message = textwrap.wrap(
                "\".xcfilelist\" files tell the build system what files are " +
                "consumed and produced by the \"Run Script\" build phases in " +
                "Xcode. At least one of these .xcfilelist files was out of date " +
                "and has been updated. You now need to restart your build.", 90)

        self._log_results("")
        for line in message:
            self._log_results(line)

    @util.LogEntryExit
    def _report_remediation_steps(self, generators):
        message = textwrap.wrap("One or more \".xcfilelist\" files are out of date. Regenerate them by running the following commands:", 90)
        message.append("")

        def add_to_message(generator, message):
            if generator.has_action():
                message.append("    `Tools/Scripts/generate-xcfilelists generate --project {} --platform {} --configuration {}{}`\n".format(
                        generator.project_tag, generator.platform, generator.configuration,
                        " --xcode {}".format(self.cmd_line_args.xcode) if self.cmd_line_args.xcode else ""))
            return message

        for generator in generators:
            message = add_to_message(generator, message)

        for line in message:
            self._log_results(line)

    @util.LogEntryExit
    def _any_have_errors(self, generators):
        return reduce(lambda acc, generator: acc or generator.has_error(), generators, None)

    @util.LogEntryExit
    def _any_have_actions(self, generators):
        return reduce(lambda acc, generator: acc or generator.has_action(), generators, None)

    @util.LogEntryExit
    def _log_progress(self, message):
        if not self.cmd_line_args.quiet:
            print("### {}".format(message))

    @util.LogEntryExit
    def _log_results(self, message):
        if not self.cmd_line_args.quiet:
            print("{}".format(message))

    # Return the path to the script to sublaunch.

    @util.LogEntryExit
    def get_generate_xcfilelists_script_path(self):
        return self.command_file

    # Return the parent of the WebKit check-out directory.

    @util.LogEntryExit
    def _get_root_parent_dir(self):
        return os.path.dirname(     # Remove "OpenSource"
                os.path.dirname(        # Remove "Tools"
                    os.path.dirname(        # Remove "Scripts"
                        os.path.dirname(        # Remove script name
                            self.get_generate_xcfilelists_script_path()))))

    # Return the path to the WebKit check-out directory.

    @util.LogEntryExit
    def get_opensource_dir(self):
        return os.path.join(self._get_root_parent_dir(), "OpenSource")

    # Return the path to the directory containing supporting build scripts.

    @util.LogEntryExit
    def get_build_scripts_dir(self):
        return os.path.join(self.get_opensource_dir(), "Source", "WTF", "Scripts")

    # Return the path to a supporting build script.

    @util.LogEntryExit
    def get_extract_dependencies_from_makefile_script(self):
        return os.path.join(self.get_opensource_dir(), "Tools", "Scripts", "extract-dependencies-from-makefile")

    # Return $(BUILT_PRODUCTS_DIR)
    # aka $(CONFIGURATION_BUILD_DIR)
    # aka $(BUILD_DIR)/$(CONFIGURATION)$(EFFECTIVE_PLATFORM_NAME)

    @util.LogEntryExit
    def get_xcode_built_products_dir(self):
        assert util.is_running_under_xcode()
        return self._getenv("BUILT_PRODUCTS_DIR")

    @util.LogEntryExit
    def get_xcode_project_temp_dir(self):
        assert util.is_running_under_xcode()
        return self._getenv("PROJECT_TEMP_DIR")

    # Return the named environment variable.
    @util.LogEntryExit
    def _getenv(self, variable_name):
        return os.environ.get(variable_name)
