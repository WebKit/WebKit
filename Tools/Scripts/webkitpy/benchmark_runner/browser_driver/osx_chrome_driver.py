#!/usr/bin/env python

import logging
import os
import subprocess
import time

# We assume that this handle can only be used when the platform is OSX
from AppKit import NSRunningApplication
from browser_driver import BrowserDriver


_log = logging.getLogger(__name__)


class OSXChromeDriver(BrowserDriver):

    def prepareEnv(self):
        self.closeBrowsers()
        self.chromePreferences = []

    def launchUrl(self, url, browserBuildPath):
        if not browserBuildPath:
            browserBuildPath = '/Applications/'
        _log.info('Launching chrome: %s with url: %s' % (os.path.join(browserBuildPath, 'Google Chrome.app'), url))
        # FIXME: May need to be modified for develop build, such as setting up libraries
        subprocess.Popen(['open', '-a', os.path.join(browserBuildPath, 'Google Chrome.app'), '--args', '--homepage', url] + self.chromePreferences).communicate()

    def closeBrowsers(self):
        _log.info('Closing all existing chrome processes')
        chromes = NSRunningApplication.runningApplicationsWithBundleIdentifier_('com.google.Chrome')
        for chrome in chromes:
            chrome.terminate()
