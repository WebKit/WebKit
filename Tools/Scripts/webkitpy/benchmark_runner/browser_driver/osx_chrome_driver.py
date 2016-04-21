#!/usr/bin/env python

import logging
import os
import subprocess
import time

from osx_browser_driver import OSXBrowserDriver


_log = logging.getLogger(__name__)


class OSXChromeDriver(OSXBrowserDriver):
    process_name = 'Google Chrome'
    browser_name = 'chrome'

    def launch_url(self, url, options, browser_build_path):
        self._launch_process(build_dir=browser_build_path, app_name='Google Chrome.app', url=url, args=['--args', '--homepage', url, '--window-size={width},{height}'.format(width=int(self._screen_size().width), height=int(self._screen_size().height))])


class OSXChromeCanaryDriver(OSXBrowserDriver):
    process_name = 'Google Chrome Canary'
    browser_name = 'chrome-canary'

    def launch_url(self, url, options, browser_build_path):
        self._launch_process(build_dir=browser_build_path, app_name='Google Chrome Canary.app', url=url, args=['--args', '--homepage', url, '--window-size={width},{height}'.format(width=int(self._screen_size().width), height=int(self._screen_size().height))])
