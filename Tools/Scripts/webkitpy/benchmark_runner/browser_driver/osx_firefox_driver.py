#!/usr/bin/env python

import logging
import os
import subprocess
import time

from osx_browser_driver import OSXBrowserDriver


_log = logging.getLogger(__name__)


class OSXFirefoxDriver(OSXBrowserDriver):
    bundleIdentifier = 'org.mozilla.firefox'

    def launchUrl(self, url, browserBuildPath):
        self.launchProcess(buildDir=browserBuildPath, appName='Firefox.app', url=url, args=[url])


class OSXFirefoxNightlyDriver(OSXBrowserDriver):
    bundleIdentifier = 'org.mozilla.nightly'

    def launchUrl(self, url, browserBuildPath):
        self.launchProcess(buildDir=browserBuildPath, appName='FirefoxNightly.app', url=url, args=[url])
