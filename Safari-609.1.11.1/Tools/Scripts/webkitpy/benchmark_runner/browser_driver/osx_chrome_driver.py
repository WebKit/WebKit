#!/usr/bin/env python

import logging
import os

from osx_browser_driver import OSXBrowserDriver


_log = logging.getLogger(__name__)


class OSXChromeDriver(OSXBrowserDriver):
    process_name = 'Google Chrome'
    browser_name = 'chrome'
    app_name = 'Google Chrome.app'
    bundle_id = 'com.google.Chrome'

    def launch_url(self, url, options, browser_build_path, browser_path):
        # FIXME: handle self._browser_path.
        args_with_url = self._insert_url(create_args(), 2, url)
        self._launch_process(build_dir=browser_build_path, app_name=self.app_name, url=url, args=args_with_url)

    def launch_driver(self, url, options, browser_build_path):
        import webkitpy.thirdparty.autoinstalled.selenium
        from selenium import webdriver
        chrome_options = create_chrome_options()
        if browser_build_path:
            app_path = os.path.join(browser_build_path, self.app_name)
            binary_path = os.path.join(app_path, "Contents/MacOS", self.process_name)
            chrome_options.binary_location = binary_path
        driver_executable = self.webdriver_binary_path
        driver = webdriver.Chrome(chrome_options=chrome_options, executable_path=driver_executable)
        self._launch_webdriver(url=url, driver=driver)
        return driver


class OSXChromeCanaryDriver(OSXBrowserDriver):
    process_name = 'Google Chrome Canary'
    browser_name = 'chrome-canary'
    app_name = 'Google Chrome Canary.app'
    bundle_id = 'com.google.Chrome.canary'

    def launch_url(self, url, options, browser_build_path, browser_path):
        # FIXME: handle self._browser_path.
        args_with_url = self._insert_url(create_args(), 2, url)
        self._launch_process(build_dir=browser_build_path, app_name=self.app_name, url=url, args=args_with_url)

    def launch_driver(self, url, options, browser_build_path):
        import webkitpy.thirdparty.autoinstalled.selenium
        from selenium import webdriver
        chrome_options = create_chrome_options()
        if not browser_build_path:
            browser_build_path = '/Applications/'
        app_path = os.path.join(browser_build_path, self.app_name)
        binary_path = os.path.join(app_path, "Contents/MacOS", self.process_name)
        chrome_options.binary_location = binary_path
        driver_executable = self.webdriver_binary_path
        driver = webdriver.Chrome(chrome_options=chrome_options, executable_path=driver_executable)
        self._launch_webdriver(url=url, driver=driver)
        return driver


def create_args():
    args = ['--args', '--homepage', create_window_size_arg()]
    return args


def create_chrome_options():
    from selenium.webdriver.chrome.options import Options
    chrome_options = Options()
    chrome_options.add_argument("--disable-web-security")
    chrome_options.add_argument("--user-data-dir")
    chrome_options.add_argument("--disable-extensions")
    chrome_options.add_argument(create_window_size_arg())
    return chrome_options


def create_window_size_arg():
    window_size_arg = '--window-size={width},{height}'.format(width=int(OSXBrowserDriver._screen_size().width), height=int(OSXBrowserDriver._screen_size().height))
    return window_size_arg
