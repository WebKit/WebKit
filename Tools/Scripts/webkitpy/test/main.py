# Copyright (C) 2012 Google, Inc.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""unit testing code for webkitpy."""

import logging
import optparse
import os
import StringIO
import sys
import traceback
import unittest

from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.system import outputcapture
from webkitpy.test.test_finder import TestFinder
from webkitpy.test.runner import TestRunner

_log = logging.getLogger(__name__)


class Tester(object):
    def __init__(self, filesystem=None):
        self.finder = TestFinder(filesystem or FileSystem())
        self.stream = sys.stderr

    def add_tree(self, top_directory, starting_subdirectory=None):
        self.finder.add_tree(top_directory, starting_subdirectory)

    def _parse_args(self):
        parser = optparse.OptionParser(usage='usage: %prog [options] [args...]')
        parser.add_option('-a', '--all', action='store_true', default=False,
                          help='run all the tests'),
        parser.add_option('-c', '--coverage', action='store_true', default=False,
                          help='generate code coverage info (requires http://pypi.python.org/pypi/coverage)'),
        parser.add_option('-q', '--quiet', action='store_true', default=False,
                          help='run quietly (errors, warnings, and progress only)'),
        parser.add_option('-t', '--timing', action='store_true', default=False,
                          help='display per-test execution time (implies --verbose)'),
        parser.add_option('-v', '--verbose', action='count', default=0,
                          help='verbose output (specify once for individual test results, twice for debug messages)')
        parser.add_option('--skip-integrationtests', action='store_true', default=False,
                          help='do not run the integration tests')
        parser.add_option('-p', '--pass-through', action='store_true', default=False,
                          help='be debugger friendly by passing captured output through to the system')

        parser.epilog = ('[args...] is an optional list of modules, test_classes, or individual tests. '
                         'If no args are given, all the tests will be run.')

        return parser.parse_args()

    def _configure(self, options):
        self._options = options

        if options.timing:
            # --timing implies --verbose
            options.verbose = max(options.verbose, 1)

        log_level = logging.INFO
        if options.quiet:
            log_level = logging.WARNING
        elif options.verbose == 2:
            log_level = logging.DEBUG
        self._configure_logging(log_level)

    def _configure_logging(self, log_level):
        """Configure the root logger.

        Configure the root logger not to log any messages from webkitpy --
        except for messages from the autoinstall module.  Also set the
        logging level as described below.
        """
        handler = logging.StreamHandler(self.stream)
        # We constrain the level on the handler rather than on the root
        # logger itself.  This is probably better because the handler is
        # configured and known only to this module, whereas the root logger
        # is an object shared (and potentially modified) by many modules.
        # Modifying the handler, then, is less intrusive and less likely to
        # interfere with modifications made by other modules (e.g. in unit
        # tests).
        handler.name = __name__
        handler.setLevel(log_level)
        formatter = logging.Formatter("%(message)s")
        handler.setFormatter(formatter)

        logger = logging.getLogger()
        logger.addHandler(handler)
        logger.setLevel(logging.NOTSET)

        # Filter out most webkitpy messages.
        #
        # Messages can be selectively re-enabled for this script by updating
        # this method accordingly.
        def filter(record):
            """Filter out autoinstall and non-third-party webkitpy messages."""
            # FIXME: Figure out a way not to use strings here, for example by
            #        using syntax like webkitpy.test.__name__.  We want to be
            #        sure not to import any non-Python 2.4 code, though, until
            #        after the version-checking code has executed.
            if (record.name.startswith("webkitpy.common.system.autoinstall") or
                record.name.startswith("webkitpy.test")):
                return True
            if record.name.startswith("webkitpy"):
                return False
            return True

        testing_filter = logging.Filter()
        testing_filter.filter = filter

        # Display a message so developers are not mystified as to why
        # logging does not work in the unit tests.
        _log.info("Suppressing most webkitpy logging while running unit tests.")
        handler.addFilter(testing_filter)

    def run(self):
        options, args = self._parse_args()
        self._configure(options)

        self.finder.clean_trees()

        names = self.finder.find_names(args, self._options.skip_integrationtests, self._options.all)
        if not names:
            _log.error('No tests to run')
            return False

        return self._run_tests(names)

    def _run_tests(self, names):
        if self._options.coverage:
            try:
                import webkitpy.thirdparty.autoinstalled.coverage as coverage
            except ImportError, e:
                _log.error("Failed to import 'coverage'; can't generate coverage numbers.")
                return False
            cov = coverage.coverage()
            cov.start()

        # Make sure PYTHONPATH is set up properly.
        sys.path = self.finder.additional_paths(sys.path) + sys.path

        _log.debug("Loading the tests...")

        loader = unittest.defaultTestLoader
        suites = []
        for name in names:
            if self.finder.is_module(name):
                # if we failed to load a name and it looks like a module,
                # try importing it directly, because loadTestsFromName()
                # produces lousy error messages for bad modules.
                try:
                    __import__(name)
                except ImportError, e:
                    _log.fatal('Failed to import %s:' % name)
                    self._log_exception()
                    return False

            suites.append(loader.loadTestsFromName(name, None))

        test_suite = unittest.TestSuite(suites)
        test_runner = TestRunner(self.stream, self._options, loader)

        _log.debug("Running the tests.")
        if self._options.pass_through:
            outputcapture.OutputCapture.stream_wrapper = _CaptureAndPassThroughStream
        result = test_runner.run(test_suite)
        if self._options.coverage:
            cov.stop()
            cov.save()
            cov.report(show_missing=False)
        return result.wasSuccessful()

    def _log_exception(self):
        s = StringIO.StringIO()
        traceback.print_exc(file=s)
        for l in s.buflist:
            _log.error('  ' + l.rstrip())


class _CaptureAndPassThroughStream(object):
    def __init__(self, stream):
        self._buffer = StringIO.StringIO()
        self._stream = stream

    def write(self, msg):
        self._stream.write(msg)

        # Note that we don't want to capture any output generated by the debugger
        # because that could cause the results of capture_output() to be invalid.
        if not self._message_is_from_pdb():
            self._buffer.write(msg)

    def _message_is_from_pdb(self):
        # We will assume that if the pdb module is in the stack then the output
        # is being generated by the python debugger (or the user calling something
        # from inside the debugger).
        import inspect
        import pdb
        stack = inspect.stack()
        return any(frame[1] == pdb.__file__.replace('.pyc', '.py') for frame in stack)

    def flush(self):
        self._stream.flush()

    def getvalue(self):
        return self._buffer.getvalue()
