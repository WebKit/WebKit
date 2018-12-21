#!/usr/bin/env python

import json
import logging

from benchmark_runner import BenchmarkRunner


_log = logging.getLogger(__name__)


class WebDriverBenchmarkRunner(BenchmarkRunner):
    name = 'webdriver'

    def _get_result(self, driver):
        result = driver.execute_script("return window.webdriver_results")
        return result

    def _run_one_test(self, web_root, test_file):
        import webkitpy.thirdparty.autoinstalled.selenium
        from selenium.webdriver.support.ui import WebDriverWait
        result = None
        try:
            url = 'file://{root}/{plan_name}/{test_file}'.format(root=web_root, plan_name=self._plan_name, test_file=test_file)
            driver = self._browser_driver.launch_driver(url, self._plan['options'], self._build_dir, self._browser_path)
            _log.info('Waiting on results from web browser')
            result = WebDriverWait(driver, self._plan['timeout'], poll_frequency=1.0).until(self._get_result)
            driver.quit()
        finally:
            self._browser_driver.close_browsers()

        return json.loads(result)
