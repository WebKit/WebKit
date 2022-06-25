import json
import logging
import os
import subprocess
import signal
import sys
import time

from webkitcorepy import Timeout

from webkitpy.benchmark_runner.benchmark_runner import BenchmarkRunner
from webkitpy.benchmark_runner.http_server_driver.http_server_driver_factory import HTTPServerDriverFactory

if sys.version_info > (3, 0):
    from urllib.parse import urljoin
else:
    from urlparse import urljoin

_log = logging.getLogger(__name__)


class WebServerBenchmarkRunner(BenchmarkRunner):
    name = 'webserver'

    def __init__(self, plan_file, local_copy, count_override, build_dir, output_file, platform, browser, browser_path, scale_unit=True, show_iteration_values=False, device_id=None, diagnose_dir=None, profile_output_directory=None):
        self._http_server_driver = HTTPServerDriverFactory.create(platform)
        self._http_server_driver.set_device_id(device_id)
        self._profile_output_directory = profile_output_directory
        super(WebServerBenchmarkRunner, self).__init__(plan_file, local_copy, count_override, build_dir, output_file, platform, browser, browser_path, scale_unit, show_iteration_values, device_id, diagnose_dir, profile_output_directory)
        if self._diagnose_dir:
            self._http_server_driver.set_http_log(os.path.join(self._diagnose_dir, 'run-benchmark-http.log'))

    def _get_result(self, test_url):
        result = self._browser_driver.add_additional_results(test_url, self._http_server_driver.fetch_result())
        assert(not self._http_server_driver.get_return_code())

        if self._profile_output_directory:
            _log.info("Getting benchmark profile results for {} and copying them to {}.".format(test_url, self._profile_output_directory))
            copy_output = subprocess.Popen(r"""log stream --style json --color none | perl -mFile::Basename -mFile::Copy -nle 'if (m/<WEBKIT_LLVM_PROFILE>.*<BEGIN>(.*)<END>/) { (my $l = $1) =~ s/\\\//\//g; my $b = File::Basename::basename($l); my $d = """ + "\"" + self._profile_output_directory + """/$b"; print "Moving $l to $d"; File::Copy::move($l, $d); }'""", shell=True, bufsize=0, preexec_fn=os.setsid)
            time.sleep(1)
            subprocess.call(["notifyutil", "-p", "com.apple.WebKit.profiledata"])
            time.sleep(7)
            # We can kill the shell with kill(), but killing children is harder.
            os.killpg(os.getpgid(copy_output.pid), signal.SIGINT)
            copy_output.kill()
            time.sleep(1)
            _log.debug("Hopefully the benchmark profile has finished writing to disk, moving on.")
        return result

    def _run_one_test(self, web_root, test_file):
        try:
            self._http_server_driver.serve(web_root)
            url = urljoin(self._http_server_driver.base_url(), self._plan_name + '/' + test_file)
            self._browser_driver.launch_url(url, self._plan['options'], self._build_dir, self._browser_path)
            with Timeout(self._plan['timeout']):
                result = self._get_result(url)
        except Exception as error:
            self._browser_driver.diagnose_test_failure(self._diagnose_dir, error)
            raise error
        finally:
            self._browser_driver.close_browsers()
            self._http_server_driver.kill_server()

        return json.loads(result)
