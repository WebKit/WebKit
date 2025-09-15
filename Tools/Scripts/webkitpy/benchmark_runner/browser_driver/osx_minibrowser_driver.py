import logging
import os
import sys
import re
import subprocess
import time

from webkitpy.benchmark_runner.browser_driver.osx_browser_driver import OSXBrowserDriver
from webkitpy.common.host import Host
from webkitpy.common.webkit_finder import WebKitFinder


_log = logging.getLogger(__name__)


class OSXMiniDriver(OSXBrowserDriver):
    process_name = 'MiniBrowser'
    browser_name = 'minibrowser'
    bundle_id = 'org.webkit.MiniBrowser'

    def __init__(self, browser_args):
        OSXBrowserDriver.__init__(self, browser_args)
        self._webkit_finder = WebKitFinder(Host().filesystem)
        _log.warning("It looks like you're running benchmarks on MiniBrowser which has different performance characteristics than Safari... Don't assume they are the same!")

    def prepare_env(self, config):
        self._process = None
        super(OSXMiniDriver, self).prepare_env(config)
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
        self._process = subprocess.Popen([self._webkit_finder.path_to_script('run-minibrowser'), '--url', url], env=env)

    def _launch_url_with_custom_path(self, url, options, browser_build_path, browser_path, env):
        browser_build_absolute_path = None
        app_path = None
        if browser_build_path:
            browser_build_absolute_path = os.path.abspath(browser_build_path)
            app_path = os.path.join(browser_build_absolute_path, 'MiniBrowser.app')
        elif browser_path:
            app_path = os.path.abspath(browser_path)
            browser_build_absolute_path = os.path.dirname(app_path)
        else:
            assert False  # self.launch_url should have dealt with this case

        contains_frameworks = any(map(lambda entry: entry.endswith('.framework'), os.listdir(browser_build_absolute_path)))
        if contains_frameworks:
            env['DYLD_FRAMEWORK_PATH'] = browser_build_absolute_path
            env['DYLD_LIBRARY_PATH'] = browser_build_absolute_path
            env['__XPC_DYLD_FRAMEWORK_PATH'] = browser_build_absolute_path
            env['__XPC_DYLD_LIBRARY_PATH'] = browser_build_absolute_path
        else:
            raise Exception('Could not find any framework "{}"'.format(browser_build_absolute_path))

        binary_path = os.path.join(app_path, 'Contents/MacOS/MiniBrowser')
        has_binary = os.path.exists(binary_path)
        if not has_binary:
            raise Exception('No binary at "{}"'.format(binary_path))

        try:
            process = subprocess.Popen([binary_path, '--url', url], env=env)
        except Exception as error:
            _log.error('Popen failed: {error}'.format(error=error))

    def close_browsers(self):
        super(OSXMiniDriver, self).close_browsers()
        if self._process and self._process.returncode:
            sys.exit('Browser crashed with exitcode %d' % self._process.returncode)

    @classmethod
    def _maximize_window(cls):
        try:
            subprocess.check_call(['/usr/bin/defaults', 'write', 'com.apple.MiniBrowser', 'NSWindow Frame BrowserWindowFrame', ' '.join(['0', '0', str(cls._screen_size().width), str(cls._screen_size().height)] * 2)])
        except Exception as error:
            _log.error('Reset window size failed - Error: {}'.format(error))
