#!/usr/bin/env python

import logging
import os

from osx_browser_driver import OSXBrowserDriver


_log = logging.getLogger(__name__)


class OSXFirefoxDriver(OSXBrowserDriver):
    process_name = 'firefox'
    browser_name = 'firefox'
    app_name = 'Firefox.app'
    bundle_id = 'org.mozilla.firefox'

    def launch_url(self, url, options, browser_build_path, browser_path):
        # FIXME: handle self._browser_path.
        args_with_url = self._insert_url(create_args(), 0, url)
        self._launch_process(build_dir=browser_build_path, app_name=self.app_name, url=url, args=args_with_url)

    def launch_driver(self, url, options, browser_build_path):
        import webkitpy.thirdparty.autoinstalled.selenium
        from selenium import webdriver
        from selenium.webdriver.firefox.options import Options
        firefox_options = Options()
        if browser_build_path:
            app_path = os.path.join(browser_build_path, self.app_name)
            binary_path = os.path.join(app_path, "Contents/MacOS", self.process_name)
            firefox_options.binary_location = binary_path
        driver_executable = self.webdriver_binary_path
        driver = webdriver.Firefox(firefox_options=firefox_options, executable_path=driver_executable)
        self._launch_webdriver(url=url, driver=driver)
        return driver


class OSXFirefoxNightlyDriver(OSXBrowserDriver):
    process_name = 'firefox'
    browser_name = 'firefox-nightly'
    app_name = 'FirefoxNightly.app'
    bundle_id = 'org.mozilla.firefox'


    def launch_url(self, url, options, browser_build_path, browser_path):
        # FIXME: handle self._browser_path.
        args_with_url = self._insert_url(create_args(), 0, url)
        self._launch_process(build_dir=browser_build_path, app_name=self.app_name, url=url, args=args_with_url)

    def launch_driver(self, url, options, browser_build_path):
        import webkitpy.thirdparty.autoinstalled.selenium
        from selenium import webdriver
        from selenium.webdriver.firefox.options import Options
        firefox_options = Options()
        if not browser_build_path:
            browser_build_path = '/Applications/'
        app_path = os.path.join(browser_build_path, self.app_name)
        binary_path = os.path.join(app_path, "Contents/MacOS", self.process_name)
        firefox_options.binary_location = binary_path
        driver_executable = self.webdriver_binary_path
        driver = webdriver.Firefox(firefox_options=firefox_options, executable_path=driver_executable)
        self._launch_webdriver(url=url, driver=driver)
        return driver


def create_args():
    args = ['--args', '-width', str(int(OSXBrowserDriver._screen_size().width)), '-height', str(int(OSXBrowserDriver._screen_size().height))]
    return args
