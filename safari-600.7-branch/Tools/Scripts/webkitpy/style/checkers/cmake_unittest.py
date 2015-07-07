# Copyright (C) 2012 Intel Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Unit test for cmake.py."""

import unittest2 as unittest

from cmake import CMakeChecker


class CMakeCheckerTest(unittest.TestCase):

    """Tests CMakeChecker class."""

    def test_init(self):
        """Test __init__() method."""
        def _mock_handle_style_error(self):
            pass

        checker = CMakeChecker("foo.cmake", _mock_handle_style_error)
        self.assertEqual(checker._handle_style_error, _mock_handle_style_error)

    def test_check(self):
        """Test check() method."""
        errors = []

        def _mock_handle_style_error(line_number, category, confidence,
                                     message):
            error = (line_number, category, confidence, message)
            errors.append(error)

        checker = CMakeChecker("foo.cmake", _mock_handle_style_error)

        lines = [
            '# This file is sample input for cmake_unittest.py and includes below problems:\n',
            'IF ()',
            '\tmessage("Error line with Tab")\n',
            '    message("Error line with endding spaces")    \n',
            '    message( "Error line with space after (")\n',
            '    message("Error line with space before (" )\n',
            '    MESSAGE("Error line with upper case non-condtional command")\n',
            '    MESSage("Error line with upper case non-condtional command")\n',
            '    message("correct message line")\n',
            'ENDif ()\n',
            '\n',
            'if()\n',
            'endif ()\n',
            '\n',
            'macro ()\n',
            'ENDMacro()\n',
            '\n',
            'function    ()\n',
            'endfunction()\n',
            '\n',
            'set(name a)\n',
            'set(name a b c)\n',
            'set(name a\n',
            'b)\n',
            'set(name',
            'abc\n',
            ')\n',
            'list(APPEND name a)\n',
            'list(APPEND name\n',
            'a\n',
            'a\n',
            ')\n',
            'list(APPEND name\n',
            'b\n',
            'a\n',
            '\n',
            'c/a.a\n',
            '\n',
            'c/b/a.a\n',
            '${aVariable}\n',
            '\n',
            'c/c.c\n',
            '\n',
            'c/b/a.a\n',
            ')\n',
            'list(REMOVE_ITEM name a)\n',
            'list(REMOVE_ITEM name\n',
            'a\n',
            '\n',
            'b\n',
            ')\n',
            'list(REMOVE_ITEM name\n',
            'a/a.a\n',
            'a/b.b\n',
            'b/a.a\n',
            '\n',
            '\n',
            'c/a.a\n',
            ')\n',
            ]
        checker.check(lines)

        self.maxDiff = None
        self.assertEqual(errors, [
            (3, 'whitespace/tab', 5, 'Line contains tab character.'),
            (2, 'command/lowercase', 5, 'Use lowercase command "if"'),
            (4, 'whitespace/trailing', 5, 'No trailing spaces'),
            (5, 'whitespace/parentheses', 5, 'No space after "("'),
            (6, 'whitespace/parentheses', 5, 'No space before ")"'),
            (7, 'command/lowercase', 5, 'Use lowercase command "message"'),
            (8, 'command/lowercase', 5, 'Use lowercase command "message"'),
            (10, 'command/lowercase', 5, 'Use lowercase command "endif"'),
            (12, 'whitespace/parentheses', 5, 'One space between command "if" and its parentheses, should be "if ("'),
            (15, 'whitespace/parentheses', 5, 'No space between command "macro" and its parentheses, should be "macro("'),
            (16, 'command/lowercase', 5, 'Use lowercase command "endmacro"'),
            (18, 'whitespace/parentheses', 5, 'No space between command "function" and its parentheses, should be "function("'),
            (23, 'list/parentheses', 5, 'First listitem "a" should be in a new line.'),
            (24, 'list/parentheses', 5, 'The parentheses after the last listitem "b" should be in a new line.'),
            (31,  'list/duplicate', 5, 'The item "a" should be added only once to the list.'),
            (35, 'list/order', 5, 'Alphabetical sorting problem. "a" should be before "b".'),
            (41, 'list/order', 5, 'Alphabetical sorting problem. "c/c.c" should be before "c/b/a.a".'),
            (49, 'list/emptyline', 5, 'There should be no empty line between "a" and "b".'),
            (54, 'list/emptyline', 5, 'There should be exactly one empty line instead of 0 between "a/b.b" and "b/a.a".'),
            (57, 'list/emptyline', 5, 'There should be exactly one empty line instead of 2 between "b/a.a" and "c/a.a".'),
            ])
