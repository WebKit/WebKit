
import logging
import os
import shutil
import subprocess
import time

from contextlib import contextmanager

from webkitcorepy import Timeout
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

    @contextmanager
    def profile(self, output_path, profile_filename, profiling_interval, trace_type='profile', timeout=300):
        trace_process = None
        additional_trace_arguments = {
            'full': ['--type=full'],
            'profile': ['--type=profile'],
        }
        if trace_type not in additional_trace_arguments:
            _log.error('`{}` is not a valid type of trace. Defaulting to `profile`.'.format(trace_type))
            trace_type = 'profile'
        try:
            _log.info('Gathering traces')
            _log.info('If passwordless sudo is not enabled, you may see a prompt for your host machine\'s admin password.')
            trace_command = ['sudo', 'ktrace', 'artrace', '-o', os.path.join(output_path, profile_filename + '.ktrace')]
            trace_command += additional_trace_arguments[trace_type]
            if profiling_interval:
                trace_command += ['-i', profiling_interval]
            _log.info('Running ktrace command: {}'.format(trace_command))
            trace_process = subprocess.Popen(trace_command)
            time.sleep(5)  # Wait a few seconds for `ktrace` process to start
            yield
        except Exception as error:
            raise Exception('Failed to start ktrace. Error: {}'.format(error))
        finally:
            if not trace_process:
                return
            try:
                subprocess.call(['sudo', 'pkill', '-2', 'ktrace'])
                _log.info('Stopping ktrace task.')
                with Timeout(timeout):
                    trace_process.wait()
            except Exception as error:
                _log.error('Failed to quit ktrace.')
            finally:
                _log.info('Killing ktrace task.')
                subprocess.call(['sudo', 'pkill', '-9', 'ktrace'])

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

    def _launch_process(self, build_dir, app_name, url, args, env=None):
        if not build_dir:
            build_dir = '/Applications/'
        app_path = os.path.join(build_dir, app_name)
        if self.browser_args:
            args += self.browser_args
        _log.info('Launching {} with url {}. args:{}'.format(app_path, url, " ".join(args)))
        # FIXME: May need to be modified for a local build such as setting up DYLD libraries
        args = ['open', '-a', app_path] + args
        try:
            process = subprocess.Popen(args, env=env)
        except Exception as error:
            _log.error('Popen failed: {error}'.format(error=error))

        return process

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

    @contextmanager
    def prevent_sleep(self, timeout):
        try:
            subprocess.Popen(["/usr/bin/caffeinate", "-dist", str(timeout)])
            yield
        finally:
            subprocess.call(["pkill", "-9", "caffeinate"])

    @classmethod
    def _screen_size(cls):
        from AppKit import NSScreen
        return NSScreen.mainScreen().frame().size

    @classmethod
    def _insert_url(cls, args, pos, url):
        temp_args = args[:]
        temp_args.insert(pos, url)
        return temp_args
