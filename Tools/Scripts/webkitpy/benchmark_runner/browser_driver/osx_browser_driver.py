
import logging
import os
import subprocess

from AppKit import NSRunningApplication
from AppKit import NSScreen
from Quartz import CGWarpMouseCursorPosition
from browser_driver import BrowserDriver


_log = logging.getLogger(__name__)


class OSXBrowserDriver(BrowserDriver):
    bundleIdentifier = None

    def prepareEnv(self):
        self.closeBrowsers()
        CGWarpMouseCursorPosition((10, 0))

    def closeBrowsers(self):
        self.terminateProcesses(self.bundleIdentifier)

    @classmethod
    def launchProcess(cls, buildDir, appName, url, args):
        if not buildDir:
            buildDir = '/Applications/'
        appPath = os.path.join(buildDir, appName)

        _log.info('Launching "%s" with url "%s"' % (appPath, url))

        # FIXME: May need to be modified for a local build such as setting up DYLD libraries
        args = ['open', '-a', appPath] + args
        cls.launchProcessWithCaffinate(args)

    @classmethod
    def terminateProcesses(cls, bundleIdentifier):
        _log.info('Closing all terminating all processes with the bundle identifier %s' % bundleIdentifier)
        processes = NSRunningApplication.runningApplicationsWithBundleIdentifier_(bundleIdentifier)
        for process in processes:
            process.terminate()

    @classmethod
    def launchProcessWithCaffinate(cls, args, env=None):
        process = subprocess.Popen(args, env=env)
        subprocess.Popen(["/usr/bin/caffeinate", "-disw", str(process.pid)])
        return process

    @classmethod
    def screenSize(cls):
        return NSScreen.mainScreen().frame().size
