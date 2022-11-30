# Copyright (C) 2018 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import argparse
import configparser
import json
import logging
import os
import tempfile
import urllib

from webkitpy.benchmark_runner.benchmark_runner import BenchmarkRunner
from webkitpy.benchmark_runner.browser_driver.browser_driver_factory import BrowserDriverFactory
from webkitpy.benchmark_runner.webserver_benchmark_runner import WebServerBenchmarkRunner
from webkitpy.benchmark_runner.run_benchmark import default_browser, default_platform, benchmark_runner_subclasses

_log = logging.getLogger(__name__)


def parse_args():
    parser = argparse.ArgumentParser(description='Automate the browser based performance benchmarks')
    # browserperfdash specific arguments.
    parser.add_argument('--config-file', dest='config_file', default=None, required=True, help='Configuration file for sending the results to the performance dashboard server(s).')
    parser.add_argument('--browser-version', dest='browser_version', default=None, required=True, help='A string that identifies the browser version.')
    # arguments shared with run-benchmark.
    parser.add_argument('--build-directory', dest='buildDir', help='Path to the browser executable (e.g. WebKitBuild/Release/).')
    parser.add_argument('--platform', dest='platform', default=default_platform(), choices=BrowserDriverFactory.available_platforms())
    parser.add_argument('--browser', dest='browser', default=default_browser(), choices=BrowserDriverFactory.available_browsers())
    parser.add_argument('--driver', default=WebServerBenchmarkRunner.name, choices=benchmark_runner_subclasses.keys(), help='Use the specified benchmark driver. Defaults to %s.' % WebServerBenchmarkRunner.name)
    parser.add_argument('--local-copy', dest='localCopy', help='Path to a local copy of the benchmark (e.g. PerformanceTests/SunSpider/).')
    parser.add_argument('--count', dest='countOverride', type=int, help='Number of times to run the benchmark (e.g. 5).')
    parser.add_argument('--timeout', dest='timeoutOverride', type=int, help='Number of seconds to wait for the benchmark to finish (e.g. 600).')
    mutual_group = parser.add_mutually_exclusive_group(required=True)
    mutual_group.add_argument('--plan', dest='plan', help='Benchmark plan to run. e.g. speedometer, jetstream')
    mutual_group.add_argument('--allplans', action='store_true', help='Run all available benchmark plans sequentially')
    args = parser.parse_args()
    return args


class BrowserPerfDashRunner(object):
    name = 'browserperfdash_runner'

    def __init__(self, args):
        self._args = args
        self._plandir = os.path.abspath(BenchmarkRunner.plan_directory())
        if not os.path.isdir(self._plandir):
            raise Exception('Cant find plandir: {plandir}'.format(plandir=self._plandir))
        self._parse_config_file(self._args.config_file)
        # This is the dictionary that will be sent as the HTTP POST request that browserperfdash expects
        # (as defined in https://github.com/Igalia/browserperfdash/blob/master/docs/design-document.md)
        # - The bot_* data its obtained from the config file
        # - the browser_* data is given at startup time via command-line arguments
        # - The test_* data is generated after each test run.
        self._result_data = {'bot_id': None,
                             'bot_password': None,
                             'browser_id': self._args.browser,
                             'browser_version': self._args.browser_version,
                             'test_id': None,
                             'test_version': None,
                             'test_data': None}

    def _parse_config_file(self, config_file):
        if not os.path.isfile(config_file):
            raise Exception('Can not open config file for uploading results at: {config_file}'.format(config_file=config_file))
        self._config_parser = configparser.RawConfigParser()
        self._config_parser.read(config_file)

    def _get_test_version_string(self, plan_name):
        plan_file_path = os.path.join(self._plandir, '{plan_name}.plan'.format(plan_name=plan_name))
        with open(plan_file_path, 'r') as plan_fd:
            plan_json = json.load(plan_fd)
            if 'svn_source' in plan_json:
                return plan_json['svn_source']
            elif 'remote_archive' in plan_json:
                return plan_json['remote_archive']
            else:
                raise Exception('Can not upload results because is not possible to determine test version for test plan: {plan_file_path}'.format(plan_file_path=plan_file_path))

    def _get_test_data_json_string(self, temp_result_file):
        temp_result_file.flush()
        temp_result_file.seek(0)
        temp_result_json = json.load(temp_result_file)
        if 'debugOutput' in temp_result_json:
            del temp_result_json['debugOutput']
        return json.dumps(temp_result_json)

    # urllib.request.urlopen always raises an exception when the http return code is not 200
    # so this wraps the call to return the HTTPError object instead of raising the exception.
    # The HTTPError object can be treated later as a http.client.HTTPResponse object.
    def _send_post_request_data(self, post_url, post_data):
        try:
            return urllib.request.urlopen(post_url, post_data)
        except urllib.error.HTTPError as e:
            return e

    def _upload_result(self):
        upload_failed = False
        for server in self._config_parser.sections():
            self._result_data['bot_id'] = self._config_parser.get(server, 'bot_id')
            self._result_data['bot_password'] = self._config_parser.get(server, 'bot_password')
            post_data = urllib.parse.urlencode(self._result_data).encode('utf-8')
            post_url = self._config_parser.get(server, 'post_url')
            try:
                post_request = self._send_post_request_data(post_url, post_data)
                if post_request.getcode() == 200:
                    _log.info('Sucesfully uploaded results to server {server_name} for test {test_name} and browser {browser_name} version {browser_version}'.format(
                               server_name=server, test_name=self._result_data['test_id'], browser_name=self._result_data['browser_id'], browser_version=self._result_data['browser_version']))
                else:
                    upload_failed = True
                    _log.error('The server {server_name} returned an error code: {http_error}'.format(server_name=server, http_error=post_request.getcode()))
                    _log.error('The error text from the server {server_name} was: "{error_text}"'.format(server_name=server, error_text=post_request.read().decode('utf-8')))
            except Exception as e:
                upload_failed = True
                _log.error('Exception while trying to upload results to server {server_name}'.format(server_name=server))
                _log.error(e)
        return not upload_failed

    def run(self):
        failed = []
        worked = []
        skipped = []
        planlist = []
        if self._args.plan:
            if not os.path.isfile(os.path.join(self._plandir, '{plan_name}.plan'.format(plan_name=self._args.plan))):
                raise Exception('Cant find a file named {plan_name}.plan in directory {plan_directory}'.format(plan_name=self._args.plan, plan_directory=self._plandir))
            planlist = [self._args.plan]
        elif self._args.allplans:
            planlist = BenchmarkRunner.available_plans()
            skippedfile = os.path.join(self._plandir, 'Skipped')
            if not planlist:
                raise Exception('Cant find any plan in the directory {plan_directory}'.format(plan_directory=self._plandir))
            if os.path.isfile(skippedfile):
                skipped = [line.strip() for line in open(skippedfile) if not line.startswith('#') and len(line) > 1]

        if len(planlist) < 1:
            _log.error('No benchmarks plans available to run in directory {plan_directory}'.format(plan_directory=self._plandir))
            return 1

        _log.info('Starting benchmark for browser {browser} and version {browser_version}'.format(browser=self._args.browser, browser_version=self._args.browser_version))

        iteration_count = 0
        for plan in sorted(planlist):
            iteration_count += 1
            if plan in skipped:
                _log.info('Skipping benchmark plan: {plan_name} because is listed on the Skipped file [benchmark {iteration} of {total}]'.format(plan_name=plan, iteration=iteration_count, total=len(planlist)))
                continue
            _log.info('Starting benchmark plan: {plan_name} [benchmark {iteration} of {total}]'.format(plan_name=plan, iteration=iteration_count, total=len(planlist)))
            try:
                # Run test and save test info
                with tempfile.NamedTemporaryFile() as temp_result_file:
                    benchmark_runner_class = benchmark_runner_subclasses[self._args.driver]
                    runner = benchmark_runner_class(plan, self._args.localCopy, self._args.countOverride, self._args.timeoutOverride, self._args.buildDir, temp_result_file.name, self._args.platform, self._args.browser, None)
                    runner.execute()
                    _log.info('Finished benchmark plan: {plan_name}'.format(plan_name=plan))
                    # Fill test info for upload
                    self._result_data['test_id'] = plan
                    self._result_data['test_version'] = self._get_test_version_string(plan)
                    # Fill obtained test results for upload
                    self._result_data['test_data'] = self._get_test_data_json_string(temp_result_file)

                # Now upload data to server(s)
                _log.info('Uploading results for plan: {plan_name} and browser {browser} version {browser_version}'.format(plan_name=plan, browser=self._args.browser, browser_version=self._args.browser_version))
                if self._upload_result():
                    worked.append(plan)
                else:
                    failed.append(plan)

            except KeyboardInterrupt:
                raise
            except:
                failed.append(plan)
                _log.exception('Error running benchmark plan: {plan_name}'.format(plan_name=plan))

        if len(worked) > 0:
            _log.info('The following benchmark plans have been upload succesfully: {list_plan_worked}'.format(list_plan_worked=worked))

        if len(failed) > 0:
            _log.error('The following benchmark plans have failed to run or to upload: {list_plan_failed}'.format(list_plan_failed=failed))
            return len(failed)

        return 0


def format_logger(logger):
    logger.setLevel(logging.INFO)
    ch = logging.StreamHandler()
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
    ch.setFormatter(formatter)
    logger.addHandler(ch)


def main():
    perfdashrunner = BrowserPerfDashRunner(parse_args())
    return perfdashrunner.run()
