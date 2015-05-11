#!/usr/bin/env python

import logging
import os
import subprocess
import time

from osx_browser_driver import OSXBrowserDriver


_log = logging.getLogger(__name__)


class OSXChromeDriver(OSXBrowserDriver):
    bundleIdentifier = 'com.google.Chrome'

    def launchUrl(self, url, browserBuildPath):
        self.launchProcess(buildDir=browserBuildPath, appName='Google Chrome.app', url=url, args=['--args', '--homepage', url])


class OSXChromeCanaryDriver(OSXBrowserDriver):
    bundleIdentifier = 'com.google.Chrome.canary'

    def launchUrl(self, url, browserBuildPath):
        self.launchProcess(buildDir=browserBuildPath, appName='Google Chrome Canary.app', url=url, args=['--args', '--homepage', url])
