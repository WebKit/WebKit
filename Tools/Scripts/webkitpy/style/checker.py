# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2010 Chris Jerdonek (chris.jerdonek@gmail.com)
# Copyright (C) 2010 ProFUSION embedded systems
# Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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

import logging
import os.path
import re

from checkers.common import categories as CommonCategories
from checkers.common import CarriageReturnChecker
from checkers.contributors import ContributorsChecker
from checkers.changelog import ChangeLogChecker
from checkers.cpp import CppChecker
from checkers.cmake import CMakeChecker
from checkers.featuredefines import FeatureDefinesChecker
from checkers.js import JSChecker
from checkers.jsonchecker import JSONChecker
from checkers.jsonchecker import JSONContributorsChecker
from checkers.jsonchecker import JSONFeaturesChecker
from checkers.jsonchecker import JSONCSSPropertiesChecker
from checkers.jstest import JSTestChecker
from checkers.messagesin import MessagesInChecker
from checkers.png import PNGChecker
from checkers.python import PythonChecker
from checkers.sdkvariant import SDKVariantChecker
from checkers.test_expectations import TestExpectationsChecker
from checkers.text import TextChecker
from checkers.watchlist import WatchListChecker
from checkers.xcodeproj import XcodeProjectFileChecker
from checkers.xml import XMLChecker
from error_handlers import DefaultStyleErrorHandler
from filter import FilterConfiguration
from optparser import ArgumentParser
from optparser import DefaultCommandOptionValues
from webkitpy.common.host import Host
from webkitpy.common.system.logutils import configure_logging as _configure_logging
from webkitpy.port.config import apple_additions


_log = logging.getLogger(__name__)


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

    # FIXME: Move the pylint rules from the pylintrc to here. This will
    # also require us to re-work lint-webkitpy to produce the equivalent output.
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

    ([  # TestNetscapePlugIn has no config.h and uses funny names like
      # NPP_SetWindow.
      os.path.join('Tools', 'DumpRenderTree', 'TestNetscapePlugIn')],
     ["-build/include",
      "-readability/naming"]),
    ([  # Ignore use of RetainPtr<NSObject *> for tests that ensure its compatibility with ReteainPtr<NSObject>.
      os.path.join('Tools', 'TestWebKitAPI', 'Tests', 'WTF', 'ns', 'RetainPtr.mm')],
     ["-runtime/retainptr"]),
    ([  # There is no clean way to avoid "yy_*" names used by flex.
      os.path.join('Source', 'WebCore', 'css', 'CSSParser.cpp'),
      # TestWebKitAPI uses funny macros like EXPECT_WK_STREQ.
      os.path.join('Tools', 'TestWebKitAPI')],
     ["-readability/naming"]),

    ([
      # The GTK+ and WPE APIs use upper case, underscore separated, words in
      # certain types of enums (e.g. signals, properties).
      os.path.join('Source', 'JavaScriptCore', 'API', 'glib'),
      os.path.join('Source', 'WebKit', 'Shared', 'API', 'glib'),
      os.path.join('Source', 'WebKit', 'UIProcess', 'API', 'glib'),
      os.path.join('Source', 'WebKit', 'UIProcess', 'API', 'gtk'),
      os.path.join('Source', 'WebKit', 'UIProcess', 'API', 'wpe'),
      os.path.join('Source', 'WebKit', 'WebProcess', 'InjectedBundle', 'API', 'glib'),
      os.path.join('Source', 'WebKit', 'WebProcess', 'InjectedBundle', 'API', 'gtk'),
      os.path.join('Source', 'WebKit', 'WebProcess', 'InjectedBundle', 'API', 'wpe')],
     ["-readability/enum_casing"]),

    ([
      # To use GStreamer GL without conflicts of GL symbols,
      # we should include gst/gl/gl.h before including OpenGL[ES]Shims
      os.path.join('Source', 'WebCore', 'platform', 'graphics', 'gstreamer', 'MediaPlayerPrivateGStreamerBase.cpp')],
     ["-build/include_order"]),

    ([
      # Header files in ForwardingHeaders have no header guards or
      # exceptional header guards (e.g., WebCore_FWD_Debugger_h).
      os.path.join(os.path.sep, 'ForwardingHeaders')],
     ["-build/header_guard"]),
    ([
      # Assembler has lots of opcodes that use underscores, so
      # we don't check for underscores in that directory.
      os.path.join('Source', 'JavaScriptCore', 'assembler'),
      os.path.join('Source', 'JavaScriptCore', 'jit', 'JIT')],
     ["-readability/naming/underscores"]),
    ([  # JITStubs has an usual syntax which causes false alarms for a few checks.
      os.path.join('JavaScriptCore', 'jit', 'JITStubs.cpp')],
     ["-readability/parameter_name",
      "-whitespace/parens"]),

    # WebKit rules:
    # WebKit and certain directories have idiosyncracies.
    ([  # NPAPI has function names with underscores.
      os.path.join('Source', 'WebKit', 'WebProcess', 'Plugins', 'Netscape')],
     ["-readability/naming"]),
    ([
      # The WebKit C API has names with underscores and whitespace-aligned
      # struct members. Also, we allow unnecessary parameter names in
      # WebKit APIs because we're matching CF's header style.
      # Additionally, we use word which starts with non-capital letter 'k'
      # for types of enums.
      os.path.join('Source', 'WebKit', 'UIProcess', 'API', 'C'),
      os.path.join('Source', 'WebKit', 'Shared', 'API', 'c'),
      os.path.join('Source', 'WebKit', 'WebProcess', 'InjectedBundle', 'API', 'c')],
     ["-readability/enum_casing",
      "-readability/naming",
      "-readability/parameter_name",
      "-whitespace/declaration"]),
    ([
      # These files define GObjects, which implies some definitions of
      # variables and functions containing underscores.
      os.path.join('Source', 'WebCore', 'platform', 'graphics', 'gstreamer', 'VideoSinkGStreamer.cpp'),
      os.path.join('Source', 'WebCore', 'platform', 'graphics', 'gstreamer', 'WebKitWebSourceGStreamer.cpp'),
      os.path.join('Source', 'WebCore', 'platform', 'audio', 'gstreamer', 'WebKitWebAudioSourceGStreamer.cpp'),
      os.path.join('Source', 'WebCore', 'platform', 'mediastream', 'gstreamer', 'GStreamerMediaStreamSource.h'),
      os.path.join('Source', 'WebCore', 'platform', 'mediastream', 'gstreamer', 'GStreamerMediaStreamSource.cpp'),
      os.path.join('Source', 'WebCore', 'platform', 'network', 'soup', 'ProxyResolverSoup.cpp'),
      os.path.join('Source', 'WebCore', 'platform', 'network', 'soup', 'ProxyResolverSoup.h')],
     ["-readability/naming",
      "-readability/enum_casing"]),

    # For third-party code, keep only the following checks--
    #
    #   No tabs: to avoid having to set the SVN allow-tabs property.
    #   No trailing white space: since this is easy to correct.
    #   No carriage-return line endings: since this is easy to correct.
    #
    ([os.path.join('webkitpy', 'thirdparty'),
      os.path.join('Source', 'ThirdParty', 'ANGLE'),
      os.path.join('Source', 'ThirdParty', 'libwebrtc'),
      os.path.join('Source', 'ThirdParty', 'openvr'),
      os.path.join('Source', 'ThirdParty', 'xdgmime'),
      os.path.join('Tools', 'WebGPUAPIStructure')],
     ["-",
      "+pep8/W191",  # Tabs
      "+pep8/W291",  # Trailing white space
      "+whitespace/carriage_return"]),

    ([  # Source/JavaScriptCore/disassembler/udis86/ is generated code.
      os.path.join('Source', 'JavaScriptCore', 'disassembler', 'udis86')],
     ["-readability/naming/underscores",
      "-whitespace/declaration",
      "-whitespace/indent"]),

    ([  # Files following GStreamer coding style (for a simpler upstreaming process for example)
      os.path.join('Source', 'WebCore', 'platform', 'mediastream', 'libwebrtc', 'GStreamerVideoEncoder.cpp'),
      os.path.join('Source', 'WebCore', 'platform', 'mediastream', 'libwebrtc', 'GStreamerVideoEncoder.h'),
     ],
     ["-whitespace/indent",
      "-whitespace/declaration",
      "-whitespace/parens",
      "-readability/null",
      "-whitespace/braces",
      "-readability/naming/underscores",
      "-readability/enum_casing",
     ]),

    ([
      # There is no way to avoid the symbols __jit_debug_register_code
      # and __jit_debug_descriptor when integrating with gdb.
      os.path.join('Source', 'JavaScriptCore', 'jit', 'GDBInterface.cpp')],
     ["-readability/naming"]),

    ([  # On some systems the trailing CR is causing parser failure.
      os.path.join('Source', 'JavaScriptCore', 'parser', 'Keywords.table')],
     ["+whitespace/carriage_return"]),

    ([  # DataDetectorsCoreSPI.h declares enum bitfields as CFIndex.
      os.path.join('Source', 'WebCore', 'PAL', 'pal', 'spi', 'cocoa', 'DataDetectorsCoreSPI.h')],
     ["-runtime/enum_bitfields"]),

    ([
      # PassKitSPI.h imports "PassKit.h" at two lines depending on the build configuration,
      # which causes a false positive error.
      os.path.join('Source', 'WebCore', 'PAL', 'pal', 'spi', 'cocoa', 'PassKitSPI.h')],
     ["-build/include"]),

    ([  # libwebrtc source and some SPI headers have identifier names with underscores.
      os.path.join('Source', 'ThirdParty', 'libwebrtc', 'Source', 'webrtc'),
      os.path.join('Source', 'WebCore', 'PAL', 'pal', 'spi')],
     ["-readability/naming/underscores"]),
]


_CPP_FILE_EXTENSIONS = [
    'c',
    'cpp',
    'h',
    'mm',
    'm',
    ]

_JS_FILE_EXTENSION = 'js'

_JSON_FILE_EXTENSION = 'json'

_PYTHON_FILE_EXTENSION = 'py'

_TEXT_FILE_EXTENSIONS = [
    'ac',
    'cc',
    'cgi',
    'css',
    'exp',
    'flex',
    'gyp',
    'gypi',
    'html',
    'idl',
    'in',
    'php',
    'pl',
    'pm',
    'pri',
    'pro',
    'rb',
    'sh',
    'table',
    'txt',
    'wm',
    'xhtml',
    'y',
    ]

_XCODEPROJ_FILE_EXTENSION = 'pbxproj'

_XML_FILE_EXTENSIONS = [
    'vcproj',
    'vsprops',
    ]

_PNG_FILE_EXTENSION = 'png'

_CMAKE_FILE_EXTENSION = 'cmake'

# Files that are never skipped by name.
#
# Do not skip these files, even when they appear in
# _SKIPPED_FILES_WITH_WARNING or _SKIPPED_FILES_WITHOUT_WARNING.
_NEVER_SKIPPED_JS_FILES = [
    'js-test-pre.js',
    'js-test-post.js',
    'js-test-post-async.js',
    'standalone-pre.js',
]

_NEVER_SKIPPED_FILES = _NEVER_SKIPPED_JS_FILES + [
    'TestExpectations',
]

# Files to skip that are less obvious.
#
# Some files should be skipped when checking style. For example,
# WebKit maintains some files in Mozilla style on purpose to ease
# future merges.
_SKIPPED_FILES_WITH_WARNING = [
    os.path.join('Tools', 'TestWebKitAPI', 'Tests', 'WebKitGtk'),

    # WebKit*.h and JSC*.h files in, except those ending in Private.h are API headers, which do not follow WebKit coding style.
    re.compile(re.escape(os.path.join('Source', 'JavaScriptCore', 'API', 'glib') + os.path.sep) + r'JSC(?!.*Private\.h).*\.h$'),
    re.compile(re.escape(os.path.join('Source', 'WebKit', 'UIProcess', 'API', 'gtk') + os.path.sep) + r'WebKit(?!.*Private\.h).*\.h$'),
    re.compile(re.escape(os.path.join('Source', 'WebKit', 'UIProcess', 'API', 'wpe') + os.path.sep) + r'WebKit(?!.*Private\.h).*\.h$'),
    re.compile(re.escape(os.path.join('Source', 'WebKit', 'WebProcess', 'InjectedBundle', 'API', 'gtk') + os.path.sep) + r'WebKit(?!.*Private\.h).*\.h$'),
    re.compile(re.escape(os.path.join('Source', 'WebKit', 'WebProcess', 'InjectedBundle', 'API', 'wpe') + os.path.sep) + r'WebKit(?!.*Private\.h).*\.h$'),
    re.compile(re.escape(os.path.join('Source', 'WebKit', 'WebProcess', 'InjectedBundle', 'API', 'wpe', 'DOM') + os.path.sep) + r'WebKit(?!.*Private\.h).*\.h$'),

    # GObject DOM bindings copied from generated code using different coding style.
    os.path.join('Source', 'WebKit', 'WebProcess', 'InjectedBundle', 'API', 'gtk', 'DOM'),

    os.path.join('Source', 'JavaScriptCore', 'API', 'glib', 'jsc.h'),
    os.path.join('Source', 'WebKit', 'UIProcess', 'API', 'gtk', 'webkit2.h'),
    os.path.join('Source', 'WebKit', 'UIProcess', 'API', 'wpe', 'webkit.h'),
    os.path.join('Source', 'WebKit', 'WebProcess', 'InjectedBundle', 'API', 'gtk', 'webkit-web-extension.h'),
    os.path.join('Source', 'WebKit', 'WebProcess', 'InjectedBundle', 'API', 'wpe', 'webkit-web-extension.h'),
    os.path.join('Source', 'WebKit', 'WebProcess', 'InjectedBundle', 'API', 'wpe', 'DOM', 'webkitdom.h')]

# Files to skip that are more common or obvious.
#
# This list should be in addition to files with FileType.NONE.  Files
# with FileType.NONE are automatically skipped without warning.
_SKIPPED_FILES_WITHOUT_WARNING = [
    "LayoutTests" + os.path.sep,

    "WebDriverTests" + os.path.sep,

    # Files generated by the bindings script should not be checked for style.
    os.path.join('Source', 'WebCore', 'bindings', 'scripts', 'test'),

    # ICU headers are imported.
    os.path.join('Source', 'JavaScriptCore', 'icu'),
    os.path.join('Source', 'WebCore', 'icu'),
    os.path.join('Source', 'WebKitLegacy', 'mac', 'icu'),
    os.path.join('Source', 'WTF', 'icu'),
    ]

# Extensions of files which are allowed to contain carriage returns.
_CARRIAGE_RETURN_ALLOWED_FILE_EXTENSIONS = [
    'png',
    'vcproj',
    'vsprops',
    ]

# The maximum number of errors to report per file, per category.
# If a category is not a key, then it has no maximum.
_MAX_REPORTS_PER_CATEGORY = {
    "whitespace/carriage_return": 1
}


def _all_categories():
    """Return the set of all categories used by check-webkit-style."""
    # Take the union across all checkers.
    categories = CommonCategories.union(CppChecker.categories)
    categories = categories.union(JSChecker.categories)
    categories = categories.union(JSONChecker.categories)
    categories = categories.union(JSTestChecker.categories)
    categories = categories.union(TestExpectationsChecker.categories)
    categories = categories.union(ChangeLogChecker.categories)
    categories = categories.union(PNGChecker.categories)
    categories = categories.union(FeatureDefinesChecker.categories)
    categories = categories.union(SDKVariantChecker.categories)

    # FIXME: Consider adding all of the pep8 categories.  Since they
    #        are not too meaningful for documentation purposes, for
    #        now we add only the categories needed for the unit tests
    #        (which validate the consistency of the configuration
    #        settings against the known categories, etc).
    categories = categories.union(["pep8/W191", "pep8/W291", "pep8/E501"])

    if apple_additions():
        categories = categories.union(apple_additions().all_categories())

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
    """Return a StyleProcessorConfiguration instance for check-webkit-style.

    Args:
      options: A CommandOptionValues instance.

    """
    filter_configuration = FilterConfiguration(
                               base_rules=_BASE_FILTER_RULES,
                               path_specific=_PATH_RULES_SPECIFIER,
                               user_rules=options.filter_rules)

    return StyleProcessorConfiguration(filter_configuration=filter_configuration,
               max_reports_per_category=_MAX_REPORTS_PER_CATEGORY,
               min_confidence=options.min_confidence,
               output_format=options.output_format,
               commit_queue=options.commit_queue)


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

    NONE = 0  # FileType.NONE evaluates to False.
    # Alphabetize remaining types
    CHANGELOG = 1
    CPP = 2
    JS = 3
    JSON = 4
    PNG = 5
    PYTHON = 6
    TEXT = 7
    WATCHLIST = 8
    XML = 9
    XCODEPROJ = 10
    CMAKE = 11
    FEATUREDEFINES = 12
    SDKVARIANT = 13


class CheckerDispatcher(object):

    """Supports determining whether and how to check style, based on path."""

    def _file_extension(self, file_path):
        """Return the file extension without the leading dot."""
        return os.path.splitext(file_path)[1].lstrip(".")

    def _should_skip_file_path(self, file_path, skip_array_entry):
        match = re.search("\s*png$", file_path)
        if match:
            return False
        if isinstance(skip_array_entry, str):
            if file_path.find(skip_array_entry) >= 0:
                return True
        elif skip_array_entry.match(file_path):
                return True
        return False

    def should_skip_with_warning(self, file_path):
        """Return whether the given file should be skipped with a warning."""
        for skipped_file in _SKIPPED_FILES_WITH_WARNING:
            if self._should_skip_file_path(file_path, skipped_file):
                return True
        return False

    def should_skip_without_warning(self, file_path):
        """Return whether the given file should be skipped without a warning."""
        if not self._file_type(file_path):  # FileType.NONE.
            return True
        # Since "LayoutTests" is in _SKIPPED_FILES_WITHOUT_WARNING, make
        # an exception to prevent files like "LayoutTests/ChangeLog" and
        # "LayoutTests/ChangeLog-2009-06-16" from being skipped.
        # Files like 'TestExpectations' are also should not be skipped.
        #
        # FIXME: Figure out a good way to avoid having to add special logic
        #        for this special case.
        basename = os.path.basename(file_path)
        if basename.startswith('ChangeLog'):
            return False
        elif basename in _NEVER_SKIPPED_FILES:
            return False
        for skipped_file in _SKIPPED_FILES_WITHOUT_WARNING:
            if self._should_skip_file_path(file_path, skipped_file):
                return True
        return False

    def should_check_and_strip_carriage_returns(self, file_path):
        return self._file_extension(file_path) not in _CARRIAGE_RETURN_ALLOWED_FILE_EXTENSIONS

    def _file_type(self, file_path):
        """Return the file type corresponding to the given file."""
        file_extension = self._file_extension(file_path)

        if (file_extension in _CPP_FILE_EXTENSIONS) or (file_path == '-'):
            # FIXME: Do something about the comment below and the issue it
            #        raises since cpp_style already relies on the extension.
            #
            # Treat stdin as C++. Since the extension is unknown when
            # reading from stdin, cpp_style tests should not rely on
            # the extension.
            return FileType.CPP
        elif file_extension == _JS_FILE_EXTENSION:
            return FileType.JS
        elif file_extension == _JSON_FILE_EXTENSION:
            return FileType.JSON
        elif file_extension == _PYTHON_FILE_EXTENSION:
            return FileType.PYTHON
        elif file_extension in _XML_FILE_EXTENSIONS:
            return FileType.XML
        elif os.path.basename(file_path).startswith('ChangeLog'):
            return FileType.CHANGELOG
        elif os.path.basename(file_path) == 'watchlist':
            return FileType.WATCHLIST
        elif file_extension == _XCODEPROJ_FILE_EXTENSION:
            return FileType.XCODEPROJ
        elif file_extension == _PNG_FILE_EXTENSION:
            return FileType.PNG
        elif ((file_extension == _CMAKE_FILE_EXTENSION) or os.path.basename(file_path) == 'CMakeLists.txt'):
            return FileType.CMAKE
        elif ((not file_extension and os.path.join("Tools", "Scripts") in file_path) or
              file_extension in _TEXT_FILE_EXTENSIONS or os.path.basename(file_path) == 'TestExpectations'):
            return FileType.TEXT
        elif os.path.basename(file_path) == "FeatureDefines.xcconfig":
            return FileType.FEATUREDEFINES
        elif os.path.basename(file_path) == "SDKVariant.xcconfig":
            return FileType.SDKVARIANT
        else:
            return FileType.NONE

    def _create_checker(self, file_type, file_path, handle_style_error,
                        min_confidence, commit_queue):
        """Instantiate and return a style checker based on file type."""
        if file_type == FileType.NONE:
            checker = None
        elif file_type == FileType.CHANGELOG:
            should_line_be_checked = None
            if handle_style_error:
                should_line_be_checked = handle_style_error.should_line_be_checked
            checker = ChangeLogChecker(file_path, handle_style_error, should_line_be_checked)
        elif file_type == FileType.CPP:
            file_extension = self._file_extension(file_path)
            checker = CppChecker(file_path, file_extension,
                                 handle_style_error, min_confidence)
        elif file_type == FileType.JS:
            basename = os.path.basename(file_path)
            # Do not attempt to check non-Inspector or 3rd-party JavaScript files as JS.
            if os.path.join('WebInspectorUI', 'UserInterface') in file_path and (not 'External' in file_path):
                checker = JSChecker(file_path, handle_style_error)
            elif basename in _NEVER_SKIPPED_JS_FILES:
                checker = JSTestChecker(file_path, handle_style_error)
            else:
                checker = TextChecker(file_path, handle_style_error)
        elif file_type == FileType.JSON:
            basename = os.path.basename(file_path)
            if basename == 'contributors.json':
                if commit_queue:
                    checker = JSONContributorsChecker(file_path, handle_style_error)
                else:
                    checker = ContributorsChecker(file_path, handle_style_error)
            elif basename == 'features.json':
                checker = JSONFeaturesChecker(file_path, handle_style_error)
            elif basename == 'CSSProperties.json':
                checker = JSONCSSPropertiesChecker(file_path, handle_style_error)
            else:
                checker = JSONChecker(file_path, handle_style_error)
        elif file_type == FileType.PYTHON:
            if apple_additions():
                checker = apple_additions().python_checker(file_path, handle_style_error)
            else:
                checker = PythonChecker(file_path, handle_style_error)
        elif file_type == FileType.XML:
            checker = XMLChecker(file_path, handle_style_error)
        elif file_type == FileType.XCODEPROJ:
            checker = XcodeProjectFileChecker(file_path, handle_style_error)
        elif file_type == FileType.PNG:
            checker = PNGChecker(file_path, handle_style_error)
        elif file_type == FileType.CMAKE:
            checker = CMakeChecker(file_path, handle_style_error)
        elif file_type == FileType.TEXT:
            basename = os.path.basename(file_path)
            if basename == 'TestExpectations':
                checker = TestExpectationsChecker(file_path, handle_style_error)
            elif file_path.endswith('.messages.in'):
                checker = MessagesInChecker(file_path, handle_style_error)
            else:
                checker = TextChecker(file_path, handle_style_error)
        elif file_type == FileType.WATCHLIST:
            checker = WatchListChecker(file_path, handle_style_error)
        elif file_type == FileType.FEATUREDEFINES:
            checker = FeatureDefinesChecker(file_path, handle_style_error)
        elif file_type == FileType.SDKVARIANT:
            checker = SDKVariantChecker(file_path, handle_style_error)
        else:
            raise ValueError('Invalid file type "%(file_type)s": the only valid file types '
                             "are %(NONE)s, %(CPP)s, and %(TEXT)s."
                             % {"file_type": file_type,
                                "NONE": FileType.NONE,
                                "CPP": FileType.CPP,
                                "TEXT": FileType.TEXT})

        return checker

    def dispatch(self, file_path, handle_style_error, min_confidence, commit_queue):
        """Instantiate and return a style checker based on file path."""
        file_type = self._file_type(file_path)

        checker = self._create_checker(file_type,
                                       file_path,
                                       handle_style_error,
                                       min_confidence,
                                       commit_queue)
        return checker


class StyleProcessorConfiguration(object):

    """Stores configuration values for the StyleProcessor class.

    Attributes:
      min_confidence: An integer between 1 and 5 inclusive that is the
                      minimum confidence level of style errors to report.

      max_reports_per_category: The maximum number of errors to report
                                per category, per file.

    """

    def __init__(self,
                 filter_configuration,
                 max_reports_per_category,
                 min_confidence,
                 output_format,
                 commit_queue):
        """Create a StyleProcessorConfiguration instance.

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

          commit_queue: A bool indicating whether the style check is performed
                        by the commit queue or not.

        """
        self._filter_configuration = filter_configuration
        self._output_format = output_format

        self.max_reports_per_category = max_reports_per_category
        self.min_confidence = min_confidence
        self.commit_queue = commit_queue

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
            format_string = "%s(%s):  %s  [%s] [%d]"
        else:
            format_string = "%s:%s:  %s  [%s] [%d]"

        _log.error(format_string % (file_path,
                                           line_number,
                                           message,
                                           category,
                                           confidence_in_error))


class ProcessorBase(object):

    """The base class for processors of lists of lines."""

    def should_process(self, file_path):
        """Return whether the file at file_path should be processed.

        The TextFileReader class calls this method prior to reading in
        the lines of a file.  Use this method, for example, to prevent
        the style checker from reading binary files into memory.

        """
        raise NotImplementedError('Subclasses should implement.')

    def process(self, lines, file_path, **kwargs):
        """Process lines of text read from a file.

        Args:
          lines: A list of lines of text to process.
          file_path: The path from which the lines were read.
          **kwargs: This argument signifies that the process() method of
                    subclasses of ProcessorBase may support additional
                    keyword arguments.
                        For example, a style checker's check() method
                    may support a "reportable_lines" parameter that represents
                    the line numbers of the lines for which style errors
                    should be reported.

        """
        raise NotImplementedError('Subclasses should implement.')

    def do_association_check(self, files, cwd, host=Host()):
        """It may be the case that changes in a file cause style errors to
        occur elsewhere in the modified file or in other files (most notably,
        this is the case for the test expectations linter). The association check
        is designed to take find issues which require parsing of files outside
        those provided to the processor.

        Args:
            files: A dictionary of file names to lists of lines modified in the provided
            file.  Files which have been removed will also be in the dictionary, they
            will map to 'None'
            cwd: Current working directory, assumed to be the root of the scm repository
            host: Current host for testing
        """
        raise NotImplementedError('Subclasses should implement.')


class StyleProcessor(ProcessorBase):

    """A ProcessorBase for checking style.

    Attributes:
      error_count: An integer that is the total number of reported
                   errors for the lifetime of this instance.

    """

    def __init__(self, configuration, mock_dispatcher=None,
                 mock_increment_error_count=None,
                 mock_carriage_checker_class=None):
        """Create an instance.

        Args:
          configuration: A StyleProcessorConfiguration instance.
          mock_dispatcher: A mock CheckerDispatcher instance.  This
                           parameter is for unit testing.  Defaults to a
                           CheckerDispatcher instance.
          mock_increment_error_count: A mock error-count incrementer.
          mock_carriage_checker_class: A mock class for checking and
                                       transforming carriage returns.
                                       This parameter is for unit testing.
                                       Defaults to CarriageReturnChecker.

        """
        if mock_dispatcher is None:
            dispatcher = CheckerDispatcher()
        else:
            dispatcher = mock_dispatcher

        if mock_increment_error_count is None:
            # The following blank line is present to avoid flagging by pep8.py.

            def increment_error_count():
                """Increment the total count of reported errors."""
                self.error_count += 1
        else:
            increment_error_count = mock_increment_error_count

        if mock_carriage_checker_class is None:
            # This needs to be a class rather than an instance since the
            # process() method instantiates one using parameters.
            carriage_checker_class = CarriageReturnChecker
        else:
            carriage_checker_class = mock_carriage_checker_class

        self.error_count = 0

        self._carriage_checker_class = carriage_checker_class
        self._configuration = configuration
        self._dispatcher = dispatcher
        self._increment_error_count = increment_error_count

    def should_process(self, file_path):
        """Return whether the file should be checked for style."""
        if self._dispatcher.should_skip_without_warning(file_path):
            return False
        if self._dispatcher.should_skip_with_warning(file_path):
            _log.warn('File exempt from style guide. Skipping: "%s"'
                      % file_path)
            return False
        return True

    def process(self, lines, file_path, line_numbers=None):
        """Check the given lines for style.

        Arguments:
          lines: A list of all lines in the file to check.
          file_path: The path of the file to process.  If possible, the path
                     should be relative to the source root.  Otherwise,
                     path-specific logic may not behave as expected.
          line_numbers: A list of line numbers of the lines for which
                        style errors should be reported, or None if errors
                        for all lines should be reported.  When not None, this
                        list normally contains the line numbers corresponding
                        to the modified lines of a patch.

        """
        _log.debug("Checking style: " + file_path)

        style_error_handler = DefaultStyleErrorHandler(
            configuration=self._configuration,
            file_path=file_path,
            increment_error_count=self._increment_error_count,
            line_numbers=line_numbers)

        carriage_checker = self._carriage_checker_class(style_error_handler)

        # Check for and remove trailing carriage returns ("\r").
        if self._dispatcher.should_check_and_strip_carriage_returns(file_path):
            lines = carriage_checker.check(lines)

        min_confidence = self._configuration.min_confidence
        checker = self._dispatcher.dispatch(file_path,
                                            style_error_handler,
                                            min_confidence,
                                            self._configuration.commit_queue)

        if checker is None:
            raise AssertionError("File should not be checked: '%s'" % file_path)

        _log.debug("Using class: " + checker.__class__.__name__)

        checker.check(lines)

    def do_association_check(self, files, cwd, host=Host()):
        _log.debug("Running TestExpectations linter")
        TestExpectationsChecker.lint_test_expectations(files, self._configuration, cwd, self._increment_error_count, host=host)
