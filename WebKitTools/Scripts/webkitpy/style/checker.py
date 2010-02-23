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
import os.path
import sys

from .. style_references import parse_patch
from error_handlers import DefaultStyleErrorHandler
from error_handlers import PatchStyleErrorHandler
from filter import FilterConfiguration
from optparser import ArgumentParser
from optparser import DefaultCommandOptionValues
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
    ([# The EFL APIs use EFL naming style, which includes
      # both lower-cased and camel-cased, underscore-sparated
      # values.
      "WebKit/efl/ewk/",
      # There is no clean way to avoid "yy_*" names used by flex.
      "WebCore/css/CSSParser.cpp",
      # There is no clean way to avoid "xxx_data" methods inside
      # Qt's autotests since they are called automatically by the
      # QtTest module.
      "WebKit/qt/tests/",
      "JavaScriptCore/qt/tests"],
     ["-readability/naming"]),
    ([# The GTK+ APIs use GTK+ naming style, which includes
      # lower-cased, underscore-separated values.
      # Also, GTK+ allows the use of NULL.
      "WebKit/gtk/webkit/",
      "WebKitTools/DumpRenderTree/gtk/"],
     ["-readability/naming",
      "-readability/null"]),
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
    return DefaultCommandOptionValues(output_format=_DEFAULT_OUTPUT_FORMAT,
                                      verbosity=_DEFAULT_VERBOSITY)


# This function assists in optparser not having to import from checker.
def check_webkit_style_parser():
    all_categories = _all_categories()
    default_options = _check_webkit_style_defaults()
    return ArgumentParser(all_categories=all_categories,
                          base_filter_rules=_BASE_FILTER_RULES,
                          default_options=default_options)


def check_webkit_style_configuration(options):
    """Return a StyleCheckerConfiguration instance for check-webkit-style.

    Args:
      options: A CommandOptionValues instance.

    """
    filter_configuration = FilterConfiguration(
                               base_rules=_BASE_FILTER_RULES,
                               path_specific=_PATH_RULES_SPECIFIER,
                               user_rules=options.filter_rules)

    return StyleCheckerConfiguration(filter_configuration=filter_configuration,
               max_reports_per_category=_MAX_REPORTS_PER_CATEGORY,
               output_format=options.output_format,
               stderr_write=sys.stderr.write,
               verbosity=options.verbosity)


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


class StyleCheckerConfiguration(object):

    """Stores configuration values for the StyleChecker class.

    Attributes:
      max_reports_per_category: The maximum number of errors to report
                                per category, per file.

      stderr_write: A function that takes a string as a parameter and
                    serves as stderr.write.

      verbosity: An integer between 1-5 inclusive that restricts output
                 to errors with a confidence score at or above this value.

    """

    def __init__(self,
                 filter_configuration,
                 max_reports_per_category,
                 output_format,
                 stderr_write,
                 verbosity):
        """Create a StyleCheckerConfiguration instance.

        Args:
          filter_configuration: A FilterConfiguration instance.  The default
                                is the "empty" filter configuration, which
                                means that all errors should be checked.

          max_reports_per_category: The maximum number of errors to report
                                    per category, per file.

          output_format: A string that is the output format.  The supported
                         output formats are "emacs" which emacs can parse
                         and "vs7" which Microsoft Visual Studio 7 can parse.

          stderr_write: A function that takes a string as a parameter and
                        serves as stderr.write.

          verbosity: An integer between 1-5 inclusive that restricts output
                     to errors with a confidence score at or above this value.
                     The default is 1, which reports all errors.

        """
        self._filter_configuration = filter_configuration
        self._output_format = output_format

        self.max_reports_per_category = max_reports_per_category
        self.stderr_write = stderr_write
        self.verbosity = verbosity

    def is_reportable(self, category, confidence_in_error, file_path):
        """Return whether an error is reportable.

        An error is reportable if both the confidence in the error is
        at least the current verbosity level and the current filter
        says the category should be checked for the given path.

        Args:
          category: A string that is a style category.
          confidence_in_error: An integer between 1 and 5, inclusive, that
                               represents the application's confidence in
                               the error.  A higher number signifies greater
                               confidence.
          file_path: The path of the file being checked

        """
        if confidence_in_error < self.verbosity:
            return False

        return self._filter_configuration.should_check(category, file_path)

    def write_style_error(self,
                          category,
                          confidence,
                          file_path,
                          line_number,
                          message):
        """Write a style error to the configured stderr."""
        if self._output_format == 'vs7':
            format_string = "%s(%s):  %s  [%s] [%d]\n"
        else:
            format_string = "%s:%s:  %s  [%s] [%d]\n"

        self.stderr_write(format_string % (file_path,
                                           line_number,
                                           message,
                                           category,
                                           confidence))


class StyleChecker(object):

    """Supports checking style in files and patches.

       Attributes:
         error_count: An integer that is the total number of reported
                      errors for the lifetime of this StyleChecker
                      instance.
         file_count: An integer that is the total number of processed
                     files.  Note that the number of skipped files is
                     included in this value.

    """

    def __init__(self, configuration):
        """Create a StyleChecker instance.

        Args:
          configuration: A StyleCheckerConfiguration instance that controls
                         the behavior of style checking.

        """
        self._configuration = configuration
        self.error_count = 0
        self.file_count = 0

    def _stderr_write(self, message):
        self._configuration.stderr_write(message)

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
            handle_style_error = DefaultStyleErrorHandler(
                                     configuration=self._configuration,
                                     file_path=file_path,
                                     increment_error_count=
                                         self._increment_error_count)
        if process_file is None:
            process_file = self._process_file

        self.file_count += 1

        dispatcher = ProcessorDispatcher()

        if dispatcher.should_skip_without_warning(file_path):
            return
        if dispatcher.should_skip_with_warning(file_path):
            self._stderr_write('Ignoring "%s": this file is exempt from the '
                               "style guide.\n" % file_path)
            return

        verbosity = self._configuration.verbosity
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
                                      self._configuration,
                                      self._increment_error_count)

            self.check_file(file_path, style_error_handler)
