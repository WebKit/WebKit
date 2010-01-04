#!/usr/bin/python
# -*- coding: utf-8; -*-
#
# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2009 Torch Mobile Inc.
# Copyright (C) 2009 Apple Inc. All rights reserved.
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

import style
# FIXME: Eliminate cpp_style dependency.
import cpp_style


class ArgumentParserTest(unittest.TestCase):
    """Tests argument parsing."""
    def test_parse_arguments(self):
        old_usage = style._USAGE
        old_style_categories = style._STYLE_CATEGORIES
        old_webkit_filter_rules = style._WEBKIT_FILTER_RULES
        old_output_format = cpp_style._cpp_style_state.output_format
        old_verbose_level = cpp_style._cpp_style_state.verbose_level
        old_filters = cpp_style._cpp_style_state.filters
        try:
            # Don't print usage during the tests, or filter categories
            style._USAGE = ''
            style._STYLE_CATEGORIES = []
            style._WEBKIT_FILTER_RULES = []

            self.assertRaises(SystemExit, style.parse_arguments, ['--badopt'])
            self.assertRaises(SystemExit, style.parse_arguments, ['--help'])
            self.assertRaises(SystemExit, style.parse_arguments, ['--filter='])
            # This is illegal because all filters must start with + or -
            self.assertRaises(ValueError, style.parse_arguments, ['--filter=foo'])
            self.assertRaises(ValueError, style.parse_arguments,
                              ['--filter=+a,b,-c'])

            self.assertEquals((['foo.cpp'], {}), style.parse_arguments(['foo.cpp']))
            self.assertEquals(old_output_format, cpp_style._cpp_style_state.output_format)
            self.assertEquals(old_verbose_level, cpp_style._cpp_style_state.verbose_level)

            self.assertEquals(([], {}), style.parse_arguments([]))
            self.assertEquals(([], {}), style.parse_arguments(['--v=0']))

            self.assertEquals((['foo.cpp'], {}),
                              style.parse_arguments(['--v=1', 'foo.cpp']))
            self.assertEquals(1, cpp_style._cpp_style_state.verbose_level)
            self.assertEquals((['foo.h'], {}),
                              style.parse_arguments(['--v=3', 'foo.h']))
            self.assertEquals(3, cpp_style._cpp_style_state.verbose_level)
            self.assertEquals((['foo.cpp'], {}),
                              style.parse_arguments(['--verbose=5', 'foo.cpp']))
            self.assertEquals(5, cpp_style._cpp_style_state.verbose_level)
            self.assertRaises(ValueError,
                              style.parse_arguments, ['--v=f', 'foo.cpp'])

            self.assertEquals((['foo.cpp'], {}),
                              style.parse_arguments(['--output=emacs', 'foo.cpp']))
            self.assertEquals('emacs', cpp_style._cpp_style_state.output_format)
            self.assertEquals((['foo.h'], {}),
                              style.parse_arguments(['--output=vs7', 'foo.h']))
            self.assertEquals('vs7', cpp_style._cpp_style_state.output_format)
            self.assertRaises(SystemExit,
                              style.parse_arguments, ['--output=blah', 'foo.cpp'])

            filt = '-,+whitespace,-whitespace/indent'
            self.assertEquals((['foo.h'], {}),
                              style.parse_arguments(['--filter='+filt, 'foo.h']))
            self.assertEquals(['-', '+whitespace', '-whitespace/indent'],
                              cpp_style._cpp_style_state.filters)

            self.assertEquals((['foo.cpp', 'foo.h'], {}),
                              style.parse_arguments(['foo.cpp', 'foo.h']))

            self.assertEquals((['foo.cpp'], {'--foo': ''}),
                              style.parse_arguments(['--foo', 'foo.cpp'], ['foo']))
            self.assertEquals((['foo.cpp'], {'--foo': 'bar'}),
                              style.parse_arguments(['--foo=bar', 'foo.cpp'], ['foo=']))
            self.assertEquals((['foo.cpp'], {}),
                              style.parse_arguments(['foo.cpp'], ['foo=']))
            self.assertRaises(SystemExit,
                              style.parse_arguments,
                              ['--footypo=bar', 'foo.cpp'], ['foo='])
        finally:
            style._USAGE = old_usage
            style._STYLE_CATEGORIES = old_style_categories
            style._WEBKIT_FILTER_RULES = old_webkit_filter_rules
            cpp_style._cpp_style_state.output_format = old_output_format
            cpp_style._cpp_style_state.verbose_level = old_verbose_level
            cpp_style._cpp_style_state.filters = old_filters


if __name__ == '__main__':
    import sys

    unittest.main()
