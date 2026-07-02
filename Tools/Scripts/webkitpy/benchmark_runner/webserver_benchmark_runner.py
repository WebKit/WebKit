import json
import logging
import os
import time

from webkitcorepy import NullContext, Timeout

from webkitpy.benchmark_runner.benchmark_runner import BenchmarkRunner
from webkitpy.benchmark_runner.http_server_driver.http_server_driver_factory import HTTPServerDriverFactory

from urllib.parse import urljoin

_log = logging.getLogger(__name__)


class WebServerBenchmarkRunner(BenchmarkRunner):
    name = 'webserver'

    def __init__(self, plan_file, local_copy, count_override, timeout_override, build_dir, output_file, platform, browser, browser_path, subtests=None, scale_unit=True, show_iteration_values=False, device_id=None, diagnose_dir=None, generate_pgo_profiles=False, profile_output_dir=None, trace_type=None, profiling_interval=None, browser_args=None, http_server_type='twisted', http_server_port=0):
        # Listen on all interfaces with browser "manual" to make easier test from another devices on the network.
        server_ip = '0.0.0.0' if browser == 'manual' else '127.0.0.1'
        self._http_server_driver = HTTPServerDriverFactory.create(platform, server_type=http_server_type, device_id=device_id, server_ip=server_ip, server_port=http_server_port)
        super().__init__(plan_file, local_copy, count_override, timeout_override, build_dir, output_file, platform, browser, browser_path, subtests=subtests, scale_unit=scale_unit, show_iteration_values=show_iteration_values, device_id=device_id, diagnose_dir=diagnose_dir, generate_pgo_profiles=generate_pgo_profiles, profile_output_dir=profile_output_dir, trace_type=trace_type, profiling_interval=profiling_interval, browser_args=browser_args, http_server_type=http_server_type, http_server_port=http_server_port)
        if self._diagnose_dir:
            self._http_server_driver.set_http_log(os.path.join(self._diagnose_dir, 'run-benchmark-http.log'))

    def _get_result(self, test_url):
        result = self._browser_driver.add_additional_results(test_url, self._http_server_driver.fetch_result())
        assert(not self._http_server_driver.get_return_code())
        return result

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
            if not any(file.startswith('test-successful-screenshot-') for file in os.listdir(self._diagnose_dir)):
                self._browser_driver._save_screenshot_to_path(self._diagnose_dir, f'test-successful-screenshot-{int(time.time())}.jpg')
            self._browser_driver.close_browsers()
        finally:
            self._http_server_driver.kill_server()
        return json.loads(result)
