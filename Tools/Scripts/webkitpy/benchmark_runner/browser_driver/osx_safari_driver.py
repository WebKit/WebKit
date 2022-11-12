import logging
import os
import sys
import re
import subprocess
import time

from webkitpy.benchmark_runner.browser_driver.osx_browser_driver import OSXBrowserDriver


_log = logging.getLogger(__name__)


class OSXSafariDriver(OSXBrowserDriver):
    process_name = 'Safari'
    browser_name = 'safari'
    bundle_id = 'com.apple.Safari'

    def prepare_env(self, config):
        self._safari_process = None
        self._safari_preferences = ["-HomePage", "about:blank", "-WarnAboutFraudulentWebsites", "0", "-ExtensionsEnabled", "0", "-ShowStatusBar", "0", "-NewWindowBehavior", "1", "-NewTabBehavior", "1", "-AlwaysRestoreSessionAtLaunch", "0", "-ApplePersistenceIgnoreStateQuietly", "1"]
        super(OSXSafariDriver, self).prepare_env(config)
        self._enable_signposts = config["enable_signposts"]
        self._maximize_window()

    def launch_url(self, url, options, browser_build_path, browser_path):
        env = {}
        for key, value in os.environ.items():
            if re.match(r'^__XPC_', key):
                env[key] = value
        if self._enable_signposts:
            env['WEBKIT_SIGNPOSTS_ENABLED'] = '1'
            env['__XPC_WEBKIT_SIGNPOSTS_ENABLED'] = '1'
            env['__XPC_JSC_exposeProfilersOnGlobalObject'] = '1'
        if browser_build_path or browser_path:
            self._launch_url_with_custom_path(url, options, browser_build_path, browser_path, env)
            return
        self._safari_process = self._launch_process(None, 'Safari.app', url, ['--url', url, '--args'] + self._safari_preferences, env=env)

    def _launch_url_with_custom_path(self, url, options, browser_build_path, browser_path, env):
        safari_app_path = '/Applications/Safari.app'
        safari_binary_path = os.path.join(safari_app_path, 'Contents/MacOS/Safari')

        _log.info('WARNING: Using custom paths to launch Safari (--browser-path or --build-directory) can return inaccurate results if the test is run remotely.')

        if browser_build_path:
            browser_build_absolute_path = os.path.abspath(browser_build_path)
            safari_app_path = os.path.join(browser_build_absolute_path, 'Safari.app')
            safari_binary_path = os.path.join(safari_app_path, 'Contents/MacOS/Safari')
            has_safari_binary = os.path.exists(safari_binary_path)
            contains_frameworks = any(map(lambda entry: entry.endswith('.framework'), os.listdir(browser_build_absolute_path)))

            if contains_frameworks:
                env['DYLD_FRAMEWORK_PATH'] = browser_build_absolute_path
                env['DYLD_LIBRARY_PATH'] = browser_build_absolute_path
                env['__XPC_DYLD_FRAMEWORK_PATH'] = browser_build_absolute_path
                env['__XPC_DYLD_LIBRARY_PATH'] = browser_build_absolute_path
            elif not has_safari_binary:
                raise Exception('Could not find any framework "{}"'.format(browser_build_path))

        elif browser_path:
            browser_path = os.path.abspath(browser_path)
            safari_binary_path = os.path.join(browser_path, 'Contents/MacOS/Safari')
            if os.path.exists(safari_binary_path):
                safari_app_path = browser_path
            else:
                raise Exception('Could not find Safari.app at {}'.format(browser_path))

        args = [safari_binary_path] + self._safari_preferences
        _log.info('Launching safari: %s with url: %s' % (safari_binary_path, url))
        try:
            self._safari_process = subprocess.Popen(args, env=env)
        except Exception as error:
            _log.error('Popen failed: {error}'.format(error=error))
        # Stop for initialization of the safari process, otherwise, open
        # command may use the system safari.
        time.sleep(3)

        if browser_build_path:
            _log.info('Checking if any open file is from "{}".'.format(browser_build_path))
            # Cannot use 'check_call' here as '+D' option will have non-zero return code when not all files under
            # specified folder are used.
            process = subprocess.Popen(
                ['/usr/sbin/lsof', '-a', '-p', str(self._safari_process.pid), '+D', browser_build_absolute_path],
                stdout=subprocess.PIPE,
                **(dict(encoding='utf-8') if sys.version_info >= (3, 6) else dict())
            )
            output = process.communicate()[0]
            if has_safari_binary:
                assert 'Safari.app/Contents/MacOS/Safari' in output, 'Safari.app is not launched from "{}"'.format(browser_build_path)
            if contains_frameworks:
                assert '.framework' in output, 'No framework is loaded from "{}"'.format(browser_build_path)

        subprocess.Popen(['open', '-a', safari_app_path, url], env=env)

    def launch_driver(self, url, options, browser_build_path):
        from selenium import webdriver
        driver = webdriver.Safari(quiet=False)
        self._launch_webdriver(url=url, driver=driver)
        return driver

    def close_browsers(self):
        super(OSXSafariDriver, self).close_browsers()
        if self._safari_process and self._safari_process.returncode:
            sys.exit('Browser crashed with exitcode %d' % self._safari_process.returncode)

    @classmethod
    def _maximize_window(cls):
        try:
            subprocess.check_call(['/usr/bin/defaults', 'write', 'com.apple.Safari', 'NSWindow Frame BrowserWindowFrame', ' '.join(['0', '0', str(cls._screen_size().width), str(cls._screen_size().height)] * 2)])
        except Exception as error:
            _log.error('Reset safari window size failed - Error: {}'.format(error))
