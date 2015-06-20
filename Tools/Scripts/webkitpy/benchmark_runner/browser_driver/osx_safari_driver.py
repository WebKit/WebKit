#!/usr/bin/env python

import logging
import os
import re
import subprocess
import time

from osx_browser_driver import OSXBrowserDriver
from webkitpy.benchmark_runner.utils import forceRemove


_log = logging.getLogger(__name__)


class OSXSafariDriver(OSXBrowserDriver):
    bundleIdentifier = 'com.apple.Safari'

    def prepareEnv(self, deviceID):
        self.safariProcess = None
        super(OSXSafariDriver, self).prepareEnv(deviceID)
        forceRemove(os.path.join(os.path.expanduser('~'), 'Library/Saved Application State/com.apple.Safari.savedState'))
        forceRemove(os.path.join(os.path.expanduser('~'), 'Library/Safari/LastSession.plist'))
        self.maximizeWindow()
        self.safariPreferences = ["-HomePage", "about:blank", "-WarnAboutFraudulentWebsites", "0", "-ExtensionsEnabled", "0", "-ShowStatusBar", "0", "-NewWindowBehavior", "1", "-NewTabBehavior", "1"]

    def launchUrl(self, url, browserBuildPath):
        args = ['/Applications/Safari.app/Contents/MacOS/Safari']
        env = {}
        if browserBuildPath:
            safariAppInBuildPath = os.path.join(browserBuildPath, 'Safari.app/Contents/MacOS/Safari')
            if os.path.exists(safariAppInBuildPath):
                args = [safariAppInBuildPath]
                env = {'DYLD_FRAMEWORK_PATH': browserBuildPath, 'DYLD_LIBRARY_PATH': browserBuildPath, '__XPC_DYLD_LIBRARY_PATH': browserBuildPath}
            else:
                _log.info('Could not find Safari.app at %s, using the system Safari instead' % safariAppInBuildPath)

        args.extend(self.safariPreferences)
        _log.info('Launching safari: %s with url: %s' % (args[0], url))
        self.safariProcess = OSXSafariDriver.launchProcessWithCaffinate(args, env)

        # Stop for initialization of the safari process, otherwise, open
        # command may use the system safari.
        time.sleep(3)
        subprocess.Popen(['open', url])

    def closeBrowsers(self):
        super(OSXSafariDriver, self).closeBrowsers()
        if self.safariProcess and self.safariProcess.returncode:
            sys.exit('Browser crashed with exitcode %d' % self._process.returncode)

    @classmethod
    def maximizeWindow(cls):
        try:
            subprocess.check_call(['/usr/bin/defaults', 'write', 'com.apple.Safari', 'NSWindow Frame BrowserWindowFrame', ' '.join(['0', '0', str(cls.screenSize().width), str(cls.screenSize().height)] * 2)])
        except Exception as error:
            _log.error('Reset safari window size failed - Error: {}'.format(error))
