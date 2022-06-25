# Copyright (C) 2012 Google Inc. All rights reserved.
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

"""Run Inspector's perf tests in perf mode."""

import os
import json
import logging
import optparse
import sys
import time
import datetime

from webkitpy.common import find_files
from webkitpy.common.checkout.scm.detection import SCMDetector
from webkitpy.common.config.urls import view_source_url
from webkitpy.common.host import Host
from webkitpy.common.net.file_uploader import FileUploader
from webkitpy.performance_tests.perftest import PerfTestFactory
from webkitpy.performance_tests.perftest import DEFAULT_TEST_RUNNER_COUNT
from webkitcorepy import string_utils


_log = logging.getLogger(__name__)


class PerfTestsRunner(object):
    _default_branch = 'webkit-trunk'
    EXIT_CODE_BAD_BUILD = -1
    EXIT_CODE_BAD_SOURCE_JSON = -2
    EXIT_CODE_BAD_MERGE = -3
    EXIT_CODE_FAILED_UPLOADING = -4
    EXIT_CODE_BAD_PREPARATION = -5

    _DEFAULT_JSON_FILENAME = 'PerformanceTestsResults.json'

    def __init__(self, args=None, port=None):
        self._options, self._args = PerfTestsRunner._parse_args(args)
        if port:
            self._port = port
            self._host = self._port.host
        else:
            self._host = Host()
            self._port = self._host.port_factory.get(self._options.platform, self._options)

        # Timeouts are controlled by the Python Driver, so DRT/WTR runs with no-timeout.
        self._options.additional_drt_flag.append('--no-timeout')

        # The GTK+ port only supports WebKit2, so it always uses WKTR.
        if self._port.name().startswith("gtk"):
            self._options.webkit_test_runner = True

        self._host.initialize_scm()
        self._webkit_base_dir_len = len(self._port.webkit_base())
        self._base_path = self._port.perf_tests_dir()
        self._timestamp = time.time()
        self._utc_timestamp = datetime.datetime.utcnow()

    @staticmethod
    def _parse_args(args=None):
        def _expand_path(option, opt_str, value, parser):
            path = os.path.expandvars(os.path.expanduser(value))
            setattr(parser.values, option.dest, path)
        perf_option_list = [
            optparse.make_option('--debug', action='store_const', const='Debug', dest="configuration",
                help='Set the configuration to Debug'),
            optparse.make_option('--release', action='store_const', const='Release', dest="configuration",
                help='Set the configuration to Release'),
            optparse.make_option("--platform",
                help="Specify port/platform being tested (i.e. chromium-mac)"),
            optparse.make_option("--builder-name",
                help=("The name of the builder shown on the waterfall running this script e.g. google-mac-2.")),
            optparse.make_option("--build-number",
                help=("The build number of the builder running this script.")),
            optparse.make_option("--build", dest="build", action="store_true", default=True,
                help="Check to ensure the DumpRenderTree build is up-to-date (default)."),
            optparse.make_option("--no-build", dest="build", action="store_false",
                help="Don't check to see if the DumpRenderTree build is up-to-date."),
            optparse.make_option("--build-directory",
                help="Path to the directory under which build files are kept (should not include configuration)"),
            optparse.make_option("--time-out-ms", default=600 * 1000,
                help="Set the timeout for each test"),
            optparse.make_option("--no-timeout", action="store_true", default=False,
                help="Disable test timeouts"),
            optparse.make_option("--no-results", action="store_false", dest="generate_results", default=True,
                help="Do no generate results JSON and results page."),
            optparse.make_option("--output-json-path", action='callback', callback=_expand_path, type="str",
                help="Path to generate a JSON file at; may contain previous results if it already exists."),
            optparse.make_option("--reset-results", action="store_true",
                help="Clears the content in the generated JSON file before adding the results."),
            optparse.make_option("--worker-config-json-path", action='callback',
                callback=_expand_path, type="str", dest="worker_config_json_path",
                help="Only used on bots. Path to a worker configuration file."),
            optparse.make_option("--description",
                help="Add a description to the output JSON file if one is generated"),
            optparse.make_option("--no-show-results", action="store_false", default=True, dest="show_results",
                help="Don't launch a browser with results after the tests are done"),
            optparse.make_option("--test-results-server",
                help="Upload the generated JSON file to the specified server when --output-json-path is present."),
            optparse.make_option("--dump-render-tree", "-1", action="store_false", default=True, dest="webkit_test_runner",
                help="Use DumpRenderTree rather than WebKitTestRunner."),
            optparse.make_option("--force", dest="use_skipped_list", action="store_false", default=True,
                help="Run all tests, including the ones in the Skipped list."),
            optparse.make_option("--profile", action="store_true",
                help="Output per-test profile information."),
            optparse.make_option("--profiler", action="store",
                help="Output per-test profile information, using the specified profiler."),
            optparse.make_option("--additional-drt-flag", action="append",
                default=[], help="Additional command line flag to pass to DumpRenderTree "
                     "Specify multiple times to add multiple flags."),
            optparse.make_option("--driver-name", type="string",
                help="Alternative DumpRenderTree binary to use"),
            optparse.make_option("--repeat", default=1, type="int",
                help="Specify number of times to run test set (default: 1)."),
            optparse.make_option("--test-runner-count", default=-1, type="int",
                help="Specify number of times to invoke test runner for each performance test."),
            optparse.make_option("--wrapper",
                help="wrapper command to insert before invocations of "
                 "DumpRenderTree or WebKitTestRunner; option is split on whitespace before "
                 "running. (Example: --wrapper='valgrind --smc-check=all')"),
            optparse.make_option('--display-server', choices=['xvfb', 'xorg', 'weston', 'wayland'], default='xvfb',
                help='"xvfb": Use a virtualized X11 server. "xorg": Use the current X11 session. '
                     '"weston": Use a virtualized Weston server. "wayland": Use the current wayland session.'),
            ]
        return optparse.OptionParser(option_list=(perf_option_list)).parse_args(args)

    def _collect_tests(self):
        test_extensions = ['.html', '.svg']

        def _is_test_file(filesystem, dirname, filename):
            return filesystem.splitext(filename)[1] in test_extensions

        filesystem = self._host.filesystem

        paths = []
        for arg in self._args:
            if filesystem.exists(filesystem.join(self._base_path, arg)):
                paths.append(arg)
            else:
                relpath = filesystem.relpath(arg, self._base_path)
                if filesystem.exists(filesystem.join(self._base_path, relpath)):
                    paths.append(filesystem.normpath(relpath))
                else:
                    _log.warn('Path was not found:' + arg)

        skipped_directories = set(['.svn', 'resources'])
        test_files = find_files.find(filesystem, self._base_path, paths, skipped_directories, _is_test_file)
        tests = []

        test_runner_count = DEFAULT_TEST_RUNNER_COUNT
        if self._options.test_runner_count > 0:
            test_runner_count = self._options.test_runner_count
        elif self._options.profile:
            test_runner_count = 1

        for path in test_files:
            relative_path = filesystem.relpath(path, self._base_path).replace('\\', '/')
            if self._options.use_skipped_list and self._port.skips_perf_test(relative_path) and filesystem.normpath(relative_path) not in paths:
                continue
            if relative_path.endswith('/index.html'):
                relative_path = relative_path[0:-len('/index.html')]
            test = PerfTestFactory.create_perf_test(self._port, relative_path, path, test_runner_count=test_runner_count)
            tests.append(test)

        return tests

    def run(self):
        if "Debug" == self._port.get_option("configuration"):
            _log.warning("""****************************************************
* WARNING: run-perf-tests is running in DEBUG mode *
****************************************************""")

        if not self._port.check_build():
            _log.error("Build not up to date for %s" % self._port._path_to_driver())
            return self.EXIT_CODE_BAD_BUILD

        # Check that the system dependencies (themes, fonts, ...) are correct.
        if not self._port.check_sys_deps():
            _log.error("Failed to check system dependencies.")
            self._port.stop_helper()
            return self.EXIT_CODE_BAD_PREPARATION

        run_count = 0
        repeat = self._options.repeat
        while (run_count < repeat):
            run_count += 1

            tests = self._collect_tests()
            runs = ' (Run %d of %d)' % (run_count, repeat) if repeat > 1 else ''
            _log.info("Running %d tests%s" % (len(tests), runs))

            for test in tests:
                if not test.prepare(self._options.time_out_ms):
                    return self.EXIT_CODE_BAD_PREPARATION

            unexpected = self._run_tests_set(sorted(list(tests), key=lambda test: test.test_name()))

            if self._options.generate_results and not self._options.profile:
                exit_code = self._generate_results()
                if exit_code:
                    return exit_code

        if self._options.generate_results and not self._options.profile:
            test_results_server = self._options.test_results_server
            if test_results_server and not self._upload_json(test_results_server, self._output_json_path()):
                return self.EXIT_CODE_FAILED_UPLOADING

            if self._options.show_results:
                self._port.show_results_html_file(self._results_page_path())

        return unexpected

    def _output_json_path(self):
        output_json_path = self._options.output_json_path
        if output_json_path:
            return output_json_path
        return self._host.filesystem.join(self._port.perf_results_directory(), self._DEFAULT_JSON_FILENAME)

    def _results_page_path(self):
        return self._host.filesystem.splitext(self._output_json_path())[0] + '.html'

    def _generate_results(self):
        options = self._options
        output_json_path = self._output_json_path()
        output = self._generate_results_dict(self._timestamp, options.description, options.platform, options.builder_name, options.build_number)

        if options.worker_config_json_path:
            output = self._merge_worker_config_json(options.worker_config_json_path, output)
            if not output:
                return self.EXIT_CODE_BAD_SOURCE_JSON

        output = self._merge_outputs_if_needed(output_json_path, output)
        if not output:
            return self.EXIT_CODE_BAD_MERGE

        filesystem = self._host.filesystem
        json_output = json.dumps(output)
        filesystem.write_text_file(output_json_path, json_output)

        template_path = filesystem.join(self._port.perf_tests_dir(), 'resources/results-template.html')
        template = filesystem.read_text_file(template_path)

        absolute_path_to_trunk = filesystem.dirname(self._port.perf_tests_dir())
        results_page = template.replace('%AbsolutePathToWebKitTrunk%', absolute_path_to_trunk)
        results_page = results_page.replace('%PeformanceTestsResultsJSON%', json_output)

        filesystem.write_text_file(self._results_page_path(), results_page)

    def _generate_results_dict(self, timestamp, description, platform, builder_name, build_number):
        revisions = {}
        for (name, path) in self._port.repository_paths():
            scm = SCMDetector(self._host.filesystem, self._host.executive).detect_scm_system(path) or self._host.scm()
            revision = scm.native_revision(path)
            revisions[name] = {'revision': revision, 'timestamp': scm.timestamp_of_native_revision(path, revision)}

        meta_info = {
            'description': description,
            'buildTime': self._datetime_in_ES5_compatible_iso_format(self._utc_timestamp),
            'platform': platform,
            'revisions': revisions,
            'builderName': builder_name,
            'buildNumber': int(build_number) if build_number else None}

        contents = {'tests': {}}
        for key, value in list(meta_info.items()):
            if value:
                contents[key] = value

        for metric in self._results:
            tests = contents['tests']
            path = metric.path()
            for i in range(0, len(path)):
                is_last_token = i + 1 == len(path)
                url = view_source_url('PerformanceTests/' + '/'.join(path[0:i + 1]))
                test_name = path[i]

                tests.setdefault(test_name, {'url': url})
                current_test = tests[test_name]
                if is_last_token:
                    current_test['url'] = view_source_url('PerformanceTests/' + metric.test_file_name())
                    current_test.setdefault('metrics', {})
                    assert metric.name() not in current_test['metrics']
                    test_results = {'current': metric.grouped_iteration_values()}
                    if metric.aggregator():
                        test_results['aggregators'] = [metric.aggregator()]
                    current_test['metrics'][metric.name()] = test_results
                else:
                    current_test.setdefault('tests', {})
                    tests = current_test['tests']

        return contents

    @staticmethod
    def _datetime_in_ES5_compatible_iso_format(datetime):
        return datetime.strftime('%Y-%m-%dT%H:%M:%S.%f')

    def _merge_worker_config_json(self, worker_config_json_path, contents):
        if not self._host.filesystem.isfile(worker_config_json_path):
            _log.error('Missing worker configuration JSON file: {}'.format(worker_config_json_path))
            return None

        try:
            worker_config_json = self._host.filesystem.open_text_file_for_reading(worker_config_json_path)
            worker_config = json.load(worker_config_json)
            for key in worker_config:
                contents['builder' + key.capitalize()] = worker_config[key]
            return contents
        except Exception as error:
            _log.error('Failed to merge worker configuration JSON file {}: {}'.format(worker_config_json_path, error))
        return None

    def _merge_outputs_if_needed(self, output_json_path, output):
        if self._options.reset_results or not self._host.filesystem.isfile(output_json_path):
            return [output]
        try:
            existing_outputs = json.loads(self._host.filesystem.read_text_file(output_json_path))
            return existing_outputs + [output]
        except Exception as error:
            _log.error("Failed to merge output JSON file %s: %s" % (output_json_path, error))
        return None

    def _upload_json(self, test_results_server, json_path, host_path="/api/report", file_uploader=FileUploader):
        hypertext_protocol = ''
        if not test_results_server.startswith('http'):
            hypertext_protocol = 'https://'
        url = hypertext_protocol + test_results_server + host_path
        uploader = file_uploader(url, 120)
        try:
            response = uploader.upload_single_text_file(self._host.filesystem, 'application/json', json_path)
        except Exception as error:
            _log.error("Failed to upload JSON file to %s in 120s: %s" % (url, error))
            return False

        response_body = [string_utils.decode(line, target_type=str).strip('\n') for line in response]
        if response_body != ['OK']:
            try:
                parsed_response = json.loads('\n'.join(response_body))
            except:
                _log.error("Uploaded JSON to %s but got a bad response:" % url)
                for line in response_body:
                    _log.error(line)
                return False
            if parsed_response.get('status') != 'OK':
                _log.error("Uploaded JSON to %s but got an error:" % url)
                _log.error(json.dumps(parsed_response, indent=4))
                return False

        _log.info("JSON file uploaded to %s." % url)
        return True

    def _run_tests_set(self, tests):
        result_count = len(tests)
        failures = 0
        self._results = []

        for i, test in enumerate(tests):
            _log.info('Running %s (%d of %d)' % (test.test_name(), i + 1, len(tests)))
            start_time = time.time()
            metrics = test.run(self._options.time_out_ms, self._options.no_timeout)

            if metrics:
                self._results += metrics
            else:
                failures += 1
                _log.error('FAILED')

            _log.info('Finished: %f s' % (time.time() - start_time))
            _log.info('')

        return failures
