import collections
import json
import logging
import os
import subprocess
import signal
import time

from webkitcorepy import NullContext, Timeout

from webkitpy.benchmark_runner.benchmark_runner import BenchmarkRunner
from webkitpy.benchmark_runner.http_server_driver.http_server_driver_factory import HTTPServerDriverFactory

from urllib.parse import urljoin

_log = logging.getLogger(__name__)


class WebServerBenchmarkRunner(BenchmarkRunner):
    name = 'webserver'

    def __init__(self, plan_file, local_copy, count_override, timeout_override, build_dir, output_file, platform, browser, browser_path, subtests=None, scale_unit=True, show_iteration_values=False, device_id=None, diagnose_dir=None, pgo_profile_output_dir=None, profile_output_dir=None, trace_type=None, profiling_interval=None, browser_args=None, http_server_type='twisted'):
        self._http_server_driver = HTTPServerDriverFactory.create(platform, server_type=http_server_type, device_id=device_id)
        super(WebServerBenchmarkRunner, self).__init__(plan_file, local_copy, count_override, timeout_override, build_dir, output_file, platform, browser, browser_path, subtests, scale_unit, show_iteration_values, device_id, diagnose_dir, pgo_profile_output_dir, profile_output_dir, trace_type, profiling_interval, browser_args)
        if self._diagnose_dir:
            self._http_server_driver.set_http_log(os.path.join(self._diagnose_dir, 'run-benchmark-http.log'))

    def _get_result(self, test_url):
        result = self._browser_driver.add_additional_results(test_url, self._http_server_driver.fetch_result())
        assert(not self._http_server_driver.get_return_code())
        return result

    def _construct_subtest_url(self, subtests):
        if not subtests or not isinstance(subtests, collections.abc.Mapping) or 'subtest_url_format' not in self._plan:
            return ''
        subtest_url = ''
        for suite, tests in subtests.items():
            for test in tests:
                subtest_url = subtest_url + '&' + self._plan['subtest_url_format'].replace('${SUITE}', suite).replace('${TEST}', test)
        return subtest_url

    def _run_one_test(self, web_root, test_file, iteration):
        enable_profiling = self._profile_output_dir and self._trace_type
        profile_filename = '{}-{}'.format(self._plan_name, iteration)
        try:
            self._http_server_driver.serve(web_root)
            url = urljoin(self._http_server_driver.base_url(), self._plan_name + '/' + test_file + self._construct_subtest_url(self._subtests))
            if '?' not in url:
                url = url.replace('&', '?', 1)
            if enable_profiling:
                context = self._browser_driver.profile(self._profile_output_dir, profile_filename, self._profiling_interval, trace_type=self._trace_type)
            else:
                context = NullContext()
            with context:
                self._browser_driver.launch_url(url, self._plan['options'], self._build_dir, self._browser_path)
                timeout = self._plan['timeout']
                with Timeout(timeout), self._browser_driver.prevent_sleep(timeout):
                    result = self._get_result(url)
        except Exception as error:
            self._browser_driver.diagnose_test_failure(self._diagnose_dir, error)
            raise error
        else:
            self._browser_driver.close_browsers()
        finally:
            self._http_server_driver.kill_server()
        return json.loads(result)
