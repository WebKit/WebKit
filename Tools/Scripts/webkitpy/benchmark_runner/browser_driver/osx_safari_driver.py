#!/usr/bin/env python

import logging
import os
import subprocess
import time

# We assume that this handle can only be used when the platform is OSX.
from AppKit import NSRunningApplication
from browser_driver import BrowserDriver
from webkitpy.benchmark_runner.utils import forceRemove


_log = logging.getLogger(__name__)


class OSXSafariDriver(BrowserDriver):

    def prepareEnv(self):
        self.closeBrowsers()
        forceRemove(os.path.join(os.path.expanduser('~'), 'Library/Saved Application State/com.apple.Safari.savedState'))
        forceRemove(os.path.join(os.path.expanduser('~'), 'Library/Safari/LastSession.plist'))
        self.safariPreferences = ["-HomePage", "about:blank", "-WarnAboutFraudulentWebsites", "0", "-ExtensionsEnabled", "0", "-ShowStatusBar", "0", "-NewWindowBehavior", "1", "-NewTabBehavior", "1"]

    def launchUrl(self, url, browserBuildPath=None):
        args = [os.path.join(browserBuildPath, 'Safari.app/Contents/MacOS/Safari')]
        args.extend(self.safariPreferences)
        _log.info('Launching safari: %s with url: %s' % (args[0], url))
        subprocess.Popen(args, env={'DYLD_FRAMEWORK_PATH': browserBuildPath, 'DYLD_LIBRARY_PATH': browserBuildPath}, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        # Stop for initialization of the safari process, otherwise, open
        # command may use the system safari.
        time.sleep(3)
        subprocess.Popen(['open', url])

    def closeBrowsers(self):
        _log.info('Closing all existing safari processes')
        safariInstances = NSRunningApplication.runningApplicationsWithBundleIdentifier_('com.apple.Safari')
        for safariInstance in safariInstances:
            safariInstance.terminate()
