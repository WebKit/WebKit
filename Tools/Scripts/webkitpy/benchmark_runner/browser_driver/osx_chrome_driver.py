import logging

from webkitpy.benchmark_runner.browser_driver.osx_browser_driver import OSXBrowserDriver


_log = logging.getLogger(__name__)


class OSXChromeDriverBase(OSXBrowserDriver):
    app_name = None

    # FIXME: handle self._browser_path.
    def launch_args_with_url(self, url):
        return ['--args', '--homepage', url, self._window_size_arg(), '--no-first-run', '--no-default-browser-check', '--disable-extensions', '--hide-crash-restore-bubble']

    def launch_url(self, url, options, browser_build_path, browser_path):
        self._launch_process(build_dir=browser_build_path, app_name=self.app_name, url=url, args=self.launch_args_with_url(url))

    def _window_size_arg(self):
        screen_size = self._screen_size()
        return '--window-size={width},{height}'.format(width=int(screen_size.width), height=int(screen_size.height))


class OSXChromeDriver(OSXChromeDriverBase):
    process_name = 'Google Chrome'
    browser_name = 'chrome'
    app_name = 'Google Chrome.app'
    bundle_id = 'com.google.Chrome'


class OSXChromeCanaryDriver(OSXChromeDriverBase):
    process_name = 'Google Chrome Canary'
    browser_name = 'chrome-canary'
    app_name = 'Google Chrome Canary.app'
    bundle_id = 'com.google.Chrome.canary'

    def launch_args_with_url(self, url):
        return super(OSXChromeCanaryDriver, self).launch_args_with_url(url) + ['--enable-field-trial-config', '--disable-features=ThrottleRepeatedNoDamageFrames']


class OSXChromeBetaDriver(OSXChromeDriverBase):
    process_name = 'Google Chrome Beta'
    browser_name = 'chrome-beta'
    app_name = 'Google Chrome Beta.app'
    bundle_id = 'com.google.Chrome.beta'

    def launch_args_with_url(self, url):
        return super(OSXChromeBetaDriver, self).launch_args_with_url(url) + ['--enable-field-trial-config', '--disable-features=ThrottleRepeatedNoDamageFrames']


class OSXChromeDevDriver(OSXChromeDriverBase):
    process_name = 'Google Chrome Dev'
    browser_name = 'chrome-dev'
    app_name = 'Google Chrome Dev.app'
    bundle_id = 'com.google.Chrome.dev'

    def launch_args_with_url(self, url):
        return super(OSXChromeDevDriver, self).launch_args_with_url(url) + ['--enable-field-trial-config', '--disable-features=ThrottleRepeatedNoDamageFrames']


class OSXChromeForTestingDriver(OSXChromeDriverBase):
    process_name = 'Google Chrome for Testing'
    browser_name = 'chrome-for-testing'
    app_name = 'Google Chrome for Testing.app'
    bundle_id = 'com.google.chrome.for.testing'

    def launch_args_with_url(self, url):
        return super(OSXChromeForTestingDriver, self).launch_args_with_url(url) + ['--enable-field-trial-config', '--disable-features=ThrottleRepeatedNoDamageFrames']


class OSXChromiumDriver(OSXChromeDriverBase):
    process_name = 'Chromium'
    browser_name = 'chromium'
    app_name = 'Chromium.app'
    bundle_id = 'org.chromium.Chromium'

    def launch_args_with_url(self, url):
        return super(OSXChromiumDriver, self).launch_args_with_url(url)
