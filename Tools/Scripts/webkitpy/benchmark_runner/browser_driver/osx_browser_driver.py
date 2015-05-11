
import logging
import os
import subprocess

from AppKit import NSRunningApplication
from browser_driver import BrowserDriver


_log = logging.getLogger(__name__)


class OSXBrowserDriver(BrowserDriver):
    bundleIdentifier = None

    def prepareEnv(self):
        self.closeBrowsers()

    def closeBrowsers(self):
        self.terminateProcesses(self.bundleIdentifier)

    @classmethod
    def launchProcess(cls, buildDir, appName, url, args):
        if not buildDir:
            buildDir = '/Applications/'
        appPath = os.path.join(buildDir, appName)

        _log.info('Launching "%s" with url "%s"' % (appPath, url))

        # FIXME: May need to be modified for a local build such as setting up DYLD libraries
        subprocess.Popen((['open', '-a', appPath] + args)).communicate()

    @classmethod
    def terminateProcesses(cls, bundleIdentifier):
        _log.info('Closing all terminating all processes with the bundle identifier %s' % bundleIdentifier)
        processes = NSRunningApplication.runningApplicationsWithBundleIdentifier_(bundleIdentifier)
        for process in processes:
            process.terminate()
