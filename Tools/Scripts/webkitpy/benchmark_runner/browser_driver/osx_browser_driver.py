
import logging
import os
import subprocess
import time

from browser_driver import BrowserDriver
from webkitpy.benchmark_runner.utils import write_defaults


_log = logging.getLogger(__name__)


class OSXBrowserDriver(BrowserDriver):
    process_name = None
    platform = 'osx'
    bundle_id = None

    def prepare_initial_env(self, config):
        pass

    def prepare_env(self, config):
        self.close_browsers()
        from Quartz import CGWarpMouseCursorPosition
        CGWarpMouseCursorPosition((10, 0))
        self.updated_dock_animation_defaults = write_defaults('com.apple.dock', 'launchanim', False)
        if self.updated_dock_animation_defaults:
            self._terminate_processes('Dock', 'com.apple.dock')

    def restore_env(self):
        if self.updated_dock_animation_defaults:
            write_defaults('com.apple.dock', 'launchanim', True)
            self._terminate_processes('Dock', 'com.apple.dock')

    def restore_env_after_all_testing(self):
        pass

    def close_browsers(self):
        self._terminate_processes(self.process_name, self.bundle_id)

    @classmethod
    def _launch_process(cls, build_dir, app_name, url, args):
        if not build_dir:
            build_dir = '/Applications/'
        app_path = os.path.join(build_dir, app_name)

        _log.info('Launching "%s" with url "%s"' % (app_path, url))

        # FIXME: May need to be modified for a local build such as setting up DYLD libraries
        args = ['open', '-a', app_path] + args
        cls._launch_process_with_caffeinate(args)

    @classmethod
    def _launch_webdriver(cls, url, driver):
        try:
            driver.maximize_window()
        except Exception as error:
            _log.error('Failed to maximize {browser} window - Error: {error}'.format(browser=driver.name, error=error))
        _log.info('Launching "%s" with url "%s"' % (driver.name, url))
        driver.get(url)

    @classmethod
    def _terminate_processes(cls, process_name, bundle_id):
        from AppKit import NSRunningApplication
        _log.info('Closing all processes with name %s' % process_name)
        for app in NSRunningApplication.runningApplicationsWithBundleIdentifier_(bundle_id):
            app.terminate()
            # Give the app time to close
            time.sleep(2)
            if not app.isTerminated():
                _log.error("Terminate failed.  Killing.")
                subprocess.call(['/usr/bin/killall', process_name])

    @classmethod
    def _launch_process_with_caffeinate(cls, args, env=None):
        try:
            process = subprocess.Popen(args, env=env)
        except Exception as error:
            _log.error('Popen failed: {error}'.format(error=error))
            return

        subprocess.Popen(["/usr/bin/caffeinate", "-disw", str(process.pid)])
        return process

    @classmethod
    def _screen_size(cls):
        from AppKit import NSScreen
        return NSScreen.mainScreen().frame().size

    @classmethod
    def _insert_url(cls, args, pos, url):
        temp_args = args[:]
        temp_args.insert(pos, url)
        return temp_args
