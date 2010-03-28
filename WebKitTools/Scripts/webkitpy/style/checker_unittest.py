#!/usr/bin/python
# -*- coding: utf-8; -*-
#
# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2009 Torch Mobile Inc.
# Copyright (C) 2009 Apple Inc. All rights reserved.
# Copyright (C) 2010 Chris Jerdonek (chris.jerdonek@gmail.com)
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

import logging
import os
import unittest

import checker as style
from webkitpy.style_references import LogTesting
from webkitpy.style_references import TestLogStream
from checker import _BASE_FILTER_RULES
from checker import _MAX_REPORTS_PER_CATEGORY
from checker import _PATH_RULES_SPECIFIER as PATH_RULES_SPECIFIER
from checker import _all_categories
from checker import check_webkit_style_configuration
from checker import check_webkit_style_parser
from checker import configure_logging
from checker import ProcessorDispatcher
from checker import StyleChecker
from checker import StyleCheckerConfiguration
from filter import validate_filter_rules
from filter import FilterConfiguration
from optparser import ArgumentParser
from optparser import CommandOptionValues
from processors.cpp import CppProcessor
from processors.text import TextProcessor


class ConfigureLoggingTestBase(unittest.TestCase):

    """Base class for testing configure_logging().

    Sub-classes should implement:

      is_debug: The is_debug parameter value to pass to configure_logging().

    """

    def setUp(self):
        is_debug = self.is_debug

        log_stream = TestLogStream(self)

        # Use a logger other than the root logger or one prefixed with
        # webkit so as not to conflict with test-webkitpy logging.
        logger = logging.getLogger("unittest")

        # Configure the test logger not to pass messages along to the
        # root logger.  This prevents test messages from being
        # propagated to loggers used by test-webkitpy logging (e.g.
        # the root logger).
        logger.propagate = False

        self._handlers = configure_logging(stream=log_stream, logger=logger,
                                           is_debug=is_debug)
        self._log = logger
        self._log_stream = log_stream

    def tearDown(self):
        """Reset logging to its original state.

        This method ensures that the logging configuration set up
        for a unit test does not affect logging in other unit tests.

        """
        logger = self._log
        for handler in self._handlers:
            logger.removeHandler(handler)

    def assert_log_messages(self, messages):
        """Assert that the logged messages equal the given messages."""
        self._log_stream.assertMessages(messages)


class ConfigureLoggingTest(ConfigureLoggingTestBase):

    """Tests the configure_logging() function."""

    is_debug = False

    def test_warning_message(self):
        self._log.warn("test message")
        self.assert_log_messages(["WARNING: test message\n"])

    def test_below_warning_message(self):
        # We test the boundary case of a logging level equal to 29.
        # In practice, we will probably only be calling log.info(),
        # which corresponds to a logging level of 20.
        level = logging.WARNING - 1  # Equals 29.
        self._log.log(level, "test message")
        self.assert_log_messages(["test message\n"])

    def test_debug_message(self):
        self._log.debug("test message")
        self.assert_log_messages([])

    def test_two_messages(self):
        self._log.info("message1")
        self._log.info("message2")
        self.assert_log_messages(["message1\n", "message2\n"])


class ConfigureLoggingDebugTest(ConfigureLoggingTestBase):

    """Tests the configure_logging() function for debugging."""

    is_debug = True

    def test_debug_message(self):
        self._log.debug("test message")
        self.assert_log_messages(["unittest: DEBUG    test message\n"])


class GlobalVariablesTest(unittest.TestCase):

    """Tests validity of the global variables."""

    def _all_categories(self):
        return _all_categories()

    def defaults(self):
        return style._check_webkit_style_defaults()

    def test_webkit_base_filter_rules(self):
        base_filter_rules = _BASE_FILTER_RULES
        defaults = self.defaults()
        already_seen = []
        validate_filter_rules(base_filter_rules, self._all_categories())
        # Also do some additional checks.
        for rule in base_filter_rules:
            # Check no leading or trailing white space.
            self.assertEquals(rule, rule.strip())
            # All categories are on by default, so defaults should
            # begin with -.
            self.assertTrue(rule.startswith('-'))
            # Check no rule occurs twice.
            self.assertFalse(rule in already_seen)
            already_seen.append(rule)

    def test_defaults(self):
        """Check that default arguments are valid."""
        default_options = self.defaults()

        # FIXME: We should not need to call parse() to determine
        #        whether the default arguments are valid.
        parser = ArgumentParser(all_categories=self._all_categories(),
                                base_filter_rules=[],
                                default_options=default_options)
        # No need to test the return value here since we test parse()
        # on valid arguments elsewhere.
        #
        # The default options are valid: no error or SystemExit.
        parser.parse(args=[], found_checkout=True)

    def test_path_rules_specifier(self):
        all_categories = self._all_categories()
        for (sub_paths, path_rules) in PATH_RULES_SPECIFIER:
            validate_filter_rules(path_rules, self._all_categories())

        config = FilterConfiguration(path_specific=PATH_RULES_SPECIFIER)

        def assertCheck(path, category):
            """Assert that the given category should be checked."""
            message = ('Should check category "%s" for path "%s".'
                       % (category, path))
            self.assertTrue(config.should_check(category, path))

        def assertNoCheck(path, category):
            """Assert that the given category should not be checked."""
            message = ('Should not check category "%s" for path "%s".'
                       % (category, path))
            self.assertFalse(config.should_check(category, path), message)

        assertCheck("random_path.cpp",
                    "build/include")
        assertNoCheck("WebKitTools/WebKitAPITest/main.cpp",
                      "build/include")
        assertNoCheck("WebKit/qt/QGVLauncher/main.cpp",
                      "build/include")
        assertNoCheck("WebKit/qt/QGVLauncher/main.cpp",
                      "readability/streams")

        assertCheck("random_path.cpp",
                    "readability/naming")
        assertNoCheck("WebKit/gtk/webkit/webkit.h",
                      "readability/naming")
        assertNoCheck("WebKitTools/DumpRenderTree/gtk/DumpRenderTree.cpp",
                      "readability/null")
        assertNoCheck("WebKit/efl/ewk/ewk_view.h",
                      "readability/naming")
        assertNoCheck("WebCore/css/CSSParser.cpp",
                      "readability/naming")
        assertNoCheck("WebKit/qt/tests/qwebelement/tst_qwebelement.cpp",
                      "readability/naming")
        assertNoCheck(
            "JavaScriptCore/qt/tests/qscriptengine/tst_qscriptengine.cpp",
            "readability/naming")
        assertNoCheck("WebCore/ForwardingHeaders/debugger/Debugger.h",
                      "build/header_guard")

        # Third-party Python code: webkitpy/thirdparty
        path = "WebKitTools/Scripts/webkitpy/thirdparty/mock.py"
        assertNoCheck(path, "build/include")
        assertNoCheck(path, "pep8/E401")  # A random pep8 category.
        assertCheck(path, "pep8/W191")
        assertCheck(path, "pep8/W291")
        assertCheck(path, "whitespace/carriage_return")

    def test_max_reports_per_category(self):
        """Check that _MAX_REPORTS_PER_CATEGORY is valid."""
        all_categories = self._all_categories()
        for category in _MAX_REPORTS_PER_CATEGORY.iterkeys():
            self.assertTrue(category in all_categories,
                            'Key "%s" is not a category' % category)


class CheckWebKitStyleFunctionTest(unittest.TestCase):

    """Tests the functions with names of the form check_webkit_style_*."""

    def test_check_webkit_style_configuration(self):
        # Exercise the code path to make sure the function does not error out.
        option_values = CommandOptionValues()
        configuration = check_webkit_style_configuration(option_values)

    def test_check_webkit_style_parser(self):
        # Exercise the code path to make sure the function does not error out.
        parser = check_webkit_style_parser()


class ProcessorDispatcherSkipTest(unittest.TestCase):

    """Tests the "should skip" methods of the ProcessorDispatcher class."""

    def test_should_skip_with_warning(self):
        """Test should_skip_with_warning()."""
        dispatcher = ProcessorDispatcher()

        # Check a non-skipped file.
        self.assertFalse(dispatcher.should_skip_with_warning("foo.txt"))

        # Check skipped files.
        paths_to_skip = [
           "gtk2drawing.c",
           "gtk2drawing.h",
           "JavaScriptCore/qt/api/qscriptengine_p.h",
           "WebCore/platform/gtk/gtk2drawing.c",
           "WebCore/platform/gtk/gtk2drawing.h",
           "WebKit/gtk/tests/testatk.c",
           "WebKit/qt/Api/qwebpage.h",
           "WebKit/qt/tests/qwebsecurityorigin/tst_qwebsecurityorigin.cpp",
            ]

        for path in paths_to_skip:
            self.assertTrue(dispatcher.should_skip_with_warning(path),
                            "Checking: " + path)

    def test_should_skip_without_warning(self):
        """Test should_skip_without_warning()."""
        dispatcher = ProcessorDispatcher()

        # Check a non-skipped file.
        self.assertFalse(dispatcher.should_skip_without_warning("foo.txt"))

        # Check skipped files.
        paths_to_skip = [
           # LayoutTests folder
           "LayoutTests/foo.txt",
            ]

        for path in paths_to_skip:
            self.assertTrue(dispatcher.should_skip_without_warning(path),
                            "Checking: " + path)


class ProcessorDispatcherDispatchTest(unittest.TestCase):

    """Tests dispatch_processor() method of ProcessorDispatcher class."""

    def mock_handle_style_error(self):
        pass

    def dispatch_processor(self, file_path):
        """Call dispatch_processor() with the given file path."""
        dispatcher = ProcessorDispatcher()
        processor = dispatcher.dispatch_processor(file_path,
                                                  self.mock_handle_style_error,
                                                  verbosity=3)
        return processor

    def assert_processor_none(self, file_path):
        """Assert that the dispatched processor is None."""
        processor = self.dispatch_processor(file_path)
        self.assertTrue(processor is None, 'Checking: "%s"' % file_path)

    def assert_processor(self, file_path, expected_class):
        """Assert the type of the dispatched processor."""
        processor = self.dispatch_processor(file_path)
        got_class = processor.__class__
        self.assertEquals(got_class, expected_class,
                          'For path "%(file_path)s" got %(got_class)s when '
                          "expecting %(expected_class)s."
                          % {"file_path": file_path,
                             "got_class": got_class,
                             "expected_class": expected_class})

    def assert_processor_cpp(self, file_path):
        """Assert that the dispatched processor is a CppProcessor."""
        self.assert_processor(file_path, CppProcessor)

    def assert_processor_text(self, file_path):
        """Assert that the dispatched processor is a TextProcessor."""
        self.assert_processor(file_path, TextProcessor)

    def test_cpp_paths(self):
        """Test paths that should be checked as C++."""
        paths = [
            "-",
            "foo.c",
            "foo.cpp",
            "foo.h",
            ]

        for path in paths:
            self.assert_processor_cpp(path)

        # Check processor attributes on a typical input.
        file_base = "foo"
        file_extension = "c"
        file_path = file_base + "." + file_extension
        self.assert_processor_cpp(file_path)
        processor = self.dispatch_processor(file_path)
        self.assertEquals(processor.file_extension, file_extension)
        self.assertEquals(processor.file_path, file_path)
        self.assertEquals(processor.handle_style_error, self.mock_handle_style_error)
        self.assertEquals(processor.verbosity, 3)
        # Check "-" for good measure.
        file_base = "-"
        file_extension = ""
        file_path = file_base
        self.assert_processor_cpp(file_path)
        processor = self.dispatch_processor(file_path)
        self.assertEquals(processor.file_extension, file_extension)
        self.assertEquals(processor.file_path, file_path)

    def test_text_paths(self):
        """Test paths that should be checked as text."""
        paths = [
           "ChangeLog",
           "foo.css",
           "foo.html",
           "foo.idl",
           "foo.js",
           "foo.mm",
           "foo.php",
           "foo.pm",
           "foo.py",
           "foo.txt",
           "FooChangeLog.bak",
           "WebCore/ChangeLog",
           "WebCore/inspector/front-end/inspector.js",
           "WebKitTools/Scripts/check-webkit=style",
           "WebKitTools/Scripts/modules/text_style.py",
            ]

        for path in paths:
            self.assert_processor_text(path)

        # Check processor attributes on a typical input.
        file_base = "foo"
        file_extension = "css"
        file_path = file_base + "." + file_extension
        self.assert_processor_text(file_path)
        processor = self.dispatch_processor(file_path)
        self.assertEquals(processor.file_path, file_path)
        self.assertEquals(processor.handle_style_error, self.mock_handle_style_error)

    def test_none_paths(self):
        """Test paths that have no file type.."""
        paths = [
           "Makefile",
           "foo.png",
           "foo.exe",
            ]

        for path in paths:
            self.assert_processor_none(path)


class StyleCheckerConfigurationTest(unittest.TestCase):

    """Tests the StyleCheckerConfiguration class."""

    def setUp(self):
        self._error_messages = []
        """The messages written to _mock_stderr_write() of this class."""

    def _mock_stderr_write(self, message):
        self._error_messages.append(message)

    def _style_checker_configuration(self, output_format="vs7"):
        """Return a StyleCheckerConfiguration instance for testing."""
        base_rules = ["-whitespace", "+whitespace/tab"]
        filter_configuration = FilterConfiguration(base_rules=base_rules)

        return StyleCheckerConfiguration(
                   filter_configuration=filter_configuration,
                   max_reports_per_category={"whitespace/newline": 1},
                   output_format=output_format,
                   stderr_write=self._mock_stderr_write,
                   verbosity=3)

    def test_init(self):
        """Test the __init__() method."""
        configuration = self._style_checker_configuration()

        # Check that __init__ sets the "public" data attributes correctly.
        self.assertEquals(configuration.max_reports_per_category,
                          {"whitespace/newline": 1})
        self.assertEquals(configuration.stderr_write, self._mock_stderr_write)
        self.assertEquals(configuration.verbosity, 3)

    def test_is_reportable(self):
        """Test the is_reportable() method."""
        config = self._style_checker_configuration()

        self.assertTrue(config.is_reportable("whitespace/tab", 3, "foo.txt"))

        # Test the confidence check code path by varying the confidence.
        self.assertFalse(config.is_reportable("whitespace/tab", 2, "foo.txt"))

        # Test the category check code path by varying the category.
        self.assertFalse(config.is_reportable("whitespace/line", 4, "foo.txt"))

    def _call_write_style_error(self, output_format):
        config = self._style_checker_configuration(output_format=output_format)
        config.write_style_error(category="whitespace/tab",
                                 confidence=5,
                                 file_path="foo.h",
                                 line_number=100,
                                 message="message")

    def test_write_style_error_emacs(self):
        """Test the write_style_error() method."""
        self._call_write_style_error("emacs")
        self.assertEquals(self._error_messages,
                          ["foo.h:100:  message  [whitespace/tab] [5]\n"])

    def test_write_style_error_vs7(self):
        """Test the write_style_error() method."""
        self._call_write_style_error("vs7")
        self.assertEquals(self._error_messages,
                          ["foo.h(100):  message  [whitespace/tab] [5]\n"])


class StyleCheckerTest(unittest.TestCase):

    """Test the StyleChecker class."""

    def _mock_stderr_write(self, message):
        pass

    def _style_checker(self, configuration):
        return StyleChecker(configuration)

    def test_init(self):
        """Test __init__ constructor."""
        configuration = StyleCheckerConfiguration(
                            filter_configuration=FilterConfiguration(),
                            max_reports_per_category={},
                            output_format="vs7",
                            stderr_write=self._mock_stderr_write,
                            verbosity=3)

        style_checker = self._style_checker(configuration)

        self.assertEquals(style_checker._configuration, configuration)
        self.assertEquals(style_checker.error_count, 0)
        self.assertEquals(style_checker.file_count, 0)


class StyleCheckerCheckFileTest(unittest.TestCase):

    """Test the check_file() method of the StyleChecker class.

    The check_file() method calls its process_file parameter when
    given a file that should not be skipped.

    The "got_*" attributes of this class are the parameters passed
    to process_file by calls to check_file() made by this test
    class. These attributes allow us to check the parameter values
    passed internally to the process_file function.

    Attributes:
      got_file_path: The file_path parameter passed by check_file()
                     to its process_file parameter.
      got_handle_style_error: The handle_style_error parameter passed
                              by check_file() to its process_file
                              parameter.
      got_processor: The processor parameter passed by check_file() to
                     its process_file parameter.
      warning_messages: A string containing all of the warning messages
                        written to the mock_stderr_write method of
                        this class.

    """
    def setUp(self):
        self._log = LogTesting.setUp(self)
        self.got_file_path = None
        self.got_handle_style_error = None
        self.got_processor = None
        self.warning_messages = ""

    def tearDown(self):
        self._log.tearDown()

    def mock_stderr_write(self, warning_message):
        self.warning_messages += warning_message

    def mock_handle_style_error(self):
        pass

    def mock_process_file(self, processor, file_path, handle_style_error):
        """A mock _process_file().

        See the documentation for this class for more information
        on this function.

        """
        self.got_file_path = file_path
        self.got_handle_style_error = handle_style_error
        self.got_processor = processor

    def assert_attributes(self,
                          expected_file_path,
                          expected_handle_style_error,
                          expected_processor,
                          expected_warning_messages):
        """Assert that the attributes of this class equal the given values."""
        self.assertEquals(self.got_file_path, expected_file_path)
        self.assertEquals(self.got_handle_style_error, expected_handle_style_error)
        self.assertEquals(self.got_processor, expected_processor)
        self.assertEquals(self.warning_messages, expected_warning_messages)

    def call_check_file(self, file_path):
        """Call the check_file() method of a test StyleChecker instance."""
        # Confirm that the attributes are reset.
        self.assert_attributes(None, None, None, "")

        configuration = StyleCheckerConfiguration(
                            filter_configuration=FilterConfiguration(),
                            max_reports_per_category={"whitespace/newline": 1},
                            output_format="vs7",
                            stderr_write=self.mock_stderr_write,
                            verbosity=3)

        style_checker = StyleChecker(configuration)

        style_checker.check_file(file_path,
                                 self.mock_handle_style_error,
                                 self.mock_process_file)

        self.assertEquals(1, style_checker.file_count)

    def test_check_file_on_skip_without_warning(self):
        """Test check_file() for a skipped-without-warning file."""

        file_path = "LayoutTests/foo.txt"

        dispatcher = ProcessorDispatcher()
        # Confirm that the input file is truly a skipped-without-warning file.
        self.assertTrue(dispatcher.should_skip_without_warning(file_path))

        # Check the outcome.
        self.call_check_file(file_path)
        self.assert_attributes(None, None, None, "")

    def test_check_file_on_skip_with_warning(self):
        """Test check_file() for a skipped-with-warning file."""

        file_path = "gtk2drawing.c"

        dispatcher = ProcessorDispatcher()
        # Check that the input file is truly a skipped-with-warning file.
        self.assertTrue(dispatcher.should_skip_with_warning(file_path))

        # Check the outcome.
        self.call_check_file(file_path)
        self.assert_attributes(None, None, None, "")
        self._log.assertMessages(["WARNING: File exempt from style guide. "
                                  'Skipping: "gtk2drawing.c"\n'])

    def test_check_file_on_non_skipped(self):

        # We use a C++ file since by using a CppProcessor, we can check
        # that all of the possible information is getting passed to
        # process_file (in particular, the verbosity).
        file_base = "foo"
        file_extension = "cpp"
        file_path = file_base + "." + file_extension

        dispatcher = ProcessorDispatcher()
        # Check that the input file is truly a C++ file.
        self.assertEquals(dispatcher._file_type(file_path), style.FileType.CPP)

        # Check the outcome.
        self.call_check_file(file_path)

        expected_processor = CppProcessor(file_path, file_extension, self.mock_handle_style_error, 3)

        self.assert_attributes(file_path,
                               self.mock_handle_style_error,
                               expected_processor,
                               "")


class StyleCheckerCheckPathsTest(unittest.TestCase):

    """Test the check_paths() method of the StyleChecker class."""

    class MockOs(object):

        class MockPath(object):

            """A mock os.path."""

            def isdir(self, path):
                return path == "directory"

        def __init__(self):
            self.path = self.MockPath()

        def walk(self, directory):
            """A mock of os.walk."""
            if directory == "directory":
                dirs = [("dir_path1", [], ["file1", "file2"]),
                        ("dir_path2", [], ["file3"])]
                return dirs
            return None

    def setUp(self):
        self._checked_files = []

    def _mock_check_file(self, file):
        self._checked_files.append(file)

    def test_check_paths(self):
        """Test StyleChecker.check_paths()."""
        checker = StyleChecker(configuration=None)
        mock_check_file = self._mock_check_file
        mock_os = self.MockOs()

        # Confirm that checked files is empty at the outset.
        self.assertEquals(self._checked_files, [])
        checker.check_paths(["path1", "directory"],
                            mock_check_file=mock_check_file,
                            mock_os=mock_os)
        self.assertEquals(self._checked_files,
                          ["path1",
                           os.path.join("dir_path1", "file1"),
                           os.path.join("dir_path1", "file2"),
                           os.path.join("dir_path2", "file3")])


if __name__ == '__main__':
    import sys

    unittest.main()
