#!/usr/bin/python
# -*- coding: utf-8; -*-
#
# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2009 Torch Mobile Inc.
# Copyright (C) 2009 Apple Inc. All rights reserved.
# Copyright (C) 2010 Chris Jerdonek (chris.jerdonek@gmail.com)
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

"""Unit tests for style.py."""

import unittest

import checker as style


class DefaultArgumentsTest(unittest.TestCase):

    """Tests validity of default arguments used by check-webkit-style."""

    def test_filter_rules(self):
        already_seen = []
        for rule in style.WEBKIT_FILTER_RULES:
            # All categories are on by default, so defaults should
            # begin with -.
            self.assertTrue(rule.startswith('-'))
            self.assertTrue(rule[1:] in style.STYLE_CATEGORIES)
            self.assertFalse(rule in already_seen)
            already_seen.append(rule)

    def test_defaults(self):
        """Check that default arguments are valid."""
        defaults = style.ArgumentDefaults(style.DEFAULT_OUTPUT_FORMAT,
                                          style.DEFAULT_VERBOSITY,
                                          style.WEBKIT_FILTER_RULES)
        # FIXME: We should not need to call parse() to determine
        #        whether the default arguments are valid.
        parser = style.ArgumentParser(defaults)
        # No need to test the return value here since we test parse()
        # on valid arguments elsewhere.
        parser.parse([]) # arguments valid: no error or SystemExit


class ArgumentPrinterTest(unittest.TestCase):

    """Tests the ArgumentPrinter class."""

    _printer = style.ArgumentPrinter()

    def _create_options(self, output_format='emacs', verbosity=3,
                        filter_rules=[], git_commit=None,
                        extra_flag_values={}):
        return style.ProcessorOptions(output_format, verbosity, filter_rules,
                                      git_commit, extra_flag_values)

    def test_to_flag_string(self):
        options = self._create_options('vs7', 5, ['+foo', '-bar'], 'git',
                                       {'a': 0, 'z': 1})
        self.assertEquals('--a=0 --filter=+foo,-bar --git-commit=git '
                          '--output=vs7 --verbose=5 --z=1',
                          self._printer.to_flag_string(options))

        # This is to check that --filter and --git-commit do not
        # show up when not user-specified.
        options = self._create_options()
        self.assertEquals('--output=emacs --verbose=3',
                          self._printer.to_flag_string(options))


class ArgumentParserTest(unittest.TestCase):

    """Test the ArgumentParser class."""

    def _parse(self):
        """Return a default parse() function for testing."""
        return self._create_parser().parse

    def _create_defaults(self, default_output_format='vs7',
                         default_verbosity=3,
                         default_filter_rules=['-', '+whitespace']):
        """"Return a default ArgumentDefaults instance for testing."""
        return style.ArgumentDefaults(default_output_format,
                                      default_verbosity,
                                      default_filter_rules)

    def _create_parser(self, defaults=None):
        """"Return an ArgumentParser instance for testing."""
        def create_usage(_defaults):
            """Return a usage string for testing."""
            return "usage"

        def doc_print(message):
            # We do not want the usage string or style categories
            # to print during unit tests, so print nothing.
            return

        if defaults is None:
            defaults = self._create_defaults()

        return style.ArgumentParser(defaults, create_usage, doc_print)

    def test_parse_documentation(self):
        parse = self._parse()

        # FIXME: Test both the printing of the usage string and the
        #        filter categories help.

        # Request the usage string.
        self.assertRaises(SystemExit, parse, ['--help'])
        # Request default filter rules and available style categories.
        self.assertRaises(SystemExit, parse, ['--filter='])

    def test_parse_bad_values(self):
        parse = self._parse()

        # Pass an unsupported argument.
        self.assertRaises(SystemExit, parse, ['--bad'])

        self.assertRaises(ValueError, parse, ['--verbose=bad'])
        self.assertRaises(ValueError, parse, ['--verbose=0'])
        self.assertRaises(ValueError, parse, ['--verbose=6'])
        parse(['--verbose=1']) # works
        parse(['--verbose=5']) # works

        self.assertRaises(ValueError, parse, ['--output=bad'])
        parse(['--output=vs7']) # works

        # Pass a filter rule not beginning with + or -.
        self.assertRaises(ValueError, parse, ['--filter=foo'])
        parse(['--filter=+foo']) # works
        # Pass files and git-commit at the same time.
        self.assertRaises(SystemExit, parse, ['--git-commit=', 'file.txt'])
        # Pass an extra flag already supported.
        self.assertRaises(ValueError, parse, [], ['filter='])
        parse([], ['extra=']) # works
        # Pass an extra flag with typo.
        self.assertRaises(SystemExit, parse, ['--extratypo='], ['extra='])
        parse(['--extra='], ['extra=']) # works
        self.assertRaises(ValueError, parse, [], ['extra=', 'extra='])


    def test_parse_default_arguments(self):
        parse = self._parse()

        (files, options) = parse([])

        self.assertEquals(files, [])

        self.assertEquals(options.output_format, 'vs7')
        self.assertEquals(options.verbosity, 3)
        self.assertEquals(options.filter_rules, ['-', '+whitespace'])
        self.assertEquals(options.git_commit, None)

    def test_parse_explicit_arguments(self):
        parse = self._parse()

        # Pass non-default explicit values.
        (files, options) = parse(['--output=emacs'])
        self.assertEquals(options.output_format, 'emacs')
        (files, options) = parse(['--verbose=4'])
        self.assertEquals(options.verbosity, 4)
        (files, options) = parse(['--git-commit=commit'])
        self.assertEquals(options.git_commit, 'commit')
        (files, options) = parse(['--filter=+foo,-bar'])
        self.assertEquals(options.filter_rules,
                          ['-', '+whitespace', '+foo', '-bar'])

        # Pass extra flag values.
        (files, options) = parse(['--extra'], ['extra'])
        self.assertEquals(options.extra_flag_values, {'--extra': ''})
        (files, options) = parse(['--extra='], ['extra='])
        self.assertEquals(options.extra_flag_values, {'--extra': ''})
        (files, options) = parse(['--extra=x'], ['extra='])
        self.assertEquals(options.extra_flag_values, {'--extra': 'x'})

    def test_parse_files(self):
        parse = self._parse()

        (files, options) = parse(['foo.cpp'])
        self.assertEquals(files, ['foo.cpp'])

        # Pass multiple files.
        (files, options) = parse(['--output=emacs', 'foo.cpp', 'bar.cpp'])
        self.assertEquals(files, ['foo.cpp', 'bar.cpp'])


if __name__ == '__main__':
    import sys

    unittest.main()
