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

import codecs
import getopt
import os.path
import sys

from .. style_references import parse_patch
from error_handlers import DefaultStyleErrorHandler
from error_handlers import PatchStyleErrorHandler
from filter import validate_filter_rules
from filter import FilterConfiguration
from processors.common import check_no_carriage_return
from processors.common import categories as CommonCategories
from processors.cpp import CppProcessor
from processors.text import TextProcessor


# These are default option values for the command-line option parser.
_DEFAULT_VERBOSITY = 1
_DEFAULT_OUTPUT_FORMAT = 'emacs'


# FIXME: For style categories we will never want to have, remove them.
#        For categories for which we want to have similar functionality,
#        modify the implementation and enable them.
#
# Throughout this module, we use "filter rule" rather than "filter"
# for an individual boolean filter flag like "+foo".  This allows us to
# reserve "filter" for what one gets by collectively applying all of
# the filter rules.
#
# The base filter rules are the filter rules that begin the list of
# filter rules used to check style.  For example, these rules precede
# any user-specified filter rules.  Since by default all categories are
# checked, this list should normally include only rules that begin
# with a "-" sign.
_BASE_FILTER_RULES = [
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


# The path-specific filter rules.
#
# This list is order sensitive.  Only the first path substring match
# is used.  See the FilterConfiguration documentation in filter.py
# for more information on this list.
#
# Each string appearing in this nested list should have at least
# one associated unit test assertion.  These assertions are located,
# for example, in the test_path_rules_specifier() unit test method of
# checker_unittest.py.
_PATH_RULES_SPECIFIER = [
    # Files in these directories are consumers of the WebKit
    # API and therefore do not follow the same header including
    # discipline as WebCore.
    (["WebKitTools/WebKitAPITest/",
      "WebKit/qt/QGVLauncher/"],
     ["-build/include",
      "-readability/streams"]),
    ([# The GTK+ APIs use GTK+ naming style, which includes
      # lower-cased, underscore-separated values.
      "WebKit/gtk/webkit/",
      # There is no clean way to avoid "yy_*" names used by flex.
      "WebCore/css/CSSParser.cpp",
      # There is no clean way to avoid "xxx_data" methods inside
      # Qt's autotests since they are called automatically by the
      # QtTest module.
      "WebKit/qt/tests/",
      "JavaScriptCore/qt/tests"],
     ["-readability/naming"]),
]


# Some files should be skipped when checking style. For example,
# WebKit maintains some files in Mozilla style on purpose to ease
# future merges.
#
# Include a warning for skipped files that are less obvious.
_SKIPPED_FILES_WITH_WARNING = [
    # The Qt API and tests do not follow WebKit style.
    # They follow Qt style. :)
    "gtk2drawing.c", # WebCore/platform/gtk/gtk2drawing.c
    "gtk2drawing.h", # WebCore/platform/gtk/gtk2drawing.h
    "JavaScriptCore/qt/api/",
    "WebKit/gtk/tests/",
    "WebKit/qt/Api/",
    "WebKit/qt/tests/",
    ]


# Don't include a warning for skipped files that are more common
# and more obvious.
_SKIPPED_FILES_WITHOUT_WARNING = [
    "LayoutTests/"
    ]


# The maximum number of errors to report per file, per category.
# If a category is not a key, then it has no maximum.
_MAX_REPORTS_PER_CATEGORY = {
    "whitespace/carriage_return": 1
}


def _all_categories():
    """Return the set of all categories used by check-webkit-style."""
    # Take the union across all processors.
    return CommonCategories.union(CppProcessor.categories)


def _check_webkit_style_defaults():
    """Return the default command-line options for check-webkit-style."""
    return DefaultCommandOptionValues(base_filter_rules=_BASE_FILTER_RULES,
                                      output_format=_DEFAULT_OUTPUT_FORMAT,
                                      verbosity=_DEFAULT_VERBOSITY)



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


# FIXME: Eliminate support for "extra_flag_values".
#
# FIXME: Remove everything from ProcessorOptions except for the
#        information that can be passed via the command line, and
#        rename to something like CheckWebKitStyleOptions.  This
#        includes, but is not limited to, removing the
#        max_reports_per_error attribute and the is_reportable()
#        method.  See also the FIXME below to create a new class
#        called something like CheckerConfiguration.
#
# This class should not have knowledge of the flag key names.
class ProcessorOptions(object):

    """Stores the option values passed by the user via the command line.

    Attributes:
      extra_flag_values: A string-string dictionary of all flag key-value
                         pairs that are not otherwise represented by this
                         class.  The default is the empty dictionary.

      filter_configuration: A FilterConfiguration instance.  The default
                            is the "empty" filter configuration, which
                            means that all errors should be checked.

      git_commit: A string representing the git commit to check.
                  The default is None.

      max_reports_per_error: The maximum number of errors to report
                             per file, per category.

      output_format: A string that is the output format.  The supported
                     output formats are "emacs" which emacs can parse
                     and "vs7" which Microsoft Visual Studio 7 can parse.

      verbosity: An integer between 1-5 inclusive that restricts output
                 to errors with a confidence score at or above this value.
                 The default is 1, which reports all errors.

    """
    def __init__(self,
                 extra_flag_values=None,
                 filter_configuration = None,
                 git_commit=None,
                 max_reports_per_category=None,
                 output_format="emacs",
                 verbosity=1):
        if extra_flag_values is None:
            extra_flag_values = {}
        if filter_configuration is None:
            filter_configuration = FilterConfiguration()
        if max_reports_per_category is None:
            max_reports_per_category = {}

        if output_format not in ("emacs", "vs7"):
            raise ValueError('Invalid "output_format" parameter: '
                             'value must be "emacs" or "vs7". '
                             'Value given: "%s".' % output_format)

        if (verbosity < 1) or (verbosity > 5):
            raise ValueError('Invalid "verbosity" parameter: '
                             "value must be an integer between 1-5 inclusive. "
                             'Value given: "%s".' % verbosity)

        self.extra_flag_values = extra_flag_values
        self.filter_configuration = filter_configuration
        self.git_commit = git_commit
        self.max_reports_per_category = max_reports_per_category
        self.output_format = output_format
        self.verbosity = verbosity

    # Useful for unit testing.
    def __eq__(self, other):
        """Return whether this ProcessorOptions instance is equal to another."""
        if self.extra_flag_values != other.extra_flag_values:
            return False
        if self.filter_configuration != other.filter_configuration:
            return False
        if self.git_commit != other.git_commit:
            return False
        if self.max_reports_per_category != other.max_reports_per_category:
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

    def is_reportable(self, category, confidence_in_error, path):
        """Return whether an error is reportable.

        An error is reportable if the confidence in the error
        is at least the current verbosity level, and if the current
        filter says that the category should be checked for the
        given path.

        Args:
          category: A string that is a style category.
          confidence_in_error: An integer between 1 and 5, inclusive, that
                               represents the application's confidence in
                               the error. A higher number signifies greater
                               confidence.
          path: The path of the file being checked

        """
        if confidence_in_error < self.verbosity:
            return False

        return self.filter_configuration.should_check(category, path)


# This class should not have knowledge of the flag key names.
class DefaultCommandOptionValues(object):

    """Stores the default check-webkit-style command-line options.

    Attributes:
      base_filter_rules: A list of boolean filter rule strings that begin
                         the list of filter rules used to check style.
      output_format: A string that is the default output format.
      verbosity: An integer that is the default verbosity level.

    """

    def __init__(self, base_filter_rules, output_format, verbosity):
        self.base_filter_rules = base_filter_rules
        self.output_format = output_format
        self.verbosity = verbosity


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
        # Only include the filter flag if user-provided rules are present.
        user_rules = options.filter_configuration.user_rules
        if user_rules:
            flags['filter'] = ",".join(user_rules)
        if options.git_commit:
            flags['git-commit'] = options.git_commit

        flag_string = ''
        # Alphabetizing lets us unit test this method.
        for key in sorted(flags.keys()):
            flag_string += self._flag_pair_to_string(key, flags[key]) + ' '

        return flag_string.strip()


# FIXME: Replace the use of getopt.getopt() with optparse.OptionParser.
class ArgumentParser(object):

    """Supports the parsing of check-webkit-style command arguments.

    Attributes:
      create_usage: A function that accepts a DefaultCommandOptionValues
                    instance and returns a string of usage instructions.
                    Defaults to the function that generates the usage
                    string for check-webkit-style.
      default_options: A DefaultCommandOptionValues instance.  Defaults
                       to the default command option values used by
                       check-webkit-style.
      stderr_write: A function that takes a string as a parameter and
                    serves as stderr.write.  Defaults to sys.stderr.write.
                    This parameter should be specified only for unit tests.

    """

    def __init__(self,
                 create_usage=None,
                 default_options=None,
                 stderr_write=None):
        if create_usage is None:
            create_usage = _create_usage
        if default_options is None:
            default_options = _check_webkit_style_defaults()
        if stderr_write is None:
            stderr_write = sys.stderr.write

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
        categories = _all_categories()
        for category in sorted(categories):
            self.stderr_write('    ' + category + '\n')

        self.stderr_write('\nDefault filter rules**:\n')
        for filter_rule in sorted(self.default_options.base_filter_rules):
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

        output_format = self.default_options.output_format
        verbosity = self.default_options.verbosity
        base_rules = self.default_options.base_filter_rules

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
        user_rules = []

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
                user_rules = self._parse_filter_flag(val)
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

        all_categories = _all_categories()
        validate_filter_rules(user_rules, all_categories)

        verbosity = int(verbosity)
        if (verbosity < 1) or (verbosity > 5):
            raise ValueError('Invalid --verbose value %s: value must '
                             'be between 1-5.' % verbosity)

        filter_configuration = FilterConfiguration(base_rules=base_rules,
                                   path_specific=_PATH_RULES_SPECIFIER,
                                   user_rules=user_rules)

        options = ProcessorOptions(extra_flag_values=extra_flag_values,
                      filter_configuration=filter_configuration,
                      git_commit=git_commit,
                      max_reports_per_category=_MAX_REPORTS_PER_CATEGORY,
                      output_format=output_format,
                      verbosity=verbosity)

        return (filenames, options)


# Enum-like idiom
class FileType:

    NONE = 1
    # Alphabetize remaining types
    CPP = 2
    TEXT = 3


class ProcessorDispatcher(object):

    """Supports determining whether and how to check style, based on path."""

    cpp_file_extensions = (
        'c',
        'cpp',
        'h',
        )

    text_file_extensions = (
        'css',
        'html',
        'idl',
        'js',
        'mm',
        'php',
        'pm',
        'py',
        'txt',
        )

    def _file_extension(self, file_path):
        """Return the file extension without the leading dot."""
        return os.path.splitext(file_path)[1].lstrip(".")

    def should_skip_with_warning(self, file_path):
        """Return whether the given file should be skipped with a warning."""
        for skipped_file in _SKIPPED_FILES_WITH_WARNING:
            if file_path.find(skipped_file) >= 0:
                return True
        return False

    def should_skip_without_warning(self, file_path):
        """Return whether the given file should be skipped without a warning."""
        for skipped_file in _SKIPPED_FILES_WITHOUT_WARNING:
            if file_path.find(skipped_file) >= 0:
                return True
        return False

    def _file_type(self, file_path):
        """Return the file type corresponding to the given file."""
        file_extension = self._file_extension(file_path)

        if (file_extension in self.cpp_file_extensions) or (file_path == '-'):
            # FIXME: Do something about the comment below and the issue it
            #        raises since cpp_style already relies on the extension.
            #
            # Treat stdin as C++. Since the extension is unknown when
            # reading from stdin, cpp_style tests should not rely on
            # the extension.
            return FileType.CPP
        elif ("ChangeLog" in file_path
              or "WebKitTools/Scripts/" in file_path
              or file_extension in self.text_file_extensions):
            return FileType.TEXT
        else:
            return FileType.NONE

    def _create_processor(self, file_type, file_path, handle_style_error, verbosity):
        """Instantiate and return a style processor based on file type."""
        if file_type == FileType.NONE:
            processor = None
        elif file_type == FileType.CPP:
            file_extension = self._file_extension(file_path)
            processor = CppProcessor(file_path, file_extension, handle_style_error, verbosity)
        elif file_type == FileType.TEXT:
            processor = TextProcessor(file_path, handle_style_error)
        else:
            raise ValueError('Invalid file type "%(file_type)s": the only valid file types '
                             "are %(NONE)s, %(CPP)s, and %(TEXT)s."
                             % {"file_type": file_type,
                                "NONE": FileType.NONE,
                                "CPP": FileType.CPP,
                                "TEXT": FileType.TEXT})

        return processor

    def dispatch_processor(self, file_path, handle_style_error, verbosity):
        """Instantiate and return a style processor based on file path."""
        file_type = self._file_type(file_path)

        processor = self._create_processor(file_type,
                                           file_path,
                                           handle_style_error,
                                           verbosity)
        return processor


# FIXME: When creating the new CheckWebKitStyleOptions class as
#        described in a FIXME above, add a new class here called
#        something like CheckerConfiguration.  The class should contain
#        attributes for options needed to process a file.  This includes
#        a subset of the CheckWebKitStyleOptions attributes, a
#        FilterConfiguration attribute, an stderr_write attribute, a
#        max_reports_per_category attribute, etc.  It can also include
#        the is_reportable() method.  The StyleChecker should accept
#        an instance of this class rather than a ProcessorOptions
#        instance.


class StyleChecker(object):

    """Supports checking style in files and patches.

       Attributes:
         error_count: An integer that is the total number of reported
                      errors for the lifetime of this StyleChecker
                      instance.
         options: A ProcessorOptions instance that controls the behavior
                  of style checking.

    """

    def __init__(self, options, stderr_write=None):
        """Create a StyleChecker instance.

        Args:
          options: See options attribute.
          stderr_write: A function that takes a string as a parameter
                        and that is called when a style error occurs.
                        Defaults to sys.stderr.write. This should be
                        used only for unit tests.

        """
        if stderr_write is None:
            stderr_write = sys.stderr.write

        self._stderr_write = stderr_write
        self.error_count = 0
        self.options = options

    def _increment_error_count(self):
        """Increment the total count of reported errors."""
        self.error_count += 1

    def _process_file(self, processor, file_path, handle_style_error):
        """Process the file using the given processor."""
        try:
            # Support the UNIX convention of using "-" for stdin.  Note that
            # we are not opening the file with universal newline support
            # (which codecs doesn't support anyway), so the resulting lines do
            # contain trailing '\r' characters if we are reading a file that
            # has CRLF endings.
            # If after the split a trailing '\r' is present, it is removed
            # below. If it is not expected to be present (i.e. os.linesep !=
            # '\r\n' as in Windows), a warning is issued below if this file
            # is processed.
            if file_path == '-':
                file = codecs.StreamReaderWriter(sys.stdin,
                                                 codecs.getreader('utf8'),
                                                 codecs.getwriter('utf8'),
                                                 'replace')
            else:
                file = codecs.open(file_path, 'r', 'utf8', 'replace')

            contents = file.read()

        except IOError:
            self._stderr_write("Skipping input '%s': Can't open for reading\n" % file_path)
            return

        lines = contents.split("\n")

        for line_number in range(len(lines)):
            # FIXME: We should probably use the SVN "eol-style" property
            #        or a white list to decide whether or not to do
            #        the carriage-return check. Originally, we did the
            #        check only if (os.linesep != '\r\n').
            #
            # FIXME: As a minor optimization, we can have
            #        check_no_carriage_return() return whether
            #        the line ends with "\r".
            check_no_carriage_return(lines[line_number], line_number,
                                     handle_style_error)
            if lines[line_number].endswith("\r"):
                lines[line_number] = lines[line_number].rstrip("\r")

        processor.process(lines)

    def check_file(self, file_path, handle_style_error=None, process_file=None):
        """Check style in the given file.

        Args:
          file_path: A string that is the path of the file to process.
          handle_style_error: The function to call when a style error
                              occurs. This parameter is meant for internal
                              use within this class. Defaults to a
                              DefaultStyleErrorHandler instance.
          process_file: The function to call to process the file. This
                        parameter should be used only for unit tests.
                        Defaults to the file processing method of this class.

        """
        if handle_style_error is None:
            handle_style_error = DefaultStyleErrorHandler(file_path=file_path,
                                     options=self.options,
                                     increment_error_count=
                                         self._increment_error_count,
                                     stderr_write=self._stderr_write)
        if process_file is None:
            process_file = self._process_file

        dispatcher = ProcessorDispatcher()

        if dispatcher.should_skip_without_warning(file_path):
            return
        if dispatcher.should_skip_with_warning(file_path):
            self._stderr_write('Ignoring "%s": this file is exempt from the '
                               "style guide.\n" % file_path)
            return

        verbosity = self.options.verbosity
        processor = dispatcher.dispatch_processor(file_path,
                                                  handle_style_error,
                                                  verbosity)
        if processor is None:
            return

        process_file(processor, file_path, handle_style_error)

    def check_patch(self, patch_string):
        """Check style in the given patch.

        Args:
          patch_string: A string that is a patch string.

        """
        patch_files = parse_patch(patch_string)
        for file_path, diff in patch_files.iteritems():
            style_error_handler = PatchStyleErrorHandler(diff,
                                      file_path,
                                      self.options,
                                      self._increment_error_count,
                                      self._stderr_write)

            self.check_file(file_path, style_error_handler)

