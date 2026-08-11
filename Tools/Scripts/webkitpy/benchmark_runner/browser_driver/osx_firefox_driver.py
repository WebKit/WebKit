import logging
import os
import shutil
import tempfile

from webkitpy.benchmark_runner.browser_driver.osx_browser_driver import OSXBrowserDriver


_log = logging.getLogger(__name__)
test_profile_directory = os.path.join(os.path.dirname(__file__), '../data/firefox_profile/')


class OSXFirefoxDriverBase(OSXBrowserDriver):
    app_name = None

    def __init__(self, *args, **kwargs):
        super(OSXFirefoxDriverBase, self).__init__(*args, **kwargs)
        self._profile_directory = None

    def prepare_env(self, config):
        super(OSXFirefoxDriverBase, self).prepare_env(config)
        self._setup_temporary_profile_directory()

    def restore_env(self):
        super(OSXFirefoxDriverBase, self).restore_env()
        self._teardown_temporary_profile_directory()

    def launch_url(self, url, options, browser_build_path, browser_path):
        # FIXME: handle self._browser_path.
        screen_size = self._screen_size()
        args_with_url = ['--args', '-width', str(int(screen_size.width)), '-height', str(int(screen_size.height))]
        if self._profile_directory and os.path.exists(self._profile_directory):
            args_with_url.extend(['--profile', self._profile_directory])
        args_with_url.append(url)
        self._launch_process(build_dir=browser_build_path, app_name=self.app_name, url=url, args=args_with_url)

    def _setup_temporary_profile_directory(self):
        self._profile_directory = os.path.join(tempfile.mkdtemp(), 'firefox_profile')
        shutil.copytree(test_profile_directory, self._profile_directory)

    def _teardown_temporary_profile_directory(self):
        if self._profile_directory and os.path.exists(self._profile_directory):
            shutil.rmtree(os.path.dirname(self._profile_directory))


class OSXFirefoxDriver(OSXFirefoxDriverBase):
    process_name = 'firefox'
    browser_name = 'firefox'
    app_name = 'Firefox.app'
    bundle_id = 'org.mozilla.firefox'


class OSXFirefoxNightlyDriver(OSXFirefoxDriverBase):
    process_name = 'firefox'
    browser_name = 'firefox-nightly'
    app_name = 'FirefoxNightly.app'
    bundle_id = 'org.mozilla.firefox'
