#!/usr/bin/env python

import logging
import os
import subprocess
import time

from webkitpy.benchmark_runner.browser_driver.osx_browser_driver import OSXBrowserDriver
from webkitpy.benchmark_runner.utils import force_remove


_log = logging.getLogger(__name__)


class OSXSafariDriver(OSXBrowserDriver):
    process_name = 'Safari'
    browser_name = 'safari'
    bundle_id = 'com.apple.Safari'

    def prepare_env(self, config):
        self._safari_process = None
        self._safari_preferences = ["-HomePage", "about:blank", "-WarnAboutFraudulentWebsites", "0", "-ExtensionsEnabled", "0", "-ShowStatusBar", "0", "-NewWindowBehavior", "1", "-NewTabBehavior", "1", "-AlwaysRestoreSessionAtLaunch", "0", "-ApplePersistenceIgnoreStateQuietly", "1"]
        super(OSXSafariDriver, self).prepare_env(config)
        self._maximize_window()

    def launch_url(self, url, options, browser_build_path, browser_path):
        args = ['/Applications/Safari.app/Contents/MacOS/Safari']
        env = {}
        if browser_build_path:
            browser_build_absolute_path = os.path.abspath(browser_build_path)
            safari_app_in_build_path = os.path.join(browser_build_absolute_path, 'Safari.app/Contents/MacOS/Safari')
            has_safari_app = os.path.exists(safari_app_in_build_path)
            content_in_path = os.listdir(browser_build_absolute_path)
            contains_frameworks = any(map(lambda entry: entry.endswith('.framework'), os.listdir(browser_build_absolute_path)))

            if has_safari_app:
                args = [safari_app_in_build_path]

            if contains_frameworks:
                env = {'DYLD_FRAMEWORK_PATH': browser_build_absolute_path, 'DYLD_LIBRARY_PATH': browser_build_absolute_path,
                    '__XPC_DYLD_FRAMEWORK_PATH': browser_build_absolute_path, '__XPC_DYLD_LIBRARY_PATH': browser_build_absolute_path}
            elif not has_safari_app:
                raise Exception('Could not find any framework "{}"'.format(browser_build_path))

        elif browser_path:
            safari_app_in_browser_path = os.path.join(browser_path, 'Contents/MacOS/Safari')
            if os.path.exists(safari_app_in_browser_path):
                args = [safari_app_in_browser_path]
            else:
                raise Exception('Could not find Safari.app at {}'.format(safari_app_in_browser_path))

        args.extend(self._safari_preferences)
        _log.info('Launching safari: %s with url: %s' % (args[0], url))
        self._safari_process = OSXSafariDriver._launch_process_with_caffeinate(args, env)

        # Stop for initialization of the safari process, otherwise, open
        # command may use the system safari.
        time.sleep(3)

        if browser_build_path:
            _log.info('Checking if any open file is from "{}".'.format(browser_build_path))
            # Cannot use 'check_call' here as '+D' option will have non-zero return code when not all files under
            # specified folder are used.
            process = subprocess.Popen(['/usr/sbin/lsof', '-a', '-p', str(self._safari_process.pid), '+D', browser_build_absolute_path], stdout=subprocess.PIPE)
            output = process.communicate()[0]
            if has_safari_app:
                assert 'Safari.app/Contents/MacOS/Safari' in output, 'Safari.app is not launched from "{}"'.format(browser_build_path)
            if contains_frameworks:
                assert '.framework' in output, 'No framework is loaded from "{}"'.format(browser_build_path)

        subprocess.Popen(['open', '-a', args[0], url])

    def launch_driver(self, url, options, browser_build_path):
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
