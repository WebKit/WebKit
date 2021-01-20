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

from __future__ import print_function

import argparse
import os
import subprocess
import sys
import traceback

# Gather information about our debugging environment right now. Do this before
# executing any "main" code so that our debugging preferences are in place by
# the time we get there and we can debug from main() on down as opposed to
# parse_args() on down.

SHOW_DEBUG_LOGGING = "-d" in sys.argv or "--debug" in sys.argv

if SHOW_DEBUG_LOGGING:

    DEBUG_LOGGING_FILE = None
    for index, arg in enumerate(sys.argv):
        if arg == "--debug-file":
            if index + 1 < len(sys.argv):
                DEBUG_LOGGING_FILE = sys.argv[index + 1]

    # Bottleneck function for printing debugging lines to either the console or
    # the screen, as appropriate.

    if DEBUG_LOGGING_FILE:

        def debug_log(msg):
            with open(DEBUG_LOGGING_FILE, "a") as f:
                print(msg, file=f)
    else:

        def debug_log(msg):
            print(msg)

    # Context Manager class for logging information about function entry/exit.
    # On entry, the function name is logged along with its parameters. On exit,
    # the function name is logged with its result. If an exception occurs, the
    # function name is logged with the exception. In all cases, the logging is
    # indented according to the level of the function in the backtrace.
    #
    # Versions of these classes exist for instance methods, class methods, and
    # global functions so that instance and class information can be extracted
    # and displayed.

    class LogEntryHelper(object):
        __slots__ = ["indent", "class_name", "function_name", "type"]

        def __init__(self, func, type):
            tb = traceback.extract_stack()
            self.indent = " " * 2 * (len(tb) - 3)
            self.class_name = None
            self.function_name = func.__name__
            self.type = type

        def log_entry(self, args, kwargs):
            if self.type == "instance":
                self.class_name = args[0].__class__.__name__ + "."
                args = args[1:]
            elif self.type == "class":
                self.class_name = args[0].__name__ + "."
                args = args[1:]
            else:
                self.class_name = ""

            self._print("args={}, kwargs={}".format(args, kwargs))

        def log_result(self, result):
            if hasattr(result, '__iter__') and not isinstance(result, str):
                for line in result:
                    self._print("result={}".format(line))
            else:
                self._print("result={}".format(result))

        def log_exception(self, exc):
            self._print("exception={}".format(exc))

        def _print(self, msg):
            debug_log("{}{}{}: {}".format(self.indent, self.class_name, self.function_name, msg))

    def LogEntryExit(func):
        def _show_debug_logging(*args, **kwargs):
            helper = LogEntryHelper(func, "instance")
            helper.log_entry(args, kwargs)
            try:
                result = func(*args, **kwargs)
                helper.log_result(result)
                return result
            except BaseException as e:
                helper.log_exception(e)
                raise
        return _show_debug_logging

    def LogEntryExitClass(func):
        def _show_debug_logging(*args, **kwargs):
            helper = LogEntryHelper(func, "class")
            helper.log_entry(args, kwargs)
            try:
                result = func(*args, **kwargs)
                helper.log_result(result)
                return result
            except BaseException as e:
                helper.log_exception(e)
                raise
        return _show_debug_logging

    def LogEntryExitGlobal(func):
        def _show_debug_logging(*args, **kwargs):
            helper = LogEntryHelper(func, None)
            helper.log_entry(args, kwargs)
            try:
                result = func(*args, **kwargs)
                helper.log_result(result)
                return result
            except BaseException as e:
                helper.log_exception(e)
                raise
        return _show_debug_logging

else:

    def debug_log(msg):
        pass

    def LogEntryExit(func):
        return func

    def LogEntryExitClass(func):
        return func

    def LogEntryExitGlobal(func):
        return func


# Utility function for operating similar to subprocess.run() in Python 3. One
# difference is that the result is a 2-tuple with stdout and stderr, rather
# than a 3-tuple that includes returncode. For our purposes, if returncode is
# non-zero, we raise an exception.

@LogEntryExitGlobal
def subprocess_run(args, **kwargs):
    kwargs["stdout"] = subprocess.PIPE
    kwargs["stderr"] = subprocess.PIPE
    input = None
    if "input" in kwargs:
        input = kwargs["input"]
        del kwargs["input"]
        kwargs["stdin"] = subprocess.PIPE
    process = subprocess.Popen(args, **kwargs)
    (stdout, stderr) = process.communicate(input=input)
    stdout = stdout.decode() if isinstance(stdout, bytes) else stdout
    stderr = stderr.decode() if isinstance(stderr, bytes) else stderr
    if process.returncode:
        raise CalledProcessError(process.returncode, args[0], stdout, stderr)
    return (stdout, stderr)


# Utility function to allow us to verify that we're running under Xcode or not.
# For example, if we are not, then we need to make sure that we don't try to
# access Xcode-specific environment variables.
#
# Note: This function use to check XCODE_INSTALL_PATH. Because if Xcode is
# running, it must have been installed. That's the theory, anyway. In
# actuality, it seems possible to install Xcode without the result having
# XCODE_INSTALL_PATH defined. So now we check XCODE_PRODUCT_BUILD_VERSION.

@LogEntryExitGlobal
def is_running_under_xcode():
    return os.environ.get("XCODE_PRODUCT_BUILD_VERSION")


# An argparse.Action subclass that validates the user-provided value against a
# list of valid values. Aliasing is supported; that is, the user can provide a
# value that can get mapped to a corresponding canonical value, and that
# resulting value is compared to the list of valid values.
#
# On error, calls parser.error().

class CheckValidItemAction(argparse.Action):
    @LogEntryExit
    def __init__(self, *args, **kwargs):
        self.item_type = kwargs.get("item_type", None)
        self.valid_items = kwargs.get("valid_items", None)
        self.aliases = kwargs.get("aliases", None)

        self.lowered_valid_items = [item.lower() for item in self.valid_items]

        kwargs.pop("item_type", None)
        kwargs.pop("valid_items", None)
        kwargs.pop("aliases", None)

        super(CheckValidItemAction, self).__init__(*args, **kwargs)

    @LogEntryExit
    def __call__(self, parser, namespace, values, option_string=None):
        try:
            validated = self.validate_item(values)
        except:
            parser.error("The {} \"{}\" is not supported.".format(self.item_type, values))
        items = getattr(namespace, self.dest, None)
        items = items[:] if items else []
        items.append(validated)
        setattr(namespace, self.dest, items)

    @LogEntryExit
    def validate_item(self, item):
        item = item.lower()
        try:
            validated_index = self.lowered_valid_items.index(item)
        except:
            if not self.aliases:
                raise
            item = self.aliases.get(item, None)
            validated_index = self.lowered_valid_items.index(item)
        return self.valid_items[validated_index]


# An argparse.Action subclass that validates the user-provided script command
# (generate, check, etc.)
#
# On error, calls parser.error().

class CheckCommandAction(argparse.Action):
    @LogEntryExit
    def __init__(self, *args, **kwargs):
        self.valid_commands = kwargs.get("valid_commands", None)
        kwargs.pop("valid_commands", None)
        super(CheckCommandAction, self).__init__(*args, **kwargs)

    @LogEntryExit
    def __call__(self, parser, namespace, value, option_string=None):
        if not value in self.valid_commands:
            parser.error('"{}" is not a valid command'.format(value))
        setattr(namespace, self.dest, value)


# Some Exceptions

class InvalidCommandError(Exception):
    pass


class InvalidArgumentError(Exception):
    pass


class InvalidConfigurationError(Exception):
    pass


# subprocess.CalledProcessError has problems with being pickled, which is
# something that we do to it. In particular, when unpickled, it throws an
# exception, and so CalledProcessError get's turned into an exception saying
# "__init__() takes at least 3 arguments (1 given)". Address this by creating
# our own CalledProcessError that's a little more generic.

class CalledProcessError(Exception):
    def __str__(self):
        returncode = self.args[0] if len(self.args) > 0 else None
        command = self.args[1] if len(self.args) > 1 else None
        stdout = self.args[2] if len(self.args) > 2 else None
        stderr = self.args[3] if len(self.args) > 3 else None

        if stderr:
            return "Command '{}' returned non-zero exit status {}: {}".format(command, returncode, stderr)
        elif stdout:
            return "Command '{}' returned non-zero exit status {}: {}".format(command, returncode, stdout)
        else:
            return "Command '{}' returned non-zero exit status {}".format(command, returncode)
