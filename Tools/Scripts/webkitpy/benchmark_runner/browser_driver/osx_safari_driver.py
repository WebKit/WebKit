#!/usr/bin/env python

import logging
import os
import subprocess
import time

from osx_browser_driver import OSXBrowserDriver
from webkitpy.benchmark_runner.utils import force_remove


_log = logging.getLogger(__name__)


class OSXSafariDriver(OSXBrowserDriver):
    process_name = 'Safari'
    browser_name = 'safari'

    def prepare_env(self, config):
        self._safari_process = None
        super(OSXSafariDriver, self).prepare_env(config)
        force_remove(os.path.join(os.path.expanduser('~'), 'Library/Saved Application State/com.apple.Safari.savedState'))
        force_remove(os.path.join(os.path.expanduser('~'), 'Library/Safari/LastSession.plist'))
        self._maximize_window()
        self._safari_preferences = ["-HomePage", "about:blank", "-WarnAboutFraudulentWebsites", "0", "-ExtensionsEnabled", "0", "-ShowStatusBar", "0", "-NewWindowBehavior", "1", "-NewTabBehavior", "1"]

    def launch_url(self, url, options, browser_build_path, browser_path):
        args = ['/Applications/Safari.app/Contents/MacOS/Safari']
        env = {}
        if browser_build_path:
            safari_app_in_build_path = os.path.join(browser_build_path, 'Safari.app/Contents/MacOS/Safari')
            if os.path.exists(safari_app_in_build_path):
                args = [safari_app_in_build_path]
                env = {'DYLD_FRAMEWORK_PATH': browser_build_path, 'DYLD_LIBRARY_PATH': browser_build_path, '__XPC_DYLD_FRAMEWORK_PATH': browser_build_path, '__XPC_DYLD_LIBRARY_PATH': browser_build_path}
            else:
                _log.info('Could not find Safari.app at %s, using the system Safari instead' % safari_app_in_build_path)
        elif browser_path:
            safari_app_in_browser_path = os.path.join(browser_path, 'Contents/MacOS/Safari')
            if os.path.exists(safari_app_in_browser_path):
                args = [safari_app_in_browser_path]
            else:
                _log.info('Could not find application at %s, using the system Safari instead' % safari_app_in_browser_path)

        args.extend(self._safari_preferences)
        _log.info('Launching safari: %s with url: %s' % (args[0], url))
        self._safari_process = OSXSafariDriver._launch_process_with_caffeinate(args, env)

        # Stop for initialization of the safari process, otherwise, open
        # command may use the system safari.
        time.sleep(3)
        subprocess.Popen(['open', '-a', args[0], url])

    def launch_driver(self, url, options, browser_build_path):
        import webkitpy.thirdparty.autoinstalled.selenium
        from selenium import webdriver
        driver = webdriver.Safari(quiet=False)
        self._launch_webdriver(url=url, driver=driver)
        return driver

    def close_browsers(self):
        super(OSXSafariDriver, self).close_browsers()
        if self._safari_process and self._safari_process.returncode:
            sys.exit('Browser crashed with exitcode %d' % self._process.returncode)

    @classmethod
    def _maximize_window(cls):
        try:
            subprocess.check_call(['/usr/bin/defaults', 'write', 'com.apple.Safari', 'NSWindow Frame BrowserWindowFrame', ' '.join(['0', '0', str(cls._screen_size().width), str(cls._screen_size().height)] * 2)])
        except Exception as error:
            _log.error('Reset safari window size failed - Error: {}'.format(error))
