# -*- coding: utf-8 -*-
#
# Copyright (C) 2009, 2010, 2012 Google Inc. All rights reserved.
# Copyright (C) 2009 Torch Mobile Inc.
# Copyright (C) 2009-2022 Apple Inc. All rights reserved.
# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
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

# This is the modified version of Google's cpplint. The original code is
# http://google-styleguide.googlecode.com/svn/trunk/cpplint/cpplint.py

"""Support for check-webkit-style."""

import codecs
import functools
import math  # for log
import os
import os.path
import re
import string
import unicodedata

from webkitcorepy import unicode, Version

from webkitpy.style.checkers.common import match, search, sub, subn
from webkitpy.style.checkers.inclusive_language import InclusiveLanguageChecker
from webkitpy.common.memoized import memoized
from webkitpy.common.version_name_map import VersionNameMap

# The key to use to provide a class to fake loading a header file.
INCLUDE_IO_INJECTION_KEY = 'include_header_io'

# Headers that we consider STL headers.
_STL_HEADERS = frozenset([
    'algobase.h', 'algorithm', 'alloc.h', 'bitset', 'deque', 'exception',
    'function.h', 'functional', 'hash_map', 'hash_map.h', 'hash_set',
    'hash_set.h', 'iterator', 'list', 'list.h', 'map', 'memory', 'pair.h',
    'pthread_alloc', 'queue', 'set', 'set.h', 'sstream', 'stack',
    'stl_alloc.h', 'stl_relops.h', 'type_traits.h',
    'utility', 'vector', 'vector.h',
    ])


# Non-STL C++ system headers.
_CPP_HEADERS = frozenset([
    'algo.h', 'builtinbuf.h', 'bvector.h', 'cassert', 'cctype',
    'cerrno', 'cfloat', 'ciso646', 'climits', 'clocale', 'cmath',
    'complex', 'complex.h', 'csetjmp', 'csignal', 'cstdarg', 'cstddef',
    'cstdio', 'cstdlib', 'cstring', 'ctime', 'cwchar', 'cwctype',
    'defalloc.h', 'deque.h', 'editbuf.h', 'exception', 'fstream',
    'fstream.h', 'hashtable.h', 'heap.h', 'indstream.h', 'iomanip',
    'iomanip.h', 'ios', 'iosfwd', 'iostream', 'iostream.h', 'istream.h',
    'iterator.h', 'limits', 'map.h', 'multimap.h', 'multiset.h',
    'numeric', 'ostream.h', 'parsestream.h', 'pfstream.h', 'PlotFile.h',
    'procbuf.h', 'pthread_alloc.h', 'rope', 'rope.h', 'ropeimpl.h',
    'SFile.h', 'slist', 'slist.h', 'stack.h', 'stdexcept',
    'stdiostream.h', 'streambuf.h', 'stream.h', 'strfile.h', 'string',
    'strstream', 'strstream.h', 'tempbuf.h', 'tree.h', 'typeinfo', 'valarray',
    ])


# Assertion macros.  These are defined in base/logging.h and
# testing/base/gunit.h.  Note that the _M versions need to come first
# for substring matching to work.
_CHECK_MACROS = [
    'DCHECK', 'CHECK',
    'EXPECT_TRUE_M', 'EXPECT_TRUE',
    'ASSERT_TRUE_M', 'ASSERT_TRUE',
    'EXPECT_FALSE_M', 'EXPECT_FALSE',
    'ASSERT_FALSE_M', 'ASSERT_FALSE',
    ]

# Replacement macros for CHECK/DCHECK/EXPECT_TRUE/EXPECT_FALSE
_CHECK_REPLACEMENT = dict([(m, {}) for m in _CHECK_MACROS])

for op, replacement in [('==', 'EQ'), ('!=', 'NE'),
                        ('>=', 'GE'), ('>', 'GT'),
                        ('<=', 'LE'), ('<', 'LT')]:
    _CHECK_REPLACEMENT['DCHECK'][op] = 'DCHECK_%s' % replacement
    _CHECK_REPLACEMENT['CHECK'][op] = 'CHECK_%s' % replacement
    _CHECK_REPLACEMENT['EXPECT_TRUE'][op] = 'EXPECT_%s' % replacement
    _CHECK_REPLACEMENT['ASSERT_TRUE'][op] = 'ASSERT_%s' % replacement
    _CHECK_REPLACEMENT['EXPECT_TRUE_M'][op] = 'EXPECT_%s_M' % replacement
    _CHECK_REPLACEMENT['ASSERT_TRUE_M'][op] = 'ASSERT_%s_M' % replacement

for op, inv_replacement in [('==', 'NE'), ('!=', 'EQ'),
                            ('>=', 'LT'), ('>', 'LE'),
                            ('<=', 'GT'), ('<', 'GE')]:
    _CHECK_REPLACEMENT['EXPECT_FALSE'][op] = 'EXPECT_%s' % inv_replacement
    _CHECK_REPLACEMENT['ASSERT_FALSE'][op] = 'ASSERT_%s' % inv_replacement
    _CHECK_REPLACEMENT['EXPECT_FALSE_M'][op] = 'EXPECT_%s_M' % inv_replacement
    _CHECK_REPLACEMENT['ASSERT_FALSE_M'][op] = 'ASSERT_%s_M' % inv_replacement


# These constants define types of headers for use with
# _IncludeState.check_next_include_order().
_CONFIG_HEADER = 0
_PRIMARY_HEADER = 1
_OTHER_HEADER = 2
_SOFT_LINK_HEADER = 3
_MOC_HEADER = 4
_WEBKIT_ADDITIONS_HEADER = 5


# Files which is generated by some sort of code generator.
# There's no need to check the style of these files.
_AUTO_GENERATED_FILES = [
    # VisualStudio resource files
    'Tools/MiniBrowser/win/MiniBrowserLibResource.h',
]


# A dictionary of items customize behavior for unit test. For example,
# INCLUDE_IO_INJECTION_KEY allows providing a custom io class which allows
# for faking a header file.
_unit_test_config = {}


_NO_CONFIG_H_PATH_PATTERNS = [
    '^Source/bmalloc/',
    '^Source/WebKitLegacy/',
]

_EXPORT_MACRO_SPEC = {
    'BEXPORT': 'Source/bmalloc',
    'JS_EXPORT': 'Source/JavaScriptCore/API',
    'JS_EXPORT_PRIVATE': 'Source/JavaScriptCore',
    'PAL_EXPORT': 'Source/WebCore/PAL',
    'WEBCORE_TESTSUPPORT_EXPORT': 'Source/WebCore/testing',
    # Excludes PAL and testing directories
    'WEBCORE_EXPORT': 'Source/WebCore/(?!(PAL|testing))',
    'WK_EXPORT': 'Source/WebKit',
    'WTF_EXPORT_PRIVATE': 'Source/WTF',
}

_EXPORT_MACROS = sorted(_EXPORT_MACRO_SPEC.keys())

def iteratively_replace_matches_with_char(pattern, char_replacement, s):
    """Returns the string with replacement done.

    Every character in the match is replaced with char.
    Due to the iterative nature, pattern should not match char or
    there will be an infinite loop.

    Example:
      pattern = r'<[^>]>' # template parameters
      char_replacement =  '_'
      s =     'A<B<C, D>>'
      Returns 'A_________'

    Args:
      pattern: The regex to match.
      char_replacement: The character to put in place of every
                        character of the match.
      s: The string on which to do the replacements.

    Returns:
      True, if the given line is blank.
    """
    while True:
        matched = search(pattern, s)
        if not matched:
            return s
        start_match_index = matched.start(0)
        end_match_index = matched.end(0)
        match_length = end_match_index - start_match_index
        s = s[:start_match_index] + char_replacement * match_length + s[end_match_index:]


def _rfind_in_lines(regex, lines, start_position, not_found_position):
    """Does a reverse find starting at start position and going backwards until
    a match is found.

    Returns the position where the regex ended.
    """
    # Put the regex in a group and proceed it with a greedy expression that
    # matches anything to ensure that we get the last possible match in a line.
    last_in_line_regex = r'.*(' + regex + ')'
    current_row = start_position.row

    # Start with the given row and trim off everything past what may be matched.
    current_line = lines[start_position.row][:start_position.column]
    while True:
        found_match = match(last_in_line_regex, current_line)
        if found_match:
            return Position(current_row, found_match.end(1))

        # A match was not found so continue backward.
        current_row -= 1
        if current_row < 0:
            return not_found_position
        current_line = lines[current_row]


def _convert_to_lower_with_underscores(text):
    """Converts all text strings in camelCase or PascalCase to lowers with underscores."""

    # First add underscores before any capital letter followed by a lower case letter
    # as long as it is in a word.
    # (This put an underscore before Password but not P and A in WPAPassword).
    text = sub(r'(?<=[A-Za-z0-9])([A-Z])(?=[a-z])', r'_\1', text)

    # Next add underscores before capitals at the end of words if it was
    # preceeded by lower case letter or number.
    # (This puts an underscore before A in isA but not A in CBA).
    text = sub(r'(?<=[a-z0-9])([A-Z])(?=\b)', r'_\1', text)

    # Next add underscores when you have a captial letter which is followed by a capital letter
    # but is not proceeded by one. (This puts an underscore before A in 'WordADay').
    text = sub(r'(?<=[a-z0-9])([A-Z][A-Z_])', r'_\1', text)

    return text.lower()


def _create_acronym(text):
    """Creates an acronym for the given text."""
    # Removes all lower case letters except those starting words.
    text = sub(r'(?<!\b)[a-z]', '', text)
    return text.upper()


def up_to_unmatched_closing_paren(s):
    """Splits a string into two parts up to first unmatched ')'.

    Args:
      s: a string which is a substring of line after '('
      (e.g., "a == (b + c))").

    Returns:
      A pair of strings (prefix before first unmatched ')',
      remainder of s after first unmatched ')'), e.g.,
      up_to_unmatched_closing_paren("a == (b + c)) { ")
      returns "a == (b + c)", " {".
      Returns None, None if there is no unmatched ')'

    """
    i = 1
    for pos, c in enumerate(s):
        if c == '(':
            i += 1
        elif c == ')':
            i -= 1
            if i == 0:
                return s[:pos], s[pos + 1:]
    return None, None


class _IncludeState(dict):
    """Tracks line numbers for includes, and the order in which includes appear.

    As a dict, an _IncludeState object serves as a mapping between include
    filename and line number on which that file was included.

    Call check_next_include_order() once for each header in the file, passing
    in the type constants defined above. Calls in an illegal order will
    raise an _IncludeError with an appropriate error message.

    """
    # self._section will move monotonically through this set. If it ever
    # needs to move backwards, check_next_include_order will raise an error.
    _INITIAL_SECTION = 0
    _CONFIG_SECTION = 1
    _PRIMARY_SECTION = 2
    _OTHER_SECTION = 3
    _SOFT_LINK_SECTION = 4

    _TYPE_NAMES = {
        _CONFIG_HEADER: 'WebCore config.h',
        _PRIMARY_HEADER: 'header this file implements',
        _OTHER_HEADER: 'other header',
        _SOFT_LINK_HEADER: '*SoftLink.h header',
        _MOC_HEADER: 'moc file',
        _WEBKIT_ADDITIONS_HEADER: 'WebKitAdditions header',
        }
    _SECTION_NAMES = {
        _INITIAL_SECTION: "... nothing.",
        _CONFIG_SECTION: "WebCore config.h.",
        _PRIMARY_SECTION: 'a header this file implements.',
        _OTHER_SECTION: 'other header.',
        _SOFT_LINK_SECTION: 'soft-link header section.',
        }

    def __init__(self):
        dict.__init__(self)
        self._section = self._INITIAL_SECTION
        self._visited_primary_section = False
        self._visited_soft_link_section = False
        self.header_types = dict()

    def visited_primary_section(self):
        return self._visited_primary_section

    def visited_soft_link_section(self):
        return self._visited_soft_link_section

    def check_next_include_order(self, header_type, filename, file_is_header, primary_header_exists, has_config_header):
        """Returns a non-empty error message if the next header is out of order.

        This function also updates the internal state to be ready to check
        the next include.

        Args:
          header_type: One of the _XXX_HEADER constants defined above.
          filename: The name of the current file.
          file_is_header: Whether the file that owns this _IncludeState is itself a header
          primary_header_exists: Whether the primary header file actually exists on disk
          has_config_header: Whether project uses config.h or not.

        Returns:
          The empty string if the header is in the right order, or an
          error message describing what's wrong.

        """
        if header_type == _CONFIG_HEADER and file_is_header:
            return 'Header file should not contain WebCore config.h.'
        if header_type == _PRIMARY_HEADER and file_is_header:
            return 'Header file should not contain itself.'
        if header_type == _MOC_HEADER:
            return ''

        error_message = ''
        if self._section < self._OTHER_SECTION:
            before_error_message = ('Found %s before %s' %
                                    (self._TYPE_NAMES[header_type],
                                     self._SECTION_NAMES[self._section + 1]))
        after_error_message = ('Found %s after %s' %
                                (self._TYPE_NAMES[header_type],
                                 self._SECTION_NAMES[self._section]))

        if header_type == _CONFIG_HEADER:
            if self._section >= self._CONFIG_SECTION:
                error_message = after_error_message
            self._section = self._CONFIG_SECTION
        elif header_type == _PRIMARY_HEADER:
            if self._section >= self._PRIMARY_SECTION:
                error_message = after_error_message
            elif has_config_header and self._section < self._CONFIG_SECTION:
                error_message = before_error_message
            self._section = self._PRIMARY_SECTION
            self._visited_primary_section = True
        elif header_type == _OTHER_HEADER:
            if not file_is_header and self._section < self._PRIMARY_SECTION:
                if primary_header_exists and not filename.endswith('SoftLink.cpp'):
                    error_message = before_error_message
            self._section = self._OTHER_SECTION
        elif header_type == _SOFT_LINK_HEADER:
            if file_is_header:
                error_message = '{} should never be included in a header.'.format(
                    self._TYPE_NAMES[header_type])
            self._section = self._SOFT_LINK_SECTION
            self._visited_soft_link_section = True

        if not error_message and self.visited_soft_link_section() and header_type != _SOFT_LINK_HEADER and header_type != _WEBKIT_ADDITIONS_HEADER:
            error_message = '*SoftLink.h header should be included after all other headers.'

        return error_message


class Position(object):
    """Holds the position of something."""
    def __init__(self, row, column):
        self.row = row
        self.column = column

    def __str__(self):
        return '(%s, %s)' % (self.row, self.column)

    def __cmp__(self, other):
        return (self.row - other.row) or (self.column - other.column)

    def __eq__(self, other):
        return self.__cmp__(other) == 0

    def __ne__(self, other):
        return self.__cmp__(other) != 0

    def __lt__(self, other):
        return self.__cmp__(other) < 0

    def __le__(self, other):
        return self.__cmp__(other) <= 0

    def __gt__(self, other):
        return self.__cmp__(other) > 0

    def __ge__(self, other):
        return self.__cmp__(other) >= 0


class Parameter(object):
    """Information about one function parameter."""
    def __init__(self, parameter, parameter_name_index, row):
        self.type = parameter[:parameter_name_index].strip()
        # Remove any initializers from the parameter name (e.g. int i = 5).
        self.name = sub(r'=.*', '', parameter[parameter_name_index:]).strip()
        self.row = row

    @memoized
    def lower_with_underscores_name(self):
        """Returns the parameter name in the lower with underscores format."""
        return _convert_to_lower_with_underscores(self.name)


class SingleLineView(object):
    """Converts multiple lines into a single line (with line breaks replaced by a
       space) to allow for easier searching."""
    def __init__(self, lines, start_position, end_position):
        """Create a SingleLineView instance.

        Args:
          lines: a list of multiple lines to combine into a single line.
          start_position: offset within lines of where to start the single line.
          end_position: just after where to end (like a slice operation).
        """
        # Get the rows of interest.
        self.trimmed_lines = lines[start_position.row:end_position.row + 1]

        # Remove the columns on the last line that aren't included.
        self.trimmed_lines[-1] = self.trimmed_lines[-1][:end_position.column]

        # Remove the columns on the first line that aren't included.
        self.trimmed_lines[0] = self.trimmed_lines[0][start_position.column:]

        # Create a single line with all of the parameters.
        self.single_line = ' '.join(self.trimmed_lines)

        # Keep the row lengths, so we can calculate the original row number
        # given a column in the single line (adding 1 due to the space added
        # during the join).
        self._row_lengths = [len(line) + 1 for line in self.trimmed_lines]
        self._starting_row = start_position.row

    def convert_column_to_row(self, single_line_column_number):
        """Convert the column number from the single line into the original
        line number.

        Special cases:
        * Columns in the added spaces are considered part of the previous line.
        * Columns beyond the end of the line are consider part the last line
        in the view."""
        total_columns = 0
        row_offset = 0
        while row_offset < len(self._row_lengths) - 1 and single_line_column_number >= total_columns + self._row_lengths[row_offset]:
            total_columns += self._row_lengths[row_offset]
            row_offset += 1
        return self._starting_row + row_offset


def create_skeleton_parameters(all_parameters):
    """Converts a parameter list to a skeleton version.

    The skeleton only has one word for the parameter name, one word for the type,
    and commas after each parameter and only there. Everything in the skeleton
    remains in the same columns as the original."""
    all_simplifications = (
        # Remove template parameters, function declaration parameters, etc.
        r'(<[^<>]*?>)|(\([^\(\)]*?\))|(\{[^\{\}]*?\})',
        # Remove all initializers.
        r'=[^,]*',
        # Remove :: and everything before it.
        r'[^,]*::',
        # Remove modifiers like &, *.
        r'[&*]',
        # Remove const modifiers.
        r'\bconst\s+(?=[A-Za-z])',
        # Remove numerical modifiers like long.
        r'\b(unsigned|long|short)\s+(?=unsigned|long|short|int|char|double|float)')

    skeleton_parameters = all_parameters
    for simplification in all_simplifications:
        skeleton_parameters = iteratively_replace_matches_with_char(simplification, ' ', skeleton_parameters)
    # If there are any parameters, then add a , after the last one to
    # make a regular pattern of a , following every parameter.
    if skeleton_parameters.strip():
        skeleton_parameters += ','
    return skeleton_parameters


def find_parameter_name_index(skeleton_parameter):
    """Determines where the parameter name starts given the skeleton parameter."""
    # The first space from the right in the simplified parameter is where the parameter
    # name starts unless the first space is before any content in the simplified parameter.
    before_name_index = skeleton_parameter.rstrip().rfind(' ')
    if before_name_index != -1 and skeleton_parameter[:before_name_index].strip():
        return before_name_index + 1
    return len(skeleton_parameter)


def parameter_list(elided_lines, start_position, end_position):
    """Generator for a function's parameters."""
    # Create new positions that omit the outer parenthesis of the parameters.
    start_position = Position(row=start_position.row, column=start_position.column + 1)
    end_position = Position(row=end_position.row, column=end_position.column - 1)
    single_line_view = SingleLineView(elided_lines, start_position, end_position)
    skeleton_parameters = create_skeleton_parameters(single_line_view.single_line)
    end_index = -1

    while True:
        # Find the end of the next parameter.
        start_index = end_index + 1
        end_index = skeleton_parameters.find(',', start_index)

        # No comma means that all parameters have been parsed.
        if end_index == -1:
            return
        row = single_line_view.convert_column_to_row(end_index)

        # Parse the parameter into a type and parameter name.
        skeleton_parameter = skeleton_parameters[start_index:end_index]
        name_offset = find_parameter_name_index(skeleton_parameter)
        parameter = single_line_view.single_line[start_index:end_index]
        yield Parameter(parameter, name_offset, row)


class _FunctionState(object):
    """Tracks current function name and the number of lines in its body.

    Attributes:
      min_confidence: The minimum confidence level to use while checking style.

    """

    _NORMAL_TRIGGER = 250  # for --v=0, 500 for --v=1, etc.
    _TEST_TRIGGER = 400    # about 50% more than _NORMAL_TRIGGER.

    def __init__(self, min_confidence):
        self.min_confidence = min_confidence
        self.current_function = ''
        self.in_a_function = False
        self.lines_in_function = 0
        # Make sure these will not be mistaken for real positions (even when a
        # small amount is added to them).
        self.body_start_position = Position(-1000, 0)
        self.end_position = Position(-1000, 0)

    def begin(self, function_name, function_name_start_position, body_start_position, end_position,
              parameter_start_position, parameter_end_position, clean_lines):
        """Start analyzing function body.

        Args:
            function_name: The name of the function being tracked.
            function_name_start_position: Position in elided where the function name starts.
            body_start_position: Position in elided of the { or the ; for a prototype.
            end_position: Position in elided just after the final } (or ; is.
            parameter_start_position: Position in elided of the '(' for the parameters.
            parameter_end_position: Position in elided just after the ')' for the parameters.
            clean_lines: A CleansedLines instance containing the file.
        """
        self.in_a_function = True
        self.lines_in_function = -1  # Don't count the open brace line.
        self.current_function = function_name
        self.function_name_start_position = function_name_start_position
        self.body_start_position = body_start_position
        self.end_position = end_position
        self.is_declaration = clean_lines.elided[body_start_position.row][body_start_position.column] == ';'
        self.parameter_start_position = parameter_start_position
        self.parameter_end_position = parameter_end_position
        characters_after_parameters = SingleLineView(clean_lines.elided, parameter_end_position, body_start_position).single_line
        self.is_final = bool(search(r'\bfinal\b', characters_after_parameters))
        self.is_override = bool(search(r'\boverride\b', characters_after_parameters))
        self.is_pure = bool(match(r'\s*=\s*0\s*', characters_after_parameters))
        self._clean_lines = clean_lines
        self._parameter_list = None

    def modifiers_and_return_type(self):
        """Returns the modifiers and the return type."""
        # Go backwards from where the function name is until we encounter one of several things:
        #   ';' or '{' or '}' or 'private:', etc. or '#' or return Position(0, 0)
        elided = self._clean_lines.elided
        start_modifiers = _rfind_in_lines(r';|\{|\}|((private|public|protected):)|(#.*)', elided, self.parameter_start_position, Position(0, 0))
        return SingleLineView(elided, start_modifiers, self.function_name_start_position).single_line.strip()

    def post_modifiers(self):
        """Returns the modifiers after the function declaration such as attributes."""
        elided = self._clean_lines.elided
        return SingleLineView(elided, self.parameter_end_position, self.body_start_position).single_line.strip()

    def body_view(self):
        """Returns the function body."""
        elided = self._clean_lines.elided
        return SingleLineView(elided, self.body_start_position, self.end_position)

    def attributes_after_definition(self, attribute_regex):
        return re.findall(attribute_regex, self.post_modifiers())

    def has_attribute(self, attribute_regex):
        regex = r'\b{attribute_regex}\b'.format(attribute_regex=attribute_regex)
        return bool(search(regex, self.modifiers_and_return_type())) or bool(search(regex, self.post_modifiers()))

    def has_return_type(self, return_type_regex):
        regex = r'\b{return_type_regex}$'.format(return_type_regex=return_type_regex)
        return bool(search(regex, self.modifiers_and_return_type()))

    def is_static(self):
        return bool(search(r'\bstatic\b', self.modifiers_and_return_type()))

    def is_virtual(self):
        return bool(search(r'\bvirtual\b', self.modifiers_and_return_type()))

    def export_macro(self):
        for m in _EXPORT_MACROS:
            export_match = match(r'\b' + m + r'\b', self.modifiers_and_return_type())
            if export_match:
                return export_match.group(0)
        return None

    def parameter_list(self):
        if not self._parameter_list:
            # Store the final result as a tuple since that is immutable.
            self._parameter_list = tuple(parameter_list(self._clean_lines.elided, self.parameter_start_position, self.parameter_end_position))

        return self._parameter_list

    def count(self, line_number):
        """Count line in current function body."""
        if self.in_a_function and line_number >= self.body_start_position.row:
            self.lines_in_function += 1

    def check(self, error, line_number):
        """Report if too many lines in function body.

        Args:
          error: The function to call with any errors found.
          line_number: The number of the line to check.
        """
        if match(r'T(EST|est)', self.current_function):
            base_trigger = self._TEST_TRIGGER
        else:
            base_trigger = self._NORMAL_TRIGGER
        trigger = base_trigger * 2 ** self.min_confidence

        if self.lines_in_function > trigger:
            error_level = int(math.log(self.lines_in_function / base_trigger, 2))
            # 50 => 0, 100 => 1, 200 => 2, 400 => 3, 800 => 4, 1600 => 5, ...
            if error_level > 5:
                error_level = 5
            error(line_number, 'readability/fn_size', error_level,
                  'Small and focused functions are preferred:'
                  ' %s has %d non-comment lines'
                  ' (error triggered by exceeding %d lines).' % (
                      self.current_function, self.lines_in_function, trigger))

    def end(self):
        """Stop analyzing function body."""
        self.in_a_function = False


class _IncludeError(Exception):
    """Indicates a problem with the include order in a file."""
    pass


class FileInfo:
    """Provides utility functions for filenames.

    FileInfo provides easy access to the components of a file's path
    relative to the project root.
    """

    def __init__(self, filename):
        self._filename = filename

    def full_name(self):
        """Make Windows paths like Unix."""
        return os.path.abspath(self._filename).replace('\\', '/')

    def repository_name(self):
        r"""Full name after removing the local path to the repository.

        If we have a real absolute path name here we can try to do something smart:
        detecting the root of the checkout and truncating /path/to/checkout from
        the name so that we get header guards that don't include things like
        "C:\Documents and Settings\..." or "/home/username/..." in them and thus
        people on different computers who have checked the source out to different
        locations won't see bogus errors.
        """
        fullname = self.full_name()

        if os.path.exists(fullname):
            project_dir = os.path.dirname(fullname)

            if os.path.exists(os.path.join(project_dir, ".svn")):
                # If there's a .svn file in the current directory, we
                # recursively look up the directory tree for the top
                # of the SVN checkout
                root_dir = project_dir
                one_up_dir = os.path.dirname(root_dir)
                while os.path.exists(os.path.join(one_up_dir, ".svn")):
                    root_dir = os.path.dirname(root_dir)
                    one_up_dir = os.path.dirname(one_up_dir)

                prefix = os.path.commonprefix([root_dir, project_dir])
                return fullname[len(prefix) + 1:]

            # Not SVN <= 1.6? Try to find a git, or svn top level directory by
            # searching up from the current path.
            root_dir = os.path.dirname(fullname)
            while (root_dir != os.path.dirname(root_dir)
                   and not os.path.exists(os.path.join(root_dir, ".git"))
                   and not os.path.exists(os.path.join(root_dir, ".svn"))):
                root_dir = os.path.dirname(root_dir)
                if (os.path.exists(os.path.join(root_dir, ".git")) or
                   os.path.exists(os.path.join(root_dir, ".svn"))):
                    prefix = os.path.commonprefix([root_dir, project_dir])
                    return fullname[len(prefix) + 1:]

        # Don't know what to do; header guard warnings may be wrong...
        return fullname

    def split(self):
        """Splits the file into the directory, basename, and extension.

        For 'chrome/browser/browser.cpp', Split() would
        return ('chrome/browser', 'browser', '.cpp')

        Returns:
          A tuple of (directory, basename, extension).
        """

        googlename = self.repository_name()
        project, rest = os.path.split(googlename)
        return (project,) + os.path.splitext(rest)

    def base_name(self):
        """File base name - text after the final slash, before the final period."""
        return self.split()[1]

    def extension(self):
        """File extension - text following the final period."""
        return self.split()[2]

    def no_extension(self):
        """File has no source file extension."""
        return '/'.join(self.split()[0:2])

    def is_source(self):
        """File has a source file extension."""
        return self.extension()[1:] in ('c', 'cc', 'cpp', 'cxx')


# Matches standard C++ escape esequences per 2.13.2.3 of the C++ standard.
_RE_PATTERN_CLEANSE_LINE_ESCAPES = re.compile(
    r'\\([abfnrtv?"\\\']|\d+|x[0-9a-fA-F]+)')
# Matches strings.  Escape codes should already be removed by ESCAPES.
_RE_PATTERN_CLEANSE_LINE_DOUBLE_QUOTES = re.compile(r'"[^"]*"')
# Matches characters.  Escape codes should already be removed by ESCAPES.
_RE_PATTERN_CLEANSE_LINE_SINGLE_QUOTES = re.compile(r"'[^']{1,4}'")
# Matches multi-line C++ comments.
# This RE is a little bit more complicated than one might expect, because we
# have to take care of space removals tools so we can handle comments inside
# statements better.
# The current rule is: We only clear spaces from both sides when we're at the
# end of the line. Otherwise, we try to remove spaces from the right side,
# if this doesn't work we try on left side but only if there's a non-character
# on the right.
_RE_PATTERN_CLEANSE_LINE_C_COMMENTS = re.compile(
    r"""(\s*/\*.*\*/\s*$|
            /\*.*\*/\s+|
         \s+/\*.*\*/(?=\W)|
            /\*.*\*/)""", re.VERBOSE)


def is_cpp_string(line):
    """Does line terminate so, that the next symbol is in string constant.

    This function does not consider single-line nor multi-line comments.

    Args:
      line: is a partial line of code starting from the 0..n.

    Returns:
      True, if next character appended to 'line' is inside a
      string constant.
    """

    line = line.replace(r'\\', 'XX')  # after this, \\" does not match to \"
    return ((line.count('"') - line.count(r'\"') - line.count("'\"'")) & 1) == 1


def find_next_multi_line_comment_start(lines, line_index):
    """Find the beginning marker for a multiline comment."""
    while line_index < len(lines):
        if lines[line_index].strip().startswith('/*'):
            # Only return this marker if the comment goes beyond this line
            if lines[line_index].strip().find('*/', 2) < 0:
                return line_index
        line_index += 1
    return len(lines)


def find_next_multi_line_comment_end(lines, line_index):
    """We are inside a comment, find the end marker."""
    while line_index < len(lines):
        if lines[line_index].strip().endswith('*/'):
            return line_index
        line_index += 1
    return len(lines)


def remove_multi_line_comments_from_range(lines, begin, end):
    """Clears a range of lines for multi-line comments."""
    # Having // dummy comments makes the lines non-empty, so we will not get
    # unnecessary blank line warnings later in the code.
    for i in range(begin, end):
        lines[i] = '// dummy'


def remove_multi_line_comments(lines, error):
    """Removes multiline (c-style) comments from lines."""
    line_index = 0
    while line_index < len(lines):
        line_index_begin = find_next_multi_line_comment_start(lines, line_index)
        if line_index_begin >= len(lines):
            return
        line_index_end = find_next_multi_line_comment_end(lines, line_index_begin)
        if line_index_end >= len(lines):
            error(line_index_begin + 1, 'readability/multiline_comment', 5,
                  'Could not find end of multi-line comment')
            return
        remove_multi_line_comments_from_range(lines, line_index_begin, line_index_end + 1)
        line_index = line_index_end + 1


def cleanse_comments(line):
    """Removes //-comments and single-line C-style /* */ comments.

    Args:
      line: A line of C++ source.

    Returns:
      The line with single-line comments removed.
    """
    comment_position = line.find('//')
    if comment_position != -1 and not is_cpp_string(line[:comment_position]):
        line = line[:comment_position]
    # get rid of /* ... */
    return _RE_PATTERN_CLEANSE_LINE_C_COMMENTS.sub('', line)


class CleansedLines(object):
    """Holds 3 copies of all lines with different preprocessing applied to them.

    1) elided member contains lines without strings and comments,
    2) lines member contains lines without comments, and
    3) raw member contains all the lines without processing.
    All these three members are of <type 'list'>, and of the same length.
    """

    def __init__(self, lines):
        self.elided = []
        self.lines = []
        self.raw_lines = lines
        self._num_lines = len(lines)
        for line_number in range(len(lines)):
            self.lines.append(cleanse_comments(lines[line_number]))
            elided = self.collapse_strings(lines[line_number])
            self.elided.append(cleanse_comments(elided))

    def num_lines(self):
        """Returns the number of lines represented."""
        return self._num_lines

    @staticmethod
    def collapse_strings(elided):
        """Collapses strings and chars on a line to simple "" or '' blocks.

        We nix strings first so we're not fooled by text like '"http://"'

        Args:
          elided: The line being processed.

        Returns:
          The line with collapsed strings.
        """
        if not _RE_PATTERN_INCLUDE.match(elided):
            # Remove escaped characters first to make quote/single quote collapsing
            # basic.  Things that look like escaped characters shouldn't occur
            # outside of strings and chars.
            elided = _RE_PATTERN_CLEANSE_LINE_ESCAPES.sub('', elided)
            elided = _RE_PATTERN_CLEANSE_LINE_SINGLE_QUOTES.sub("''", elided)
            elided = _RE_PATTERN_CLEANSE_LINE_DOUBLE_QUOTES.sub('""', elided)
        return elided


def close_expression(elided, position):
    """If input points to ( or { or [, finds the position that closes it.

    If elided[position.row][position.column] points to a '(' or '{' or '[',
    finds the line_number/pos that correspond to the closing of the expression.

     Args:
       elided: A CleansedLines.elided instance containing the file.
       position: The position of the opening item.

     Returns:
      The Position *past* the closing brace, or Position(len(elided), -1)
      if we never find a close. Note we ignore strings and comments when matching.
    """
    line = elided[position.row]
    start_character = line[position.column]
    if start_character == '(':
        enclosing_character_regex = r'[\(\)]'
    elif start_character == '[':
        enclosing_character_regex = r'[\[\]]'
    elif start_character == '{':
        enclosing_character_regex = r'[\{\}]'
    else:
        return Position(len(elided), -1)

    current_column = position.column + 1
    line_number = position.row
    net_open = 1
    for line in elided[position.row:]:
        line = line[current_column:]

        # Search the current line for opening and closing characters.
        while True:
            next_enclosing_character = search(enclosing_character_regex, line)
            # No more on this line.
            if not next_enclosing_character:
                break
            current_column += next_enclosing_character.end(0)
            line = line[next_enclosing_character.end(0):]
            if next_enclosing_character.group(0) == start_character:
                net_open += 1
            else:
                net_open -= 1
                if not net_open:
                    return Position(line_number, current_column)

        # Proceed to the next line.
        line_number += 1
        current_column = 0

    # The given item was not closed.
    return Position(len(elided), -1)


def check_for_copyright(lines, error):
    """Logs an error if no Copyright message appears at the top of the file."""

    # We'll say it should occur by line 10. Don't forget there's a
    # dummy line at the front.
    for line in range(1, min(len(lines), 11)):
        if re.search(r'Copyright', lines[line], re.I):
            break
    else:                       # means no copyright line was found
        error(0, 'legal/copyright', 5,
              'No copyright message found.  '
              'You should have a line: "Copyright [year] <Copyright Owner>"')


def check_for_header_guard(file_path, lines, error):
    """Checks that the file contains a header guard.

    Logs an error if there was an #ifndef guard in a header
    that should be a #pragma once guard, or if there is a missing
    #pragma once guard.

    Args:
      file_path: Path to the header file that is being processed.
      lines: An array of strings, each representing a line of the file.
      error: The function to call with any errors found.
    """

    filename = os.path.split(file_path)[-1]
    if filename == 'config.h' or filename.endswith('Prefix.h'):
        return

    first_blank_line_number = 0
    has_import_statement = False
    has_objc_check = False
    has_objc_keywords = False
    for line_number, line in enumerate(lines):
        if line == '' and first_blank_line_number == 0:
            first_blank_line_number = line_number
        if line.startswith('#pragma once'):
            return
        if line.startswith('#import '):
            has_import_statement = True
        if '__OBJC__' in line:
            has_objc_check = True
        if functools.reduce(lambda x, y: x or y, map(lambda x: x in line, ['@class', '@interface', '@protocol'])):
            has_objc_keywords = True

    if (has_import_statement or has_objc_keywords) and not has_objc_check:
        return  # Objective-C-only headers don't need guards.

    # If there is no #pragma once, but there is an #ifndef, warn only if it was modified.
    ifndef_line_number = 0
    previous_line = None
    for line_number, line in enumerate(lines):
        if previous_line is not None:
            previous_line_split = previous_line.split()
            line_split = line.split()
            if len(previous_line_split) >= 2 and len(line_split) >= 2:
                if previous_line_split[0] == '#ifndef' and line_split[0] == '#define' \
                        and previous_line_split[1] == line_split[1]:
                    error(line_number, 'build/header_guard', 5,
                          'Use #pragma once instead of #ifndef for header guard.')
                    return
        previous_line = line

    error(first_blank_line_number + 1, 'build/header_guard_missing', 5,
          'Missing #pragma once for header guard.')


def check_for_unicode_replacement_characters(lines, error):
    """Logs an error for each line containing Unicode replacement characters.

    These indicate that either the file contained invalid UTF-8 (likely)
    or Unicode replacement characters (which it shouldn't).  Note that
    it's possible for this to throw off line numbering if the invalid
    UTF-8 occurred adjacent to a newline.

    Args:
      lines: An array of strings, each representing a line of the file.
      error: The function to call with any errors found.
    """
    for line_number, line in enumerate(lines):
        if u'\ufffd' in line:
            error(line_number, 'readability/utf8', 5,
                  'Line contains invalid UTF-8 (or Unicode replacement character).')


def check_for_missing_new_line_at_eof(lines, error):
    """Logs an error if there is not a newline character at the end of the file.

    Args:
      lines: An array of strings, each representing a line of the file.
      error: The function to call with any errors found.
    """

    # The array lines() was created by adding two newlines to the
    # original file (go figure), then splitting on \n.
    # To verify that the file ends in \n, we just have to make sure the
    # last-but-two element of lines() exists and is empty.
    if len(lines) < 3 or lines[-2]:
        error(len(lines) - 2, 'whitespace/ending_newline', 5,
              'Could not find a newline character at the end of the file.')


def check_for_extra_new_line_at_eof(lines, error):
    """Logs an error if there is not a single newline at the end of the file.

    Args:
      lines: An array of strings, each representing a line of the file.
      error: The function to call with any errors found.
    """
    # The array lines() was created by adding two newlines to the
    # original file (go figure), then splitting on \n.
    # len(lines) < 3 means that the original file contain one or less lines,
    # so there is no way to be 'more then one newline at the end'.
    # The case when the -2. line is non-empty should addressed in the
    # check_for_missing_new_line_at_eof so it can be ignored here.
    if len(lines) > 3:
        if not lines[-2] and not lines[-3]:
            error(len(lines) - 2, 'whitespace/ending_newline', 5,
                  'There was more than one newline at the end of the file.')


def check_for_multiline_comments_and_strings(clean_lines, line_number, error):
    """Logs an error if we see /* ... */ or "..." that extend past one line.

    /* ... */ comments are legit inside macros, for one line.
    Otherwise, we prefer // comments, so it's ok to warn about the
    other.  Likewise, it's ok for strings to extend across multiple
    lines, as long as a line continuation character (backslash)
    terminates each line. Although not currently prohibited by the C++
    style guide, it's ugly and unnecessary. We don't do well with either
    in this lint program, so we warn about both.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """
    line = clean_lines.elided[line_number]

    # Remove all \\ (escaped backslashes) from the line. They are OK, and the
    # second (escaped) slash may trigger later \" detection erroneously.
    line = line.replace('\\\\', '')

    if line.count('/*') > line.count('*/'):
        error(line_number, 'readability/multiline_comment', 5,
              'Complex multi-line /*...*/-style comment found. '
              'Lint may give bogus warnings.  '
              'Consider replacing these with //-style comments, '
              'with #if 0...#endif, '
              'or with more clearly structured multi-line comments.')

    if (line.count('"') - line.count('\\"')) % 2:
        error(line_number, 'readability/multiline_string', 5,
              'Multi-line string ("...") found.  This lint script doesn\'t '
              'do well with such strings, and may give bogus warnings.  They\'re '
              'ugly and unnecessary, and you should use concatenation instead".')


_THREADING_LIST = (
    ('asctime(', 'asctime_r('),
    ('ctime(', 'ctime_r('),
    ('getgrgid(', 'getgrgid_r('),
    ('getgrnam(', 'getgrnam_r('),
    ('getlogin(', 'getlogin_r('),
    ('getpwnam(', 'getpwnam_r('),
    ('getpwuid(', 'getpwuid_r('),
    ('gmtime(', 'gmtime_r('),
    ('localtime(', 'localtime_r('),
    ('rand(', 'rand_r('),
    ('readdir(', 'readdir_r('),
    ('strtok(', 'strtok_r('),
    ('ttyname(', 'ttyname_r('),
    )


def check_posix_threading(clean_lines, line_number, error):
    """Checks for calls to thread-unsafe functions.

    Much code has been originally written without consideration of
    multi-threading. Also, engineers are relying on their old experience;
    they have learned posix before threading extensions were added. These
    tests guide the engineers to use thread-safe functions (when using
    posix directly).

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """
    line = clean_lines.elided[line_number]
    for single_thread_function, multithread_safe_function in _THREADING_LIST:
        index = line.find(single_thread_function)
        # Comparisons made explicit for clarity
        if index >= 0 and (index == 0 or (not line[index - 1].isalnum()
                                          and line[index - 1] not in ('_', '.', '>'))):
            error(line_number, 'runtime/threadsafe_fn', 2,
                  'Consider using ' + multithread_safe_function +
                  '...) instead of ' + single_thread_function +
                  '...) for improved thread safety.')


# Matches invalid increment: *count++, which moves pointer instead of
# incrementing a value.
_RE_PATTERN_INVALID_INCREMENT = re.compile(
    r'^\s*\*\w+(\+\+|--);')


def check_invalid_increment(clean_lines, line_number, error):
    """Checks for invalid increment *count++.

    For example following function:
    void increment_counter(int* count) {
        *count++;
    }
    is invalid, because it effectively does count++, moving pointer, and should
    be replaced with ++*count, (*count)++ or *count += 1.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """
    line = clean_lines.elided[line_number]
    if _RE_PATTERN_INVALID_INCREMENT.match(line):
        error(line_number, 'runtime/invalid_increment', 5,
              'Changing pointer instead of value (or unused value of operator*).')


# Matches Xcode *VERSION_MIN_REQUIRED and *VERSION_MAX_ALLOWED macros.
_RE_PATTERN_XCODE_VERSION_MACRO = re.compile(
    r'.+(VERSION_MIN_REQUIRED|VERSION_MAX_ALLOWED)')

_RE_PATTERN_XCODE_MIN_REQUIRED_MACRO = re.compile(
    r'.+?([A-Z_]+)_VERSION_MIN_REQUIRED [><=]+ (\d+)')

_RE_PATTERN_PLATFORM_HEADER = re.compile(
    r'Source/WTF/wtf/Platform[a-zA-Z]+\.h')


def check_os_version_checks(filename, clean_lines, line_number, error):
    """ Checks for mistakes using VERSION_MIN_REQUIRED and VERSION_MAX_ALLOWED macros:
    1. These should only be used centrally to defined named HAVE, USE or ENABLE style macros.
    2. VERSION_MIN_REQUIRED never changes for a minor OS version.

    These should be centralized in the wtf/Platform*.h suite of files.

    Args:
      filename: Name of the file that is being processed.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number]

    for version_match in _RE_PATTERN_XCODE_MIN_REQUIRED_MACRO.finditer(line):
        os_prefix = version_match.group(1)
        version_number = int(version_match.group(2))
        if os_prefix == '__MAC_OS_X' and version_number % 100 != 0 or os_prefix != '__MAC_OS_X' and version_number % 10000 != 0:
            error(line_number, 'build/version_check', 5, 'Incorrect OS version check. VERSION_MIN_REQUIRED values never include a minor version. You may be looking for a combination of VERSION_MIN_REQUIRED for target OS version check and VERSION_MAX_ALLOWED for SDK check.')
            break

    if _RE_PATTERN_PLATFORM_HEADER.match(filename):
        return

    if _RE_PATTERN_XCODE_VERSION_MACRO.match(line):
        error(line_number, 'build/version_check', 5, 'Misplaced OS version check. Please use a named macro in one of headers in the wtf/Platform.h suite of files or an appropriate internal file.')

class _ClassInfo(object):
    """Stores information about a class."""

    def __init__(self, name, line_number, export_macro):
        self.name = name
        self.line_number = line_number
        self.export_macro = export_macro
        self.seen_open_brace = False
        self.is_derived = False
        self.virtual_method_line_number = None
        self.has_virtual_destructor = False
        self.brace_depth = 0


class _ClassState(object):
    """Holds the current state of the parse relating to class declarations.

    It maintains a stack of _ClassInfos representing the parser's guess
    as to the current nesting of class declarations. The innermost class
    is at the top (back) of the stack. Typically, the stack will either
    be empty or have exactly one entry.
    """

    def __init__(self):
        self.classinfo_stack = []

    def check_finished(self, error):
        """Checks that all classes have been completely parsed.

        Call this when all lines in a file have been processed.
        Args:
          error: The function to call with any errors found.
        """
        if self.classinfo_stack:
            # Note: This test can result in false positives if #ifdef constructs
            # get in the way of brace matching. See the testBuildClass test in
            # cpp_style_unittest.py for an example of this.
            error(self.classinfo_stack[0].line_number, 'build/class', 5,
                  'Failed to find complete declaration of class %s' %
                  self.classinfo_stack[0].name)


class _FileState(object):
    def __init__(self, clean_lines, file_extension):
        self._did_inside_namespace_indent_warning = False
        self._clean_lines = clean_lines
        if file_extension == 'm':
            self._is_objective_cpp = False
            self._is_objective_c = True
            self._is_c = False
        elif file_extension == 'mm':
            self._is_objective_cpp = True
            self._is_objective_c = False
            self._is_c = False
        elif file_extension == 'h':
            # In the case of header files, it is unknown if the file
            # is C / Objective-C / Objective-C++ or not, so set this value to None and then
            # if it is requested, use heuristics to guess the value.
            self._is_objective_cpp = None
            self._is_objective_c = None
            self._is_c = None
        elif file_extension == 'c':
            self._is_objective_cpp = False
            self._is_c = True
            self._is_objective_c = False
        else:
            self._is_objective_cpp = False
            self._is_objective_c = False
            self._is_c = False

    def set_did_inside_namespace_indent_warning(self):
        self._did_inside_namespace_indent_warning = True

    def did_inside_namespace_indent_warning(self):
        return self._did_inside_namespace_indent_warning

    def is_objective_c(self):
        if self._is_objective_c is None:
            for line in self._clean_lines.elided:
                # Starting with @ or #import seem like the best indications
                # that we have an Objective C file.
                if line.startswith("@") or line.startswith("#import"):
                    self._is_objective_c = True
                    break
            else:
                self._is_objective_c = False
        return self._is_objective_c

    def is_objective_c_or_objective_cpp(self):
        return self._is_objective_cpp or self.is_objective_c()

    def is_c(self):
        if self._is_c is None:
            for line in self._clean_lines.lines:
                # if extern "C" is found, then it is a good indication
                # that we have a C header file.
                if line.startswith('extern "C"'):
                    self._is_c = True
                    break
            else:
                self._is_c = False
        return self._is_c

    def is_c_or_objective_c(self):
        """Return whether the file extension corresponds to C or Objective-C."""
        return self.is_c() or self.is_objective_c()


class _EnumState(object):
    """Maintains whether currently in an enum declaration, and checks whether
    enum declarations follow the style guide.
    """

    def __init__(self):
        self.in_enum_decl = False
        self.is_webidl_enum = False
        self.enum_decl_name = None

    def process_clean_line(self, line):
        # FIXME: The regular expressions for expr_all_uppercase and expr_enum_end only accept integers
        # and identifiers for the value of the enumerator, but do not accept any other constant
        # expressions. However, this is sufficient for now (11/27/2012).
        expr_all_uppercase = r'\s*(?P<value>[A-Z0-9_]+)\s*(?:=\s*[a-zA-Z0-9]+\s*)?,?\s*$'
        expr_starts_lowercase = r'\s*[a-jl-z]'
        expr_enum_end = r'}\s*(?:[a-zA-Z0-9]+\s*(?:=\s*[a-zA-Z0-9]+)?)?\s*;\s*'
        expr_enum_start = r'\s*(?:enum(?:\s+class)?(?:\s+(?P<identifier>[a-zA-Z0-9]+))?)(?:\s*:\s*[a-zA-Z0-9_]+?)?\s*\{?\s*'

        def is_case_error(enum_name, value_declaration):
            all_uppercase = match(expr_all_uppercase, value_declaration)
            if all_uppercase:
                if self.is_webidl_enum:
                    return False
                if enum_name in _ALLOW_ALL_UPPERCASE_ENUM:
                    return False
                return not all_uppercase.group('value') in _ALLOW_ABBREVIATION_ENUM_VALUES
            return match(expr_starts_lowercase, value_declaration)

        if self.in_enum_decl:
            if match(r'\s*' + expr_enum_end + r'$', line):
                self.in_enum_decl = False
                self.is_webidl_enum = False
                return True
            elif is_case_error(self.enum_decl_name, line):
                return False
        matched = match(expr_enum_start + r'$', line)
        if matched:
            self.in_enum_decl = True
            self.enum_decl_name = matched.group('identifier')
        else:
            matched = match(expr_enum_start + r'(?P<members>[^{]*)' + expr_enum_end + r'$', line)
            if matched:
                members = matched.group('members').split(',')
                enum_name = matched.group('identifier')
                for member in members:
                    if is_case_error(enum_name, member):
                        self.is_webidl_enum = False
                        return False
                return True
        return True


def regex_for_lambdas_and_blocks(line, line_number, file_state, error):
    cpp_result = search(r'\s\[.*?\]\s', line)
    objc_result = search(r'(\s\^\s?\(.*?\)\s?|\^\s*\{|:\^(\s|\w+)?\(.*?\)\s\{)', line)
    if cpp_result:
        group = cpp_result.group()
        targ_error = None

        if search(r'(\[\s|\s\]|\s,)', group):
            targ_error = [line_number, 'whitespace/brackets', 4,
              'Extra space in capture list.',line]

        if targ_error and regex_for_lambdas_and_blocks.__last_error != targ_error:
            error(targ_error[0], targ_error[1], targ_error[2], targ_error[3])
        regex_for_lambdas_and_blocks.__last_error = targ_error
        return True

    if objc_result and file_state.is_objective_c_or_objective_cpp():
        group = objc_result.group()
        targ_error = None

        if search(r'(\(\s|\s\)|\s,)', group):
            targ_error = [line_number, 'whitespace/brackets', 4,
              'Extra space in block arguments.',line]
        if search(r'\^\s+\{', group):
            targ_error = [line_number, 'whitespace/brackets', 4,
              'Extra space between ^ and block definition.',line]
        if search(r'\^\s\(', group):
            targ_error = [line_number, 'whitespace/brackets', 4,
              'Extra space between ^ and block arguments.',line]

        if targ_error and regex_for_lambdas_and_blocks.__last_error != targ_error:
            error(targ_error[0], targ_error[1], targ_error[2], targ_error[3])
        regex_for_lambdas_and_blocks.__last_error = targ_error
        return True

    return False

regex_for_lambdas_and_blocks.__last_error = None


def check_for_non_standard_constructs(clean_lines, line_number,
                                      class_state, error):
    r"""Logs an error if we see certain non-ANSI constructs ignored by gcc-2.

    Complain about several constructs which gcc-2 accepts, but which are
    not standard C++.  Warning about these in lint is one way to ease the
    transition to new compilers.
    - put storage class first (e.g. "static const" instead of "const static").
    - "%lld" instead of %qd" in printf-type functions.
    - "%1$d" is non-standard in printf-type functions.
    - "\%" is an undefined character escape sequence.
    - text after #endif is not allowed.
    - invalid inner-style forward declaration.
    - >? and <? operators, and their >?= and <?= cousins.
    - classes with virtual methods need virtual destructors (compiler warning
        available, but not turned on yet.)

    Additionally, check for constructor/destructor style violations as it
    is very convenient to do so while checking for gcc-2 compliance.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      class_state: A _ClassState instance which maintains information about
                   the current stack of nested class declarations being parsed.
      error: A callable to which errors are reported, which takes parameters:
             line number, error level, and message
    """

    # Remove comments from the line, but leave in strings for now.
    line = clean_lines.lines[line_number]

    if search(r'printf\s*\(.*".*%[-+ ]?\d*q', line):
        error(line_number, 'runtime/printf_format', 3,
              '%q in format strings is deprecated.  Use %ll instead.')

    if search(r'printf\s*\(.*".*%\d+\$', line):
        error(line_number, 'runtime/printf_format', 2,
              '%N$ formats are unconventional.  Try rewriting to avoid them.')

    # Remove escaped backslashes before looking for undefined escapes.
    line = line.replace('\\\\', '')

    if search(r'("|\').*\\(%|\[|\(|{)', line):
        error(line_number, 'build/printf_format', 3,
              '%, [, (, and { are undefined character escapes.  Unescape them.')

    # For the rest, work with both comments and strings removed.
    line = clean_lines.elided[line_number]

    if search(r'\b(const|volatile|void|char|short|int|long'
              r'|float|double|signed|unsigned'
              r'|schar|u?int8|u?int16|u?int32|u?int64)'
              r'\s+(auto|register|static|extern|typedef)\b',
              line):
        error(line_number, 'build/storage_class', 5,
              'Storage class (static, extern, typedef, etc) should be first.')

    if match(r'\s*#\s*endif\s*[^/\s]+', line):
        error(line_number, 'build/endif_comment', 5,
              'Uncommented text after #endif is non-standard.  Use a comment.')

    if match(r'\s*class\s+(\w+\s*::\s*)+\w+\s*;', line):
        error(line_number, 'build/forward_decl', 5,
              'Inner-style forward declarations are invalid.  Remove this line.')

    if search(r'(\w+|[+-]?\d+(\.\d*)?)\s*(<|>)\?=?\s*(\w+|[+-]?\d+)(\.\d*)?', line):
        error(line_number, 'build/deprecated', 3,
              '>? and <? (max and min) operators are non-standard and deprecated.')

    # Track class entry and exit, and attempt to find cases within the
    # class declaration that don't meet the C++ style
    # guidelines. Tracking is very dependent on the code matching Google
    # style guidelines, but it seems to perform well enough in testing
    # to be a worthwhile addition to the checks.
    classinfo_stack = class_state.classinfo_stack
    # Look for a class declaration
    class_decl_match = match(
        r'\s*(template\s*<[\w\s<>,:]*>\s*)?(class|struct)(\s+(WTF_EXPORT|WTF_EXPORT_PRIVATE|PAL_EXPORT|JS_EXPORT_PRIVATE|WEBCORE_EXPORT))?\s+(\w+(::\w+)*)', line)
    if class_decl_match:
        classinfo_stack.append(_ClassInfo(class_decl_match.group(5), line_number, class_decl_match.group(4)))

    # Everything else in this function uses the top of the stack if it's
    # not empty.
    if not classinfo_stack:
        return

    classinfo = classinfo_stack[-1]

    # If the opening brace hasn't been seen look for it and also
    # parent class declarations.
    if not classinfo.seen_open_brace:
        # If the line has a ';' in it, assume it's a forward declaration or
        # a single-line class declaration, which we won't process.
        if line.find(';') != -1:
            classinfo_stack.pop()
            return
        classinfo.seen_open_brace = (line.find('{') != -1)
        # Look for a bare ':'
        if search('(^|[^:]):($|[^:])', line):
            classinfo.is_derived = True
        if not classinfo.seen_open_brace:
            return  # Everything else in this function is for after open brace

    # The class may have been declared with namespace or classname qualifiers.
    # The constructor and destructor will not have those qualifiers.
    base_classname = classinfo.name.split('::')[-1]

    # Look for single-argument constructors that aren't marked explicit.
    # Technically a valid construct, but against style.
    args = match(r'(?<!explicit)\s+%s\s*\(([^,()]+)\)'
                 % re.escape(base_classname),
                 line)
    if (args
        and args.group(1) != 'void'
        and not match(r'(const\s+)?%s\s*&' % re.escape(base_classname),
                      args.group(1).strip())):
        error(line_number, 'runtime/explicit', 5,
              'Single-argument constructors should be marked explicit.')

    # Look for methods declared virtual.
    if search(r'\bvirtual\b', line):
        classinfo.virtual_method_line_number = line_number
        # Only look for a destructor declaration on the same line. It would
        # be extremely unlikely for the destructor declaration to occupy
        # more than one line.
        if search(r'~%s\s*\(' % base_classname, line):
            classinfo.has_virtual_destructor = True

    # Look for class end.
    brace_depth = classinfo.brace_depth
    brace_depth = brace_depth + line.count('{') - line.count('}')
    if brace_depth <= 0:
        classinfo = classinfo_stack.pop()
        # Try to detect missing virtual destructor declarations.
        # For now, only warn if a non-derived class with virtual methods lacks
        # a virtual destructor. This is to make it less likely that people will
        # declare derived virtual destructors without declaring the base
        # destructor virtual.
        if ((classinfo.virtual_method_line_number is not None)
            and (not classinfo.has_virtual_destructor)
            and (not classinfo.is_derived)):  # Only warn for base classes
            error(classinfo.line_number, 'runtime/virtual', 4,
                  'The class %s probably needs a virtual destructor due to '
                  'having virtual method(s), one declared at line %d.'
                  % (classinfo.name, classinfo.virtual_method_line_number))
    else:
        classinfo.brace_depth = brace_depth


def check_spacing_for_function_call(line, line_number, file_state, error):
    """Checks for the correctness of various spacing around function calls.

    Args:
      line: The text of the line to check.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    # Since function calls often occur inside if/for/while/switch
    # expressions - which have their own, more liberal conventions - we
    # first see if we should be looking inside such an expression for a
    # function call, to which we can apply more strict standards.
    function_call = line    # if there's no control flow construct, look at whole line
    for pattern in (r'\bif\s*\((.*)\)\s*{',
                    r'\bfor\s*\((.*)\)\s*{',
                    r'\bwhile\s*\((.*)\)\s*[{;]',
                    r'\bswitch\s*\((.*)\)\s*{'):
        matched = search(pattern, line)
        if matched:
            function_call = matched.group(1)    # look inside the parens for function calls
            break

    # Except in if/for/while/switch, there should never be space
    # immediately inside parens (eg "f( 3, 4 )").  We make an exception
    # for nested parens ( (a+b) + c ).  Likewise, there should never be
    # a space before a ( when it's a function argument.  I assume it's a
    # function argument when the char before the whitespace is legal in
    # a function name (alnum + _) and we're not starting a macro. Also ignore
    # pointers and references to arrays and functions coz they're too tricky:
    # we use a very simple way to recognize these:
    # " (something)(maybe-something)" or
    # " (something)(maybe-something," or
    # " (something)[something]"
    # Note that we assume the contents of [] to be short enough that
    # they'll never need to wrap.
    if (  # Ignore control structures.
        not search(r'\b(if|for|while|switch|return|new|delete)\b', function_call)
        # Ignore lambda functions
        and not regex_for_lambdas_and_blocks(function_call, line_number, file_state, error)
        # Ignore pointers/references to functions.
        and not search(r' \([^)]+\)\([^)]*(\)|,$)', function_call)
        # Ignore pointers/references to arrays.
        and not search(r' \([^)]+\)\[[^\]]+\]', function_call)):
        if search(r'\w\s*\([ \t](?!\s*\\$)', function_call):      # a ( used for a fn call
            error(line_number, 'whitespace/parens', 4,
                  'Extra space after ( in function call')
        elif search(r'\([ \t]+(?!(\s*\\)|\()', function_call):
            error(line_number, 'whitespace/parens', 2,
                  'Extra space after (')
        if (search(r'\w\s+\(', function_call)
            and not match(r'\s*((#|typedef|@property|@interface|@implementation|@synchronized)|} @catch\b)', function_call)):
            error(line_number, 'whitespace/parens', 4,
                  'Extra space before ( in function call')
        # If the ) is followed only by a newline or a { + newline, assume it's
        # part of a control statement (if/while/etc), and don't complain
        if search(r'[^)\s]\s+\)(?!\s*$|{\s*$)', function_call):
            error(line_number, 'whitespace/parens', 2,
                  'Extra space before )')


def is_blank_line(line):
    """Returns true if the given line is blank.

    We consider a line to be blank if the line is empty or consists of
    only white spaces.

    Args:
      line: A line of a string.

    Returns:
      True, if the given line is blank.
    """
    return not line or line.isspace()


def detect_functions(clean_lines, line_number, function_state, error):
    """Finds where functions start and end.

    Uses a simplistic algorithm assuming other style guidelines
    (especially spacing) are followed.
    Trivial bodies are unchecked, so constructors with huge initializer lists
    may be missed.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      function_state: Current function name and lines in body so far.
      error: The function to call with any errors found.
    """
    # Are we now past the end of a function?
    if function_state.end_position.row + 1 == line_number:
        function_state.end()

    # If we're in a function, don't try to detect a new one.
    if function_state.in_a_function:
        return

    lines = clean_lines.lines
    line = lines[line_number]
    raw = clean_lines.raw_lines
    raw_line = raw[line_number]

    # Lines ending with a \ indicate a macro. Don't try to check them.
    if raw_line.endswith('\\'):
        return

    regexp = r'\s*(\w(\w|::|\*|\&|\s|<|>|,|~|(operator\s*(/|-|=|!|\+)+))*)\('  # decls * & space::name( ...
    match_result = match(regexp, line)
    if not match_result:
        return

    # If the name is all caps and underscores, figure it's a macro and
    # ignore it, unless it's TEST or TEST_F.
    function_name = match_result.group(1).split()[-1]
    if function_name != 'TEST' and function_name != 'TEST_F' and match(r'[A-Z_]+$', function_name):
        return

    joined_line = ''
    for start_line_number in range(line_number, clean_lines.num_lines()):
        start_line = clean_lines.elided[start_line_number]
        joined_line += ' ' + start_line.lstrip()
        body_match = search(r'{|;', start_line)
        if body_match:
            body_start_position = Position(start_line_number, body_match.start(0))

            # Replace template constructs with _ so that no spaces remain in the function name,
            # while keeping the column numbers of other characters the same as "line".
            line_with_no_templates = iteratively_replace_matches_with_char(r'<[^<>]*>', '_', line)
            match_function = search(r'((\w|:|<|>|,|~|(operator\s*(/|-|=|!|\+)+))*)\(', line_with_no_templates)
            if not match_function:
                return  # The '(' must have been inside of a template.

            # Use the column numbers from the modified line to find the
            # function name in the original line.
            function = line[match_function.start(1):match_function.end(1)]
            function_name_start_position = Position(line_number, match_function.start(1))

            if match(r'TEST', function):    # Handle TEST... macros
                parameter_regexp = search(r'(\(.*\))', joined_line)
                if parameter_regexp:             # Ignore bad syntax
                    function += parameter_regexp.group(1)
            else:
                function += '()'

            parameter_start_position = Position(line_number, match_function.end(1))
            parameter_end_position = close_expression(clean_lines.elided, parameter_start_position)
            if parameter_end_position.row == len(clean_lines.elided):
                # No end was found.
                return

            if start_line[body_start_position.column] == ';':
                end_position = Position(body_start_position.row, body_start_position.column + 1)
            else:
                end_position = close_expression(clean_lines.elided, body_start_position)

            # Check for nonsensical positions. (This happens in test cases which check code snippets.)
            if parameter_end_position > body_start_position:
                return

            function_state.begin(function, function_name_start_position, body_start_position, end_position,
                                 parameter_start_position, parameter_end_position, clean_lines)
            return

    # No body for the function (or evidence of a non-function) was found.
    error(line_number, 'readability/fn_size', 5,
          'Lint failed to find start of function body.')


def check_for_function_lengths(clean_lines, line_number, function_state, error):
    """Reports for long function bodies.

    For an overview why this is done, see:
    http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml#Write_Short_Functions

    Blank/comment lines are not counted so as to avoid encouraging the removal
    of vertical space and commments just to get through a lint check.
    NOLINT *on the last line of a function* disables this check.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      function_state: Current function name and lines in body so far.
      error: The function to call with any errors found.
    """
    lines = clean_lines.lines
    line = lines[line_number]
    raw = clean_lines.raw_lines
    raw_line = raw[line_number]

    if function_state.end_position.row == line_number:  # last line
        if not search(r'\bNOLINT\b', raw_line):
            function_state.check(error, line_number)
    elif not match(r'^\s*$', line):
        function_state.count(line_number)  # Count non-blank/non-comment lines.


def _check_parameter_name_against_text(parameter, text, error):
    """Checks to see if the parameter name is contained within the text.

    Return false if the check failed (i.e. an error was produced).
    """

    # Treat 'lower with underscores' as a canonical form because it is
    # case insensitive while still retaining word breaks. (This ensures that
    # 'elate' doesn't look like it is duplicating of 'NateLate'.)
    canonical_parameter_name = parameter.lower_with_underscores_name()
    if canonical_parameter_name == "]":
        return True  # Work around a bug parsing some Objective-C code.

    # Appends "object" to all text to catch variables that did the same (but only
    # do this when the parameter name is more than a single character to avoid
    # flagging 'b' which may be an ok variable when used in an rgba function).
    if len(canonical_parameter_name) > 1:
        text = sub(r'(\w)\b', r'\1Object', text)
    canonical_text = _convert_to_lower_with_underscores(text)

    # Used to detect cases like ec for ExceptionCode.
    acronym = _create_acronym(text).lower()
    if canonical_text.find(canonical_parameter_name) != -1 or acronym.find(canonical_parameter_name) != -1:
        error(parameter.row, 'readability/parameter_name', 5,
              'The parameter name "%s" adds no information, so it should be removed.' % parameter.name)
        return False
    return True


def _split_identifier_into_words(identifier):
    words = []
    if not identifier:
        return words

    # Remove prefixes that aren't part of the identifier name.
    identifier = re.sub(r'^[gms]?_', '', identifier)
    # Remove bitfield lengths.
    identifier = re.sub(r':[0-9]+$', '', identifier)

    identifier_length = len(identifier)

    match_upper_re = re.compile(r'^[A-Z]+')
    match_upper_lower_re = re.compile(r'^[A-Z][a-z]+')
    match_lower_re = re.compile(r'^[a-z]+')

    match_lower = match_lower_re.search(identifier)
    match_upper_lower = match_upper_lower_re.search(identifier)
    match_upper = match_upper_re.search(identifier)
    if match_lower:
        word = match_lower.group(0)
        words.append(word)
        if len(word) == identifier_length:
            return words
        identifier = identifier[len(word):]
    elif match_upper_lower:
        word = match_upper_lower.group(0)
        words.append(word)
        if len(word) == identifier_length:
            return words
        identifier = identifier[len(word):]
    elif match_upper:
        word = match_upper.group(0)
        if len(word) == identifier_length:
            words.append(word)
            return words
        if identifier[len(word)].islower():
            word = word[:-1]
        words.append(word)
        identifier = identifier[len(word):]

    match_number_re = re.compile(r'^[0-9]+')
    while identifier:
        identifier_length = len(identifier)
        if identifier.startswith('_'):
            if len(identifier) == 1:
                return words
            identifier = identifier[1:]
            continue
        if identifier.startswith('::'):
            words.append('::')
            if len(identifier) == 2:
                return words
            identifier = identifier[2:]
            continue
        match_upper_lower = match_upper_lower_re.search(identifier)
        if match_upper_lower:
            word = match_upper_lower.group(0)
            words.append(word)
            if len(word) == identifier_length:
                return words
            identifier = identifier[len(word):]
            continue
        match_upper = match_upper_re.search(identifier)
        if match_upper:
            word = match_upper.group(0)
            if len(word) == len(identifier):
                words.append(word)
                return words
            if identifier[len(word)].islower():
                word = word[:-1]
            words.append(word)
            identifier = identifier[len(word):]
            continue
        match_number = match_number_re.search(identifier)
        if match_number:
            word = match_number.group(0)
            words.append(word)
            if len(word) == identifier_length:
                return words
            identifier = identifier[len(word):]
            continue
        match_lower = match_lower_re.search(identifier)
        if match_lower:
            word = match_lower.group(0)
            words.append(word)
            if len(word) == identifier_length:
                return words
            identifier = identifier[len(word):]
            continue
        assert False, 'Could not match "%s"' % identifier

    return words


def _check_identifier_name_for_acronyms(identifier, line_number, is_class_or_namespace_or_struct_name, error):
    """Checks to see if the identifier name contains an acronym with improper case.

    Using "url" is okay at the start of an identifier name, and "URL" is okay in the
    middle or at the end of an identifier name, but "Url" is never okay.
    """
    acronyms = '|'.join(['MIME', 'URL'])
    acronym_exceptions = '|'.join(['cfurl', 'curl', 'Curl', 'nsurl', 'urls'])

    identifier_words = _split_identifier_into_words(identifier)

    is_constructor = False
    if identifier_words.count('::') == 1:
        names = identifier.split('::')
        if names[0] == names[1]:
            is_constructor = True

    contains_acronym_lowercase_re = re.compile('(%s)' % acronyms.lower())
    is_acronym_any_case_re = re.compile('^(%s)$' % acronyms, re.IGNORECASE)
    is_acronym_lowercase_re = re.compile('^(%s)$' % acronyms.lower())
    is_acronym_uppercase_re = re.compile('^(%s)$' % acronyms.upper())
    is_acronym_exception_any_case_re = re.compile('^(%s)$' % acronym_exceptions)

    start_of_variable = True
    for i in range(0, len(identifier_words)):
        word = identifier_words[i]

        if word == '::':
            start_of_variable = True
            continue

        if start_of_variable:
            start_of_variable = False
            # Identifiers that start with an acronym must be all lowercase, except for class/namespace/struct names.
            if is_acronym_any_case_re.search(word):
                if is_acronym_lowercase_re.search(word):
                    continue
                elif is_acronym_uppercase_re.search(word) and \
                        (is_class_or_namespace_or_struct_name or '::' in identifier_words[i:] or is_constructor):
                    continue
                else:
                    error(line_number, 'readability/naming/acronym', 5,
                          'The identifier name "%s" starts with an acronym that is not all lowercase.' % identifier)
                    return False
        else:
            # Identifiers that contain or end with an acronym must be all uppercase.
            if is_acronym_any_case_re.search(word):
                if is_acronym_uppercase_re.search(word):
                    continue
                else:
                    error(line_number, 'readability/naming/acronym', 5,
                          'The identifier name "%s" contains an acronym that is not all uppercase.' % identifier)
                    return False

        if contains_acronym_lowercase_re.search(word) and not is_acronym_exception_any_case_re.search(word):
            error(line_number, 'readability/naming/acronym', 3,
                  'The identifier name "%s" _may_ contain an acronym that is not all uppercase.' % identifier)
            continue

    return True


def _error_redundant_specifier(line_number, redundant_specifier, good_specifier, error):
    error(line_number, 'readability/inheritance', 4,
          '"%s" is redundant since function is already declared as "%s"'
          % (redundant_specifier, good_specifier))


def check_function_definition(filename, file_extension, clean_lines, line_number, class_state, function_state, error):
    """Check that function definitions for style issues.

    Specifically, check that parameter names in declarations add information.

    Args:
       filename: Filename of the file that is being processed.
       file_extension: The current file extension, without the leading dot.
       clean_lines: A CleansedLines instance containing the file.
       line_number: The number of the line to check.
       function_state: Current function name and lines in body so far.
       error: The function to call with any errors found.
    """
    if line_number != function_state.body_start_position.row:
        return

    # Check for decode() functions that don't have WARN_UNUSED_RETURN attribute.
    function_name = function_state.current_function.split('..')[-1]
    if function_name.startswith('decode') or function_name.startswith('platformDecode'):
        if file_extension == 'h' or (function_state.is_static() or function_state.is_declaration):
            if function_state.has_return_type('(auto|bool)'):
                if not function_state.has_attribute('WARN_UNUSED_RETURN'):
                    error(line_number, 'security/missing_warn_unused_return', 5,
                          'decode() function returning a value is missing WARN_UNUSED_RETURN attribute')

    attributes = function_state.attributes_after_definition(r'(\bWARN_[0-9A-Z_]+\b|__attribute__\(\(__[a-z_]+__\)\))')
    if len(attributes) > 0:
        attribute_text = ', '.join(attributes)
        plural = 's' if len(attributes) > 1 else ''
        error(line_number, 'readability/function', 5,
              'Function attribute{plural} ({attributes}) should appear before the function definition'.
              format(attributes=attribute_text, plural=plural))

    parameter_list = function_state.parameter_list()
    for parameter in parameter_list:
        # Do checks specific to function declarations and parameter names.
        if not function_state.is_declaration or not parameter.name:
            continue

        # Check the parameter name against the function name for single parameter set functions.
        if len(parameter_list) == 1 and match('set[A-Z]', function_state.current_function):
            trimmed_function_name = function_state.current_function[len('set'):]
            if not _check_parameter_name_against_text(parameter, trimmed_function_name, error):
                continue  # Since an error was noted for this name, move to the next parameter.

        # Check the parameter name against the type.
        if not _check_parameter_name_against_text(parameter, parameter.type, error):
            continue  # Since an error was noted for this name, move to the next parameter.

    if function_state.is_virtual():
        if function_state.is_override:
            _error_redundant_specifier(line_number, 'virtual', 'override', error)

        if function_state.is_final:
            _error_redundant_specifier(line_number, 'virtual', 'final', error)

    if function_state.is_override and function_state.is_final:
        _error_redundant_specifier(line_number, 'override', 'final', error)

    if not function_state.is_declaration:
        if (function_state.export_macro()
                and not function_state.export_macro() == 'JS_EXPORT_PRIVATE'):
            error(line_number, 'build/webcore_export', 4,
                  'Inline functions should not be annotated with %s. Remove the macro, or '
                  'move the inline function definition out-of-line.' %
                  function_state.export_macro())
        elif (class_state.classinfo_stack
                and class_state.classinfo_stack[-1].export_macro
                and not class_state.classinfo_stack[-1].export_macro == 'JS_EXPORT_PRIVATE'):
            error(line_number, 'build/webcore_export', 4,
                  'Inline functions should not be in classes annotated with %s. Remove the '
                  'macro from the class and apply it to each appropriate method, or move '
                  'the inline function definition out-of-line.' %
                  class_state.classinfo_stack[-1].export_macro)
    elif function_state.export_macro():
        export_macro = function_state.export_macro()
        path = _EXPORT_MACRO_SPEC[export_macro]
        if not match(path, _unix_path(filename)):
            error(line_number, 'build/export_macro', 5,
                  '%s should only appear in directories matching %s.' % (export_macro, path))


def check_function_body(filename, file_extension, clean_lines, line_number, class_state, function_state, error):
    """Check function bodies for style issues.

    Args:
       filename: Filename of the file that is being processed.
       file_extension: The current file extension, without the leading dot.
       clean_lines: A CleansedLines instance containing the file.
       line_number: The number of the line to check.
       function_state: Current function name and lines in body so far.
       error: The function to call with any errors found.
    """
    if line_number != function_state.end_position.row:  # last line
        return

    function_body_view = function_state.body_view()
    function_line_count = len(function_body_view.trimmed_lines)

    # Check for uncontrolled fall-through after ASSERT_NOT_REACHED() statement.
    for i in range(0, function_line_count):
        current_line = function_body_view.trimmed_lines[i]
        if not re.search(r'[^_]ASSERT_NOT_REACHED\(\);', current_line):
            continue

        min_index = max(0, i - 1)
        max_index = min(function_line_count, i + 4)
        partial_function_body = ' '.join(function_body_view.trimmed_lines[min_index:max_index])

        if search(r'[^_]ASSERT_NOT_REACHED\(\);\s*(continue|return(\s+[^;]+)?);', partial_function_body) \
                or search(r'[^_]ASSERT_NOT_REACHED\(\);(\s*#endif)?(\s*})+\s*$', partial_function_body) \
                or search(r'[^_]ASSERT_NOT_REACHED\(\);(\s*completionHandler[^;]+;)?(\s*})+\s*$', partial_function_body) \
                or search(r'[^_]ASSERT_NOT_REACHED\(\);(\s*[^;]+;)?\s*return(\s+[^;]+)?;', partial_function_body) \
                or search(r'(default|case\s+.+):\s*[^_]ASSERT_NOT_REACHED\(\);(\s*[^;]+;)*\s*break;', partial_function_body):
            continue

        error(line_number - (function_line_count - (i + 1)), 'security/assertion_fallthrough', 4,
              'ASSERT_NOT_REACHED() statement fallthrough may result in unexpected code execution.')


def check_for_leaky_patterns(clean_lines, line_number, function_state, error):
    """Check for constructs known to be leak prone.
    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      function_state: Current function name and lines in body so far.
      error: The function to call with any errors found.
    """
    lines = clean_lines.lines
    line = lines[line_number]

    matched_get_dc = search(r'\b(?P<function_name>GetDC(Ex)?)\s*\(', line)
    if matched_get_dc:
        error(line_number, 'runtime/leaky_pattern', 5,
              'Use the class HWndDC instead of calling %s to avoid potential '
              'memory leaks.' % matched_get_dc.group('function_name'))

    matched_create_dc = search(r'\b(?P<function_name>Create(Compatible)?DC)\s*\(', line)
    matched_own_dc = search(r'\badoptGDIObject\b', line)
    if matched_create_dc and not matched_own_dc:
        error(line_number, 'runtime/leaky_pattern', 5,
              'Use adoptGDIObject and GDIObject<HDC> when calling %s to avoid potential '
              'memory leaks.' % matched_create_dc.group('function_name'))


def check_spacing(file_extension, clean_lines, line_number, file_state, error):
    """Checks for the correctness of various spacing issues in the code.

    Things we check for: spaces around operators, spaces after
    if/for/while/switch, no spaces around parens in function calls, two
    spaces between code and comment, don't start a block with a blank
    line, don't end a function with a blank line, don't have too many
    blank lines in a row.

    Args:
      file_extension: The current file extension, without the leading dot.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    raw = clean_lines.raw_lines
    line = raw[line_number]

    # Before nixing comments, check if the line is blank for no good
    # reason.  This includes the first line after a block is opened, and
    # blank lines at the end of a function (ie, right before a line like '}').
    if is_blank_line(line):
        elided = clean_lines.elided
        previous_line = elided[line_number - 1]
        previous_brace = previous_line.rfind('{')
        # FIXME: Don't complain if line before blank line, and line after,
        #        both start with alnums and are indented the same amount.
        #        This ignores whitespace at the start of a namespace block
        #        because those are not usually indented.
        if (previous_brace != -1 and previous_line[previous_brace:].find('}') == -1
            and previous_line[:previous_brace].find('namespace') == -1):
            # OK, we have a blank line at the start of a code block.  Before we
            # complain, we check if it is an exception to the rule: The previous
            # non-empty line has the parameters of a function header that are indented
            # 4 spaces (because they did not fit in a 80 column line when placed on
            # the same line as the function name).  We also check for the case where
            # the previous line is indented 6 spaces, which may happen when the
            # initializers of a constructor do not fit into a 80 column line.
            exception = False
            if match(r' {6}\w', previous_line):  # Initializer list?
                # We are looking for the opening column of initializer list, which
                # should be indented 4 spaces to cause 6 space indentation afterwards.
                search_position = line_number - 2
                while (search_position >= 0
                       and match(r' {6}\w', elided[search_position])):
                    search_position -= 1
                exception = (search_position >= 0
                             and elided[search_position][:5] == '    :')
            else:
                # Search for the function arguments or an initializer list.  We use a
                # simple heuristic here: If the line is indented 4 spaces; and we have a
                # closing paren, without the opening paren, followed by an opening brace
                # or colon (for initializer lists) we assume that it is the last line of
                # a function header.  If we have a colon indented 4 spaces, it is an
                # initializer list.
                exception = (match(r' {4}\w[^\(]*\)\s*(const\s*)?(\{\s*$|:)',
                                   previous_line)
                             or match(r' {4}:', previous_line))

            if not exception:
                error(line_number, 'whitespace/blank_line', 2,
                      'Blank line at the start of a code block.  Is this needed?')
        # This doesn't ignore whitespace at the end of a namespace block
        # because that is too hard without pairing open/close braces;
        # however, a special exception is made for namespace closing
        # brackets which have a comment containing "namespace".
        #
        # Also, ignore blank lines at the end of a block in a long if-else
        # chain, like this:
        #   if (condition1) {
        #     // Something followed by a blank line
        #
        #   } else if (condition2) {
        #     // Something else
        #   }
        if line_number + 1 < clean_lines.num_lines():
            next_line = raw[line_number + 1]
            if (next_line
                and match(r'\s*}', next_line)
                and next_line.find('namespace') == -1
                and next_line.find('} else ') == -1):
                error(line_number, 'whitespace/blank_line', 3,
                      'Blank line at the end of a code block.  Is this needed?')

    # Next, we check for proper spacing with respect to comments.
    comment_position = line.find('//')
    if comment_position != -1:
        # Check if the // may be in quotes.  If so, ignore it
        # Comparisons made explicit for clarity
        if (line.count('"', 0, comment_position) - line.count('\\"', 0, comment_position)) % 2 == 0:   # not in quotes
            # Allow one space before end of line comment.
            if (not match(r'^\s*$', line[:comment_position])
                and (comment_position >= 1
                and ((line[comment_position - 1] not in string.whitespace)
                     or (comment_position >= 2
                         and line[comment_position - 2] in string.whitespace)))):
                error(line_number, 'whitespace/comments', 5,
                      'One space before end of line comments')
            # There should always be a space between the // and the comment
            commentend = comment_position + 2
            if commentend < len(line) and not line[commentend] == ' ':
                # but some lines are exceptions -- e.g. if they're big
                # comment delimiters like:
                # //----------------------------------------------------------
                # or they begin with multiple slashes followed by a space:
                # //////// Header comment
                matched = (search(r'[=/-]{4,}\s*$', line[commentend:])
                           or search(r'^/+ ', line[commentend:]))
                if not matched:
                    error(line_number, 'whitespace/comments', 4,
                          'Should have a space between // and comment')

            # There should only be one space after punctuation in a comment.
            if search(r'[.!?,;:]\s\s+\w', line[comment_position:]):
                error(line_number, 'whitespace/comments', 5,
                      'Should have only a single space after a punctuation in a comment.')

    line = clean_lines.elided[line_number]  # get rid of comments and strings

    # Objective-C @property lines.
    is_objective_c_property = search(r'^@property', line)
    if is_objective_c_property:
        # "@property (attr)" not "@property(attr)"
        if search(r'^@property\(', line):
            error(line_number, 'whitespace/property', 4,
                  'Should have space between @property and attributes.')
        # "(getter=foo)" not "(getter = foo)"
        if search(r'(\s+=|=\s+)', line):
            error(line_number, 'whitespace/property', 4,
                  'Should not have spaces around = in property attributes.')

    # Objective-C @synthesize lines.
    is_objective_c_synthesize = search(r'^@synthesize', line)
    if is_objective_c_synthesize:
        # "prop = _varName" not "prop=_varName"
        # We skip the check in case @synthesize is used without equal.
        if search(r'=', line) and not search(r'\s+=\s+', line):
            error(line_number, 'whitespace/property', 4,
                  'Should have spaces around = in property synthesis.')

    # Don't try to do spacing checks for operator methods
    line = sub(r'operator(==|!=|<|<<|<=|>=|>>|>|\+=|-=|\*=|/=|%=|&=|\|=|^=|<<=|>>=|/)\(', r'operator\(', line)
    # Don't try to do spacing checks for #include, #import, #if, or #elif statements at
    # minimum because it messes up checks for spacing around /
    if match(r'\s*#\s*(?:include|import|if|elif)', line):
        return
    if not is_objective_c_property and not is_objective_c_synthesize and search(r'[\w.]=[\w.]', line):
        error(line_number, 'whitespace/operators', 4,
              'Missing spaces around =')

    # FIXME: It's not ok to have spaces around binary operators like .

    # You should always have whitespace around binary operators.
    # Alas, we can't test <, >, <<, >>, or && because they're legitimately used sans spaces
    # (a->b, vector<int> a, Foo&& a).  The only time we can tell is a < with no >, and
    # only if it's not template params list spilling into the next line.
    matched = search(r'[^<>=!\s](==|!=|\+=|-=|\*=|/=|/|\|=|&=|<<=|>>=|<=|>=|\|\||\|)[^<>=!\s]', line)
    if not matched:
        # Note that while it seems that the '<[^<]*' term in the following
        # regexp could be simplified to '<.*', which would indeed match
        # the same class of strings, the [^<] means that searching for the
        # regexp takes linear rather than quadratic time.
        if not search(r'<[^<]*,\s*$', line):  # template params spill
            matched = search(r'[^<>=!\s](<)[^<>=!\s]([^>]|->)*$', line)
    # It is necessary to check this, because rvaule references can be in
    # parameter packs (c++11 feature)
    if matched and not search(r'&&\.\.\.', line):
        error(line_number, 'whitespace/operators', 3,
              'Missing spaces around %s' % matched.group(1))

    # There shouldn't be space around unary operators
    matched = search(r'(!\s|~\s|[\s]--[\s;]|[\s]\+\+[\s;])', line)
    if matched:
        error(line_number, 'whitespace/operators', 4,
              'Extra space for operator %s' % matched.group(1))

    # A pet peeve of mine: no spaces after an if, while, switch, or for
    matched = search(r' (if\(|for\(|while\(|switch\()', line)
    if matched:
        error(line_number, 'whitespace/parens', 5,
              'Missing space before ( in %s' % matched.group(1))

    # For if/for/while/switch, the left and right parens should be
    # consistent about how many spaces are inside the parens, and
    # there should either be zero or one spaces inside the parens.
    # We don't want: "if ( foo)" or "if ( foo   )".
    # Exception: "for ( ; foo; bar)" and "for (foo; bar; )" are allowed.
    # Exception: "for (foo n in [foo bar:baz])" is allowed because of obj-c method calls
    matched = search(r'\b(?P<statement>if|for|while|switch)\s*\((?P<remainder>.*)$', line)
    if matched:
        statement = matched.group('statement')
        condition, rest = up_to_unmatched_closing_paren(matched.group('remainder'))
        if condition is not None:
            if statement == 'for' and search(r'(?:[^ :]:[^:]|[^:]:[^ :])', condition) and not search(r'\[[^\]]+:[^\]]*\]', condition):
                    error(line_number, 'whitespace/colon', 4, 'Missing space around : in range-based for statement')
            condition_match = search(r'(?P<leading>[ ]*)(?P<separator>.).*[^ ]+(?P<trailing>[ ]*)', condition)
            if condition_match:
                n_leading = len(condition_match.group('leading'))
                n_trailing = len(condition_match.group('trailing'))
                if n_leading != 0:
                    for_exception = statement == 'for' and condition.startswith(' ;')
                    if not for_exception:
                        error(line_number, 'whitespace/parens', 5,
                              'Extra space after ( in %s' % statement)
                if n_trailing != 0:
                    for_exception = statement == 'for' and condition.endswith('; ')
                    if not for_exception:
                        error(line_number, 'whitespace/parens', 5,
                              'Extra space before ) in %s' % statement)

            # Do not check for more than one command in macros
            in_preprocessor_directive = match(r'\s*#', line)
            if not in_preprocessor_directive and not match(r'((\s*{\s*}?)|(\s*;?))\s*\\?$', rest):
                error(line_number, 'whitespace/parens', 4,
                      'More than one command on the same line in %s' % statement)

    # You should always have a space after a comma (either as fn arg or operator)
    if search(r',[^\s]', line):
        error(line_number, 'whitespace/comma', 3,
              'Missing space after ,')

    matched = search(r'^\s*(?P<token1>[a-zA-Z0-9_\*&]+)\s\s+(?P<token2>[a-zA-Z0-9_\*&]+)', line)
    if matched:
        error(line_number, 'whitespace/declaration', 3,
              'Extra space between %s and %s' % (matched.group('token1'), matched.group('token2')))

    if file_extension == 'cpp':
        # C++ should have the & or * beside the type not the variable name.
        matched = match(r'\s*(?P<pre_part>\w[\w\s]+)\s+(?P<pointer_operator>\*|\&)\s*\w+', line)
        if matched and not matched.group('pre_part').startswith('return') and not matched.group('pre_part').startswith('delete'):
            error(line_number, 'whitespace/declaration', 3,
                  'Declaration has space between type name and %s in %s' % (matched.group('pointer_operator'), matched.group(0).strip()))

    elif file_extension == 'c':
        # C Pointer declaration should have the * beside the variable not the type name.
        matched = search(r'^\s*\w+\*\s+\w+', line)
        if matched:
            error(line_number, 'whitespace/declaration', 3,
                  'Declaration has space between * and variable name in %s' % matched.group(0).strip())

    # Next we will look for issues with function calls.
    check_spacing_for_function_call(line, line_number, file_state, error)

    # Except after an opening paren, ^ for blocks, @ for Objective-C literal
    # NSDictionary, or os_log format parameters, you should have spaces before
    # your braces. Since you should never have braces at the beginning of a
    # line, this is an easy test.
    if search(r'[^ ({\^@%]{', line):
        error(line_number, 'whitespace/braces', 5,
              'Missing space before {')

    # Make sure '} else {' has spaces.
    if search(r'}else', line):
        error(line_number, 'whitespace/braces', 5,
              'Missing space before else')

    # You shouldn't have spaces before your brackets, except maybe after
    # 'delete []' or 'new char * []'. Objective-C can't follow this rule
    # because of method calls.
    if file_extension != 'mm' and file_extension != 'm':
        if search(r'\w\s+\[', line) and not search(r'(delete|return|auto)\s+\[', line):
            error(line_number, 'whitespace/brackets', 5,
                  'Extra space before [.')

    # Try to avoid false positives when braces are inside of a string
    if not search(r'"[^"]*\{.*\}[^"]*"', line):
        # There should always be a single space in between braces on the same
        # line.
        if search(r'\{\}', line):
            error(line_number, 'whitespace/braces', 5, 'Missing space inside { }.')
        if search(r'\{\s\s+\}', line):
            error(line_number, 'whitespace/braces', 5, 'Too many spaces inside { }.')

        # Also a single space inside of braces with an initializer value inside
        # of them.
        if search(r'\{[^\s\}]', line):
            error(line_number, 'whitespace/braces', 5, 'Missing space after {.')
        if search(r'[^\s\{]\}', line):
            error(line_number, 'whitespace/braces', 5, 'Missing space before }.')
        if search(r'\{\s\s+[^\s\}]', line):
            error(line_number, 'whitespace/braces', 5, 'Too many spaces after {.')
        if search(r'[^\s\{]\s\+\}', line):
            error(line_number, 'whitespace/braces', 5, 'Too many spaces before }.')

    # You shouldn't have a space before a semicolon at the end of the line.
    # There's a special case for "for" since the style guide allows space before
    # the semicolon there.
    if search(r':\s*;\s*$', line):
        error(line_number, 'whitespace/semicolon', 5,
              'Semicolon defining empty statement. Use { } instead.')
    elif search(r'^\s*;\s*$', line):
        error(line_number, 'whitespace/semicolon', 5,
              'Line contains only semicolon. If this should be an empty statement, '
              'use { } instead.')
    elif (search(r'\s+;\s*$', line) and not search(r'\bfor\b', line)):
        error(line_number, 'whitespace/semicolon', 5,
              'Extra space before last semicolon. If this should be an empty '
              'statement, use { } instead.')
    elif (search(r'\b(for|while)\s*\(.*\)\s*;\s*$', line)
          and line.count('(') == line.count(')')
          # Allow do {} while();
          and not search(r'}\s*while', line)):
        error(line_number, 'whitespace/semicolon', 5,
              'Semicolon defining empty statement for this loop. Use { } instead.')


def check_member_initialization_list(clean_lines, line_number, error):
    """ Look for style errors in member initialization list of classes.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    raw = clean_lines.raw_lines
    line = raw[line_number]

    if search(r'\b([A-Za-z0-9_]*_)\(\1\)', line):
        error(line_number, 'runtime/init', 4,
              'You seem to be initializing a member variable with itself.')

    # Check the style of the initializer list.
    # Each member (and superclass) should be indented on a separate line,
    # with the colon or comma preceding the member on that line.
    begin_line = line
    # match the start of initialization list
    if search(r'^(?P<indentation>\s*)((explicit\s+)?[^(\s|\?)]+\([^\?]*\)\s?\:|^(\s|\?)*\:)([^\:]|\Z)[^;]*$', line):
        if search(r'[^:]\:[^\:\s]+', line) and not search(r'^\s*:\s\S+', line):
            error(line_number, 'whitespace/init', 4,
                'Missing spaces around :')
        if (not line.lstrip().startswith(':')) and search(r'[^\s]\(.*\)\s?\:.*[^;]*$', line):
            error(line_number, 'whitespace/indent', 4,
                'Should be indented on a separate line, with the colon or comma first on that line.')
        else:
            begin_line, begin_line_number = get_previous_non_blank_line(clean_lines, line_number)

        matched = search(r'(?P<indentation>\s*).*', begin_line)
        indentation = matched.group('indentation')
        inner_indentation = indentation + ' ' * 4

        while(not search(r'{', line)):
            # Don't check inheritance style and precompiler directives
            if (not line.startswith('#')) and search(r'\S\(.*\)', line):
                if not line.startswith(inner_indentation) and begin_line != line:
                    error(line_number, 'whitespace/indent', 4,
                        'Wrong number of spaces before statement. (expected: %d)' % len(inner_indentation))
                if search(r'\S\s*,\s*$', line):
                    error(line_number, 'whitespace/init', 4,
                        'Comma should be at the beginning of the line in a member initialization list.')

            # To avoid infinite loop, if can't find the end of member initialization list
            if line_number < len(raw) - 1:
                line_number = line_number + 1
                line = raw[line_number]
            else:
                break


def get_previous_non_blank_line(clean_lines, line_number):
    """Return the most recent non-blank line and its line number.

    Args:
      clean_lines: A CleansedLines instance containing the file contents.
      line_number: The number of the line to check.

    Returns:
      A tuple with two elements.  The first element is the contents of the last
      non-blank line before the current line, or the empty string if this is the
      first non-blank line.  The second is the line number of that line, or -1
      if this is the first non-blank line.
    """

    previous_line_number = line_number - 1
    while previous_line_number >= 0:
        previous_line = clean_lines.elided[previous_line_number]
        if not is_blank_line(previous_line):     # if not a blank line...
            return (previous_line, previous_line_number)
        previous_line_number -= 1
    return ('', -1)


def check_namespace_indentation(clean_lines, line_number, file_extension, file_state, error):
    """Looks for indentation errors inside of namespaces.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_extension: The extension (dot not included) of the file.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    namespace_match = match(r'(?P<namespace_indentation>\s*)namespace\s+\S+\s*{\s*$', line)
    if not namespace_match:
        return

    current_indentation_level = len(namespace_match.group('namespace_indentation'))
    if current_indentation_level > 0:
        # Don't warn about an indented namespace if we already warned about indented code.
        if not file_state.did_inside_namespace_indent_warning():
            error(line_number, 'whitespace/indent', 4,
                  'namespace should never be indented.')
        return
    looking_for_semicolon = False
    line_offset = 0
    in_preprocessor_directive = False
    for current_line in clean_lines.elided[line_number + 1:]:
        line_offset += 1
        if not current_line.strip():
            continue
        if not current_indentation_level:
            if not (in_preprocessor_directive or looking_for_semicolon):
                if not match(r'\S', current_line) and not file_state.did_inside_namespace_indent_warning():
                    file_state.set_did_inside_namespace_indent_warning()
                    error(line_number + line_offset, 'whitespace/indent', 4,
                          'Code inside a namespace should not be indented.')
            if in_preprocessor_directive or (current_line.strip()[0] == '#'):  # This takes care of preprocessor directive syntax.
                in_preprocessor_directive = current_line[-1] == '\\'
            else:
                looking_for_semicolon = ((current_line.find(';') == -1) and (current_line.strip()[-1] != '}')) or (current_line[-1] == '\\')
        else:
            looking_for_semicolon = False  # If we have a brace we may not need a semicolon.
        current_indentation_level += current_line.count('{') - current_line.count('}')
        current_indentation_level += current_line.count('(') - current_line.count(')')
        if current_indentation_level < 0:
            break


# Enum declaration allowlist
_ALLOW_ALL_UPPERCASE_ENUM = ['JSTokenType']

# Enum value allowlist
_ALLOW_ABBREVIATION_ENUM_VALUES = ['AM', 'CF', 'PM', 'URL', 'XHR']

def check_enum_casing(clean_lines, line_number, enum_state, error):
    """Looks for incorrectly named enum values.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      enum_state: A _EnumState instance which maintains enum declaration state.
      error: The function to call with any errors found.
    """

    enum_state.is_webidl_enum |= bool(match(r'\s*// Web(?:Kit)?IDL enum\s*$', clean_lines.raw_lines[line_number]))

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.
    if not enum_state.process_clean_line(line):
        error(line_number, 'readability/enum_casing', 4,
              'enum members should use InterCaps with an initial capital letter or initial \'k\' for C-style enums.')


def check_once_flag(clean_lines, line_number, enum_state, error):
    """Looks for non-static std::once_flag / dispatch_once_t.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      enum_state: A _EnumState instance which maintains enum declaration state.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    using_std_once_flag = search(r'std::once_flag|dispatch_once_t', line)
    if not using_std_once_flag:
        return

    using_std_once_flag_with_static = search(r'static\s+(?:std::once_flag|dispatch_once_t)', line)
    if using_std_once_flag_with_static:
        return

    error(line_number, 'runtime/once_flag', 4, "std::once_flag / dispatch_once_t should be in `static` storage.")


def check_directive_indentation(clean_lines, line_number, file_state, error):
    """Looks for indentation of preprocessor directives.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    indented_preprocessor_directives = match(r'\s+#', line)
    if not indented_preprocessor_directives:
        return

    error(line_number, 'whitespace/indent', 4, 'preprocessor directives (e.g., #ifdef, #define, #import) should never be indented.')


def get_initial_spaces_for_line(clean_line):
    initial_spaces = 0
    while initial_spaces < len(clean_line) and clean_line[initial_spaces] == ' ':
        initial_spaces += 1
    return initial_spaces


def check_indentation_amount(clean_lines, line_number, error):
    line = clean_lines.elided[line_number]
    initial_spaces = get_initial_spaces_for_line(line)

    if initial_spaces % 4:
        error(line_number, 'whitespace/indent', 3,
              'Weird number of spaces at line-start.  Are you using a 4-space indent?')
        return

    previous_line = get_previous_non_blank_line(clean_lines, line_number)[0]
    if not previous_line.strip() or match(r'\s*\w+\s*:\s*$', previous_line) or previous_line[0] == '#':
        return

    previous_line_initial_spaces = get_initial_spaces_for_line(previous_line)
    if initial_spaces > previous_line_initial_spaces + 4:
        error(line_number, 'whitespace/indent', 3, 'When wrapping a line, only indent 4 spaces.')


def check_using_std(clean_lines, line_number, file_state, error):
    """Looks for 'using std::foo;' statements which should be replaced with 'using namespace std;'.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """

    # This check doesn't apply to C or Objective-C implementation files.
    if file_state.is_c_or_objective_c():
        return

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    using_std_match = match(r'\s*using\s+std::(?P<method_name>\S+)\s*;\s*$', line)
    if not using_std_match:
        return

    method_name = using_std_match.group('method_name')
    error(line_number, 'build/using_std', 4,
          "Use 'using namespace std;' instead of 'using std::%s;'." % method_name)


def check_using_namespace(clean_lines, line_number, file_extension, error):
    """Looks for 'using namespace foo;' which should be removed.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_extension: The extension (dot not included) of the file.
      error: The function to call with any errors found.
    """

    # This check applies only to headers.
    if file_extension != 'h':
        return

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    using_namespace_match = match(r'\s*using\s+namespace\s+(?P<method_name>\S+)\s*;\s*$', line)
    if not using_namespace_match:
        return

    method_name = using_namespace_match.group('method_name')
    error(line_number, 'build/using_namespace', 4,
          "Do not use 'using namespace %s;'." % method_name)


def check_max_min_macros(clean_lines, line_number, file_state, error):
    """Looks for use of MAX() and MIN() macros that should be replaced with std::max() and std::min().

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """

    # This check doesn't apply to C or Objective-C implementation files.
    if file_state.is_c_or_objective_c():
        return

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    max_min_macros_search = search(r'\b(?P<max_min_macro>(MAX|MIN))\s*\(', line)
    if not max_min_macros_search:
        return

    max_min_macro = max_min_macros_search.group('max_min_macro')
    max_min_macro_lower = max_min_macro.lower()
    error(line_number, 'runtime/max_min_macros', 4,
          'Use std::%s() or std::%s<type>() instead of the %s() macro.'
          % (max_min_macro_lower, max_min_macro_lower, max_min_macro))


def check_wtf_checked_size(clean_lines, line_number, file_state, error):
    """Looks for use of 'Checked<size_t, RecordOverflow>' which should be replaced with 'CheckedSize'.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """

    if file_state.is_c_or_objective_c():
        return

    line = clean_lines.elided[line_number]

    using_checked_size_record_overflow = search(r'\bChecked\s*<\s*size_t,\s*RecordOverflow\s*>\s*(\b|\()', line)
    if not using_checked_size_record_overflow:
        return

    error(line_number, 'runtime/wtf_checked_size', 5,
          "Use 'CheckedSize' instead of 'Checked<size_t, RecordOverflow>'.")


def check_wtf_move(clean_lines, line_number, file_state, error):
    """Looks for use of 'std::move()' which should be replaced with 'WTFMove()'.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """

    # This check doesn't apply to C or Objective-C implementation files.
    if file_state.is_c_or_objective_c():
        return

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    using_std_move = search(r'\bstd::move\s*\(', line)
    if not using_std_move:
        return

    error(line_number, 'runtime/wtf_move', 4, "Use 'WTFMove()' instead of 'std::move()'.")


def check_callonmainthread(filename, clean_lines, line_number, file_state, error):
    """Looks for use of 'callOnMainThread()' which should be replaced with 'callOnMainRunLoop()'.

    Args:
      filename: The current file cpp_style is running over.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """

    if not _is_webkit2_file(filename):
        return

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.
    using_callonmainthread = search(r'\bcallOnMainThread\s*\(', line)
    if using_callonmainthread:
        error(line_number, 'runtime/callonmainthread', 4, "Use 'callOnMainRunLoop()' instead of 'callOnMainThread()' in Source/WebKit.")
    using_callonmainthreadandwait = search(r'\bcallOnMainThreadAndWait\s*\(', line)
    if using_callonmainthreadandwait:
        error(line_number, 'runtime/callonmainthread', 4, "Use 'callOnMainRunLoopAndWait()' instead of 'callOnMainThreadAndWait()' in Source/WebKit.")


def check_ismainthread(filename, clean_lines, line_number, file_state, error):
    """Looks for use of 'isMainThread()' which should be replaced with 'isMainRunLoop()'.

    Args:
      filename: The current file cpp_style is running over.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """

    if not _is_webkit2_file(filename):
        return

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.
    using_ismainthread = search(r'\bisMainThread\s*\(', line)
    if not using_ismainthread:
        return

    error(line_number, 'runtime/ismainthread', 4, "Use 'isMainRunLoop()' instead of 'isMainThread()' in Source/WebKit.")


def check_wtf_make_unique(clean_lines, line_number, file_state, error):
    """Looks for use of 'std::make_unique<>' which should be replaced with 'WTF::makeUnique<>'.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    using_std_make_unique_search = search(r'\bstd::make_unique\s*<([^(]+)', line)
    if not using_std_make_unique_search:
        return

    typename = using_std_make_unique_search.group(1).strip()[:-1].strip()
    if typename.endswith('[]'):
        error(line_number, 'runtime/wtf_make_unique', 4,
              "Use 'WTF::makeUniqueArray<{new_typename}>' instead of 'std::make_unique<{original_typename}>'.".format(
                  new_typename=typename[:-2], original_typename=typename))
    else:
        error(line_number, 'runtime/wtf_make_unique', 4,
              "Use 'WTF::makeUnique<{typename}>' instead of 'std::make_unique<{typename}>'.".format(typename=typename))


def check_wtf_never_destroyed(clean_lines, line_number, file_state, error):
    """Looks for use of 'NeverDestroyed<Lock/Condition>' which should be replaced with 'Lock/Condition'.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    using_wtf_never_destroyed_search = search(r'\b(?:Lazy)?NeverDestroyed\s*<([^(>]+)>', line)  # LazyNeverDestroyed is also caught.
    if not using_wtf_never_destroyed_search:
        return

    typename = using_wtf_never_destroyed_search.group(1).strip()
    if search(r'(Lock|Condition)', typename):
        error(line_number, 'runtime/wtf_never_destroyed', 4, "Use 'static Lock/Condition' instead of 'NeverDestroyed<Lock/Condition>'.")


def check_lock_guard(clean_lines, line_number, file_state, error):
    """Looks for use of 'std::lock_guard<>' which should be replaced with 'WTF::Locker'.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    using_std_lock_guard_search = search(r'\bstd::lock_guard\s*<([^(]+)', line)
    if not using_std_lock_guard_search:
        return

    error(line_number, 'runtime/lock_guard', 4, "Use 'Locker locker { lock }' instead of 'std::lock_guard<>'.")


def check_ctype_functions(clean_lines, line_number, file_state, error):
    """Looks for use of the standard functions in ctype.h and suggest they be replaced
       by use of equivilent ones in <wtf/ASCIICType.h>?.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    ctype_function_search = search(r'\b(?P<ctype_function>(isalnum|isalpha|isascii|isblank|iscntrl|isdigit|isgraph|islower|isprint|ispunct|isspace|isupper|isxdigit|toascii|tolower|toupper))\s*\(', line)
    if not ctype_function_search:
        return

    ctype_function = ctype_function_search.group('ctype_function')
    error(line_number, 'runtime/ctype_function', 4,
          'Use equivelent function in <wtf/ASCIICType.h> instead of the %s() function.'
          % (ctype_function))


def check_switch_indentation(clean_lines, line_number, error):
    """Looks for indentation errors inside of switch statements.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    switch_match = match(r'(?P<switch_indentation>\s*)switch\s*\(.+\)\s*{\s*$', line)
    if not switch_match:
        return

    switch_indentation = switch_match.group('switch_indentation')
    inner_indentation = switch_indentation + ' ' * 4
    line_offset = 0
    encountered_nested_switch = False

    for current_line in clean_lines.elided[line_number + 1:]:
        line_offset += 1

        # Skip not only empty lines but also those with preprocessor directives.
        if current_line.strip() == '' or current_line.startswith('#'):
            continue

        if match(r'\s*switch\s*\(.+\)\s*{\s*$', current_line):
            # Complexity alarm - another switch statement nested inside the one
            # that we're currently testing. We'll need to track the extent of
            # that inner switch if the upcoming label tests are still supposed
            # to work correctly. Let's not do that; instead, we'll finish
            # checking this line, and then leave it like that. Assuming the
            # indentation is done consistently (even if incorrectly), this will
            # still catch all indentation issues in practice.
            encountered_nested_switch = True

        current_indentation_match = match(r'(?P<indentation>\s*)(?P<remaining_line>.*)$', current_line)
        current_indentation = current_indentation_match.group('indentation')
        remaining_line = current_indentation_match.group('remaining_line')

        # End the check at the end of the switch statement.
        if remaining_line.startswith('}') and current_indentation == switch_indentation:
            break
        # Case and default branches should not be indented. The regexp also
        # catches single-line cases like "default: break;" but does not trigger
        # on stuff like "Document::Foo();".
        elif match(r'(default|case\s+.*)\s*:([^:].*)?$', remaining_line):
            if current_indentation != switch_indentation:
                error(line_number + line_offset, 'whitespace/indent', 4,
                      'A case label should not be indented, but line up with its switch statement.')
                # Don't throw an error for multiple badly indented labels,
                # one should be enough to figure out the problem.
                break
        # We ignore goto labels at the very beginning of a line.
        elif match(r'\w+\s*:\s*$', remaining_line):
            continue
        # It's not a goto label, so check if it's indented at least as far as
        # the switch statement plus one more level of indentation.
        elif not current_indentation.startswith(inner_indentation):
            error(line_number + line_offset, 'whitespace/indent', 4,
                  'Non-label code inside switch statements should be indented.')
            # Don't throw an error for multiple badly indented statements,
            # one should be enough to figure out the problem.
            break

        if encountered_nested_switch:
            break


def check_braces(clean_lines, line_number, file_state, error):
    """Looks for misplaced braces (e.g. at the end of line).

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    if match(r'\s*{\s*$', line):
        # We allow an open brace to start a line in the case where someone
        # is using braces for function definition or in a block to
        # explicitly create a new scope, which is commonly used to control
        # the lifetime of stack-allocated variables.  We don't detect this
        # perfectly: we just don't complain if the last non-whitespace
        # character on the previous non-blank line is ';', ':', '{', '}',
        # ')', or ') const' and doesn't begin with 'if|for|while|switch|else'.
        # We also allow '#' for #endif and '=' for array initialization,
        # and '- (' and '+ (' for Objective-C methods.
        # Also we don't complain if the last non-whitespace character
        # on the previous non-blank line is '{' because it's likely to
        # indicate the begining of a nested code block.
        previous_line = get_previous_non_blank_line(clean_lines, line_number)[0]
        if ((not search(r'[;:}{)=]\s*$|\)\s*((const|override|const override|final|const final|noexcept|const noexcept)\s*)?(->\s*\S+)?\s*$', previous_line)
             or search(r'\b(if|for|while|switch|else|CF_OPTIONS|NS_ENUM|NS_ERROR_ENUM|NS_OPTIONS)\b', previous_line)
             or regex_for_lambdas_and_blocks(previous_line, line_number, file_state, error))
            and previous_line.find('#') < 0
            and previous_line.find('- (') != 0
            and previous_line.find('+ (') != 0
            and not search(r'{\s*$', previous_line)):
            error(line_number, 'whitespace/braces', 4,
                  'This { should be at the end of the previous line')
    elif (search(r'\)\s*(((const|override|final)\s*)*\s*)?{\s*$', line)
          and line.count('(') == line.count(')')
          and not search(r'(\s*(if|for|while|switch|CF_OPTIONS|NS_ENUM|NS_ERROR_ENUM|NS_OPTIONS|@synchronized)|} @catch)\b', line)
          and not regex_for_lambdas_and_blocks(line, line_number, file_state, error)
          and line.find("](") < 0
          and not match(r'\s+[A-Z_][A-Z_0-9]+\b', line)):
        error(line_number, 'whitespace/braces', 4,
              'Place brace on its own line for function definitions.')

    if (match(r'\s*}\s*(else\s*({\s*)?)?$', line) and line_number > 1):
        # We check if a closed brace has started a line to see if a
        # one line control statement was previous.
        previous_line = clean_lines.elided[line_number - 2]
        last_open_brace = previous_line.rfind('{')
        if (last_open_brace != -1 and previous_line.find('}', last_open_brace) == -1
            and search(r'\b(if|for|while|else)\b', previous_line)):
            error(line_number, 'whitespace/braces', 4,
                  'One line control clauses should not use braces.')

    # An else clause should be on the same line as the preceding closing brace.
    if match(r'\s*else\s*', line):
        previous_line = get_previous_non_blank_line(clean_lines, line_number)[0]
        if match(r'\s*}\s*$', previous_line):
            error(line_number, 'whitespace/newline', 4,
                  'An else should appear on the same line as the preceding }')

    # Likewise, an else should never have the else clause on the same line
    if (search(r'\belse [^\s{]', line)
        and not search(r'\belse if\b', line)
        and not search(r'\belse\s*\\$', line)):
        error(line_number, 'whitespace/newline', 4,
              'Else clause should never be on same line as else (use 2 lines)')

    # In the same way, a do/while should never be on one line
    if match(r'\s*do [^\s{]', line):
        error(line_number, 'whitespace/newline', 4,
              'do/while clauses should not be on a single line')

    # Multi line control clauses should use braces. We check the
    # indentation level of the statements.
    if (match(r'^\s*\b(if|for|while|else)\b\s', line)
        and match(r'.*[^{]$', line)
        and len(clean_lines.elided) > line_number + 2):
        has_braces = False
        begin_line = line
        begin_line_number = line_number
        while (clean_lines.elided[begin_line_number + 1].strip().startswith("&&")
            or clean_lines.elided[begin_line_number + 1].strip().startswith("||")
            or search(r'^#\S*', clean_lines.elided[begin_line_number + 1])):
            begin_line_number = begin_line_number + 1
            begin_line = clean_lines.elided[begin_line_number]

        if search(r'.*{(.*?\\)?$', begin_line):
            has_braces = True

        next_line = clean_lines.elided[begin_line_number + 1]
        after_next_line = clean_lines.elided[begin_line_number + 2]
        control_indent = search(r'^(?P<indentation>\s*).*', line).group('indentation')
        next_line_indent = search(r'^(?P<indentation>\s*).*', next_line).group('indentation')
        after_next_line_indent = search(r'^(?P<indentation>\s*).*', after_next_line).group('indentation')
        if (after_next_line != ''
            and not has_braces
            and control_indent < next_line_indent
            and control_indent < after_next_line_indent):
            error(line_number, 'whitespace/braces', 4, 'Multi line control clauses should use braces.')

    # Braces shouldn't be followed by a ; unless they're defining a struct
    # or initializing an array.
    # We can't tell in general, but we can for some common cases.
    previous_line_number = line_number
    while True:
        (previous_line, previous_line_number) = get_previous_non_blank_line(clean_lines, previous_line_number)
        if match(r'\s+{.*}\s*;', line) and not previous_line.count(';'):
            line = previous_line + line
        else:
            break
    if (search(r'{.*}\s*;', line)
        and line.count('{') == line.count('}')
        and not search(r'struct|class|enum|\s*=\s*{', line)):
        error(line_number, 'readability/braces', 4,
              "You don't need a ; after a }")


def check_exit_statement_simplifications(clean_lines, line_number, error):
    """Looks for else or else-if statements that should be written as an
    if statement when the prior if concludes with a return, break, continue or
    goto statement.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    else_match = match(r'(?P<else_indentation>\s*)(\}\s*)?else(\s+if\s*\(|(?P<else>\s*(\{\s*)?\Z))', line)
    if not else_match:
        return

    else_indentation = else_match.group('else_indentation')
    inner_indentation = else_indentation + ' ' * 4

    previous_lines = clean_lines.elided[:line_number]
    previous_lines.reverse()
    line_offset = 0
    encountered_exit_statement = False

    for current_line in previous_lines:
        line_offset -= 1

        # Skip not only empty lines but also those with preprocessor directives
        # and goto labels.
        if current_line.strip() == '' or current_line.startswith('#') or match(r'\w+\s*:\s*$', current_line):
            continue

        # Skip lines with closing braces on the original indentation level.
        # Even though the styleguide says they should be on the same line as
        # the "else if" statement, we also want to check for instances where
        # the current code does not comply with the coding style. Thus, ignore
        # these lines and proceed to the line before that.
        if current_line == else_indentation + '}':
            continue

        current_indentation_match = match(r'(?P<indentation>\s*)(?P<remaining_line>.*)$', current_line)
        current_indentation = current_indentation_match.group('indentation')
        remaining_line = current_indentation_match.group('remaining_line')

        # As we're going up the lines, the first real statement to encounter
        # has to be an exit statement (return, break, continue or goto) -
        # otherwise, this check doesn't apply.
        if not encountered_exit_statement:
            # We only want to find exit statements if they are on exactly
            # the same level of indentation as expected from the code inside
            # the block. If the indentation doesn't strictly match then we
            # might have a nested if or something, which must be ignored.
            if current_indentation != inner_indentation:
                break
            if match(r'(return(\W+.*)|(break|continue)\s*;|goto\s*\w+;)$', remaining_line):
                encountered_exit_statement = True
                continue
            break

        # When code execution reaches this point, we've found an exit statement
        # as last statement of the previous block. Now we only need to make
        # sure that the block belongs to an "if", then we can throw an error.

        # Skip lines with opening braces on the original indentation level,
        # similar to the closing braces check above. ("if (condition)\n{")
        if current_line == else_indentation + '{':
            continue

        # Skip everything that's further indented than our "else" or "else if".
        if current_indentation.startswith(else_indentation) and current_indentation != else_indentation:
            continue

        # So we've got a line with same (or less) indentation. Is it an "if"?
        # If yes: throw an error. If no: don't throw an error.
        # Whatever the outcome, this is the end of our loop.
        if match(r'if\s*\(', remaining_line):
            if else_match.start('else') != -1:
                error(line_number + line_offset, 'readability/control_flow', 4,
                      'An else statement can be removed when the prior "if" '
                      'concludes with a return, break, continue or goto statement.')
            else:
                error(line_number + line_offset, 'readability/control_flow', 4,
                      'An else if statement should be written as an if statement '
                      'when the prior "if" concludes with a return, break, '
                      'continue or goto statement.')
        break


def replaceable_check(operator, macro, line):
    """Determine whether a basic CHECK can be replaced with a more specific one.

    For example suggest using CHECK_EQ instead of CHECK(a == b) and
    similarly for CHECK_GE, CHECK_GT, CHECK_LE, CHECK_LT, CHECK_NE.

    Args:
      operator: The C++ operator used in the CHECK.
      macro: The CHECK or EXPECT macro being called.
      line: The current source line.

    Returns:
      True if the CHECK can be replaced with a more specific one.
    """

    # This matches decimal and hex integers, strings, and chars (in that order).
    match_constant = r'([-+]?(\d+|0[xX][0-9a-fA-F]+)[lLuU]{0,3}|".*"|\'.*\')'

    # Expression to match two sides of the operator with something that
    # looks like a literal, since CHECK(x == iterator) won't compile.
    # This means we can't catch all the cases where a more specific
    # CHECK is possible, but it's less annoying than dealing with
    # extraneous warnings.
    match_this = (r'\s*' + macro + r'\((\s*' +
                  match_constant + r'\s*' + operator + r'[^<>].*|'
                  r'.*[^<>]' + operator + r'\s*' + match_constant +
                  r'\s*\))')

    # Don't complain about CHECK(x == NULL) or similar because
    # CHECK_EQ(x, NULL) won't compile (requires a cast).
    # Also, don't complain about more complex boolean expressions
    # involving && or || such as CHECK(a == b || c == d).
    return match(match_this, line) and not search(r'NULL|&&|\|\|', line)


def check_check(clean_lines, line_number, error):
    """Checks the use of CHECK and EXPECT macros.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    # Decide the set of replacement macros that should be suggested
    raw_lines = clean_lines.raw_lines
    current_macro = ''
    for macro in _CHECK_MACROS:
        if raw_lines[line_number].find(macro) >= 0:
            current_macro = macro
            break
    if not current_macro:
        # Don't waste time here if line doesn't contain 'CHECK' or 'EXPECT'
        return

    line = clean_lines.elided[line_number]        # get rid of comments and strings

    # Encourage replacing plain CHECKs with CHECK_EQ/CHECK_NE/etc.
    for operator in ['==', '!=', '>=', '>', '<=', '<']:
        if replaceable_check(operator, current_macro, line):
            error(line_number, 'readability/check', 2,
                  'Consider using %s instead of %s(a %s b)' % (
                      _CHECK_REPLACEMENT[current_macro][operator],
                      current_macro, operator))
            break


def check_for_comparisons_to_zero(clean_lines, line_number, error):
    # Get the line without comments and strings.
    line = clean_lines.elided[line_number]

    # Include NULL here so that users don't have to convert NULL to 0 first and then get this error.
    if search(r'[=!]=\s*(NULL|0|true|false)[^\w.]', line) or search(r'[^\w.](NULL|0|true|false)\s*[=!]=', line):
        if not search('LIKELY', line) and not search('UNLIKELY', line):
            error(line_number, 'readability/comparison_to_zero', 5,
                  'Tests for true/false, null/non-null, and zero/non-zero should all be done without equality comparisons.')


def check_for_null(clean_lines, line_number, file_state, error):
    # This check doesn't apply to C, Objective-C, Objective-C++ implementation files.
    if file_state.is_c() or file_state.is_objective_c_or_objective_cpp():
        return

    line = clean_lines.elided[line_number]

    # Don't warn about NULL usage in g_*(). See Bug 32858 and 39372.
    if search(r'\bg(_[a-z]+)+\b', line):
        return

    # Don't warn about NULL usage in gst_*(). See Bug 70498.
    if search(r'\bgst(_[a-z]+)+\b', line):
        return

    # Don't warn about NULL usage in gdk_pixbuf_save_to_*{join,concat}(). See Bug 43090.
    if search(r'\bgdk_pixbuf_save_to\w+\b', line):
        return

    # Don't warn about NULL usage in gtk_widget_style_get(), gtk_style_context_get_style(), or gtk_style_context_get(). See Bug 51758
    if search(r'\bgtk_widget_style_get\(\w+\b', line) or search(r'\bgtk_style_context_get_style\(\w+\b', line) or search(r'\bgtk_style_context_get\(\w+\b', line):
        return

    # Don't warn about NULL usage in soup_server_new(). See Bug 77890.
    if search(r'\bsoup_server_new\(\w+\b', line):
        return

    if search(r'\bNULL\b', line):
        # FIXME: We should recommend using nullptr instead of NULL in C++ code per
        # <https://www.webkit.org/coding/coding-style.html#zero-null>.
        error(line_number, 'readability/null', 5, 'Use nullptr instead of NULL.')
        return

    line = clean_lines.raw_lines[line_number]
    # See if NULL occurs in any comments in the line. If the search for NULL using the raw line
    # matches, then do the check with strings collapsed to avoid giving errors for
    # NULLs occurring in strings.
    if search(r'\bNULL\b', line) and search(r'\bNULL\b', CleansedLines.collapse_strings(line)):
        error(line_number, 'readability/null', 4, 'Use nullptr instead of NULL (even in *comments*).')


def check_soft_link_class_alloc(clean_lines, line_number, error):
    """Checks that allocating an instance of a soft-linked class uses alloc[Class]Instance.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number]

    matched = search(r'\[get(\w+)Class\(\)\s+alloc\]', line)
    if matched:
        error(line_number, 'runtime/soft-linked-alloc', 4,
              'Using +alloc with a soft-linked class. Use alloc%sInstance() instead.' % matched.group(1))


def get_line_width(line):
    """Determines the width of the line in column positions.

    Args:
      line: A string, which may be a Unicode string.

    Returns:
      The width of the line in column positions, accounting for Unicode
      combining characters and wide characters.
    """
    if isinstance(line, unicode):
        width = 0
        for c in unicodedata.normalize('NFC', line):
            if unicodedata.east_asian_width(c) in ('W', 'F'):
                width += 2
            elif not unicodedata.combining(c):
                width += 1
        return width
    return len(line)


def check_arguments_for_wk_api_available(clean_lines, line_number, error):
    """Checks the allowable arguments of WK_API_AVAILABLE

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    @memoized
    def max_version_for_platform(platform_name):
        return VersionNameMap.map().max_public_version(platform=platform_name)

    def check_version_string(version_string, platform_name):
        mapping = {
            'macos': 'WK_MAC_TBA',
            'ios': 'WK_IOS_TBA',
        }

        platform_tba_macro = mapping.get(platform_name)
        if version_string == platform_tba_macro:
            return

        try:
            version = Version.from_string(version_string)
        except ValueError:
            error(line_number, 'build/wk_api_available', 5, '%s(%s) is invalid; expected %s or a major.minor version' % (platform_name, version_string, platform_tba_macro))
            return

        if not version_string.count('.'):
            error(line_number, 'build/wk_api_available', 5, '%s(%s) is invalid; version number should have one decimal' % (platform_name, version_string))
            return

        max_version = max_version_for_platform(platform_name)
        if version > max_version:
            error(line_number, 'build/wk_api_available', 5, '%s(%s) is invalid; version number should not exceed %s' % (platform_name, version_string, max_version))
            return

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    wk_api_available = search(r'WK_API_AVAILABLE\(macosx\(', line)
    if wk_api_available:
        error(line_number, 'build/wk_api_available', 5, 'macosx() is deprecated; use macos() instead')
        return

    wk_api_available = search(r'WK_API_AVAILABLE\(macos\(([^\)]+)\), ios\(([^\)]+)\)\)', line)
    if wk_api_available:
        check_version_string(wk_api_available.group(1), "macos")
        check_version_string(wk_api_available.group(2), "ios")

    wk_api_available = search(r'WK_API_AVAILABLE\(ios\(([^\)]+)\), macos\(([^\)]+)\)\)', line)
    if wk_api_available:
        check_version_string(wk_api_available.group(1), "ios")
        check_version_string(wk_api_available.group(2), "macos")

    wk_api_available = search(r'WK_API_AVAILABLE\(macos\(([^\)]+)\)\)', line)
    if wk_api_available:
        check_version_string(wk_api_available.group(1), "macos")

    wk_api_available = search(r'WK_API_AVAILABLE\(ios\(([^\)]+)\)\)', line)
    if wk_api_available:
        check_version_string(wk_api_available.group(1), "ios")


def check_objc_protocol(clean_lines, line_number, file_extension, error):
    """Looks for spaces between type names and protocol names.

    Because the style checker uses regexes, we don't know what's a type and what's a protocol.
    So, this check is a simplification and just checks for "id <"

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_extension: The extension (without the dot) of the filename.
      error: The function to call with any errors found.
    """

    if file_extension != "m" and file_extension != "mm":
        return

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    using_space_with_protocol = search(r'\bid <', line)
    if not using_space_with_protocol:
        return

    error(line_number, 'spacing/objc-protocol', 2, "Protocol names shouldn't have a space before them.")


def check_style(clean_lines, line_number, file_extension, class_state, file_state, enum_state, error):
    """Checks rules from the 'C++ style rules' section of cppguide.html.

    Most of these rules are hard to test (naming, comment style), but we
    do what we can.  In particular we check for 4-space indents, line lengths,
    tab usage, spaces inside code, etc.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_extension: The extension (without the dot) of the filename.
      class_state: A _ClassState instance which maintains information about
                   the current stack of nested class declarations being parsed.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      enum_state: A _EnumState instance which maintains the current enum state.
      error: The function to call with any errors found.
    """

    raw_lines = clean_lines.raw_lines
    line = raw_lines[line_number]

    if line.find('\t') != -1:
        error(line_number, 'whitespace/tab', 1,
              'Tab found; better to use spaces')

    cleansed_line = clean_lines.elided[line_number]
    if line and line[-1].isspace():
        error(line_number, 'whitespace/end_of_line', 4,
              'Line ends in whitespace.  Consider deleting these extra spaces.')

    if (cleansed_line.count(';') > 1
        # for loops are allowed two ;'s (and may run over two lines).
        and cleansed_line.find('for') == -1
        and (get_previous_non_blank_line(clean_lines, line_number)[0].find('for') == -1
             or get_previous_non_blank_line(clean_lines, line_number)[0].find(';') != -1)
        # It's ok to have many commands in a switch case that fits in 1 line
        and not ((cleansed_line.find('case ') != -1
                  or cleansed_line.find('default:') != -1)
                 and cleansed_line.find('break;') != -1)
        # Also it's ok to have many commands in trivial single-line accessors in class definitions.
        and not (match(r'.*\(.*\).*{.*.}', line)
                 and class_state.classinfo_stack
                 and line.count('{') == line.count('}'))
        and not cleansed_line.startswith('#define ')
        # It's ok to use use WTF_MAKE_NONCOPYABLE and WTF_MAKE_FAST_ALLOCATED macros in 1 line
        and not (cleansed_line.find("WTF_MAKE_NONCOPYABLE") != -1
                 and cleansed_line.find("WTF_MAKE_FAST_ALLOCATED") != -1)):
        error(line_number, 'whitespace/newline', 4,
              'More than one command on the same line')

    if cleansed_line.strip().endswith('||') or cleansed_line.strip().endswith(' &&'):
        error(line_number, 'whitespace/operators', 4,
              'Boolean expressions that span multiple lines should have their '
              'operators on the left side of the line instead of the right side.')

    # Some more style checks
    check_namespace_indentation(clean_lines, line_number, file_extension, file_state, error)
    check_directive_indentation(clean_lines, line_number, file_state, error)
    check_using_std(clean_lines, line_number, file_state, error)
    check_using_namespace(clean_lines, line_number, file_extension, error)
    check_max_min_macros(clean_lines, line_number, file_state, error)
    check_wtf_checked_size(clean_lines, line_number, file_state, error)
    check_wtf_move(clean_lines, line_number, file_state, error)
    check_wtf_make_unique(clean_lines, line_number, file_state, error)
    check_wtf_never_destroyed(clean_lines, line_number, file_state, error)
    check_lock_guard(clean_lines, line_number, file_state, error)
    check_ctype_functions(clean_lines, line_number, file_state, error)
    check_switch_indentation(clean_lines, line_number, error)
    check_braces(clean_lines, line_number, file_state, error)
    check_exit_statement_simplifications(clean_lines, line_number, error)
    check_spacing(file_extension, clean_lines, line_number, file_state, error)
    check_member_initialization_list(clean_lines, line_number, error)
    check_check(clean_lines, line_number, error)
    check_for_comparisons_to_zero(clean_lines, line_number, error)
    check_for_null(clean_lines, line_number, file_state, error)
    check_soft_link_class_alloc(clean_lines, line_number, error)
    check_indentation_amount(clean_lines, line_number, error)
    check_enum_casing(clean_lines, line_number, enum_state, error)
    check_once_flag(clean_lines, line_number, file_state, error)
    check_arguments_for_wk_api_available(clean_lines, line_number, error)
    check_objc_protocol(clean_lines, line_number, file_extension, error)


_RE_PATTERN_INCLUDE_NEW_STYLE = re.compile(r'#(?:include|import) +"[^/]+\.h"')
_RE_PATTERN_INCLUDE = re.compile(r'^\s*#\s*(?:include|import)\s*([<"])([^>"]*)[>"].*$')
# Matches the first component of a filename delimited by -s and _s. That is:
#  _RE_FIRST_COMPONENT.match('foo').group(0) == 'foo'
#  _RE_FIRST_COMPONENT.match('foo.cpp').group(0) == 'foo'
#  _RE_FIRST_COMPONENT.match('foo-bar_baz.cpp').group(0) == 'foo'
#  _RE_FIRST_COMPONENT.match('foo_bar-baz.cpp').group(0) == 'foo'
_RE_FIRST_COMPONENT = re.compile(r'^[^-_.]+')


def _drop_common_suffixes(filename):
    """Drops common suffixes like _test.cpp or -inl.h from filename.

    For example:
      >>> _drop_common_suffixes('foo/foo-inl.h')
      'foo/foo'
      >>> _drop_common_suffixes('foo/bar/foo.cpp')
      'foo/bar/foo'
      >>> _drop_common_suffixes('foo/foo_internal.h')
      'foo/foo'
      >>> _drop_common_suffixes('foo/foo_unusualinternal.h')
      'foo/foo_unusualinternal'

    Args:
      filename: The input filename.

    Returns:
      The filename with the common suffix removed.
    """
    for suffix in ('test.cpp', 'regtest.cpp', 'unittest.cpp',
                   'inl.h', 'impl.h', 'internal.h'):
        if (filename.endswith(suffix) and len(filename) > len(suffix)
            and filename[-len(suffix) - 1] in ('-', '_')):
            return filename[:-len(suffix) - 1]
    return os.path.splitext(filename)[0]


def _classify_include(filename, include, is_system, include_state):
    """Figures out what kind of header 'include' is.

    Args:
      filename: The current file cpp_style is running over.
      include: The path to a #included file.
      is_system: True if the #include used <> rather than "".
      include_state: An _IncludeState instance in which the headers are inserted.

    Returns:
      One of the _XXX_HEADER constants.

    For example:
      >>> _classify_include('foo.cpp', 'config.h', False)
      _CONFIG_HEADER
      >>> _classify_include('foo.cpp', 'foo.h', False)
      _PRIMARY_HEADER
      >>> _classify_include('foo.cpp', 'bar.h', False)
      _OTHER_HEADER
    """

    if 'WebKitAdditions/' in include:
        return _WEBKIT_ADDITIONS_HEADER

    # If it is a system header we know it is classified as _OTHER_HEADER.
    if is_system and not include.startswith('public/') and not include.startswith('wtf/') and not include.endswith('SoftLink.h'):
        return _OTHER_HEADER

    # If the include is named config.h then this is WebCore/config.h.
    if include == "config.h":
        return _CONFIG_HEADER

    # If the include is named *SoftLink.h, then it's a soft-link header.
    if include.endswith('SoftLink.h'):
        return _SOFT_LINK_HEADER

    # There cannot be primary includes in header files themselves. Only an
    # include exactly matches the header filename will be is flagged as
    # primary, so that it triggers the "don't include yourself" check.
    if filename.endswith('.h') and filename != include:
        return _OTHER_HEADER

    # If the target file basename starts with the include we're checking
    # then we consider it the primary header.
    target_base = FileInfo(filename).base_name()
    include_base = FileInfo(include).base_name()

    # If we haven't encountered a primary header, then be lenient in checking.
    if not include_state.visited_primary_section():
        if target_base.find(include_base) != -1:
            return _PRIMARY_HEADER
        if include_base in ['{}Internal'.format(target_base), '{}Private'.format(target_base)]:
            return _PRIMARY_HEADER

    # If we already encountered a primary header, perform a strict comparison.
    # In case the two filename bases are the same then the above lenient check
    # probably was a false positive.
    elif include_state.visited_primary_section() and target_base == include_base:
        if include == "ResourceHandleWin.h":
            # FIXME: Thus far, we've only seen one example of these, but if we
            # start to see more, please consider generalizing this check
            # somehow.
            return _OTHER_HEADER
        return _PRIMARY_HEADER

    return _OTHER_HEADER


def _does_primary_header_exist(filename):
    """Return a primary header file name for a file, or empty string
    if the file is not source file or primary header does not exist.
    """
    fileinfo = FileInfo(filename)
    if not fileinfo.is_source():
        return False
    primary_header = fileinfo.no_extension() + ".h"
    return os.path.isfile(primary_header)


def _is_javascriptcore_file(filename):
    return filename.startswith('Source/JavaScriptCore/')


def _is_webkit2_file(filename):
    return filename.startswith('Source/WebKit/')


def check_include_line(filename, file_extension, clean_lines, line_number, include_state, error):
    """Check rules that are applicable to #include lines.

    Strings on #include lines are NOT removed from elided line, to make
    certain tasks easier. However, to prevent false positives, checks
    applicable to #include lines in CheckLanguage must be put here.

    Args:
      filename: The name of the current file.
      file_extension: The current file extension, without the leading dot.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      include_state: An _IncludeState instance in which the headers are inserted.
      error: The function to call with any errors found.
    """
    # FIXME: For readability or as a possible optimization, consider
    #        exiting early here by checking whether the "build/include"
    #        category should be checked for the given filename.  This
    #        may involve having the error handler classes expose a
    #        should_check() method, in addition to the usual __call__
    #        method.
    line = clean_lines.lines[line_number]

    matched = _RE_PATTERN_INCLUDE.search(line)
    if not matched:
        return

    include = matched.group(2)
    is_system = (matched.group(1) == '<')

    # FIXME: Remove once JavaScriptCore builds with ARC enabled (Bug 221117).
    if _is_javascriptcore_file(filename) and include == 'wtf/BlockPtr.h':
        error(line_number, 'security/javascriptcore_wtf_blockptr', 5,
              'Replace WTF::BlockPtr with WTF::Function. '
              'WTF::BlockPtr is not safe to use until JavaScriptCore builds with ARC enabled.')

    # Look for any of the stream classes that are part of standard C++.
    if match(r'(f|ind|io|i|o|parse|pf|stdio|str|)?stream$', include):
        error(line_number, 'readability/streams', 3,
              'Streams are highly discouraged.')

    # Look for specific includes to fix.
    if include.startswith('wtf/') and not is_system:
        error(line_number, 'build/include', 4,
              'wtf includes should be <wtf/file.h> instead of "wtf/file.h".')

    duplicate_header = include in include_state
    if duplicate_header:
        error(line_number, 'build/include', 4,
              '"%s" already included at %s:%s' %
              (include, filename, include_state[include]))
    else:
        include_state[include] = line_number

    header_type = _classify_include(filename, include, is_system, include_state)
    primary_header_exists = _does_primary_header_exist(filename)
    has_config_header = check_has_config_header(filename)

    include_state.header_types[line_number] = header_type

    # Only proceed if this isn't a duplicate header.
    if duplicate_header:
        return

    # We want to ensure that headers appear in the right order:
    # 1) for implementation files: config.h, primary header, blank line, alphabetically sorted
    # 2) for header files: alphabetically sorted
    # The include_state object keeps track of the last type seen
    # and complains if the header types are out of order or missing.
    error_message = include_state.check_next_include_order(header_type,
                                                           filename,
                                                           file_extension == "h",
                                                           primary_header_exists,
                                                           has_config_header)

    # Check to make sure *SoftLink.h headers always appear last and never in a header.
    if error_message and include_state.visited_soft_link_section():
        error(line_number, 'build/include_order', 4, error_message)
        return

    # Check to make sure we have a blank line after and none before primary header.
    if not error_message and header_type == _PRIMARY_HEADER:
        next_line = clean_lines.raw_lines[line_number + 1]
        previous_line = clean_lines.raw_lines[line_number - 1]
        if not is_blank_line(next_line):
            error(line_number, 'build/include_order', 4,
                'You should add a blank line after implementation file\'s own header.')
        if has_config_header and is_blank_line(previous_line):
            error(line_number, 'build/include_order', 4,
                'You should not add a blank line before implementation file\'s own header.')

    # Check to make sure all headers besides config.h and the primary header are
    # alphabetically sorted.
    if not error_message and header_type == _OTHER_HEADER and not search(r'\A#include.*\.lut\.h', line):
        previous_line_number = line_number - 1
        previous_line = clean_lines.lines[previous_line_number]
        previous_match = _RE_PATTERN_INCLUDE.search(previous_line)
        while (not previous_match and previous_line_number > 0
               and not search(r'\A(#if|#ifdef|#ifndef|#else|#elif|#endif)', previous_line)):
            previous_line_number -= 1
            previous_line = clean_lines.lines[previous_line_number]
            previous_match = _RE_PATTERN_INCLUDE.search(previous_line)
        if previous_match and previous_line_number in include_state.header_types:
            previous_header_type = include_state.header_types[previous_line_number]
            if previous_header_type == _OTHER_HEADER:
                if '<' in previous_line and '"' in line:
                    error(line_number, 'build/include_order', 4, 'Bad include order. Mixing system and custom headers.')
                elif previous_line.strip() > line.strip():
                    # This type of error is potentially a problem with this line or the previous one,
                    # so if the error is filtered for one line, report it for the next. This is so that
                    # we properly handle patches, for which only modified lines produce errors.
                    if not error(line_number - 1, 'build/include_order', 4, 'Alphabetical sorting problem.'):
                        error(line_number, 'build/include_order', 4, 'Alphabetical sorting problem.')

    if error_message:
        if file_extension == 'h':
            error(line_number, 'build/include_order', 4,
                  '%s Should be: alphabetically sorted.' %
                  error_message)
        else:
            error(line_number, 'build/include_order', 4,
                  '%s Should be: config.h, primary header, blank line, and then alphabetically sorted.' %
                  error_message)


def check_language(filename, clean_lines, line_number, file_extension, include_state,
                   file_state, error):
    """Checks rules from the 'C++ language rules' section of cppguide.html.

    Some of these rules are hard to test (function overloading, using
    uint32 inappropriately), but we do the best we can.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_extension: The extension (without the dot) of the filename.
      include_state: An _IncludeState instance in which the headers are inserted.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """
    # If the line is empty or consists of entirely a comment, no need to
    # check it.
    line = clean_lines.elided[line_number]
    if not line:
        return

    matched = _RE_PATTERN_INCLUDE.search(line)
    if matched:
        check_include_line(filename, file_extension, clean_lines, line_number, include_state, error)
        return

    # FIXME: figure out if they're using default arguments in fn proto.

    # Check to see if they're using an conversion function cast.
    # I just try to capture the most common basic types, though there are more.
    # Parameterless conversion functions, such as bool(), are allowed as they are
    # probably a member operator declaration or default constructor.
    matched = search(
        r'\b(int|float|double|bool|char|int32|uint32|int64|uint64)\([^)]', line)
    if matched:
        # gMock methods are defined using some variant of MOCK_METHODx(name, type)
        # where type may be float(), int(string), etc.  Without context they are
        # virtually indistinguishable from int(x) casts.
        if not match(r'^\s*MOCK_(CONST_)?METHOD\d+(_T)?\(', line):
            error(line_number, 'readability/casting', 4,
                  'Using deprecated casting style.  '
                  'Use static_cast<%s>(...) instead' %
                  matched.group(1))

    check_c_style_cast(line_number, line, clean_lines.raw_lines[line_number],
                       'static_cast',
                       r'\((int|float|double|bool|char|u?int(16|32|64))\)',
                       error)
    # This doesn't catch all cases.  Consider (const char * const)"hello".
    check_c_style_cast(line_number, line, clean_lines.raw_lines[line_number],
                       'reinterpret_cast', r'\((\w+\s?\*+\s?)\)', error)

    # In addition, we look for people taking the address of a cast.  This
    # is dangerous -- casts can assign to temporaries, so the pointer doesn't
    # point where you think.
    if search(
        r'(&\([^)]+\)[\w(])|(&(static|dynamic|reinterpret)_cast\b)', line):
        error(line_number, 'runtime/casting', 4,
              ('Are you taking an address of a cast?  '
               'This is dangerous: could be a temp var.  '
               'Take the address before doing the cast, rather than after'))

    # Check for people declaring static/global STL strings at the top level.
    # This is dangerous because the C++ language does not guarantee that
    # globals with constructors are initialized before the first access.
    matched = match(
        r'((?:|static +)(?:|const +))string +([a-zA-Z0-9_:]+)\b(.*)',
        line)
    # Make sure it's not a function.
    # Function template specialization looks like: "string foo<Type>(...".
    # Class template definitions look like: "string Foo<Type>::Method(...".
    if matched and not match(r'\s*(<.*>)?(::[a-zA-Z0-9_]+)?\s*\(([^"]|$)',
                             matched.group(3)):
        error(line_number, 'runtime/string', 4,
              'For a static/global string constant, use a C style string instead: '
              '"%schar %s[]".' %
              (matched.group(1), matched.group(2)))

    # Check that we're not using RTTI outside of testing code.
    if search(r'\bdynamic_cast<', line):
        error(line_number, 'runtime/rtti', 5,
              'Do not use dynamic_cast<>.  If you need to cast within a class '
              "hierarchy, use static_cast<> to upcast.  Google doesn't support "
              'RTTI.')

    if file_extension == 'h':
        # FIXME: check that 1-arg constructors are explicit.
        #        How to tell it's a constructor?
        #        (handled in check_for_non_standard_constructs for now)
        pass

    # Check if people are using the verboten C basic types.  The only exception
    # we regularly allow is "unsigned short port" for port.
    if search(r'\bshort port\b', line):
        if not search(r'\bunsigned short port\b', line):
            error(line_number, 'runtime/int', 4,
                  'Use "unsigned short" for ports, not "short"')

    # When snprintf is used, the second argument shouldn't be a literal.
    matched = search(r'snprintf\s*\(([^,]*),\s*([0-9]*)\s*,', line)
    if matched:
        error(line_number, 'runtime/printf', 3,
              'If you can, use sizeof(%s) instead of %s as the 2nd arg '
              'to snprintf.' % (matched.group(1), matched.group(2)))

    # Warn when Debug ASSERT_WITH_SECURITY_IMPLICATION() is used.
    if filename != 'Source/WTF/wtf/Assertions.h':
        if search(r'\bASSERT_WITH_SECURITY_IMPLICATION\b\(', line):
            error(line_number, 'security/assertion', 5,
                'Please replace ASSERT_WITH_SECURITY_IMPLICATION() with '
                'RELEASE_ASSERT_WITH_SECURITY_IMPLICATION().')

    # FIXME: Remove once JavaScriptCore builds with ARC enabled (Bug 221117).
    if _is_javascriptcore_file(filename) and search(r'\b(WTF::)?BlockPtr\s*<', line):
        error(line_number, 'security/javascriptcore_wtf_blockptr', 5,
              'Replace WTF::BlockPtr with WTF::Function. '
              'WTF::BlockPtr is not safe to use until JavaScriptCore builds with ARC enabled.')

    # Check if some verboten C functions are being used.
    if search(r'\bsprintf\b', line):
        error(line_number, 'security/printf', 5,
              'Never use sprintf.  Use snprintf instead.')
    matched = search(r'\b(strcpy|strcat)\b', line)
    if matched:
        error(line_number, 'security/printf', 4,
              'Almost always, snprintf is better than %s.' % matched.group(1))

    if search(r'\bsscanf\b', line):
        error(line_number, 'runtime/printf', 1,
              'sscanf can be ok, but is slow and can overflow buffers.')

    if search(r'\bmktemp\b', line):
        error(line_number, 'security/temp_file', 5,
              'Never use mktemp.  Use mkstemp or mkostemp instead.')

    if search(r'\bdispatch_set_target_queue\b', line):
        error(line_number, 'runtime/dispatch_set_target_queue', 5,
              'Never use dispatch_set_target_queue.  Use dispatch_queue_create_with_target instead.')

    matched = search(r'\b(RetainPtr<.*)', line)
    if matched:
        match_line = matched.group(1)
        nested_angle_bracket_count = 1
        previous_closing_angle_bracket_index = -1
        closing_angle_bracket_index = 9 # Used if only one pair of angle brackets.
        for i in range(10, len(match_line) - 1):
            if match_line[i] == '<':
                nested_angle_bracket_count += 1
            if match_line[i] == '>':
                nested_angle_bracket_count -= 1
                previous_closing_angle_bracket_index = closing_angle_bracket_index
                closing_angle_bracket_index = i
            if nested_angle_bracket_count == 0:
                if "*" in match_line[previous_closing_angle_bracket_index:closing_angle_bracket_index]:
                    error(line_number, 'runtime/retainptr', 5,
                          'RetainPtr<> should never contain a type with \'*\'. Correct: RetainPtr<NSString>, RetainPtr<CFStringRef>.')
                break

    matched = re.compile(r'^\s*SOFT_LINK_(PRIVATE_)?FRAMEWORK.*\((\S+)\)').search(line)
    if matched:
        framework_name = matched.group(2)
        if file_extension == 'h' and not search(r'^\s*SOFT_LINK_(PRIVATE_)?FRAMEWORK_FOR_HEADER.*\(', line):
            error(line_number, 'softlink/header', 5,
                  'Never soft-link frameworks in headers. Put the soft-link macros in a source file, or create {framework}SoftLink.{{cpp,mm}} instead.'.format(framework=framework_name))

        frameworks_with_soft_links = ['CoreMedia', 'CoreVideo', 'DataDetectorsCore', 'LocalAuthentication', 'MediaAccessibility', 'MediaRemote', 'PassKit', 'QuickLook', 'UIKit', 'VideoToolbox']
        if framework_name in frameworks_with_soft_links and not re.compile(r'^\s*SOFT_LINK_(PRIVATE_)?FRAMEWORK_FOR_(HEADER|SOURCE)(_WITH_EXPORT)?\({}\)'.format(framework_name)).search(line):
            error(line_number, 'softlink/framework', 5,
                  'Use {framework}SoftLink.{{cpp,h,mm}} to soft-link to {framework}.framework.'.format(framework=framework_name))

    # Check for suspicious usage of "if" like
    # } if (a == b) {
    if search(r'\}\s*if\s*\(', line):
        error(line_number, 'readability/braces', 4,
              'Did you mean "else if"? If not, start a new line for "if".')

    # Check for potential format string bugs like printf(foo).
    # We constrain the pattern not to pick things like DocidForPrintf(foo).
    # Not perfect but it can catch printf(foo.c_str()) and printf(foo->c_str())
    matched = re.search(r'\b((?:string)?printf)\s*\(([\w.\->()]+)\)', line, re.I)
    if matched:
        error(line_number, 'security/printf', 4,
              'Potential format string bug. Do %s("%%s", %s) instead.'
              % (matched.group(1), matched.group(2)))

    # Check for potential memset bugs like memset(buf, sizeof(buf), 0).
    matched = search(r'memset\s*\(([^,]*),\s*([^,]*),\s*0\s*\)', line)
    if matched and not match(r"^''|-?[0-9]+|0x[0-9A-Fa-f]$", matched.group(2)):
        error(line_number, 'runtime/memset', 4,
              'Did you mean "memset(%s, 0, %s)"?'
              % (matched.group(1), matched.group(2)))

    # Detect variable-length arrays.
    matched = match(r'\s*(.+::)?(\w+) [a-z]\w*\[(.+)];', line)
    if (matched and matched.group(2) != 'return' and matched.group(2) != 'delete' and
        matched.group(3).find(']') == -1):
        # Split the size using space and arithmetic operators as delimiters.
        # If any of the resulting tokens are not compile time constants then
        # report the error.
        tokens = re.split(r'\s|\+|\-|\*|\/|<<|>>]', matched.group(3))
        is_const = True
        skip_next = False
        for tok in tokens:
            if skip_next:
                skip_next = False
                continue

            if search(r'sizeof\(.+\)', tok):
                continue
            if search(r'arraysize\(\w+\)', tok):
                continue

            tok = tok.lstrip('(')
            tok = tok.rstrip(')')
            if not tok:
                continue
            if match(r'\d+', tok):
                continue
            if match(r'0[xX][0-9a-fA-F]+', tok):
                continue
            if match(r'k[A-Z0-9]\w*', tok):
                continue
            if match(r'(.+::)?k[A-Z0-9]\w*', tok):
                continue
            if match(r'(.+::)?[A-Z][A-Z0-9_]*', tok):
                continue
            # A catch all for tricky sizeof cases, including 'sizeof expression',
            # 'sizeof(*type)', 'sizeof(const type)', 'sizeof(struct StructName)'
            # requires skipping the next token becasue we split on ' ' and '*'.
            if tok.startswith('sizeof'):
                skip_next = True
                continue
            is_const = False
            break
        if not is_const:
            error(line_number, 'runtime/arrays', 1,
                  'Do not use variable-length arrays.  Use an appropriately named '
                  "('k' followed by CamelCase) compile-time constant for the size.")

    # Check for use of unnamed namespaces in header files.  Registration
    # macros are typically OK, so we allow use of "namespace {" on lines
    # that end with backslashes.
    if (file_extension == 'h'
        and search(r'\bnamespace\s*{', line)
        and line[-1] != '\\'):
        error(line_number, 'build/namespaces', 4,
              'Do not use unnamed namespaces in header files.  See '
              'http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml#Namespaces'
              ' for more information.')

    # Check for plain bitfields declared without either "singed" or "unsigned".
    # Most compilers treat such bitfields as signed, but there are still compilers like
    # RVCT 4.0 that use unsigned by default.
    matched = re.match(r'\s*((const|mutable)\s+)?(char|(short(\s+int)?)|int|long(\s+(long|int))?)\s+[a-zA-Z_][a-zA-Z0-9_]*\s*:\s*\d+\s*;', line)
    if matched:
        error(line_number, 'runtime/bitfields', 5,
              'Please declare integral type bitfields with either signed or unsigned.')

    # Beware of using enums as types for bitfields, there can sometimes be a mismatch
    # between the signedness of the enum and the enum bitfield type.
    # Visual Studio's 8.0 compiler is an example where it represents bit fields of
    # enum types with a signed type, but enums as unsigned.
    matched = re.match(r'\s*((const|mutable)\s+)?([a-zA-Z_][a-zA-Z0-9_]*)\s+[a-zA-Z_][a-zA-Z0-9_]*\s*:\s*\d+\s*;', line)
    if matched:
        # Make sure the type is an enum and not an integral type
        if not match(r'bool|char|(short(\s+int)?)|int|long(\s+(long|int))|(signed|unsigned)(\s+int)?', matched.group(3)):
            error(line_number, 'runtime/enum_bitfields', 5,
                  'Please declare enum bitfields as unsigned integral types.')

    check_identifier_name_in_declaration(filename, line_number, line, file_state, error)

    # Check for unsigned int (should be just 'unsigned')
    if search(r'\bunsigned int\b', line):
        error(line_number, 'runtime/unsigned', 1,
              'Omit int when using unsigned')

    # Check that we're not using static_cast<Text*>.
    if search(r'\bstatic_cast<Text\*>', line):
        error(line_number, 'readability/check', 4,
              'Consider using toText helper function in WebCore/dom/Text.h '
              'instead of static_cast<Text*>')


def check_identifier_name_in_declaration(filename, line_number, line, file_state, error):
    """Checks if identifier names contain any anti-patterns like underscores.

    As identifiers in libraries we are using have a bunch of
    underscores, we only warn about the declarations of identifiers
    and don't check use of identifiers.

    Args:
      filename: The name of the current file.
      line_number: The number of the line to check.
      line: The line of code to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """
    # We don't check a return statement.
    if match(r'\s*(return|delete)\b', line):
        return

    # Make sure Ref/RefPtrs used as protectors are named correctly, and do this before we start stripping things off the input.
    ref_regexp = r'^\s*Ref(Ptr)?<([\w_]|::)+> (?P<protector_name>[\w_]+)(\(| = )(\*|&)*(m_)?(?P<protected_name>[\w_]+)\)?;'
    ref_check = match(ref_regexp, line)
    if ref_check:
        protector_name = ref_check.group('protector_name')
        protected_name = ref_check.group('protected_name')
        cap_protected_name = protected_name[0].upper() + protected_name[1:]
        # Ignore function declarations where cap_protected_name == protected_name indicates a type name.
        if cap_protected_name != protected_name:
            expected_protector_name = 'protected' + cap_protected_name
            if protected_name == 'this' and protector_name != 'protectedThis':
                error(line_number, 'readability/naming/protected', 4, "\'" + protector_name + "\' is incorrectly named. It should be named \'protectedThis\'.")
            elif protector_name == expected_protector_name or protector_name == 'protector':
                return
            else:
                error(line_number, 'readability/naming/protected', 4, "\'" + protector_name + "\' is incorrectly named. It should be named \'protector\' or \'" + expected_protector_name + "\'.")

    # Basically, a declaration is a type name followed by whitespaces
    # followed by an identifier. The type name can be complicated
    # due to type adjectives and templates. We remove them first to
    # simplify the process to find declarations of identifiers.

    # Convert "long long", "long double", and "long long int" to
    # simple types, but don't remove simple "long".
    line = sub(r'long (long )?(?=long|double|int)', '', line)
    # Convert unsigned/signed types to simple types, too.
    line = sub(r'(unsigned|signed) (?=char|short|int|long)', '', line)

    is_class_or_namespace_or_struct_name = False
    class_namespace_struct_name_re = re.compile('\\b(class|explicit|namespace|struct|%s)(?=\\W)' % '|'.join(_EXPORT_MACROS))
    if class_namespace_struct_name_re.search(line):
        is_class_or_namespace_or_struct_name = True

    # Remove keywords that aren't types.
    line = sub(r'\b(inline|using|static|const|volatile|register|extern|typedef|restrict|struct|class|virtual)(?=\W)', '', line)

    # Remove "new" and "new (expr)" to simplify, too.
    line = sub(r'new\s+', '', line)
    line = sub(r'new\s*(\([^)]*\))', '', line)

    # Remove all template parameters by removing matching < and >.
    # Loop until no templates are removed to remove nested templates.
    while True:
        line, number_of_replacements = subn(r'<([\w\s:]|::)+\s*[*&]*\s*>', '', line)
        if not number_of_replacements:
            break

    # Declarations of local variables can be in condition expressions
    # of control flow statements (e.g., "if (RenderObject* p = o->parent())").
    # We remove the keywords and the first parenthesis.
    #
    # Declarations in "while", "if", and "switch" are different from
    # other declarations in two aspects:
    #
    # - There can be only one declaration between the parentheses.
    #   (i.e., you cannot write "if (int i = 0, j = 1) {}")
    # - The variable must be initialized.
    #   (i.e., you cannot write "if (int i) {}")
    #
    # and we will need different treatments for them.
    line = sub(r'^\s*for\s*\(', '', line)
    line, control_statement = subn(r'^\s*(while|else if|if|switch)\s*\(', '', line)

    # Detect variable and functions.
    type_regexp = r'\w([\w]|\s*[*&]\s*|::)+'
    identifier_regexp = r'(?P<identifier>[\w:]+)'
    maybe_bitfield_regexp = r'(:\s*\d+\s*)?'
    character_after_identifier_regexp = r'(?P<character_after_identifier>[\[;()=,])(?!=)'
    declaration_without_type_regexp = r'\s*' + identifier_regexp + r'\s*' + maybe_bitfield_regexp + character_after_identifier_regexp
    declaration_with_type_regexp = r'\s*' + type_regexp + r'\s' + declaration_without_type_regexp
    constructor_regexp = r'\s*([\w_]*::)*(?P<pre_part>[\w_]+)::(?P<post_part>[\w_]+)[(]'
    is_function_arguments = False
    number_of_identifiers = 0
    while True:
        # If we are seeing the first identifier or arguments of a
        # function, there should be a type name before an identifier.
        constructor_check = match(constructor_regexp, line)
        is_constructor = constructor_check and constructor_check.group('pre_part') == constructor_check.group('post_part')
        if not is_constructor and (not number_of_identifiers or is_function_arguments):
            declaration_regexp = declaration_with_type_regexp
        else:
            declaration_regexp = declaration_without_type_regexp

        matched = match(declaration_regexp, line)
        if not matched:
            return
        identifier = matched.group('identifier')
        character_after_identifier = matched.group('character_after_identifier')

        # It's possible for the regular expression to match ':' in modern Objective-C for loops
        # or NSDictionary initialization lists.
        if identifier == ':':
            return

        # If we removed a non-for-control statement, the character after
        # the identifier should be '='. With this rule, we can avoid
        # warning for cases like "if (val & INT_MAX) {".
        if control_statement and character_after_identifier != '=':
            return

        is_function_arguments = is_function_arguments or character_after_identifier == '('

        # Remove "m_" and "s_" to allow them.
        modified_identifier = sub(r'(^|(?<=::))[ms]_', '', identifier)
        if not file_state.is_objective_c_or_objective_cpp() and modified_identifier.find('_') >= 0:
            # Various exceptions to the rule: JavaScript op codes functions, const_iterator.
            if (not (_is_javascriptcore_file(filename) and (modified_identifier.find('op_') >= 0 or modified_identifier.find('intrinsic_') >= 0))
                and not (('gtk' in filename or 'glib' in filename or 'wpe' in filename or 'atk' in filename) and modified_identifier.startswith('webkit_'))
                and not ('glib' in filename and modified_identifier.startswith('jsc_'))
                and not modified_identifier.startswith('tst_')
                and not modified_identifier.startswith('webkit_dom_object_')
                and not modified_identifier.startswith('webkit_soup')
                and not modified_identifier.startswith('NPN_')
                and not modified_identifier.startswith('NPP_')
                and not modified_identifier.startswith('NP_')
                and not modified_identifier.startswith('_q_')
                and not modified_identifier.startswith('cairo_')
                and not modified_identifier.startswith('Ecore_')
                and not modified_identifier.startswith('Eina_')
                and not modified_identifier.startswith('Evas_')
                and not modified_identifier.startswith('Ewk_')
                and not modified_identifier.startswith('cti_')
                and not modified_identifier.find('::_q_') >= 0
                and not modified_identifier == "const_iterator"
                and not modified_identifier == "vm_throw"
                and not modified_identifier == "DFG_OPERATION"
                and not modified_identifier == "LOG_CHANNEL"
                and not modified_identifier == "WTF_GUARDED_BY_LOCK"
                and not modified_identifier == "WTF_GUARDED_BY_CAPABILITY"
                and not modified_identifier.startswith("_AX")
                and not modified_identifier.find('chrono_literals') >= 0):
                error(line_number, 'readability/naming/underscores', 4, identifier + " is incorrectly named. Don't use underscores in your identifier names.")

        # Check for variables named 'l', these are too easy to confuse with '1' in some fonts
        if modified_identifier == 'l':
            error(line_number, 'readability/naming', 4, identifier + " is incorrectly named. Don't use the single letter 'l' as an identifier name.")

        _check_identifier_name_for_acronyms(identifier, line_number, is_class_or_namespace_or_struct_name, error)

        # There can be only one declaration in non-for-control statements.
        if control_statement:
            return
        # We should continue checking if this is a function
        # declaration because we need to check its arguments.
        # Also, we need to check multiple declarations.
        if character_after_identifier != '(' and character_after_identifier != ',':
            return

        number_of_identifiers += 1
        line = line[matched.end():]


def check_c_style_cast(line_number, line, raw_line, cast_type, pattern,
                       error):
    """Checks for a C-style cast by looking for the pattern.

    This also handles sizeof(type) warnings, due to similarity of content.

    Args:
      line_number: The number of the line to check.
      line: The line of code to check.
      raw_line: The raw line of code to check, with comments.
      cast_type: The string for the C++ cast to recommend.  This is either
                 reinterpret_cast or static_cast, depending.
      pattern: The regular expression used to find C-style casts.
      error: The function to call with any errors found.
    """
    matched = search(pattern, line)
    if not matched:
        return

    # e.g., sizeof(int)
    sizeof_match = match(r'.*sizeof\s*$', line[0:matched.start(1) - 1])
    if sizeof_match:
        error(line_number, 'runtime/sizeof', 1,
              'Using sizeof(type).  Use sizeof(varname) instead if possible')
        return

    remainder = line[matched.end(0):]

    # The close paren is for function pointers as arguments to a function.
    # eg, void foo(void (*bar)(int));
    # The semicolon check is a more basic function check; also possibly a
    # function pointer typedef.
    # eg, void foo(int); or void foo(int) const;
    # The equals check is for function pointer assignment.
    # eg, void *(*foo)(int) = ...
    #
    # Right now, this will only catch cases where there's a single argument, and
    # it's unnamed.  It should probably be expanded to check for multiple
    # arguments with some unnamed.
    function_match = match(r'\s*(\)|=|(const)?\s*(;|\{|throw\(\)))', remainder)
    if function_match:
        if (not function_match.group(3)
            or function_match.group(3) == ';'
            or raw_line.find('/*') < 0):
            error(line_number, 'readability/function', 3,
                  'All parameters should be named in a function')
        return

    # At this point, all that should be left is actual casts.
    error(line_number, 'readability/casting', 4,
          'Using C-style cast.  Use %s<%s>(...) instead' %
          (cast_type, matched.group(1)))


_HEADERS_CONTAINING_TEMPLATES = (
    ('<deque>', ('deque',)),
    ('<functional>', ('unary_function', 'binary_function',
                      'plus', 'minus', 'multiplies', 'divides', 'modulus',
                      'negate',
                      'equal_to', 'not_equal_to', 'greater', 'less',
                      'greater_equal', 'less_equal',
                      'logical_and', 'logical_or', 'logical_not',
                      'unary_negate', 'not1', 'binary_negate', 'not2',
                      'bind1st', 'bind2nd',
                      'pointer_to_unary_function',
                      'pointer_to_binary_function',
                      'ptr_fun',
                      'mem_fun_t', 'mem_fun', 'mem_fun1_t', 'mem_fun1_ref_t',
                      'mem_fun_ref_t',
                      'const_mem_fun_t', 'const_mem_fun1_t',
                      'const_mem_fun_ref_t', 'const_mem_fun1_ref_t',
                      'mem_fun_ref',
                     )),
    ('<limits>', ('numeric_limits',)),
    ('<list>', ('list',)),
    ('<map>', ('map', 'multimap',)),
    ('<memory>', ('allocator',)),
    ('<queue>', ('queue', 'priority_queue',)),
    ('<set>', ('set', 'multiset',)),
    ('<stack>', ('stack',)),
    ('<string>', ('char_traits', 'basic_string',)),
    ('<utility>', ('pair',)),
    ('<vector>', ('vector',)),

    # gcc extensions.
    # Note: std::hash is their hash, ::hash is our hash
    ('<hash_map>', ('hash_map', 'hash_multimap',)),
    ('<hash_set>', ('hash_set', 'hash_multiset',)),
    ('<slist>', ('slist',)),
    )

_HEADERS_ACCEPTED_BUT_NOT_PROMOTED = {
    # We can trust with reasonable confidence that map gives us pair<>, too.
    'pair<>': ('map', 'multimap', 'hash_map', 'hash_multimap')
}

_RE_PATTERN_STRING = re.compile(r'\bstring\b')

_re_pattern_algorithm_header = []
for _template in ('copy', 'max', 'min', 'min_element', 'sort', 'swap',
                  'transform'):
    # Match max<type>(..., ...), max(..., ...), but not foo->max, foo.max or
    # type::max().
    _re_pattern_algorithm_header.append(
        (re.compile(r'[^>.]\b' + _template + r'(<.*?>)?\([^\)]'),
         _template,
         '<algorithm>'))

_re_pattern_templates = []
for _header, _templates in _HEADERS_CONTAINING_TEMPLATES:
    for _template in _templates:
        _re_pattern_templates.append(
            (re.compile(r'(\<|\b)' + _template + r'\s*\<'),
             _template + '<>',
             _header))


def _unix_path(file_path):
    if os.path.sep == '/':
        return file_path
    return file_path.replace(os.path.sep, '/')


def is_generated_file(file_path):
    """Check if the file is auto-generated."""
    file_path = _unix_path(file_path)
    return file_path in _AUTO_GENERATED_FILES


def check_has_config_header(file_path):
    """Check if the module uses config.h"""
    file_path = _unix_path(file_path)
    for pattern in _NO_CONFIG_H_PATH_PATTERNS:
        if re.match(pattern, file_path):
            return False
    return True


def files_belong_to_same_module(filename_cpp, filename_h):
    """Check if these two filenames belong to the same module.

    The concept of a 'module' here is a as follows:
    foo.h, foo-inl.h, foo.cpp, foo_test.cpp and foo_unittest.cpp belong to the
    same 'module' if they are in the same directory.
    some/path/public/xyzzy and some/path/internal/xyzzy are also considered
    to belong to the same module here.

    If the filename_cpp contains a longer path than the filename_h, for example,
    '/absolute/path/to/base/sysinfo.cpp', and this file would include
    'base/sysinfo.h', this function also produces the prefix needed to open the
    header. This is used by the caller of this function to more robustly open the
    header file. We don't have access to the real include paths in this context,
    so we need this guesswork here.

    Known bugs: tools/base/bar.cpp and base/bar.h belong to the same module
    according to this implementation. Because of this, this function gives
    some false positives. This should be sufficiently rare in practice.

    Args:
      filename_cpp: is the path for the .cpp file
      filename_h: is the path for the header path

    Returns:
      Tuple with a bool and a string:
      bool: True if filename_cpp and filename_h belong to the same module.
      string: the additional prefix needed to open the header file.
    """

    if not filename_cpp.endswith('.cpp'):
        return (False, '')
    filename_cpp = filename_cpp[:-len('.cpp')]
    if filename_cpp.endswith('_unittest'):
        filename_cpp = filename_cpp[:-len('_unittest')]
    elif filename_cpp.endswith('_test'):
        filename_cpp = filename_cpp[:-len('_test')]
    filename_cpp = filename_cpp.replace('/public/', '/')
    filename_cpp = filename_cpp.replace('/internal/', '/')

    if not filename_h.endswith('.h'):
        return (False, '')
    filename_h = filename_h[:-len('.h')]
    if filename_h.endswith('-inl'):
        filename_h = filename_h[:-len('-inl')]
    filename_h = filename_h.replace('/public/', '/')
    filename_h = filename_h.replace('/internal/', '/')

    files_belong_to_same_module = filename_cpp.endswith(filename_h)
    common_path = ''
    if files_belong_to_same_module:
        common_path = filename_cpp[:-len(filename_h)]
    return files_belong_to_same_module, common_path


def update_include_state(filename, include_state, io=codecs):
    """Fill up the include_state with new includes found from the file.

    Args:
      filename: the name of the header to read.
      include_state: an _IncludeState instance in which the headers are inserted.
      io: The io factory to use to read the file. Provided for testability.

    Returns:
      True if a header was succesfully added. False otherwise.
    """
    io = _unit_test_config.get(INCLUDE_IO_INJECTION_KEY, codecs)
    header_file = None
    try:
        header_file = io.open(filename, 'r', 'utf8', 'replace')
    except IOError:
        return False
    line_number = 0
    for line in header_file:
        line_number += 1
        clean_line = cleanse_comments(line)
        matched = _RE_PATTERN_INCLUDE.search(clean_line)
        if matched:
            include = matched.group(2)
            # The value formatting is cute, but not really used right now.
            # What matters here is that the key is in include_state.
            include_state.setdefault(include, '%s:%d' % (filename, line_number))
    return True


def check_for_include_what_you_use(filename, clean_lines, include_state, error):
    """Reports for missing stl includes.

    This function will output warnings to make sure you are including the headers
    necessary for the stl containers and functions that you use. We only give one
    reason to include a header. For example, if you use both equal_to<> and
    less<> in a .h file, only one (the latter in the file) of these will be
    reported as a reason to include the <functional>.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      include_state: An _IncludeState instance.
      error: The function to call with any errors found.
    """
    required = {}  # A map of header name to line_number and the template entity.
        # Example of required: { '<functional>': (1219, 'less<>') }

    for line_number in range(clean_lines.num_lines()):
        line = clean_lines.elided[line_number]
        if not line or line[0] == '#':
            continue

        # String is special -- it is a non-templatized type in STL.
        if _RE_PATTERN_STRING.search(line):
            required['<string>'] = (line_number, 'string')

        for pattern, template, header in _re_pattern_algorithm_header:
            if pattern.search(line):
                required[header] = (line_number, template)

        # The following function is just a speed up, no semantics are changed.
        if not '<' in line:  # Reduces the cpu time usage by skipping lines.
            continue

        for pattern, template, header in _re_pattern_templates:
            if pattern.search(line):
                required[header] = (line_number, template)

    # The policy is that if you #include something in foo.h you don't need to
    # include it again in foo.cpp. Here, we will look at possible includes.
    # Let's copy the include_state so it is only messed up within this function.
    include_state = include_state.copy()

    # Did we find the header for this file (if any) and succesfully load it?
    header_found = False

    # Use the absolute path so that matching works properly.
    abs_filename = os.path.abspath(filename)

    # For Emacs's flymake.
    # If cpp_style is invoked from Emacs's flymake, a temporary file is generated
    # by flymake and that file name might end with '_flymake.cpp'. In that case,
    # restore original file name here so that the corresponding header file can be
    # found.
    # e.g. If the file name is 'foo_flymake.cpp', we should search for 'foo.h'
    # instead of 'foo_flymake.h'
    abs_filename = re.sub(r'_flymake\.cpp$', '.cpp', abs_filename)

    # include_state is modified during iteration, so we iterate over a copy of
    # the keys.
    for header in list(include_state.keys()):  # NOLINT
        (same_module, common_path) = files_belong_to_same_module(abs_filename, header)
        fullpath = common_path + header
        if same_module and update_include_state(fullpath, include_state):
            header_found = True

    # If we can't find the header file for a .cpp, assume it's because we don't
    # know where to look. In that case we'll give up as we're not sure they
    # didn't include it in the .h file.
    # FIXME: Do a better job of finding .h files so we are confident that
    #        not having the .h file means there isn't one.
    if filename.endswith('.cpp') and not header_found:
        return

    # All the lines have been processed, report the errors found.
    for required_header_unstripped in required:
        template = required[required_header_unstripped][1]
        if template in _HEADERS_ACCEPTED_BUT_NOT_PROMOTED:
            headers = _HEADERS_ACCEPTED_BUT_NOT_PROMOTED[template]
            if [True for header in headers if header in include_state]:
                continue
        if required_header_unstripped.strip('<>"') not in include_state:
            error(required[required_header_unstripped][0],
                  'build/include_what_you_use', 4,
                  'Add #include ' + required_header_unstripped + ' for ' + template)


def check_platformh_comments(lines, error):
    for line_number, line in enumerate(lines):
        if line_number not in (0, len(lines) - 1):
            if line.find("//") != -1:
                error(line_number, 'build/cpp_comment', 5, 'CPP comments are not allowed in Platform.h, '
                                                           'please use C comments /* ... */')


def process_line(filename, file_extension,
                 clean_lines, line, include_state, function_state,
                 class_state, file_state, enum_state, asm_state, error):
    """Processes a single line in the file.

    Args:
      filename: Filename of the file that is being processed.
      file_extension: The extension (dot not included) of the file.
      clean_lines: An array of strings, each representing a line of the file,
                   with comments stripped.
      line: Number of line being processed.
      include_state: An _IncludeState instance in which the headers are inserted.
      function_state: A _FunctionState instance which counts function lines, etc.
      class_state: A _ClassState instance which maintains information about
                   the current stack of nested class declarations being parsed.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      enum_state: A _EnumState instance which maintains an enum declaration
                  state.
      asm_state: The state of inline ASM code.
      error: A callable to which errors are reported, which takes arguments:
             line number, error level, and message

    """
    raw_lines = clean_lines.raw_lines
    detect_functions(clean_lines, line, function_state, error)
    check_for_function_lengths(clean_lines, line, function_state, error)
    if search(r'\bNOLINT\b', raw_lines[line]):  # ignore nolint lines
        return
    if match(r'\s*\b__asm\b', raw_lines[line]):  # Ignore asm lines as they format differently.
        return
    asm_state.process_line(raw_lines[line])
    if asm_state.is_in_asm():  # Ignore further checks because asm blocks formatted differently.
        return
    check_function_definition(filename, file_extension, clean_lines, line, class_state, function_state, error)
    check_function_body(filename, file_extension, clean_lines, line, class_state, function_state, error)
    check_for_leaky_patterns(clean_lines, line, function_state, error)
    check_for_multiline_comments_and_strings(clean_lines, line, error)
    check_style(clean_lines, line, file_extension, class_state, file_state, enum_state, error)
    check_language(filename, clean_lines, line, file_extension, include_state,
                   file_state, error)
    check_for_non_standard_constructs(clean_lines, line, class_state, error)
    check_posix_threading(clean_lines, line, error)
    check_invalid_increment(clean_lines, line, error)
    check_os_version_checks(filename, clean_lines, line, error)
    check_callonmainthread(filename, clean_lines, line, file_state, error)
    check_ismainthread(filename, clean_lines, line, file_state, error)


class _InlineASMState(object):
    """Stores the state for the inline asm codes."""
    def __init__(self):
        self._is_in_asm = False

    def process_line(self, line):
        if match(r'\s*asm\s+(volatile)?\(', line):
            self._is_in_asm = True
        elif search(r'\);$', line) and self._is_in_asm:  # Can not do more without a proper parser (or lexer).
            self._is_in_asm = False

    def is_in_asm(self):
        return self._is_in_asm


def _process_lines(filename, file_extension, lines, error, min_confidence):
    """Performs lint checks and reports any errors to the given error function.

    Args:
      filename: Filename of the file that is being processed.
      file_extension: The extension (dot not included) of the file.
      lines: An array of strings, each representing a line of the file, with the
             last element being empty if the file is termined with a newline.
      error: A callable to which errors are reported, which takes 4 arguments:
    """
    lines = (['// marker so line numbers and indices both start at 1'] + lines +
             ['// marker so line numbers end in a known way'])

    include_state = _IncludeState()
    function_state = _FunctionState(min_confidence)
    class_state = _ClassState()

    check_for_copyright(lines, error)

    if file_extension == 'h':
        check_for_header_guard(filename, lines, error)
        if filename == 'Source/WTF/wtf/Platform.h':
            check_platformh_comments(lines, error)

    remove_multi_line_comments(lines, error)
    clean_lines = CleansedLines(lines)
    file_state = _FileState(clean_lines, file_extension)
    enum_state = _EnumState()
    asm_state = _InlineASMState()
    for line in range(clean_lines.num_lines()):
        process_line(filename, file_extension, clean_lines, line,
                     include_state, function_state, class_state, file_state,
                     enum_state, asm_state, error)
    class_state.check_finished(error)

    check_for_include_what_you_use(filename, clean_lines, include_state, error)

    # We check here rather than inside process_line so that we see raw
    # lines rather than "cleaned" lines.
    check_for_unicode_replacement_characters(lines, error)

    check_for_missing_new_line_at_eof(lines, error)
    check_for_extra_new_line_at_eof(lines, error)


class CppChecker(object):

    """Processes C++ lines for checking style."""

    # This list is used to--
    #
    # (1) generate an explicit list of all possible categories,
    # (2) unit test that all checked categories have valid names, and
    # (3) unit test that all categories are getting unit tested.
    #
    categories = set([
        'build/class',
        'build/deprecated',
        'build/endif_comment',
        'build/export_macro',
        'build/forward_decl',
        'build/header_guard',
        'build/header_guard_missing',
        'build/include',
        'build/include_order',
        'build/include_what_you_use',
        'build/namespaces',
        'build/printf_format',
        'build/storage_class',
        'build/using_std',
        'build/using_namespace',
        'build/cpp_comment',
        'build/webcore_export',
        'build/wk_api_available',
        'build/version_check',
        'legal/copyright',
        'policy/language',
        'readability/braces',
        'readability/casting',
        'readability/check',
        'readability/comparison_to_zero',
        'readability/constructors',
        'readability/control_flow',
        'readability/enum_casing',
        'readability/fn_size',
        'readability/function',
        'readability/inheritance',
        'readability/multiline_comment',
        'readability/multiline_string',
        'readability/parameter_name',
        'readability/naming',
        'readability/naming/acronym',
        'readability/naming/protected',
        'readability/naming/underscores',
        'readability/null',
        'readability/streams',
        'readability/todo',
        'readability/utf8',
        'runtime/arrays',
        'runtime/bitfields',
        'runtime/callonmainthread',
        'runtime/casting',
        'runtime/ctype_function',
        'runtime/dispatch_set_target_queue',
        'runtime/enum_bitfields',
        'runtime/explicit',
        'runtime/init',
        'runtime/int',
        'runtime/invalid_increment',
        'runtime/ismainthread',
        'runtime/leaky_pattern',
        'runtime/lock_guard',
        'runtime/max_min_macros',
        'runtime/memset',
        'runtime/once_flag',
        'runtime/printf',
        'runtime/printf_format',
        'runtime/references',
        'runtime/retainptr',
        'runtime/rtti',
        'runtime/sizeof',
        'runtime/soft-linked-alloc',
        'runtime/string',
        'runtime/threadsafe_fn',
        'runtime/unsigned',
        'runtime/virtual',
        'runtime/wtf_checked_size',
        'runtime/wtf_make_unique',
        'runtime/wtf_move',
        'runtime/wtf_never_destroyed',
        'security/assertion',
        'security/assertion_fallthrough',
        'security/javascriptcore_wtf_blockptr',
        'security/missing_warn_unused_return',
        'security/printf',
        'security/temp_file',
        'softlink/framework',
        'softlink/header',
        'whitespace/blank_line',
        'whitespace/braces',
        'whitespace/brackets',
        'whitespace/colon',
        'whitespace/comma',
        'whitespace/comments',
        'whitespace/declaration',
        'whitespace/end_of_line',
        'whitespace/ending_newline',
        'whitespace/indent',
        'whitespace/init',
        'whitespace/line_length',
        'whitespace/newline',
        'whitespace/operators',
        'whitespace/property',
        'whitespace/parens',
        'whitespace/semicolon',
        'whitespace/tab',
        'whitespace/todo',
        ])

    def __init__(self, file_path, file_extension, handle_style_error,
                 min_confidence, unit_test_config={}):
        """Create a CppChecker instance.

        Args:
          file_extension: A string that is the file extension, without
                          the leading dot.

        """
        global _unit_test_config
        self.file_extension = file_extension
        self.file_path = file_path
        self.handle_style_error = handle_style_error
        self.min_confidence = min_confidence
        self._inclusive_language_checker = InclusiveLanguageChecker(handle_style_error)
        _unit_test_config = unit_test_config

    # Useful for unit testing.
    def __eq__(self, other):
        """Return whether this CppChecker instance is equal to another."""
        if self.file_extension != other.file_extension:
            return False
        if self.file_path != other.file_path:
            return False
        if self.handle_style_error != other.handle_style_error:
            return False
        if self.min_confidence != other.min_confidence:
            return False

        return True

    # Useful for unit testing.
    def __ne__(self, other):
        # Python does not automatically deduce __ne__() from __eq__().
        return not self.__eq__(other)

    def check(self, lines):
        if is_generated_file(self.file_path):
            return
        _process_lines(self.file_path, self.file_extension, lines,
                       self.handle_style_error, self.min_confidence)
        self._inclusive_language_checker.check(lines)
