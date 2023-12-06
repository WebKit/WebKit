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
    tester.add_tree(os.path.join(_webkit_root, 'Tools', 'Scripts', 'libraries', 'reporelaypy'), 'reporelaypy')

    # AppleWin is the only platform that does not support Modern WebKit
    # FIXME: Find a better way to detect this currently assuming cygwin means AppleWin
    if sys.platform != 'cygwin':
        tester.add_tree(os.path.join(_webkit_root, 'Source', 'WebKit', 'Scripts'), 'webkit')

    tester.skip(('webkitpy.common.checkout.scm.scm_unittest',), 'are really, really, slow', 31818)
    if sys.platform.startswith('win'):
        tester.skip(('webkitpy.common.checkout', 'webkitpy.tool'), 'fail horribly on win32', 54526)
        tester.skip(('reporelaypy',), 'fail to install lupa and don\'t have to test on win32', 243316)
        tester.skip(('webkitflaskpy',), 'fail to install lupa and don\'t have to test on win32', 253419)

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

    # Only run allowlisted tests on Python 2.
    py2_tests = {
        "webkit.messages_unittest",
        "webkit.model_unittest",
        "webkit.parser_unittest",
        "webkitbugspy.tests.bugzilla_unittest",
        "webkitbugspy.tests.github_unittest",
        "webkitbugspy.tests.radar_unittest",
        "webkitbugspy.tests.user_unittest",
        "webkitcorepy.tests.call_by_need_unittest",
        "webkitcorepy.tests.decorators_unittest",
        "webkitcorepy.tests.environment_unittest",
        "webkitcorepy.tests.file_lock_unittest",
        "webkitcorepy.tests.filtered_call_unittest",
        "webkitcorepy.tests.measure_time_unittest",
        "webkitcorepy.tests.mocks.context_stack_unittest",
        "webkitcorepy.tests.mocks.requests_unittest",
        "webkitcorepy.tests.mocks.subprocess_unittest",
        "webkitcorepy.tests.mocks.time_unittest",
        "webkitcorepy.tests.nested_fuzzy_dict_unittest",
        "webkitcorepy.tests.null_context_unittest",
        "webkitcorepy.tests.output_capture_unittest",
        "webkitcorepy.tests.partial_proxy_unittest",
        "webkitcorepy.tests.string_utils_unittest",
        "webkitcorepy.tests.subprocess_utils_unittest",
        "webkitcorepy.tests.task_pool_unittest",
        "webkitcorepy.tests.terminal_unittest",
        "webkitcorepy.tests.timeout_unittest",
        "webkitcorepy.tests.version_unittest",
        "webkitflaskpy.tests.database_unittest",
        "webkitflaskpy.tests.ipc_manager_unittest",
        "webkitflaskpy.tests.util_unittest",
        "webkitpy.api_tests.manager_unittest",
        "webkitpy.benchmark_runner.benchmark_results_unittest",
        "webkitpy.browserperfdash.browserperfdash_unittest",
        "webkitpy.common.attribute_saver_unittest",
        "webkitpy.common.checkout.changelog_unittest",
        "webkitpy.common.checkout.checkout_unittest",
        "webkitpy.common.checkout.commitinfo_unittest",
        "webkitpy.common.checkout.diff_parser_unittest",
        "webkitpy.common.checkout.scm.detection_unittest",
        "webkitpy.common.checkout.scm.scm_unittest",
        "webkitpy.common.checkout.scm.stub_repository_unittest",
        "webkitpy.common.config.committers_unittest",
        "webkitpy.common.config.contributors_json_unittest",
        "webkitpy.common.config.ports_unittest",
        "webkitpy.common.config.urls_unittest",
        "webkitpy.common.editdistance_unittest",
        "webkitpy.common.find_files_unittest",
        "webkitpy.common.memoized_unittest",
        "webkitpy.common.net.abstracttestresults_unittest",
        "webkitpy.common.net.bugzilla.attachment_unittest",
        "webkitpy.common.net.bugzilla.bug_unittest",
        "webkitpy.common.net.bugzilla.bugzilla_unittest",
        "webkitpy.common.net.bugzilla.results_fetcher_unittest",
        "webkitpy.common.net.bugzilla.test_expectation_updater_unittest",
        "webkitpy.common.net.buildbot.buildbot_unittest",
        "webkitpy.common.net.credentials_unittest",
        "webkitpy.common.net.ewsserver_unittest",
        "webkitpy.common.net.failuremap_unittest",
        "webkitpy.common.net.layouttestresults_unittest",
        "webkitpy.common.net.networktransaction_unittest",
        "webkitpy.common.net.resultsjsonparser_unittest",
        "webkitpy.common.prettypatch_unittest",
        "webkitpy.common.read_checksum_from_png_unittest",
        "webkitpy.common.system.crashlogs_unittest",
        "webkitpy.common.system.environment_unittest",
        "webkitpy.common.system.executive_unittest",
        "webkitpy.common.system.filesystem_mock_unittest",
        "webkitpy.common.system.filesystem_unittest",
        "webkitpy.common.system.logutils_unittest",
        "webkitpy.common.system.outputtee_unittest",
        "webkitpy.common.system.path_unittest",
        "webkitpy.common.system.pemfile_unittest",
        "webkitpy.common.system.platforminfo_unittest",
        "webkitpy.common.system.profiler_unittest",
        "webkitpy.common.system.stack_utils_unittest",
        "webkitpy.common.system.user_unittest",
        "webkitpy.common.system.workspace_unittest",
        "webkitpy.common.test_expectations_unittest",
        "webkitpy.common.version_name_map_unittest",
        "webkitpy.common.watchlist.amountchangedpattern_unittest",
        "webkitpy.common.watchlist.changedlinepattern_unittest",
        "webkitpy.common.watchlist.filenamepattern_unittest",
        "webkitpy.common.watchlist.watchlist_unittest",
        "webkitpy.common.watchlist.watchlistparser_unittest",
        "webkitpy.common.watchlist.watchlistrule_unittest",
        "webkitpy.featuredefines.matcher_unittest",
        "webkitpy.layout_tests.controllers.layout_test_finder_legacy_unittest",
        "webkitpy.layout_tests.controllers.layout_test_runner_unittest",
        "webkitpy.layout_tests.controllers.manager_unittest",
        "webkitpy.layout_tests.controllers.single_test_runner_unittest",
        "webkitpy.layout_tests.controllers.test_result_writer_unittest",
        "webkitpy.layout_tests.layout_package.json_results_generator_unittest",
        "webkitpy.layout_tests.lint_test_expectations_unittest",
        "webkitpy.layout_tests.models.test_configuration_unittest",
        "webkitpy.layout_tests.models.test_expectations_unittest",
        "webkitpy.layout_tests.models.test_failures_unittest",
        "webkitpy.layout_tests.models.test_results_unittest",
        "webkitpy.layout_tests.models.test_run_results_unittest",
        "webkitpy.layout_tests.run_webkit_tests_integrationtest",
        "webkitpy.layout_tests.servers.apache_http_server_unittest",
        "webkitpy.layout_tests.servers.http_server_base_unittest",
        "webkitpy.layout_tests.servers.http_server_integrationtest",
        "webkitpy.layout_tests.servers.http_server_unittest",
        "webkitpy.layout_tests.servers.web_platform_test_server_unittest",
        "webkitpy.layout_tests.views.buildbot_results_unittest",
        "webkitpy.layout_tests.views.metered_stream_unittest",
        "webkitpy.layout_tests.views.printing_unittest",
        "webkitpy.performance_tests.perftest_unittest",
        "webkitpy.performance_tests.perftestsrunner_integrationtest",
        "webkitpy.performance_tests.perftestsrunner_unittest",
        "webkitpy.port.base_unittest",
        "webkitpy.port.config_unittest",
        "webkitpy.port.driver_unittest",
        "webkitpy.port.factory_unittest",
        "webkitpy.port.gtk_unittest",
        "webkitpy.port.headlessdriver_unittest",
        "webkitpy.port.ios_device_unittest",
        "webkitpy.port.ios_simulator_unittest",
        "webkitpy.port.leakdetector_unittest",
        "webkitpy.port.leakdetector_valgrind_unittest",
        "webkitpy.port.linux_get_crash_log_unittest",
        "webkitpy.port.mac_unittest",
        "webkitpy.port.server_process_unittest",
        "webkitpy.port.watch_simulator_unittest",
        "webkitpy.port.waylanddriver_unittest",
        "webkitpy.port.westondriver_unittest",
        "webkitpy.port.win_unittest",
        "webkitpy.port.wpe_unittest",
        "webkitpy.port.xorgdriver_unittest",
        "webkitpy.port.xvfbdriver_unittest",
        "webkitpy.results.upload_unittest",
        "webkitpy.style.checker_unittest",
        "webkitpy.style.checkers.basexcconfig_unittest",
        "webkitpy.style.checkers.changelog_unittest",
        "webkitpy.style.checkers.cmake_unittest",
        "webkitpy.style.checkers.common_unittest",
        "webkitpy.style.checkers.cpp_unittest",
        "webkitpy.style.checkers.js_unittest",
        "webkitpy.style.checkers.jsonchecker_unittest",
        "webkitpy.style.checkers.jstest_unittest",
        "webkitpy.style.checkers.messagesin_unittest",
        "webkitpy.style.checkers.png_unittest",
        "webkitpy.style.checkers.python_unittest",
        "webkitpy.style.checkers.test_expectations_unittest",
        "webkitpy.style.checkers.text_unittest",
        "webkitpy.style.checkers.watchlist_unittest",
        "webkitpy.style.checkers.xcodeproj_unittest",
        "webkitpy.style.checkers.xml_unittest",
        "webkitpy.style.error_handlers_unittest",
        "webkitpy.style.filereader_unittest",
        "webkitpy.style.filter_unittest",
        "webkitpy.style.main_unittest",
        "webkitpy.style.optparser_unittest",
        "webkitpy.style.patchreader_unittest",
        "webkitpy.test.finder_unittest",
        "webkitpy.test.main_unittest",
        "webkitpy.test.runner_unittest",
        "webkitpy.tool.bot.botinfo_unittest",
        "webkitpy.tool.bot.queueengine_unittest",
        "webkitpy.tool.commands.applywatchlistlocal_unittest",
        "webkitpy.tool.commands.download_unittest",
        "webkitpy.tool.commands.queues_unittest",
        "webkitpy.tool.commands.suggestnominations_unittest",
        "webkitpy.tool.commands.upload_unittest",
        "webkitpy.tool.mocktool_unittest",
        "webkitpy.tool.multicommandtool_unittest",
        "webkitpy.tool.servers.gardeningserver_unittest",
        "webkitpy.tool.servers.rebaselineserver_unittest",
        "webkitpy.tool.servers.reflectionhandler_unittest",
        "webkitpy.tool.steps.applywatchlist_unittest",
        "webkitpy.tool.steps.cleanworkingdirectory_unittest",
        "webkitpy.tool.steps.closebugforlanddiff_unittest",
        "webkitpy.tool.steps.commit_unittest",
        "webkitpy.tool.steps.discardlocalchanges_unittest",
        "webkitpy.tool.steps.haslanded_unittest",
        "webkitpy.tool.steps.preparechangelog_unittest",
        "webkitpy.tool.steps.preparechangelogforrevert_unittest",
        "webkitpy.tool.steps.steps_unittest",
        "webkitpy.tool.steps.suggestreviewers_unittest",
        "webkitpy.tool.steps.update_unittest",
        "webkitpy.tool.steps.updatechangelogswithreview_unittest",
        "webkitpy.tool.steps.validatechangelogs_unittest",
        "webkitpy.w3c.test_converter_unittest",
        "webkitpy.w3c.test_exporter_unittest",
        "webkitpy.w3c.test_importer_unittest",
        "webkitpy.w3c.test_parser_unittest",
        "webkitpy.w3c.wpt_github_unittest",
        "webkitpy.w3c.wpt_runner_unittest",
        "webkitpy.xcode.device_type_unittest",
        "webkitpy.xcode.sdk_unittest",
        "webkitpy.xcode.simulated_device_unittest",
        "webkitscmpy.test.branch_unittest",
        "webkitscmpy.test.canonicalize_unittest",
        "webkitscmpy.test.checkout_unittest",
        "webkitscmpy.test.cherry_pick_unittest",
        "webkitscmpy.test.classify_unittest",
        "webkitscmpy.test.clean_unittest",
        "webkitscmpy.test.clone_unittest",
        "webkitscmpy.test.commit_unittest",
        "webkitscmpy.test.contributor_unittest",
        "webkitscmpy.test.find_unittest",
        "webkitscmpy.test.git_unittest",
        "webkitscmpy.test.install_git_lfs_unittest",
        "webkitscmpy.test.install_hooks_unittest",
        "webkitscmpy.test.land_unittest",
        "webkitscmpy.test.log_unittest",
        "webkitscmpy.test.pickable_unittest",
        "webkitscmpy.test.publish_unittest",
        "webkitscmpy.test.pull_request_unittest",
        "webkitscmpy.test.revert_unittest",
        "webkitscmpy.test.scm_unittest",
        "webkitscmpy.test.setup_git_svn_unittest",
        "webkitscmpy.test.setup_unittest",
        "webkitscmpy.test.show_unittest",
        "webkitscmpy.test.squash_unittest",
        "webkitscmpy.test.svn_unittest",
        "webkitscmpy.test.trace_unittest",
        "webkitscmpy.test.track_unittest",
    }

    all_tests = set(tester.finder.find_names([], True))

    if sys.version_info < (3,):
        tester.expect_error_on_import(
            sorted(all_tests - py2_tests), "aren't in the Python 2 allowlist", 264431
        )

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
        self._expect_error_on_import_tests = []

    def add_tree(self, top_directory, starting_subdirectory):
        self.finder.add_tree(top_directory, starting_subdirectory)

    def skip(self, names, reason, bugid):
        self.finder.skip(names, reason, bugid)

    def expect_error_on_import(self, names, reason, bugid):
        self.finder.skip(names, reason, bugid)
        self._expect_error_on_import_tests.extend(names)

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
        if any([n.startswith('webkitflaskpy') for n in names]):
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
        if not self._check_imports(names, self._expect_error_on_import_tests):
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

    def _check_imports(self, names, non_importable_names):
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

        for name in non_importable_names:
            try:
                __import__(name)
            except (ImportError, SyntaxError):
                pass
            except Exception as e:
                _log.fatal(
                    "Importing %s expected to fail with (ImportError, SyntaxError)"
                    % name
                )
                self._log_exception()
                return False
            else:
                _log.fatal(
                    "Importing %s expected to fail with (ImportError, SyntaxError), but did not raise"
                    % name
                )
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
