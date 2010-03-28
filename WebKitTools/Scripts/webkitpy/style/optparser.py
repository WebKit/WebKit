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
import logging
import os.path
import sys

from filter import validate_filter_rules
# This module should not import anything from checker.py.

_log = logging.getLogger(__name__)


def _create_usage(default_options):
    """Return the usage string to display for command help.

    Args:
      default_options: A DefaultCommandOptionValues instance.

    """
    usage = """
Syntax: %(program_name)s [--filter=-x,+y,...] [--git-commit=<SingleCommit>]
        [--min-confidence=#] [--output=vs7] [--verbose] [file or directory] ...

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

    git-commit=<SingleCommit>
      Checks the style of everything from the given commit to the local tree.

    min-confidence=#
      An integer between 1 and 5 inclusive that is the minimum confidence
      level of style errors to report.  In particular, the value 1 displays
      all errors.  The default is %(default_min_confidence)s.

    output=vs7
      The output format, which may be one of
        emacs : to ease emacs parsing
        vs7   : compatible with Visual Studio
      Defaults to "%(default_output_format)s". Other formats are unsupported.

    verbose
      Logging is verbose if this flag is present.

Path considerations:

  Certain style-checking behavior depends on the paths relative to
  the WebKit source root of the files being checked.  For example,
  certain types of errors may be handled differently for files in
  WebKit/gtk/webkit/ (e.g. by suppressing "readability/naming" errors
  for files in this directory).

  Consequently, if the path relative to the source root cannot be
  determined for a file being checked, then style checking may not
  work correctly for that file.  This can occur, for example, if no
  WebKit checkout can be found, or if the source root can be detected,
  but one of the files being checked lies outside the source tree.

  If a WebKit checkout can be detected and all files being checked
  are in the source tree, then all paths will automatically be
  converted to paths relative to the source root prior to checking.
  This is also useful for display purposes.

  Currently, this command can detect the source root only if the
  command is run from within a WebKit checkout (i.e. if the current
  working directory is below the root of a checkout).  In particular,
  it is not recommended to run this script from a directory outside
  a checkout.

  Running this script from a top-level WebKit source directory and
  checking only files in the source tree will ensure that all style
  checking behaves correctly -- whether or not a checkout can be
  detected.  This is because all file paths will already be relative
  to the source root and so will not need to be converted.

""" % {'program_name': os.path.basename(sys.argv[0]),
       'default_min_confidence': default_options.min_confidence,
       'default_output_format': default_options.output_format}

    return usage


# This class should not have knowledge of the flag key names.
class DefaultCommandOptionValues(object):

    """Stores the default check-webkit-style command-line options.

    Attributes:
      output_format: A string that is the default output format.
      min_confidence: An integer that is the default minimum confidence level.

    """

    def __init__(self, min_confidence, output_format):
        self.min_confidence = min_confidence
        self.output_format = output_format


# This class should not have knowledge of the flag key names.
class CommandOptionValues(object):

    """Stores the option values passed by the user via the command line.

    Attributes:
      is_verbose: A boolean value of whether verbose logging is enabled.

      filter_rules: The list of filter rules provided by the user.
                    These rules are appended to the base rules and
                    path-specific rules and so take precedence over
                    the base filter rules, etc.

      git_commit: A string representing the git commit to check.
                  The default is None.

      min_confidence: An integer between 1 and 5 inclusive that is the
                      minimum confidence level of style errors to report.
                      The default is 1, which reports all errors.

      output_format: A string that is the output format.  The supported
                     output formats are "emacs" which emacs can parse
                     and "vs7" which Microsoft Visual Studio 7 can parse.

    """
    def __init__(self,
                 filter_rules=None,
                 git_commit=None,
                 is_verbose=False,
                 min_confidence=1,
                 output_format="emacs"):
        if filter_rules is None:
            filter_rules = []

        if (min_confidence < 1) or (min_confidence > 5):
            raise ValueError('Invalid "min_confidence" parameter: value '
                             "must be an integer between 1 and 5 inclusive. "
                             'Value given: "%s".' % min_confidence)

        if output_format not in ("emacs", "vs7"):
            raise ValueError('Invalid "output_format" parameter: '
                             'value must be "emacs" or "vs7". '
                             'Value given: "%s".' % output_format)

        self.filter_rules = filter_rules
        self.git_commit = git_commit
        self.is_verbose = is_verbose
        self.min_confidence = min_confidence
        self.output_format = output_format

    # Useful for unit testing.
    def __eq__(self, other):
        """Return whether this instance is equal to another."""
        if self.filter_rules != other.filter_rules:
            return False
        if self.git_commit != other.git_commit:
            return False
        if self.is_verbose != other.is_verbose:
            return False
        if self.min_confidence != other.min_confidence:
            return False
        if self.output_format != other.output_format:
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
        flags = {}
        flags['min-confidence'] = options.min_confidence
        flags['output'] = options.output_format
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

    def parse(self, args, found_checkout):
        """Parse the command line arguments to check-webkit-style.

        Args:
          args: A list of command-line arguments as returned by sys.argv[1:].
          found_checkout: A boolean value of whether the current working
                          directory was found to be inside a WebKit checkout.

        Returns:
          A tuple of (paths, options)

          paths: The list of paths to check.
          options: A CommandOptionValues instance.

        """
        is_verbose = False
        min_confidence = self.default_options.min_confidence
        output_format = self.default_options.output_format

        # The flags that the CommandOptionValues class supports.
        flags = ['filter=', 'git-commit=', 'help', 'min-confidence=',
                 'output=', 'verbose']

        try:
            (opts, paths) = getopt.getopt(args, '', flags)
        except getopt.GetoptError, err:
            # FIXME: Settle on an error handling approach: come up
            #        with a consistent guideline as to when and whether
            #        a ValueError should be raised versus calling
            #        sys.exit when needing to interrupt execution.
            self._exit_with_usage('Invalid arguments: %s' % err)

        git_commit = None
        filter_rules = []

        for (opt, val) in opts:
            # Process --help first (out of alphabetical order).
            if opt == '--help':
                self._exit_with_usage()
            elif opt == '--filter':
                if not val:
                    self._exit_with_categories()
                filter_rules = self._parse_filter_flag(val)
            elif opt == '--git-commit':
                git_commit = val
            elif opt == '--min-confidence':
                min_confidence = val
            elif opt == '--output':
                output_format = val
            elif opt == "--verbose":
                is_verbose = True
            else:
                # We should never get here because getopt.getopt()
                # raises an error in this case.
                raise ValueError('Invalid option: "%s"' % opt)

        # Check validity of resulting values.
        if not found_checkout and not paths:
            _log.error("WebKit checkout not found: You must run this script "
                       "from inside a WebKit checkout if you are not passing "
                       "specific paths to check.")
            sys.exit(1)

        if paths and (git_commit != None):
            self._exit_with_usage('It is not possible to check files and a '
                                  'specific commit at the same time.')

        if output_format not in ('emacs', 'vs7'):
            raise ValueError('Invalid --output value "%s": The only '
                             'allowed output formats are emacs and vs7.' %
                             output_format)

        validate_filter_rules(filter_rules, self._all_categories)

        min_confidence = int(min_confidence)
        if (min_confidence < 1) or (min_confidence > 5):
            raise ValueError('Invalid --min-confidence value %s: value must '
                             'be between 1 and 5 inclusive.' % min_confidence)

        options = CommandOptionValues(filter_rules=filter_rules,
                                      git_commit=git_commit,
                                      is_verbose=is_verbose,
                                      min_confidence=min_confidence,
                                      output_format=output_format)

        return (paths, options)

