#!/usr/bin/env python

import logging
import os
import subprocess
import time

from osx_browser_driver import OSXBrowserDriver


_log = logging.getLogger(__name__)


class OSXFirefoxDriver(OSXBrowserDriver):
    process_name = 'firefox'
    browser_name = 'firefox'

    def launch_url(self, url, options, browser_build_path):
        self._launch_process(build_dir=browser_build_path, app_name='Firefox.app', url=url, args=[url, '--args', '-width', str(int(self._screen_size().width)), '-height', str(int(self._screen_size().height))])


class OSXFirefoxNightlyDriver(OSXBrowserDriver):
    process_name = 'firefox'
    browser_name = 'firefox-nightly'

    def launch_url(self, url, options, browser_build_path):
        self._launch_process(build_dir=browser_build_path, app_name='FirefoxNightly.app', url=url, args=[url, '--args', '-width', str(int(self._screen_size().width)), '-height', str(int(self._screen_size().height))])
