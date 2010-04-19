# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2010 Chris Jerdonek (chris.jerdonek@gmail.com)
# Copyright (C) 2010 ProFUSION embedded systems
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
import logging
import os.path
import sys

from error_handlers import DefaultStyleErrorHandler
from filter import FilterConfiguration
from optparser import ArgumentParser
from optparser import DefaultCommandOptionValues
from processors.common import categories as CommonCategories
from processors.common import CarriageReturnProcessor
from processors.cpp import CppProcessor
from processors.python import PythonProcessor
from processors.text import TextProcessor
from webkitpy.style_references import parse_patch
from webkitpy.style_references import configure_logging as _configure_logging

_log = logging.getLogger("webkitpy.style.checker")

# These are default option values for the command-line option parser.
_DEFAULT_MIN_CONFIDENCE = 1
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
    # List Python pep8 categories last.
    #
    # Because much of WebKit's Python code base does not abide by the
    # PEP8 79 character limit, we ignore the 79-character-limit category
    # pep8/E501 for now.
    #
    # FIXME: Consider bringing WebKit's Python code base into conformance
    #        with the 79 character limit, or some higher limit that is
    #        agreeable to the WebKit project.
    '-pep8/E501',
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
    ([# Header files in ForwardingHeaders have no header guards or
      # exceptional header guards (e.g., WebCore_FWD_Debugger_h).
      "/ForwardingHeaders/"],
     ["-build/header_guard"]),

    # For third-party Python code, keep only the following checks--
    #
    #   No tabs: to avoid having to set the SVN allow-tabs property.
    #   No trailing white space: since this is easy to correct.
    #   No carriage-return line endings: since this is easy to correct.
    #
    (["webkitpy/thirdparty/"],
     ["-",
      "+pep8/W191",  # Tabs
      "+pep8/W291",  # Trailing white space
      "+whitespace/carriage_return"]),
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
    "LayoutTests/",
    ".pyc",
    ]


# The maximum number of errors to report per file, per category.
# If a category is not a key, then it has no maximum.
_MAX_REPORTS_PER_CATEGORY = {
    "whitespace/carriage_return": 1
}


def _all_categories():
    """Return the set of all categories used by check-webkit-style."""
    # Take the union across all processors.
    categories = CommonCategories.union(CppProcessor.categories)

    # FIXME: Consider adding all of the pep8 categories.  Since they
    #        are not too meaningful for documentation purposes, for
    #        now we add only the categories needed for the unit tests
    #        (which validate the consistency of the configuration
    #        settings against the known categories, etc).
    categories = categories.union(["pep8/W191", "pep8/W291", "pep8/E501"])

    return categories


def _check_webkit_style_defaults():
    """Return the default command-line options for check-webkit-style."""
    return DefaultCommandOptionValues(min_confidence=_DEFAULT_MIN_CONFIDENCE,
                                      output_format=_DEFAULT_OUTPUT_FORMAT)


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
               min_confidence=options.min_confidence,
               output_format=options.output_format,
               stderr_write=sys.stderr.write)


def _create_log_handlers(stream):
    """Create and return a default list of logging.Handler instances.

    Format WARNING messages and above to display the logging level, and
    messages strictly below WARNING not to display it.

    Args:
      stream: See the configure_logging() docstring.

    """
    # Handles logging.WARNING and above.
    error_handler = logging.StreamHandler(stream)
    error_handler.setLevel(logging.WARNING)
    formatter = logging.Formatter("%(levelname)s: %(message)s")
    error_handler.setFormatter(formatter)

    # Create a logging.Filter instance that only accepts messages
    # below WARNING (i.e. filters out anything WARNING or above).
    non_error_filter = logging.Filter()
    # The filter method accepts a logging.LogRecord instance.
    non_error_filter.filter = lambda record: record.levelno < logging.WARNING

    non_error_handler = logging.StreamHandler(stream)
    non_error_handler.addFilter(non_error_filter)
    formatter = logging.Formatter("%(message)s")
    non_error_handler.setFormatter(formatter)

    return [error_handler, non_error_handler]


def _create_debug_log_handlers(stream):
    """Create and return a list of logging.Handler instances for debugging.

    Args:
      stream: See the configure_logging() docstring.

    """
    handler = logging.StreamHandler(stream)
    formatter = logging.Formatter("%(name)s: %(levelname)-8s %(message)s")
    handler.setFormatter(formatter)

    return [handler]


def configure_logging(stream, logger=None, is_verbose=False):
    """Configure logging, and return the list of handlers added.

    Returns:
      A list of references to the logging handlers added to the root
      logger.  This allows the caller to later remove the handlers
      using logger.removeHandler.  This is useful primarily during unit
      testing where the caller may want to configure logging temporarily
      and then undo the configuring.

    Args:
      stream: A file-like object to which to log.  The stream must
              define an "encoding" data attribute, or else logging
              raises an error.
      logger: A logging.logger instance to configure.  This parameter
              should be used only in unit tests.  Defaults to the
              root logger.
      is_verbose: A boolean value of whether logging should be verbose.

    """
    # If the stream does not define an "encoding" data attribute, the
    # logging module can throw an error like the following:
    #
    # Traceback (most recent call last):
    #   File "/System/Library/Frameworks/Python.framework/Versions/2.6/...
    #         lib/python2.6/logging/__init__.py", line 761, in emit
    #     self.stream.write(fs % msg.encode(self.stream.encoding))
    # LookupError: unknown encoding: unknown
    if logger is None:
        logger = logging.getLogger()

    if is_verbose:
        logging_level = logging.DEBUG
        handlers = _create_debug_log_handlers(stream)
    else:
        logging_level = logging.INFO
        handlers = _create_log_handlers(stream)

    handlers = _configure_logging(logging_level=logging_level, logger=logger,
                                  handlers=handlers)

    return handlers


# Enum-like idiom
class FileType:

    NONE = 1
    # Alphabetize remaining types
    CPP = 2
    PYTHON = 3
    TEXT = 4


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
        elif file_extension == "py":
            return FileType.PYTHON
        elif ("ChangeLog" in file_path or
              (not file_extension and "WebKitTools/Scripts/" in file_path) or
              file_extension in self.text_file_extensions):
            return FileType.TEXT
        else:
            return FileType.NONE

    def _create_processor(self, file_type, file_path, handle_style_error,
                          min_confidence):
        """Instantiate and return a style processor based on file type."""
        if file_type == FileType.NONE:
            processor = None
        elif file_type == FileType.CPP:
            file_extension = self._file_extension(file_path)
            processor = CppProcessor(file_path, file_extension,
                                     handle_style_error, min_confidence)
        elif file_type == FileType.PYTHON:
            processor = PythonProcessor(file_path, handle_style_error)
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

    def dispatch_processor(self, file_path, handle_style_error, min_confidence):
        """Instantiate and return a style processor based on file path."""
        file_type = self._file_type(file_path)

        processor = self._create_processor(file_type,
                                           file_path,
                                           handle_style_error,
                                           min_confidence)
        return processor


# FIXME: Remove the stderr_write attribute from this class and replace
#        its use with calls to a logging module logger.
class StyleCheckerConfiguration(object):

    """Stores configuration values for the StyleChecker class.

    Attributes:
      min_confidence: An integer between 1 and 5 inclusive that is the
                      minimum confidence level of style errors to report.

      max_reports_per_category: The maximum number of errors to report
                                per category, per file.

      stderr_write: A function that takes a string as a parameter and
                    serves as stderr.write.

    """

    def __init__(self,
                 filter_configuration,
                 max_reports_per_category,
                 min_confidence,
                 output_format,
                 stderr_write):
        """Create a StyleCheckerConfiguration instance.

        Args:
          filter_configuration: A FilterConfiguration instance.  The default
                                is the "empty" filter configuration, which
                                means that all errors should be checked.

          max_reports_per_category: The maximum number of errors to report
                                    per category, per file.

          min_confidence: An integer between 1 and 5 inclusive that is the
                          minimum confidence level of style errors to report.
                          The default is 1, which reports all style errors.

          output_format: A string that is the output format.  The supported
                         output formats are "emacs" which emacs can parse
                         and "vs7" which Microsoft Visual Studio 7 can parse.

          stderr_write: A function that takes a string as a parameter and
                        serves as stderr.write.

        """
        self._filter_configuration = filter_configuration
        self._output_format = output_format

        self.max_reports_per_category = max_reports_per_category
        self.min_confidence = min_confidence
        self.stderr_write = stderr_write

    def is_reportable(self, category, confidence_in_error, file_path):
        """Return whether an error is reportable.

        An error is reportable if both the confidence in the error is
        at least the minimum confidence level and the current filter
        says the category should be checked for the given path.

        Args:
          category: A string that is a style category.
          confidence_in_error: An integer between 1 and 5 inclusive that is
                               the application's confidence in the error.
                               A higher number means greater confidence.
          file_path: The path of the file being checked

        """
        if confidence_in_error < self.min_confidence:
            return False

        return self._filter_configuration.should_check(category, file_path)

    def write_style_error(self,
                          category,
                          confidence_in_error,
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
                                           confidence_in_error))


class ProcessorBase(object):

    """The base class for processors of lists of lines."""

    def should_process(self, file_path):
        """Return whether the file at file_path should be processed."""
        raise NotImplementedError('Subclasses should implement.')

    def process(self, lines, file_path, **kwargs):
        """Process lines of text read from a file.

        Args:
          lines: A list of lines of text to process.
          file_path: The path from which the lines were read.
          **kwargs: This argument signifies that the process() method of
                    subclasses of ProcessorBase may support additional
                    keyword arguments.
                        For example, a style processor's process() method
                    may support a "reportable_lines" parameter that represents
                    the line numbers of the lines for which style errors
                    should be reported.

        """
        raise NotImplementedError('Subclasses should implement.')


# FIXME: Modify this class to start using the TextFileReader class in
#        webkitpy/style/filereader.py.  This probably means creating
#        a StyleProcessor class that inherits from ProcessorBase.
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

    def _increment_error_count(self):
        """Increment the total count of reported errors."""
        self.error_count += 1

    def _read_lines(self, file_path):
        """Read the file at a path, and return its lines.

        Raises:
          IOError: if the file does not exist or cannot be read.

        """
        # Support the UNIX convention of using "-" for stdin.
        if file_path == '-':
            file = codecs.StreamReaderWriter(sys.stdin,
                                             codecs.getreader('utf8'),
                                             codecs.getwriter('utf8'),
                                             'replace')
        else:
            # We do not open the file with universal newline support
            # (codecs does not support it anyway), so the resulting
            # lines contain trailing "\r" characters if we are reading
            # a file with CRLF endings.
            file = codecs.open(file_path, 'r', 'utf8', 'replace')

        contents = file.read()

        lines = contents.split("\n")
        return lines

    def _process_file(self, processor, file_path, handle_style_error):
        """Process the file using the given style processor."""
        try:
            lines = self._read_lines(file_path)
        except IOError:
            message = 'Could not read file. Skipping: "%s"' % file_path
            _log.warn(message)
            return

        # Check for and remove trailing carriage returns ("\r").
        #
        # FIXME: We should probably use the SVN "eol-style" property
        #        or a white list to decide whether or not to do
        #        the carriage-return check. Originally, we did the
        #        check only if (os.linesep != '\r\n').
        carriage_return_processor = CarriageReturnProcessor(handle_style_error)
        lines = carriage_return_processor.process(lines)

        processor.process(lines)

    def check_paths(self, paths, mock_check_file=None, mock_os=None):
        """Check style in the given files or directories.

        Args:
          paths: A list of file paths and directory paths.
          mock_check_file: A mock of self.check_file for unit testing.
          mock_os: A mock os for unit testing.

        """
        check_file = self.check_file if mock_check_file is None else \
                     mock_check_file
        os_module = os if mock_os is None else mock_os

        for path in paths:
            if os_module.path.isdir(path):
                self._check_directory(directory=path,
                                      check_file=check_file,
                                      mock_os_walk=os_module.walk)
            else:
                check_file(path)

    def _check_directory(self, directory, check_file, mock_os_walk=None):
        """Check style in all files in a directory, recursively.

        Args:
          directory: A path to a directory.
          check_file: The function to use in place of self.check_file().
          mock_os_walk: A mock os.walk for unit testing.

        """
        os_walk = os.walk if mock_os_walk is None else mock_os_walk

        for dir_path, dir_names, file_names in os_walk(directory):
            for file_name in file_names:
                file_path = os.path.join(dir_path, file_name)
                check_file(file_path)

    def check_file(self, file_path, line_numbers=None,
                   mock_handle_style_error=None,
                   mock_os_path_exists=None,
                   mock_process_file=None):
        """Check style in the given file.

        Args:
          file_path: The path of the file to process.  If possible, the path
                     should be relative to the source root.  Otherwise,
                     path-specific logic may not behave as expected.
          line_numbers: An array of line numbers of the lines for which
                        style errors should be reported, or None if errors
                        for all lines should be reported.  Normally, this
                        array contains the line numbers corresponding to the
                        modified lines of a patch.
          mock_handle_style_error: A unit-testing replacement for the function
                                   to call when a style error occurs. Defaults
                                   to a DefaultStyleErrorHandler instance.
          mock_os_path_exists: A unit-test replacement for os.path.exists.
                               This parameter should only be used for unit
                               tests.
          mock_process_file: The function to call to process the file. This
                             parameter should be used only for unit tests.
                             Defaults to the file processing method of this
                             class.

        """
        if mock_handle_style_error is None:
            increment = self._increment_error_count
            handle_style_error = DefaultStyleErrorHandler(
                                     configuration=self._configuration,
                                     file_path=file_path,
                                     increment_error_count=increment,
                                     line_numbers=line_numbers)
        else:
            handle_style_error = mock_handle_style_error

        os_path_exists = (os.path.exists if mock_os_path_exists is None else
                          mock_os_path_exists)
        process_file = (self._process_file if mock_process_file is None else
                        mock_process_file)

        if not os_path_exists(file_path):
            _log.warn("Skipping non-existent file: %s" % file_path)
            return

        _log.debug("Checking: " + file_path)

        self.file_count += 1

        dispatcher = ProcessorDispatcher()

        if dispatcher.should_skip_without_warning(file_path):
            return
        if dispatcher.should_skip_with_warning(file_path):
            _log.warn('File exempt from style guide. Skipping: "%s"'
                      % file_path)
            return

        min_confidence = self._configuration.min_confidence
        processor = dispatcher.dispatch_processor(file_path,
                                                  handle_style_error,
                                                  min_confidence)
        if processor is None:
            _log.debug('File not a recognized type to check. Skipping: "%s"'
                       % file_path)
            return

        _log.debug("Using class: " + processor.__class__.__name__)

        process_file(processor, file_path, handle_style_error)


class PatchChecker(object):

    """Supports checking style in patches."""

    def __init__(self, style_checker):
        """Create a PatchChecker instance.

        Args:
          style_checker: A StyleChecker instance.

        """
        self._file_checker = style_checker

    def check(self, patch_string):
        """Check style in the given patch."""
        patch_files = parse_patch(patch_string)

        # The diff variable is a DiffFile instance.
        for path, diff in patch_files.iteritems():
            line_numbers = set()
            for line in diff.lines:
                # When deleted line is not set, it means that
                # the line is newly added (or modified).
                if not line[0]:
                    line_numbers.add(line[1])

            _log.debug('Found %s new or modified lines in: %s'
                       % (len(line_numbers), path))

            self._file_checker.check_file(file_path=path,
                                          line_numbers=line_numbers)
