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

# NOTE: We intentionally do not depend on anything else in webkitpy here to avoid breaking test-webkitpy.

_log = logging.getLogger(__name__)


class Tester(object):
    @staticmethod
    def clean_packages(dirs):
        """Delete all .pyc files under dirs that have no .py file."""
        for dir_to_clean in dirs:
            _log.debug("Cleaning orphaned *.pyc files from: %s" % dir_to_clean)
            for dir_path, dir_names, file_names in os.walk(dir_to_clean):
                for file_name in file_names:
                    if file_name.endswith(".pyc") and file_name[:-1] not in file_names:
                        file_path = os.path.join(dir_path, file_name)
                        _log.info("Deleting orphan *.pyc file: %s" % file_path)
                        os.remove(file_path)

    def __init__(self):
        self._verbosity = 1

    def parse_args(self, argv):
        parser = optparse.OptionParser(usage='usage: %prog [options] [args...]')
        parser.add_option('-a', '--all', action='store_true', default=False,
                          help='run all the tests'),
        parser.add_option('-c', '--coverage', action='store_true', default=False,
                          help='generate code coverage info (requires http://pypi.python.org/pypi/coverage)'),
        parser.add_option('-n', '--dryrun', action='store_true', default=False,
                          help='do not actually run the tests'),
        parser.add_option('-q', '--quiet', action='store_true', default=False,
                          help='run quietly (errors, warnings, and progress only)'),
        parser.add_option('-s', '--silent', action='store_true', default=False,
                          help='run silently (errors and warnings only)'),
        parser.add_option('-x', '--xml', action='store_true', default=False,
                          help='output xUnit-style XML output')
        parser.add_option('-v', '--verbose', action='count', default=0,
                          help='verbose output (specify once for individual test results, twice for debug messages)')
        parser.add_option('--skip-integrationtests', action='store_true', default=False,
                          help='do not run the integration tests')

        parser.epilog = ('[args...] is an optional list of modules, test_classes, or individual tests. '
                         'If no args are given, all the tests will be run.')

        self.progName = os.path.basename(argv[0])
        return parser.parse_args(argv[1:])

    def configure(self, options):
        self._options = options

        if options.silent:
            self._verbosity = 0
            self._configure_logging(logging.WARNING)
        elif options.quiet:
            self._verbosity = 1
            self._configure_logging(logging.WARNING)
        elif options.verbose == 0:
            self._verbosity = 1
            self._configure_logging(logging.INFO)
        elif options.verbose == 1:
            self._verbosity = 2
            self._configure_logging(logging.INFO)
        elif options.verbose == 2:
            self._verbosity = 2
            self._configure_logging(logging.DEBUG)

    def _configure_logging(self, log_level):
        """Configure the root logger.

        Configure the root logger not to log any messages from webkitpy --
        except for messages from the autoinstall module.  Also set the
        logging level as described below.
        """
        handler = logging.StreamHandler(sys.stderr)
        # We constrain the level on the handler rather than on the root
        # logger itself.  This is probably better because the handler is
        # configured and known only to this module, whereas the root logger
        # is an object shared (and potentially modified) by many modules.
        # Modifying the handler, then, is less intrusive and less likely to
        # interfere with modifications made by other modules (e.g. in unit
        # tests).
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

    def run(self, dirs, args):
<<<<<<< HEAD
        return not self._run_tests(dirs, self._find_modules(dirs, args))

    def _find_modules(self, dirs, modules):
        # FIXME: We should consider moving webkitpy off of using "webkitpy." to prefix
        # all includes.  If we did that, then this would use path instead of dirname(path).
        # QueueStatusServer.__init__ has a sys.path import hack due to this code.
        sys.path.extend(set(os.path.dirname(path) for path in dirs))
        if modules:
            return modules
=======
        args = args or self._find_modules(dirs)
        return self._run_tests(dirs, args)
>>>>>>> test-webkitpy: clean up handling of test directories, QueueStatusServer

    def _find_modules(self, dirs):
        modules = []
        for dir_to_search in dirs:
            modules.extend(self._find_modules_under(dir_to_search, '_unittest.py'))
            if not self._options.skip_integrationtests:
                modules.extend(self._find_modules_under(dir_to_search, '_integrationtest.py'))
        modules.sort()

        for module in modules:
            _log.debug("Found: %s" % module)

        # FIXME: Figure out how to move this to test-webkitpy in order to to make this file more generic.
        if not self._options.all:
            slow_tests = ('webkitpy.common.checkout.scm.scm_unittest',)
            self._exclude(modules, slow_tests, 'are really, really slow', 31818)

            if sys.platform == 'win32':
                win32_blacklist = ('webkitpy.common.checkout',
                                   'webkitpy.common.config',
                                   'webkitpy.tool')
                self._exclude(modules, win32_blacklist, 'fail horribly on win32', 54526)

        return modules

    def _exclude(self, modules, module_prefixes, reason, bugid):
        _log.info('Skipping tests in the following modules or packages because they %s:' % reason)
        for prefix in module_prefixes:
            _log.info('    %s' % prefix)
            modules_to_exclude = filter(lambda m: m.startswith(prefix), modules)
            for m in modules_to_exclude:
                if len(modules_to_exclude) > 1:
                    _log.debug('        %s' % m)
                modules.remove(m)
        _log.info('    (https://bugs.webkit.org/show_bug.cgi?id=%d; use --all to include)' % bugid)
        _log.info('')

    def _find_modules_under(self, dir_to_search, suffix):

        def to_package(dir_path):
            return dir_path.replace(dir_to_search + os.sep, '').replace(os.sep, '.')

        def to_module(filename, package):
            return package + '.' + filename.replace('.py', '')

        modules = []
        for dir_path, _, filenames in os.walk(dir_to_search):
            package = to_package(dir_path)
            modules.extend(to_module(f, package) for f in filenames if f.endswith(suffix))
        return modules

    def _run_tests(self, dirs, args):
        # FIXME: implement a more useful dryrun that recurses into the test suite.
        if self._options.dryrun:
            return True

        if self._options.coverage:
            try:
                import coverage
            except ImportError, e:
                _log.error("Failed to import 'coverage'; can't generate coverage numbers.")
                return False
            cov = coverage.coverage()
            cov.start()

        _log.debug("Loading the tests...")

        loader = unittest.defaultTestLoader
        suites = []
        for name in args:
            if self._is_module(dirs, name):
                # import modules explicitly before loading their tests because
                # loadTestsFromName() produces lousy error messages for bad modules.
                try:
                    __import__(name)
                except ImportError, e:
                    _log.fatal('Failed to import %s:' % name)
                    self._log_exception()
                    return False
            suites.append(loader.loadTestsFromName(name, None))

        test_suite = unittest.TestSuite(suites)
        if self._options.xml:
            from webkitpy.thirdparty.autoinstalled.xmlrunner import XMLTestRunner
            test_runner = XMLTestRunner(output='test-webkitpy-xml-reports')
        else:
            test_runner = unittest.TextTestRunner(verbosity=self._verbosity)

        _log.debug("Running the tests.")
        result = test_runner.run(test_suite)
        if self._options.coverage:
            cov.stop()
            cov.save()
        return result.wasSuccessful()

    def _is_module(self, dirs, name):
        relpath = name.replace('.', os.sep) + '.py'
        return any(os.path.exists(os.path.join(d, relpath)) for d in dirs)

    def _log_exception(self):
        s = StringIO.StringIO()
        traceback.print_exc(file=s)
        for l in s.buflist:
            _log.error('  ' + l.rstrip())
