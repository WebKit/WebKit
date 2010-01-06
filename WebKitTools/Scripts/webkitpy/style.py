# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2010 Chris Jerdonek (chris.jerdonek@gmail.com)
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

import getopt
import os.path
import sys

import cpp_style
import text_style
from diff_parser import DiffParser


# These defaults are used by check-webkit-style.
DEFAULT_VERBOSITY = 1
DEFAULT_OUTPUT_FORMAT = 'emacs'


# FIXME: For style categories we will never want to have, remove them.
#        For categories for which we want to have similar functionality,
#        modify the implementation and enable them.
#
# Throughout this module, we use "filter rule" rather than "filter"
# for an individual boolean filter flag like "+foo". This allows us to
# reserve "filter" for what one gets by collectively applying all of
# the filter rules.
#
# The _WEBKIT_FILTER_RULES are prepended to any user-specified filter
# rules. Since by default all errors are on, only include rules that
# begin with a - sign.
WEBKIT_FILTER_RULES = [
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


# FIXME: The STYLE_CATEGORIES show up in both file types cpp_style.py
#        and text_style.py. Break this list into possibly overlapping
#        sub-lists, and store each sub-list in the corresponding .py
#        file. The file style.py can obtain the master list by taking
#        the union. This will allow the unit tests for each file type
#        to check that all of their respective style categories are
#        represented -- without having to reference style.py or be
#        aware of the existence of other file types.
#
# We categorize each style rule we print.  Here are the categories.
# We want an explicit list so we can display a full list to the user.
# If you add a new error message with a new category, add it to the list
# here!  cpp_style_unittest.py should tell you if you forget to do this.
STYLE_CATEGORIES = [
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


def _create_usage(defaults):
    """Return the usage string to display for command help.

    Args:
      defaults: An ArgumentDefaults instance.

    """
    usage = """
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
      A number 1-5 that restricts output to errors with a confidence
      score at or above this value. In particular, the value 1 displays
      all errors. The default is %(default_verbosity)s.

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
""" % {'program_name': os.path.basename(sys.argv[0]),
       'default_verbosity': defaults.verbosity,
       'default_output_format': defaults.output_format}

    return usage


# This class should not have knowledge of the flag key names.
class ProcessorOptions(object):

    """A container to store options to use when checking style.

    Attributes:
      output_format: A string that is the output format. The supported
                     output formats are "emacs" which emacs can parse
                     and "vs7" which Microsoft Visual Studio 7 can parse.

      verbosity: An integer 1-5 that restricts output to errors with a
                 confidence score at or above this value.
                 The default is 1, which displays all errors.

      filter_rules: A list of strings that are boolean filter rules used
                    to determine whether a style category should be checked.
                    Each string should start with + or -. An example
                    string is "+whitespace/indent". The list includes any
                    prepended default filter rules. The default is the
                    empty list, which includes all categories.

      git_commit: A string representing the git commit to check.
                  The default is None.

      extra_flag_values: A string-string dictionary of all flag key-value
                         pairs that are not otherwise represented by this
                         class. The default is the empty dictionary.
    """

    def __init__(self, output_format, verbosity=1, filter_rules=None,
                 git_commit=None, extra_flag_values=None):
        if filter_rules is None:
            filter_rules = []
        if extra_flag_values is None:
            extra_flag_values = {}

        self.output_format = output_format
        self.verbosity = verbosity
        self.filter_rules = filter_rules
        self.git_commit = git_commit
        self.extra_flag_values = extra_flag_values


# FIXME: Eliminate the need for this function.
#        Options should be passed into process_file instead.
def set_options(options):
    """Initialize global _CppStyleState instance.

    This needs to be called before calling process_file.

    Args:
      options: A ProcessorOptions instance.
    """
    cpp_style._set_output_format(options.output_format)
    cpp_style._set_verbose_level(options.verbosity)
    cpp_style._set_filters(options.filter_rules)


# This class should not have knowledge of the flag key names.
class ArgumentDefaults(object):

    """A container to store default argument values.

    Attributes:
      output_format: A string that is the default output format.
      verbosity: An integer that is the default verbosity level.
      filter_rules: A list of strings that are boolean filter rules
                    to prepend to any user-specified rules.

    """

    def __init__(self, default_output_format, default_verbosity,
                 default_filter_rules):
        self.output_format = default_output_format
        self.verbosity = default_verbosity
        self.filter_rules = default_filter_rules


class ArgumentPrinter(object):

    """Supports the printing of check-webkit-style command arguments."""

    def _flag_pair_to_string(self, flag_key, flag_value):
        return '--%(key)s=%(val)s' % {'key': flag_key, 'val': flag_value }

    def to_flag_string(self, options):
        """Return a flag string yielding the given ProcessorOptions instance.

        This method orders the flag values alphabetically by the flag key.

        Args:
          options: A ProcessorOptions instance.

        """
        flags = options.extra_flag_values.copy()

        flags['output'] = options.output_format
        flags['verbose'] = options.verbosity
        if options.filter_rules:
            flags['filter'] = ','.join(options.filter_rules)
        if options.git_commit:
            flags['git-commit'] = options.git_commit

        flag_string = ''
        # Alphabetizing lets us unit test this method.
        for key in sorted(flags.keys()):
            flag_string += self._flag_pair_to_string(key, flags[key]) + ' '

        return flag_string.strip()


class ArgumentParser(object):

    """Supports the parsing of check-webkit-style command arguments.

    Attributes:
      defaults: An ArgumentDefaults instance.
      create_usage: A function that accepts an ArgumentDefaults instance
                    and returns a string of usage instructions.
                    This defaults to the function used to generate the
                    usage string for check-webkit-style.
      doc_print: A function that accepts a string parameter and that is
                 called to display help messages. This defaults to
                 sys.stderr.write().

    """

    def __init__(self, argument_defaults, create_usage=None, doc_print=None):
        if create_usage is None:
            create_usage = _create_usage
        if doc_print is None:
            doc_print = sys.stderr.write

        self.defaults = argument_defaults
        self.create_usage = create_usage
        self.doc_print = doc_print

    def _exit_with_usage(self, error_message=''):
        """Exit and print a usage string with an optional error message.

        Args:
          error_message: A string that is an error message to print.
        """
        usage = self.create_usage(self.defaults)
        self.doc_print(usage)
        if error_message:
            sys.exit('\nFATAL ERROR: ' + error_message)
        else:
            sys.exit(1)

    def _exit_with_categories(self):
        """Exit and print the style categories and default filter rules."""
        self.doc_print('\nAll categories:\n')
        for category in sorted(STYLE_CATEGORIES):
            self.doc_print('    ' + category + '\n')

        self.doc_print('\nDefault filter rules**:\n')
        for filter_rule in sorted(self.defaults.filter_rules):
            self.doc_print('    ' + filter_rule + '\n')
        self.doc_print('\n**The command always evaluates the above rules, '
                       'and before any --filter flag.\n\n')

        sys.exit(0)

    def _parse_filter_flag(self, flag_value):
        """Parse the value of the --filter flag.

        These filters are applied when deciding whether to emit a given
        error message.

        Args:
          flag_value: A string of comma-separated filter rules, for
                      example "-whitespace,+whitespace/indent".

        """
        filters = []
        for uncleaned_filter in flag_value.split(','):
            filter = uncleaned_filter.strip()
            if not filter:
                continue
            filters.append(filter)
        return filters

    def parse(self, args, extra_flags=None):
        """Parse the command line arguments to check-webkit-style.

        Args:
          args: A list of command-line arguments as returned by sys.argv[1:].
          extra_flags: A list of flags whose values we want to extract, but
                       are not supported by the ProcessorOptions class.
                       An example flag "new_flag=". This defaults to the
                       empty list.

        Returns:
          A tuple of (filenames, options)

          filenames: The list of filenames to check.
          options: A ProcessorOptions instance.

        """
        if extra_flags is None:
            extra_flags = []

        output_format = self.defaults.output_format
        verbosity = self.defaults.verbosity
        filter_rules = self.defaults.filter_rules

        # The flags already supported by the ProcessorOptions class.
        flags = ['help', 'output=', 'verbose=', 'filter=', 'git-commit=']

        for extra_flag in extra_flags:
            if extra_flag in flags:
                raise ValueError('Flag \'%(extra_flag)s is duplicated '
                                 'or already supported.' %
                                 {'extra_flag': extra_flag})
            flags.append(extra_flag)

        try:
            (opts, filenames) = getopt.getopt(args, '', flags)
        except getopt.GetoptError:
            # FIXME: Settle on an error handling approach: come up
            #        with a consistent guideline as to when and whether
            #        a ValueError should be raised versus calling
            #        sys.exit when needing to interrupt execution.
            self._exit_with_usage('Invalid arguments.')

        extra_flag_values = {}
        git_commit = None

        for (opt, val) in opts:
            if opt == '--help':
                self._exit_with_usage()
            elif opt == '--output':
                output_format = val
            elif opt == '--verbose':
                verbosity = val
            elif opt == '--git-commit':
                git_commit = val
            elif opt == '--filter':
                if not val:
                    self._exit_with_categories()
                # Prepend the defaults.
                filter_rules = filter_rules + self._parse_filter_flag(val)
            else:
                extra_flag_values[opt] = val

        # Check validity of resulting values.
        if filenames and (git_commit != None):
            self._exit_with_usage('It is not possible to check files and a '
                                  'specific commit at the same time.')

        if output_format not in ('emacs', 'vs7'):
            raise ValueError('Invalid --output value "%s": The only '
                             'allowed output formats are emacs and vs7.' %
                             output_format)

        verbosity = int(verbosity)
        if ((verbosity < 1) or (verbosity > 5)):
            raise ValueError('Invalid --verbose value %s: value must '
                             'be between 1-5.' % verbosity)

        for rule in filter_rules:
            if not (rule.startswith('+') or rule.startswith('-')):
                raise ValueError('Invalid filter rule "%s": every rule '
                                 'rule in the --filter flag must start '
                                 'with + or -.' % rule)

        options = ProcessorOptions(output_format, verbosity, filter_rules,
                                   git_commit, extra_flag_values)

        return (filenames, options)


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
