# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Supports the parsing of command-line options for check-webkit-style."""

import getopt
import os.path
import sys

from filter import validate_filter_rules
# This module should not import anything from checker.py.


def _create_usage(default_options):
    """Return the usage string to display for command help.

    Args:
      default_options: A DefaultCommandOptionValues instance.

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
       'default_verbosity': default_options.verbosity,
       'default_output_format': default_options.output_format}

    return usage


# This class should not have knowledge of the flag key names.
class DefaultCommandOptionValues(object):

    """Stores the default check-webkit-style command-line options.

    Attributes:
      output_format: A string that is the default output format.
      verbosity: An integer that is the default verbosity level.

    """

    def __init__(self, output_format, verbosity):
        self.output_format = output_format
        self.verbosity = verbosity


# FIXME: Eliminate support for "extra_flag_values".
#
# This class should not have knowledge of the flag key names.
class CommandOptionValues(object):

    """Stores the option values passed by the user via the command line.

    Attributes:
      extra_flag_values: A string-string dictionary of all flag key-value
                         pairs that are not otherwise represented by this
                         class.  The default is the empty dictionary.

      filter_rules: The list of filter rules provided by the user.
                    These rules are appended to the base rules and
                    path-specific rules and so take precedence over
                    the base filter rules, etc.

      git_commit: A string representing the git commit to check.
                  The default is None.

      output_format: A string that is the output format.  The supported
                     output formats are "emacs" which emacs can parse
                     and "vs7" which Microsoft Visual Studio 7 can parse.

      verbosity: An integer between 1-5 inclusive that restricts output
                 to errors with a confidence score at or above this value.
                 The default is 1, which reports all errors.

    """
    def __init__(self,
                 extra_flag_values=None,
                 filter_rules=None,
                 git_commit=None,
                 output_format="emacs",
                 verbosity=1):
        if extra_flag_values is None:
            extra_flag_values = {}
        if filter_rules is None:
            filter_rules = []

        if output_format not in ("emacs", "vs7"):
            raise ValueError('Invalid "output_format" parameter: '
                             'value must be "emacs" or "vs7". '
                             'Value given: "%s".' % output_format)

        if (verbosity < 1) or (verbosity > 5):
            raise ValueError('Invalid "verbosity" parameter: '
                             "value must be an integer between 1-5 inclusive. "
                             'Value given: "%s".' % verbosity)

        self.extra_flag_values = extra_flag_values
        self.filter_rules = filter_rules
        self.git_commit = git_commit
        self.output_format = output_format
        self.verbosity = verbosity

    # Useful for unit testing.
    def __eq__(self, other):
        """Return whether this instance is equal to another."""
        if self.extra_flag_values != other.extra_flag_values:
            return False
        if self.filter_rules != other.filter_rules:
            return False
        if self.git_commit != other.git_commit:
            return False
        if self.output_format != other.output_format:
            return False
        if self.verbosity != other.verbosity:
            return False

        return True

    # Useful for unit testing.
    def __ne__(self, other):
        # Python does not automatically deduce this from __eq__().
        return not self.__eq__(other)


class ArgumentPrinter(object):

    """Supports the printing of check-webkit-style command arguments."""

    def _flag_pair_to_string(self, flag_key, flag_value):
        return '--%(key)s=%(val)s' % {'key': flag_key, 'val': flag_value }

    def to_flag_string(self, options):
        """Return a flag string of the given CommandOptionValues instance.

        This method orders the flag values alphabetically by the flag key.

        Args:
          options: A CommandOptionValues instance.

        """
        flags = options.extra_flag_values.copy()

        flags['output'] = options.output_format
        flags['verbose'] = options.verbosity
        # Only include the filter flag if user-provided rules are present.
        filter_rules = options.filter_rules
        if filter_rules:
            flags['filter'] = ",".join(filter_rules)
        if options.git_commit:
            flags['git-commit'] = options.git_commit

        flag_string = ''
        # Alphabetizing lets us unit test this method.
        for key in sorted(flags.keys()):
            flag_string += self._flag_pair_to_string(key, flags[key]) + ' '

        return flag_string.strip()


# FIXME: Replace the use of getopt.getopt() with optparse.OptionParser.
class ArgumentParser(object):

    # FIXME: Move the documentation of the attributes to the __init__
    #        docstring after making the attributes internal.
    """Supports the parsing of check-webkit-style command arguments.

    Attributes:
      create_usage: A function that accepts a DefaultCommandOptionValues
                    instance and returns a string of usage instructions.
                    Defaults to the function that generates the usage
                    string for check-webkit-style.
      default_options: A DefaultCommandOptionValues instance that provides
                       the default values for options not explicitly
                       provided by the user.
      stderr_write: A function that takes a string as a parameter and
                    serves as stderr.write.  Defaults to sys.stderr.write.
                    This parameter should be specified only for unit tests.

    """

    def __init__(self,
                 all_categories,
                 default_options,
                 base_filter_rules=None,
                 create_usage=None,
                 stderr_write=None):
        """Create an ArgumentParser instance.

        Args:
          all_categories: The set of all available style categories.
          default_options: See the corresponding attribute in the class
                           docstring.
        Keyword Args:
          base_filter_rules: The list of filter rules at the beginning of
                             the list of rules used to check style.  This
                             list has the least precedence when checking
                             style and precedes any user-provided rules.
                             The class uses this parameter only for display
                             purposes to the user.  Defaults to the empty list.
          create_usage: See the documentation of the corresponding
                        attribute in the class docstring.
          stderr_write: See the documentation of the corresponding
                        attribute in the class docstring.

        """
        if base_filter_rules is None:
            base_filter_rules = []
        if create_usage is None:
            create_usage = _create_usage
        if stderr_write is None:
            stderr_write = sys.stderr.write

        self._all_categories = all_categories
        self._base_filter_rules = base_filter_rules
        # FIXME: Rename these to reflect that they are internal.
        self.create_usage = create_usage
        self.default_options = default_options
        self.stderr_write = stderr_write

    def _exit_with_usage(self, error_message=''):
        """Exit and print a usage string with an optional error message.

        Args:
          error_message: A string that is an error message to print.

        """
        usage = self.create_usage(self.default_options)
        self.stderr_write(usage)
        if error_message:
            sys.exit('\nFATAL ERROR: ' + error_message)
        else:
            sys.exit(1)

    def _exit_with_categories(self):
        """Exit and print the style categories and default filter rules."""
        self.stderr_write('\nAll categories:\n')
        for category in sorted(self._all_categories):
            self.stderr_write('    ' + category + '\n')

        self.stderr_write('\nDefault filter rules**:\n')
        for filter_rule in sorted(self._base_filter_rules):
            self.stderr_write('    ' + filter_rule + '\n')
        self.stderr_write('\n**The command always evaluates the above rules, '
                          'and before any --filter flag.\n\n')

        sys.exit(0)

    def _parse_filter_flag(self, flag_value):
        """Parse the --filter flag, and return a list of filter rules.

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
                       are not supported by the CommandOptionValues class.
                       An example flag "new_flag=". This defaults to the
                       empty list.

        Returns:
          A tuple of (filenames, options)

          filenames: The list of filenames to check.
          options: A CommandOptionValues instance.

        """
        if extra_flags is None:
            extra_flags = []

        output_format = self.default_options.output_format
        verbosity = self.default_options.verbosity

        # The flags already supported by the CommandOptionValues class.
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
        filter_rules = []

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
                filter_rules = self._parse_filter_flag(val)
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

        validate_filter_rules(filter_rules, self._all_categories)

        verbosity = int(verbosity)
        if (verbosity < 1) or (verbosity > 5):
            raise ValueError('Invalid --verbose value %s: value must '
                             'be between 1-5.' % verbosity)

        options = CommandOptionValues(extra_flag_values=extra_flag_values,
                                 filter_rules=filter_rules,
                                 git_commit=git_commit,
                                 output_format=output_format,
                                 verbosity=verbosity)

        return (filenames, options)

