import logging
import os

from webkitpy.benchmark_runner.browser_driver.osx_browser_driver import OSXBrowserDriver


_log = logging.getLogger(__name__)


class OSXChromeDriverBase(OSXBrowserDriver):
    app_name = None

    # FIXME: handle self._browser_path.
    def launch_args_with_url(self, url):
        return ['--args', '--homepage', url, self._window_size_arg(), '--no-first-run', '--no-default-browser-check', '--disable-extensions', '--hide-crash-restore-bubble']

    def launch_url(self, url, options, browser_build_path, browser_path):
        self._launch_process(build_dir=browser_build_path, app_name=self.app_name, url=url, args=self.launch_args_with_url(url))

    def launch_driver(self, url, options, browser_build_path):
        from selenium import webdriver
        chrome_options = self._create_chrome_options(browser_build_path)
        driver_executable = self.webdriver_binary_path
        driver = webdriver.Chrome(chrome_options=chrome_options, executable_path=driver_executable)
        self._launch_webdriver(url=url, driver=driver)
        return driver

    def _create_chrome_options(self, browser_build_path):
        from selenium.webdriver.chrome.options import Options
        chrome_options = Options()
        chrome_options.add_argument("--disable-web-security")
        chrome_options.add_argument("--user-data-dir")
        chrome_options.add_argument("--disable-extensions")
        chrome_options.add_argument(self._window_size_arg())
        self._set_chrome_binary_location(chrome_options, browser_build_path)
        return chrome_options

    def _window_size_arg(self):
        screen_size = self._screen_size()
        return '--window-size={width},{height}'.format(width=int(screen_size.width), height=int(screen_size.height))

    def _set_chrome_binary_location(self, options, browser_build_path):
        pass

def set_binary_location_impl(options, browser_build_path, app_name, process_name):
    if not browser_build_path:
        return
    app_path = os.path.join(browser_build_path, app_name)
    binary_path = os.path.join(app_path, "Contents/MacOS", process_name)
    options.binary_location = binary_path


class OSXChromeDriver(OSXChromeDriverBase):
    process_name = 'Google Chrome'
    browser_name = 'chrome'
    app_name = 'Google Chrome.app'
    bundle_id = 'com.google.Chrome'

    def _set_chrome_binary_location(self, options, browser_build_path):
        set_binary_location_impl(options, browser_build_path, self.app_name, self.process_name)


class OSXChromeCanaryDriver(OSXChromeDriverBase):
    process_name = 'Google Chrome Canary'
    browser_name = 'chrome-canary'
    app_name = 'Google Chrome Canary.app'
    bundle_id = 'com.google.Chrome.canary'

    def _set_chrome_binary_location(self, options, browser_build_path):
        set_binary_location_impl(options, browser_build_path, self.app_name, self.process_name)

    def launch_args_with_url(self, url):
        return super(OSXChromeCanaryDriver, self).launch_args_with_url(url) + ['--enable-field-trial-config']


class OSXChromeBetaDriver(OSXChromeDriverBase):
    process_name = 'Google Chrome Beta'
    browser_name = 'chrome-beta'
    app_name = 'Google Chrome Beta.app'
    bundle_id = 'com.google.Chrome.beta'

    def _set_chrome_binary_location(self, options, browser_build_path):
        set_binary_location_impl(options, browser_build_path, self.app_name, self.process_name)

    def launch_args_with_url(self, url):
        return super(OSXChromeBetaDriver, self).launch_args_with_url(url) + ['--enable-field-trial-config']


class OSXChromeDevDriver(OSXChromeDriverBase):
    process_name = 'Google Chrome Dev'
    browser_name = 'chrome-dev'
    app_name = 'Google Chrome Dev.app'
    bundle_id = 'com.google.Chrome.dev'

    def _set_chrome_binary_location(self, options, browser_build_path):
        set_binary_location_impl(options, browser_build_path, self.app_name, self.process_name)

    def launch_args_with_url(self, url):
        return super(OSXChromeDevDriver, self).launch_args_with_url(url) + ['--enable-field-trial-config']


class OSXChromeForTestingDriver(OSXChromeDriverBase):
    process_name = 'Google Chrome for Testing'
    browser_name = 'chrome-for-testing'
    app_name = 'Google Chrome for Testing.app'
    bundle_id = 'com.google.chrome.for.testing'

    def _set_chrome_binary_location(self, options, browser_build_path):
        set_binary_location_impl(options, browser_build_path, self.app_name, self.process_name)

    def launch_args_with_url(self, url):
        return super(OSXChromeForTestingDriver, self).launch_args_with_url(url) + ['--enable-field-trial-config']


class OSXChromiumDriver(OSXChromeDriverBase):
    process_name = 'Chromium'
    browser_name = 'chromium'
    app_name = 'Chromium.app'
    bundle_id = 'org.chromium.Chromium'

    def _set_chrome_binary_location(self, options, browser_build_path):
        set_binary_location_impl(options, browser_build_path, self.app_name, self.process_name)

    def launch_args_with_url(self, url):
        return super(OSXChromiumDriver, self).launch_args_with_url(url)
