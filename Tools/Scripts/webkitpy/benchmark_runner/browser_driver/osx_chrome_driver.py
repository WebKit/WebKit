import logging
import os

from webkitpy.benchmark_runner.browser_driver.osx_browser_driver import OSXBrowserDriver


_log = logging.getLogger(__name__)


class OSXChromeDriverBase(OSXBrowserDriver):
    app_name = None

    def launch_url(self, url, options, browser_build_path, browser_path):
        # FIXME: handle self._browser_path.
        args_with_url = ['--args', '--homepage', url, self._window_size_arg(), '--no-first-run',
                         '--no-default-browser-check', '--disable-extensions']
        self._launch_process(build_dir=browser_build_path, app_name=self.app_name, url=url, args=args_with_url)

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


class OSXChromeDriver(OSXChromeDriverBase):
    process_name = 'Google Chrome'
    browser_name = 'chrome'
    app_name = 'Google Chrome.app'
    bundle_id = 'com.google.Chrome'

    def _set_chrome_binary_location(self, options, browser_build_path):
        if not browser_build_path:
            return
        app_path = os.path.join(browser_build_path, self.app_name)
        binary_path = os.path.join(app_path, "Contents/MacOS", self.process_name)
        options.binary_location = binary_path


class OSXChromeCanaryDriver(OSXBrowserDriver):
    process_name = 'Google Chrome Canary'
    browser_name = 'chrome-canary'
    app_name = 'Google Chrome Canary.app'
    bundle_id = 'com.google.Chrome.canary'

    def _set_chrome_binary_location(self, options, browser_build_path):
        if not browser_build_path:
            browser_build_path = '/Applications/'
        app_path = os.path.join(browser_build_path, self.app_name)
        binary_path = os.path.join(app_path, "Contents/MacOS", self.process_name)
        options.binary_location = binary_path
