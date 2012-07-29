#!/usr/bin/env python
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

import json
import logging
import optparse
import re
import sys
import time

from webkitpy.common import find_files
from webkitpy.common.host import Host
from webkitpy.common.net.file_uploader import FileUploader
from webkitpy.layout_tests.views import printing
from webkitpy.performance_tests.perftest import PerfTestFactory
from webkitpy.performance_tests.perftest import ReplayPerfTest


_log = logging.getLogger(__name__)


class PerfTestsRunner(object):
    _default_branch = 'webkit-trunk'
    _EXIT_CODE_BAD_BUILD = -1
    _EXIT_CODE_BAD_JSON = -2
    _EXIT_CODE_FAILED_UPLOADING = -3
    _EXIT_CODE_BAD_PREPARATION = -4

    def __init__(self, args=None, port=None):
        self._options, self._args = PerfTestsRunner._parse_args(args)
        if port:
            self._port = port
            self._host = self._port.host
        else:
            self._host = Host()
            self._port = self._host.port_factory.get(self._options.platform, self._options)
        self._host._initialize_scm()
        self._webkit_base_dir_len = len(self._port.webkit_base())
        self._base_path = self._port.perf_tests_dir()
        self._results = {}
        self._timestamp = time.time()

    @staticmethod
    def _parse_args(args=None):
        perf_option_list = [
            optparse.make_option('--debug', action='store_const', const='Debug', dest="configuration",
                help='Set the configuration to Debug'),
            optparse.make_option('--release', action='store_const', const='Release', dest="configuration",
                help='Set the configuration to Release'),
            optparse.make_option("--platform",
                help="Specify port/platform being tested (i.e. chromium-mac)"),
            optparse.make_option("--chromium",
                action="store_const", const='chromium', dest='platform', help='Alias for --platform=chromium'),
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
            optparse.make_option("--pause-before-testing", dest="pause_before_testing", action="store_true", default=False,
                help="Pause before running the tests to let user attach a performance monitor."),
            optparse.make_option("--output-json-path",
                help="Filename of the JSON file that summaries the results."),
            optparse.make_option("--source-json-path",
                help="Path to a JSON file to be merged into the JSON file when --output-json-path is present."),
            optparse.make_option("--test-results-server",
                help="Upload the generated JSON file to the specified server when --output-json-path is present."),
            optparse.make_option("--webkit-test-runner", "-2", action="store_true",
                help="Use WebKitTestRunner rather than DumpRenderTree."),
            optparse.make_option("--replay", dest="replay", action="store_true", default=False,
                help="Run replay tests."),
            optparse.make_option("--force", dest="skipped", action="store_true", default=False,
                help="Run all tests, including the ones in the Skipped list."),
            ]
        return optparse.OptionParser(option_list=(perf_option_list)).parse_args(args)

    def _collect_tests(self):
        """Return the list of tests found."""

        test_extensions = ['.html', '.svg']
        if self._options.replay:
            test_extensions.append('.replay')

        def _is_test_file(filesystem, dirname, filename):
            return filesystem.splitext(filename)[1] in test_extensions

        filesystem = self._host.filesystem

        paths = []
        for arg in self._args:
            paths.append(arg)
            relpath = filesystem.relpath(arg, self._base_path)
            if relpath:
                paths.append(relpath)

        skipped_directories = set(['.svn', 'resources'])
        test_files = find_files.find(filesystem, self._base_path, paths, skipped_directories, _is_test_file)
        tests = []
        for path in test_files:
            relative_path = self._port.relative_perf_test_filename(path).replace('\\', '/')
            if self._port.skips_perf_test(relative_path) and not self._options.skipped:
                continue
            test = PerfTestFactory.create_perf_test(self._port, relative_path, path)
            tests.append(test)

        return tests

    def run(self):
        if not self._port.check_build(needs_http=False):
            _log.error("Build not up to date for %s" % self._port._path_to_driver())
            return self._EXIT_CODE_BAD_BUILD

        tests = self._collect_tests()
        _log.info("Running %d tests" % len(tests))

        for test in tests:
            if not test.prepare(self._options.time_out_ms):
                return self._EXIT_CODE_BAD_PREPARATION

        unexpected = self._run_tests_set(sorted(list(tests), key=lambda test: test.test_name()), self._port)

        options = self._options
        if self._options.output_json_path:
            # FIXME: Add --branch or auto-detect the branch we're in
            test_results_server = options.test_results_server
            branch = self._default_branch if test_results_server else None
            build_number = int(options.build_number) if options.build_number else None

            if not self._generate_json(self._timestamp, options.output_json_path, options.source_json_path,
                not test_results_server,
                branch, options.platform, options.builder_name, build_number) and not unexpected:
                return self._EXIT_CODE_BAD_JSON

            if test_results_server and not self._upload_json(test_results_server, options.output_json_path):
                return self._EXIT_CODE_FAILED_UPLOADING

        return unexpected

    def _generate_json(self, timestamp, output_json_path, source_json_path, should_generate_results_page,
        branch, platform, builder_name, build_number):

        contents = {'timestamp': int(timestamp), 'results': self._results}
        for (name, path) in self._port.repository_paths():
            contents[name + '-revision'] = self._host.scm().svn_revision(path)

        for key, value in {'branch': branch, 'platform': platform, 'builder-name': builder_name, 'build-number': build_number}.items():
            if value:
                contents[key] = value

        filesystem = self._host.filesystem
        succeeded = False
        if source_json_path:
            try:
                source_json_file = filesystem.open_text_file_for_reading(source_json_path)
                source_json = json.load(source_json_file)
                contents = dict(source_json.items() + contents.items())
                succeeded = True
            except IOError, error:
                _log.error("Failed to read %s: %s" % (source_json_path, error))
            except ValueError, error:
                _log.error("Failed to parse %s: %s" % (source_json_path, error))
            except TypeError, error:
                _log.error("Failed to merge JSON files: %s" % error)
            if not succeeded:
                return False

        if should_generate_results_page:
            if filesystem.isfile(output_json_path):
                existing_contents = json.loads(filesystem.read_text_file(output_json_path))
                existing_contents.append(contents)
                contents = existing_contents
            else:
                contents = [contents]

        serialized_contents = json.dumps(contents)
        filesystem.write_text_file(output_json_path, serialized_contents)

        if should_generate_results_page:
            jquery_path = filesystem.join(self._port.perf_tests_dir(), 'Dromaeo/resources/dromaeo/web/lib/jquery-1.6.4.js')
            jquery = filesystem.read_text_file(jquery_path)

            template_path = filesystem.join(self._port.perf_tests_dir(), 'resources/results-template.html')
            template = filesystem.read_text_file(template_path)

            results_page = template.replace('<?WebKitPerfTestRunnerInsertionPoint?>',
                '<script>%s</script><script id="json">%s</script>' % (jquery, serialized_contents))

            filesystem.write_text_file(filesystem.splitext(output_json_path)[0] + '.html', results_page)

        return True

    def _upload_json(self, test_results_server, json_path, file_uploader=FileUploader):
        uploader = file_uploader("https://%s/api/test/report" % test_results_server, 120)
        try:
            response = uploader.upload_single_text_file(self._host.filesystem, 'application/json', json_path)
        except Exception, error:
            _log.error("Failed to upload JSON file in 120s: %s" % error)
            return False

        response_body = [line.strip('\n') for line in response]
        if response_body != ['OK']:
            _log.error("Uploaded JSON but got a bad response:")
            for line in response_body:
                _log.error(line)
            return False

        _log.info("JSON file uploaded.")
        return True

    def _print_status(self, tests, expected, unexpected):
        if len(tests) == expected + unexpected:
            status = "Ran %d tests" % len(tests)
        else:
            status = "Running %d of %d tests" % (expected + unexpected + 1, len(tests))
        if unexpected:
            status += " (%d didn't run)" % unexpected
        _log.info(status)

    def _run_tests_set(self, tests, port):
        result_count = len(tests)
        expected = 0
        unexpected = 0
        driver = None

        for test in tests:
            driver = port.create_driver(worker_number=1, no_timeout=True)

            if self._options.pause_before_testing:
                driver.start()
                if not self._host.user.confirm("Ready to run test?"):
                    driver.stop()
                    return unexpected

            _log.info('Running %s (%d of %d)' % (test.test_name(), expected + unexpected + 1, len(tests)))
            if self._run_single_test(test, driver):
                expected = expected + 1
            else:
                unexpected = unexpected + 1

            _log.info('')

            driver.stop()

        return unexpected

    def _run_single_test(self, test, driver):
        start_time = time.time()

        new_results = test.run(driver, self._options.time_out_ms)
        if new_results:
            self._results.update(new_results)
        else:
            _log.error('FAILED')

        _log.debug("Finished: %f s" % (time.time() - start_time))

        return new_results != None
