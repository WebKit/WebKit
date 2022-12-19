# Copyright (C) 2012 Google, Inc.
# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
# Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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

import itertools
import json
import logging
import multiprocessing
import operator
import optparse
import os
import sys
import time
import traceback
import unittest

from webkitpy.common.system.logutils import configure_logging
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.host import Host
from webkitpy.test.finder import Finder
from webkitpy.test.printer import Printer
from webkitpy.test.runner import Runner
from webkitpy.results.upload import Upload
from webkitpy.results.options import upload_options

from webkitcorepy import StringIO

_log = logging.getLogger(__name__)

_host = Host()
_webkit_root = None


def main():
    global _webkit_root
    configure_logging(logger=_log)

    up = os.path.dirname
    _webkit_root = up(up(up(up(up(os.path.abspath(__file__))))))

    tester = Tester()
    tester.add_tree(os.path.join(_webkit_root, 'Tools', 'Scripts'), 'webkitpy')
    tester.add_tree(os.path.join(_webkit_root, 'Tools', 'Scripts', 'libraries', 'webkitcorepy'), 'webkitcorepy')
    tester.add_tree(os.path.join(_webkit_root, 'Tools', 'Scripts', 'libraries', 'webkitbugspy'), 'webkitbugspy')
    tester.add_tree(os.path.join(_webkit_root, 'Tools', 'Scripts', 'libraries', 'webkitscmpy'), 'webkitscmpy')
    tester.add_tree(os.path.join(_webkit_root, 'Tools', 'Scripts', 'libraries', 'webkitflaskpy'), 'webkitflaskpy')
    if sys.version_info > (3, 0):
        tester.add_tree(os.path.join(_webkit_root, 'Tools', 'Scripts', 'libraries', 'reporelaypy'), 'reporelaypy')

    # AppleWin is the only platform that does not support Modern WebKit
    # FIXME: Find a better way to detect this currently assuming cygwin means AppleWin
    if sys.platform != 'cygwin':
        tester.add_tree(os.path.join(_webkit_root, 'Source', 'WebKit', 'Scripts'), 'webkit')

    tester.skip(('webkitpy.common.checkout.scm.scm_unittest',), 'are really, really, slow', 31818)
    if sys.platform.startswith('win'):
        tester.skip(('webkitpy.common.checkout', 'webkitpy.tool'), 'fail horribly on win32', 54526)
        tester.skip(('reporelaypy',), 'fail to install lupa and don\'t have to test on win32', 243316)

    # Tests that are platform specific
    mac_only_tests = (
        'webkitpy.xcode',
        'webkitpy.port.ios_device_unittest',
        'webkitpy.port.ios_simulator_unittest',
        'webkitpy.port.mac_unittest',
        'webkitpy.port.watch_simulator_unittest',
    )
    linux_only_tests = (
        'webkitpy.port.gtk_unittest',
        'webkitpy.port.headlessdriver_unittest',
        'webkitpy.port.linux_get_crash_log_unittest',
        'webkitpy.port.waylanddriver_unittest',
        'webkitpy.port.westondriver_unittest',
        'webkitpy.port.wpe_unittest',
        'webkitpy.port.xorgdriver_unittest',
        'webkitpy.port.xvfbdriver_unittest',
    )
    windows_only_tests = ('webkitpy.port.win_unittest',)

    # Skip platform specific tests on Windows and Linux
    # The webkitpy EWS is run on Mac so only skip tests that won't run on it
    if sys.platform.startswith('darwin'):
        skip_tests = None
    elif sys.platform.startswith('win'):
        skip_tests = mac_only_tests + linux_only_tests + \
            ('webkitpy.port.leakdetector_unittest', 'webkitpy.port.leakdetector_valgrind_unittest')
    else:
        skip_tests = mac_only_tests + windows_only_tests

    if skip_tests is not None:
        tester.skip(skip_tests, 'are not relevant for the platform running tests', 222066)

    return not tester.run()


def _print_results_as_json(stream, all_test_names, failures, errors):
    def result_dict_from_tuple(result_tuple):
        return {'name': result_tuple[0], 'result': result_tuple[1]}

    results = {}
    results['failures'] = list(map(result_dict_from_tuple, sorted(failures, key=operator.itemgetter(0))))
    results['errors'] = list(map(result_dict_from_tuple, sorted(errors, key=operator.itemgetter(0))))
    results['passes'] = sorted(set(all_test_names) - set(map(operator.itemgetter(0), failures)) - set(map(operator.itemgetter(0), errors)))

    json.dump(results, stream, separators=(',', ':'))


class Tester(object):
    def __init__(self, filesystem=None):
        self.finder = Finder(filesystem or FileSystem())
        self.printer = Printer(sys.stderr)
        self._options = None
        self.upload_style = 'release'

    def add_tree(self, top_directory, starting_subdirectory=None):
        self.finder.add_tree(top_directory, starting_subdirectory)

    def skip(self, names, reason, bugid):
        self.finder.skip(names, reason, bugid)

    def _parse_args(self, argv=None):
        parser = optparse.OptionParser(usage='usage: %prog [options] [args...]')

        upload_group = optparse.OptionGroup(parser, 'Upload Options')
        upload_group.add_options(upload_options())
        parser.add_option_group(upload_group)

        parser.add_option('-a', '--all', action='store_true', default=False,
                          help='run all the tests')
        parser.add_option('-c', '--coverage', action='store_true', default=False,
                          help='generate code coverage info (requires http://pypi.python.org/pypi/coverage)')
        parser.add_option('-i', '--integration-tests', action='store_true', default=False,
                          help='run integration tests as well as unit tests'),
        parser.add_option('-j', '--child-processes', action='store', type='int', default=(1 if sys.platform.startswith('win') else multiprocessing.cpu_count()),
                          help='number of tests to run in parallel (default=%default)')
        parser.add_option('-p', '--pass-through', action='store_true', default=False,
                          help='be debugger friendly by passing captured output through to the system')
        parser.add_option('-q', '--quiet', action='store_true', default=False,
                          help='run quietly (errors, warnings, and progress only)')
        parser.add_option('-t', '--timing', action='store_true', default=False,
                          help='display per-test execution time (implies --verbose)')
        parser.add_option('-v', '--verbose', action='count', default=0,
                          help='verbose output (specify once for individual test results, twice for debug messages)')
        # FIXME: Remove '--json' argument.
        parser.add_option('--json', action='store_true', default=False,
                          help='write JSON formatted test results to stdout')
        parser.add_option('--json-output', action='store', type='string', dest='json_file_name',
                          help='Create a file at specified path, listing test results in JSON format.')

        parser.epilog = ('[args...] is an optional list of modules, test_classes, or individual tests. '
                         'If no args are given, all the tests will be run.')

        return parser.parse_args(argv)

    def run(self, argv=None):
        self._options, args = self._parse_args(argv)
        self.printer.configure(self._options)

        self.finder.clean_trees()

        names = self.finder.find_names(args, self._options.all)
        if not names:
            _log.error('No tests to run')
            return False

        return self._run_tests(names)

    def _run_tests(self, names):
        # Make sure PYTHONPATH is set up properly.
        sys.path = self.finder.additional_paths(sys.path) + sys.path

        from webkitcorepy import AutoInstall

        # Force registration of all autoinstalled packages.
        if any([n.startswith('reporelaypy') for n in names]):
            import reporelaypy
        import webkitflaskpy

        AutoInstall.install_everything()

        start_time = time.time()

        if getattr(self._options, 'coverage', False):
            _log.warning("Checking code coverage, so running things serially")
            self._options.child_processes = 1

            import coverage
            cov = coverage.coverage(omit=[
                "/usr/*",
                "*/webkitpy/thirdparty/*",
            ])
            cov.start()

        self.printer.write_update("Checking imports ...")
        if not self._check_imports(names):
            return False

        self.printer.write_update("Finding the individual test methods ...")
        loader = _Loader()
        parallel_tests, serial_tests = self._test_names(loader, names)

        self.printer.write_update("Running the tests ...")
        self.printer.num_tests = len(parallel_tests) + len(serial_tests)
        start = time.time()
        test_runner = Runner(self.printer, loader)
        test_runner.run(parallel_tests, getattr(self._options, 'child_processes', 1))
        test_runner.run(serial_tests, 1)
        end_time = time.time()

        self.printer.print_result(time.time() - start)

        if getattr(self._options, 'json', False):
            _print_results_as_json(sys.stdout, itertools.chain(parallel_tests, serial_tests), test_runner.failures, test_runner.errors)

        if getattr(self._options, 'json_file_name', None):
            self._options.json_file_name = os.path.abspath(self._options.json_file_name)
            with open(self._options.json_file_name, 'w') as json_file:
                _print_results_as_json(json_file, itertools.chain(parallel_tests, serial_tests), test_runner.failures, test_runner.errors)

        if getattr(self._options, 'coverage', False):
            cov.stop()
            cov.save()

        failed_uploads = 0
        if getattr(self._options, 'report_urls', None):
            self.printer.meter.writeln('\n')
            self.printer.write_update('Preparing upload data ...')

            # Empty test results indicate a PASS.
            results = {test: {} for test in test_runner.tests_run}
            for test, errors in test_runner.errors:
                results[test] = Upload.create_test_result(actual=Upload.Expectations.ERROR, log='/n'.join(errors))
            for test, failures in test_runner.failures:
                results[test] = Upload.create_test_result(actual=Upload.Expectations.FAIL, log='/n'.join(failures))

            _host.initialize_scm()
            upload = Upload(
                suite=self._options.suite or 'webkitpy-tests',
                configuration=Upload.create_configuration(
                    platform=_host.platform.os_name,
                    version=str(_host.platform.os_version),
                    version_name=_host.platform.os_version_name(),
                    style=self.upload_style,
                    sdk=_host.platform.build_version(),
                    flavor=self._options.result_report_flavor,
                    architecture=_host.platform.architecture(),
                ),
                details=Upload.create_details(options=self._options),
                commits=[Upload.create_commit(
                    repository_id='webkit',
                    id=_host.scm().native_revision(_webkit_root),
                    branch=_host.scm().native_branch(_webkit_root),
                )],
                run_stats=Upload.create_run_stats(
                    start_time=start_time,
                    end_time=end_time,
                    tests_skipped=len(test_runner.tests_run) - len(parallel_tests) - len(serial_tests),
                ),
                results=results,
            )
            for url in self._options.report_urls:
                self.printer.write_update('Uploading to {} ...'.format(url))
                failed_uploads = failed_uploads if upload.upload(url, log_line_func=self.printer.meter.writeln) else (failed_uploads + 1)
            self.printer.meter.writeln('Uploads completed!')

        if getattr(self._options, 'coverage', False):
            cov.report(show_missing=False)

        return not self.printer.num_errors and not self.printer.num_failures and not failed_uploads

    def _check_imports(self, names):
        for name in names:
            if self.finder.is_module(name):
                # if we failed to load a name and it looks like a module,
                # try importing it directly, because loadTestsFromName()
                # produces lousy error messages for bad modules.
                try:
                    __import__(name)
                except ImportError:
                    _log.fatal('Failed to import %s:' % name)
                    self._log_exception()
                    return False
        return True

    def _test_names(self, loader, names):
        parallel_test_method_prefixes = ['test_']
        serial_test_method_prefixes = ['serial_test_']
        if getattr(self._options, 'integration_tests', None):
            parallel_test_method_prefixes.append('integration_test_')
            serial_test_method_prefixes.append('serial_integration_test_')

        parallel_tests = []
        loader.test_method_prefixes = parallel_test_method_prefixes
        for name in names:
            parallel_tests.extend(self._all_test_names(loader.loadTestsFromName(name, None)))

        serial_tests = []
        loader.test_method_prefixes = serial_test_method_prefixes
        for name in names:
            serial_tests.extend(self._all_test_names(loader.loadTestsFromName(name, None)))

        # loader.loadTestsFromName() will not verify that names begin with one of the test_method_prefixes
        # if the names were explicitly provided (e.g., MainTest.test_basic), so this means that any individual
        # tests will be included in both parallel_tests and serial_tests, and we need to de-dup them.
        serial_tests = list(set(serial_tests).difference(set(parallel_tests)))

        return (parallel_tests, serial_tests)

    def _all_test_names(self, suite):
        names = []
        if hasattr(suite, '_tests'):
            for t in suite._tests:
                names.extend(self._all_test_names(t))
        else:
            names.append(suite.id())
        return names

    def _log_exception(self):
        s = StringIO()
        traceback.print_exc(file=s)
        for l in s.getvalue().splitlines():
            _log.error('  ' + l.rstrip())


class _Loader(unittest.TestLoader):
    test_method_prefixes = []

    def getTestCaseNames(self, testCaseClass):
        should_skip_class_method = getattr(testCaseClass, "shouldSkip", None)
        if callable(should_skip_class_method):
            if testCaseClass.shouldSkip():
                _log.info('Skipping tests in %s' % (testCaseClass.__name__))
                return []

        def isTestMethod(attrname, testCaseClass=testCaseClass):
            if not hasattr(getattr(testCaseClass, attrname), '__call__'):
                return False
            return (any(attrname.startswith(prefix) for prefix in self.test_method_prefixes))

        return sorted(filter(isTestMethod, dir(testCaseClass)))


if __name__ == '__main__':
    sys.exit(main())
