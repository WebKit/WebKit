# Copyright (C) 2009 Google Inc. All rights reserved.
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

"""Front end of some style-checker modules."""

# FIXME: Move more code from cpp_style to here.
# check-webkit-style should not refer cpp_style directly.

import getopt
import os.path
import sys

import cpp_style
import text_style
from diff_parser import DiffParser


# Default options
_DEFAULT_VERBOSITY = 1
_DEFAULT_OUTPUT_FORMAT = 'emacs'


# FIXME: For style categories we will never want to have, remove them.
#        For categories for which we want to have similar functionality,
#        modify the implementation and enable them.
# FIXME: Add a unit test to ensure the corresponding categories
#        are elements of _STYLE_CATEGORIES.
#
# For unambiguous terminology, we use "filter rule" rather than "filter"
# for an individual boolean filter flag like "+foo". This allows us to 
# reserve "filter" for what one gets by collectively applying all of 
# the filter rules as specified by a --filter flag.
_WEBKIT_FILTER_RULES = [
    '-build/endif_comment',
    '-build/include_what_you_use',  # <string> for std::string
    '-build/storage_class',  # const static
    '-legal/copyright',
    '-readability/multiline_comment',
    '-readability/braces',  # int foo() {};
    '-readability/fn_size',
    '-readability/casting',
    '-readability/function',
    '-runtime/arrays',  # variable length array
    '-runtime/casting',
    '-runtime/sizeof',
    '-runtime/explicit',  # explicit
    '-runtime/virtual',  # virtual dtor
    '-runtime/printf',
    '-runtime/threadsafe_fn',
    '-runtime/rtti',
    '-whitespace/blank_line',
    '-whitespace/end_of_line',
    '-whitespace/labels',
    ]


# We categorize each style rule we print.  Here are the categories.
# We want an explicit list so we can list them all in cpp_style --filter=.
# If you add a new error message with a new category, add it to the list
# here!  cpp_style_unittest.py should tell you if you forget to do this.
_STYLE_CATEGORIES = [
    'build/class',
    'build/deprecated',
    'build/endif_comment',
    'build/forward_decl',
    'build/header_guard',
    'build/include',
    'build/include_order',
    'build/include_what_you_use',
    'build/namespaces',
    'build/printf_format',
    'build/storage_class',
    'build/using_std',
    'legal/copyright',
    'readability/braces',
    'readability/casting',
    'readability/check',
    'readability/comparison_to_zero',
    'readability/constructors',
    'readability/control_flow',
    'readability/fn_size',
    'readability/function',
    'readability/multiline_comment',
    'readability/multiline_string',
    'readability/naming',
    'readability/null',
    'readability/streams',
    'readability/todo',
    'readability/utf8',
    'runtime/arrays',
    'runtime/casting',
    'runtime/explicit',
    'runtime/init',
    'runtime/int',
    'runtime/invalid_increment',
    'runtime/max_min_macros',
    'runtime/memset',
    'runtime/printf',
    'runtime/printf_format',
    'runtime/references',
    'runtime/rtti',
    'runtime/sizeof',
    'runtime/string',
    'runtime/threadsafe_fn',
    'runtime/virtual',
    'whitespace/blank_line',
    'whitespace/braces',
    'whitespace/comma',
    'whitespace/comments',
    'whitespace/declaration',
    'whitespace/end_of_line',
    'whitespace/ending_newline',
    'whitespace/indent',
    'whitespace/labels',
    'whitespace/line_length',
    'whitespace/newline',
    'whitespace/operators',
    'whitespace/parens',
    'whitespace/semicolon',
    'whitespace/tab',
    'whitespace/todo',
    ]


_USAGE = """
Syntax: %(program_name)s [--verbose=#] [--git-commit=<SingleCommit>] [--output=vs7]
        [--filter=-x,+y,...] [file] ...

  The style guidelines this tries to follow are here:
    http://webkit.org/coding/coding-style.html

  Every style error is given a confidence score from 1-5, with 5 meaning
  we are certain of the problem, and 1 meaning it could be a legitimate
  construct.  This can miss some errors and does not substitute for
  code review.

  To prevent specific lines from being linted, add a '// NOLINT' comment to the
  end of the line.

  Linted extensions are .cpp, .c and .h.  Other file types are ignored.

  The file parameter is optional and accepts multiple files.  Leaving
  out the file parameter applies the check to all files considered changed
  by your source control management system.

  Flags:

    verbose=#
      A number 0-5 to restrict errors to certain verbosity levels.
      Defaults to %(default_verbosity)s.

    git-commit=<SingleCommit>
      Checks the style of everything from the given commit to the local tree.

    output=vs7
      The output format, which may be one of
        emacs : to ease emacs parsing
        vs7   : compatible with Visual Studio
      Defaults to "%(default_output_format)s". Other formats are unsupported.

    filter=-x,+y,...
      A comma-separated list of boolean filter rules used to filter
      which categories of style guidelines to check.  The script checks
      a category if the category passes the filter rules, as follows.

      Any webkit category starts out passing.  All filter rules are then
      evaluated left to right, with later rules taking precedence.  For
      example, the rule "+foo" passes any category that starts with "foo",
      and "-foo" fails any such category.  The filter input "-whitespace,
      +whitespace/braces" fails the category "whitespace/tab" and passes
      "whitespace/braces".

      Examples: --filter=-whitespace,+whitespace/braces
                --filter=-whitespace,-runtime/printf,+runtime/printf_format
                --filter=-,+build/include_what_you_use

      Category names appear in error messages in brackets, for example
      [whitespace/indent].  To see a list of all categories available to
      %(program_name)s, along with which are enabled by default, pass
      the empty filter as follows:
         --filter=
""" % {
    'program_name': os.path.basename(sys.argv[0]),
    'default_verbosity': _DEFAULT_VERBOSITY,
    'default_output_format': _DEFAULT_OUTPUT_FORMAT
    }


def use_webkit_styles():
    """Configures this module for WebKit style."""
    cpp_style._DEFAULT_FILTER_RULES = _WEBKIT_FILTER_RULES


def exit_with_usage(error_message, display_help=False):
    """Exit and print a usage string with an optional error message.

    Args:
      error_message: The optional error message.
      display_help: Whether to display help output. Suppressing help
                    output is useful for unit tests.
    """
    if display_help:
        sys.stderr.write(_USAGE)
    if error_message:
        sys.exit('\nFATAL ERROR: ' + error_message)
    else:
        sys.exit(1)


def exit_with_categories(display_help=False):
    """Exit and print all style categories, along with the default filter.

    These category names appear in error messages.  They can be filtered
    using the --filter flag.

    Args:
      display_help: Whether to display help output. Suppressing help
                    output is useful for unit tests.
    """
    if display_help:
        sys.stderr.write('\nAll categories:\n')
        for category in sorted(_STYLE_CATEGORIES):
            sys.stderr.write('    ' + category + '\n')

        sys.stderr.write('\nDefault filter rules**:\n')
        for filter_rule in sorted(_WEBKIT_FILTER_RULES):
            sys.stderr.write('    ' + filter_rule + '\n')
        sys.stderr.write('\n**The command always evaluates the above '
                         'rules, and before any --filter flag.\n\n')

    sys.exit(0)


def parse_arguments(args, additional_flags=[], display_help=False):
    """Parses the command line arguments.

    This may set the output format and verbosity level as side-effects.

    Args:
      args: The command line arguments:
      additional_flags: A list of strings which specifies flags we allow.
      display_help: Whether to display help output. Suppressing help
                    output is useful for unit tests.

    Returns:
      A tuple of (filenames, flags)

      filenames: The list of filenames to lint.
      flags: The dict of the flag names and the flag values.
    """
    flags = ['help', 'output=', 'verbose=', 'filter='] + additional_flags
    additional_flag_values = {}
    try:
        (opts, filenames) = getopt.getopt(args, '', flags)
    except getopt.GetoptError:
        exit_with_usage('Invalid arguments.', display_help)

    verbosity = _DEFAULT_VERBOSITY
    output_format = _DEFAULT_OUTPUT_FORMAT
    filters = ''

    for (opt, val) in opts:
        if opt == '--help':
            exit_with_usage(None, display_help)
        elif opt == '--output':
            if not val in ('emacs', 'vs7'):
                exit_with_usage('The only allowed output formats are emacs and vs7.',
                                display_help)
            output_format = val
        elif opt == '--verbose':
            verbosity = int(val)
        elif opt == '--filter':
            filters = val
            if not filters:
                exit_with_categories(display_help)
        else:
            additional_flag_values[opt] = val

    cpp_style._set_output_format(output_format)
    cpp_style._set_verbose_level(verbosity)
    cpp_style._set_filters(filters)

    return (filenames, additional_flag_values)


def process_file(filename):
    """Checks style for the specified file.

    If the specified filename is '-', applies cpp_style to the standard input.
    """
    if cpp_style.can_handle(filename) or filename == '-':
        cpp_style.process_file(filename)
    elif text_style.can_handle(filename):
        text_style.process_file(filename)


def process_patch(patch_string):
    """Does lint on a single patch.

    Args:
      patch_string: A string of a patch.
    """
    patch = DiffParser(patch_string.splitlines())
    for filename, diff in patch.files.iteritems():
        file_extension = os.path.splitext(filename)[1]
        line_numbers = set()

        def error_for_patch(filename, line_number, category, confidence, message):
            """Wrapper function of cpp_style.error for patches.

            This function outputs errors only if the line number
            corresponds to lines which are modified or added.
            """
            if not line_numbers:
                for line in diff.lines:
                    # When deleted line is not set, it means that
                    # the line is newly added.
                    if not line[0]:
                        line_numbers.add(line[1])

            if line_number in line_numbers:
                cpp_style.error(filename, line_number, category, confidence, message)

        if cpp_style.can_handle(filename):
            cpp_style.process_file(filename, error=error_for_patch)
        elif text_style.can_handle(filename):
            text_style.process_file(filename, error=error_for_patch)


def error_count():
    """Returns the total error count."""
    return cpp_style.error_count()
