
import logging
import os
import shutil
import subprocess
import time

from webkitpy.benchmark_runner.browser_driver.browser_driver import BrowserDriver
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
        from webkitpy.autoinstalled.pyobjc_frameworks import Quartz
        Quartz.CGWarpMouseCursorPosition((10, 0))
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

    def _save_screenshot_to_path(self, output_directory, filename):
        jpg_image_path = os.path.join(output_directory, filename)
        try:
            subprocess.call(['screencapture', jpg_image_path])
            _log.info('Saved screenshot to "{}"'.format(jpg_image_path))
        except subprocess.CalledProcessError as error:
            _log.error('Failed to save screenshot - Error: {error}'.format(error=error))

    def diagnose_test_failure(self, diagnose_directory, error):
        _log.info('Diagnosing benchmark failure: "{}"'.format(error))

        if not diagnose_directory:
            _log.info('Diagnose directory is not specified, will skip diagnosing.')
            return

        if os.path.exists(diagnose_directory):
            _log.info('Diagnose directory: "{}" already exists, cleaning it up'.format(diagnose_directory))
            try:
                if os.path.isdir(diagnose_directory):
                    if len(os.listdir(diagnose_directory)):
                        shutil.rmtree(diagnose_directory)
                elif os.path.isfile(diagnose_directory):
                    os.remove(diagnose_directory)
            except Exception as error:
                _log.error('Could not remove diagnose directory {} - error: {}'.format(diagnose_directory, error))
        if not os.path.exists(diagnose_directory):
            os.makedirs(diagnose_directory)

        self._save_screenshot_to_path(diagnose_directory, 'test-failure-screenshot-{}.jpg'.format(int(time.time())))

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
        from webkitpy.autoinstalled.pyobjc_frameworks import AppKit
        _log.info('Closing all processes with name %s' % process_name)
        for app in AppKit.NSRunningApplication.runningApplicationsWithBundleIdentifier_(bundle_id):
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
