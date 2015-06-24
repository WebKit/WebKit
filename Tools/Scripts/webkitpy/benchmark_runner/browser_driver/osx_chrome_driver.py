#!/usr/bin/env python

import logging
import os
import subprocess
import time

from osx_browser_driver import OSXBrowserDriver


_log = logging.getLogger(__name__)


class OSXChromeDriver(OSXBrowserDriver):
    bundle_identifier = 'com.google.Chrome'
    browser_name = 'chrome'

    def launch_url(self, url, browser_build_path):
        self._launch_process(build_dir=browser_build_path, app_name='Google Chrome.app', url=url, args=['--args', '--homepage', url, '--window-size={width},{height}'.format(width=int(self._screen_size().width), height=int(self._screen_size().height))])


class OSXChromeCanaryDriver(OSXBrowserDriver):
    bundle_identifier = 'com.google.Chrome.canary'
    browser_name = 'chrome-canary'

    def launch_url(self, url, browser_build_path):
        self._launch_process(build_dir=browser_build_path, app_name='Google Chrome Canary.app', url=url, args=['--args', '--homepage', url, '--window-size={width},{height}'.format(width=int(self._screen_size().width), height=int(self._screen_size().height))])
