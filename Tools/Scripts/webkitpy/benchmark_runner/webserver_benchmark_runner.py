import collections
import json
import logging
import os
import subprocess
import signal
import sys
import time

from webkitcorepy import NullContext, Timeout

from webkitpy.benchmark_runner.benchmark_runner import BenchmarkRunner
from webkitpy.benchmark_runner.http_server_driver.http_server_driver_factory import HTTPServerDriverFactory

if sys.version_info > (3, 0):
    from urllib.parse import urljoin
else:
    from urlparse import urljoin

_log = logging.getLogger(__name__)


class WebServerBenchmarkRunner(BenchmarkRunner):
    name = 'webserver'

    def __init__(self, plan_file, local_copy, count_override, timeout_override, build_dir, output_file, platform, browser, browser_path, subtests=None, scale_unit=True, show_iteration_values=False, device_id=None, diagnose_dir=None, pgo_profile_output_dir=None, profile_output_dir=None):
        self._http_server_driver = HTTPServerDriverFactory.create(platform)
        self._http_server_driver.set_device_id(device_id)
        super(WebServerBenchmarkRunner, self).__init__(plan_file, local_copy, count_override, timeout_override, build_dir, output_file, platform, browser, browser_path, subtests, scale_unit, show_iteration_values, device_id, diagnose_dir, pgo_profile_output_dir, profile_output_dir)
        if self._diagnose_dir:
            self._http_server_driver.set_http_log(os.path.join(self._diagnose_dir, 'run-benchmark-http.log'))

    def _get_result(self, test_url):
        result = self._browser_driver.add_additional_results(test_url, self._http_server_driver.fetch_result())
        assert(not self._http_server_driver.get_return_code())

        if self._pgo_profile_output_dir:
            _log.info('Getting benchmark PGO profile results for {} and copying them to {}.'.format(test_url, self._pgo_profile_output_dir))
            copy_output = subprocess.Popen(r"""log stream --style json --color none | perl -mFile::Basename -mFile::Copy -nle 'if (m/<WEBKIT_LLVM_PROFILE>.*<BEGIN>(.*)<END>/) { (my $l = $1) =~ s/\\\//\//g; my $b = File::Basename::basename($l); my $d = """ + "\"" + self._pgo_profile_output_dir + """/$b"; print "Moving $l to $d"; File::Copy::move($l, $d); }'""", shell=True, bufsize=0, preexec_fn=os.setsid)
            time.sleep(1)
            subprocess.call(['notifyutil', '-p', 'com.apple.WebKit.profiledata'])
            time.sleep(7)
            # We can kill the shell with kill(), but killing children is harder.
            os.killpg(os.getpgid(copy_output.pid), signal.SIGINT)
            copy_output.kill()
            time.sleep(1)
            _log.debug('Hopefully the benchmark profile has finished writing to disk, moving on.')
        return result

    def _construct_subtest_url(self, subtests):
        print(subtests)
        if not subtests or not isinstance(subtests, collections.Mapping) or 'subtest_url_format' not in self._plan:
            return ''
        subtest_url = ''
        for suite, tests in subtests.items():
            for test in tests:
                subtest_url = subtest_url + '&' + self._plan['subtest_url_format'].replace('${SUITE}', suite).replace('${TEST}', test)
        return subtest_url

    def _run_one_test(self, web_root, test_file, iteration):
        enable_profiling = True if self._profile_output_dir else False
        profile_filename = '{}/{}-{}'.format(self._profile_output_dir, self._plan_name, iteration)
        try:
            self._http_server_driver.serve(web_root)
            url = urljoin(self._http_server_driver.base_url(), self._plan_name + '/' + test_file + self._construct_subtest_url(self._subtests))
            if enable_profiling:
                context = self._browser_driver.profile(profile_filename)
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
        finally:
            self._browser_driver.close_browsers()
            self._http_server_driver.kill_server()

        return json.loads(result)
