# Copyright (C) 2016 Igalia S.L. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import shutil
import sys
import tempfile
import logging
import subprocess
try:
    import psutil
except ImportError:
    pass
from webkitpy.benchmark_runner.utils import force_remove
from webkitpy.benchmark_runner.browser_driver.browser_driver import BrowserDriver


_log = logging.getLogger(__name__)


class LinuxBrowserDriver(BrowserDriver):
    browser_name = None
    process_search_list = []
    platform = 'linux'

    def __init__(self, browser_args):
        super().__init__(browser_args)
        self._temp_profiledir = None
        self._pgo_collection_active = False
        self._pgo_profile_directory = None
        self.process_name = self._get_first_executable_path_from_list(self.process_search_list)
        if self.process_name is None:
            raise ValueError('Cant find executable for browser {browser_name}. Searched list: {browser_process_list}'.format(
                              browser_name=self.browser_name, browser_process_list=self.process_search_list))

    def _clean_temp_profiledir(self):
        if self._temp_profiledir:
            force_remove(self._temp_profiledir)
            self._temp_profiledir = None

    def prepare_env(self, config):
        self._browser_process = None
        self._default_browser_arguments = None
        self._temp_profiledir = tempfile.mkdtemp()
        self._test_environ = dict(os.environ)
        self._test_environ['HOME'] = self._temp_profiledir
        if self._pgo_collection_active and not self._test_environ.get('LLVM_PROFILE_FILE'):
            profile_dir = self.pgo_profile_output_directories[0]
            self._test_environ['LLVM_PROFILE_FILE'] = os.path.join(
                profile_dir, '{browser}-%p-%m.profraw'.format(browser=self.browser_name or 'webkit'))

    def prepare_initial_env(self, config):
        pass

    def restore_env(self):
        self._clean_temp_profiledir()

    def restore_env_after_all_testing(self):
        if self._pgo_profile_directory:
            force_remove(self._pgo_profile_directory)
            self._pgo_profile_directory = None

    def close_browsers(self):
        if self._browser_process:
            if self._browser_process.poll() is None:  # still running
                if 'psutil' in sys.modules:
                    main_browser_process = psutil.Process(self._browser_process.pid)
                    browser_children = main_browser_process.children(recursive=True)
                    _log.info('Killing browser {browser_name} with pid {browser_pid} and cmd: {browser_cmd}'.format(
                               browser_name=self.browser_name, browser_pid=self._browser_process.pid,
                               browser_cmd=' '.join(main_browser_process.cmdline()).strip() or main_browser_process.name()))
                    if self._pgo_collection_active:
                        self._graceful_pgo_teardown(main_browser_process, browser_children)
                    else:
                        main_browser_process.kill()
                        for browser_child in browser_children:
                            if browser_child.is_running():
                                try:
                                    browser_child.kill()
                                    _log.info('Killed still alive {browser_name} child with pid {browser_pid} and cmd: {browser_cmd}'.format(
                                              browser_name=self.browser_name, browser_pid=browser_child.pid,
                                              browser_cmd=' '.join(browser_child.cmdline()).strip() or browser_child.name()))
                                except psutil.NoSuchProcess:
                                    # Race: child ended between is_running() and kill().
                                    pass
                else:
                    _log.info('Killing browser {browser_name} with pid {browser_pid}'.format(
                               browser_name=self.browser_name, browser_pid=self._browser_process.pid))
                    if self._pgo_collection_active:
                        self._browser_process.terminate()
                        try:
                            rc = self._browser_process.wait(timeout=10)
                            _log.info('[pgo] browser pid {pid} exited with {rc} (negative value = killed by that signal)'.format(
                                      pid=self._browser_process.pid, rc=rc))
                        except subprocess.TimeoutExpired:
                            _log.warning('[pgo] browser pid {pid} did not exit within timeout; SIGKILL'.format(pid=self._browser_process.pid))
                            self._browser_process.kill()
                    else:
                        self._browser_process.kill()
                    _log.warning('python psutil not found, can\'t check for '
                                 'still-alive browser children to kill.')
            else:
                _log.error('Browser {browser_name} with pid {browser_pid} ended prematurely with return code {browser_retcode}.'.format(
                            browser_name=self.browser_name, browser_pid=self._browser_process.pid,
                            browser_retcode=self._browser_process.returncode))
        self._clean_temp_profiledir()

    def _graceful_pgo_teardown(self, main_browser_process, browser_children):
        all_procs = [main_browser_process] + browser_children
        proc_names = {}
        for proc in all_procs:
            try:
                proc_names[proc.pid] = ' '.join(proc.cmdline()).strip() or proc.name()
            except psutil.NoSuchProcess:
                proc_names[proc.pid] = '<gone>'
        _log.info('[pgo] graceful teardown of {n} process(es): {names}'.format(
                  n=len(all_procs), names=proc_names))
        # Leave the main process last as it might be a wrapper script like run-minibrowser
        for proc in browser_children + [main_browser_process]:
            try:
                proc.terminate()
            except psutil.NoSuchProcess:
                pass
        gone, alive = psutil.wait_procs(all_procs, timeout=10, callback=lambda proc: _log.info(
            '[pgo] pid {pid} ({name}) exited with {rc}'.format(
                pid=proc.pid, name=proc_names.get(proc.pid, '?'), rc=proc.returncode)))
        for proc in alive:
            _log.warning('[pgo] pid {pid} ({name}) did not exit within timeout; SIGKILL (profile might be lost)'.format(
                         pid=proc.pid, name=proc_names.get(proc.pid, '?')))
            try:
                proc.kill()
            except psutil.NoSuchProcess:
                pass

    @property
    def pgo_profile_output_directories(self):
        # A caller-set LLVM_PROFILE_FILE wins; its directory must be absolute (LLVM writes relative to
        # the browser cwd, which we can't discover) and free of %p/%m/%t wildcards (which would make
        # the real output dir vary at runtime). Otherwise return the private per-run dir that
        # prepare_pgo_profile_collection() created.
        profile_file = os.environ.get('LLVM_PROFILE_FILE')
        if profile_file:
            if not os.path.isabs(profile_file):
                raise ValueError('LLVM_PROFILE_FILE={profile_file} must be an absolute path so .profraw '
                                 'lands in a discoverable directory.'.format(profile_file=profile_file))
            directory = os.path.dirname(os.path.realpath(profile_file))
            if '%' in directory:
                raise ValueError('LLVM_PROFILE_FILE={profile_file} expands a wildcard in its directory; '
                                 'keep %p/%m in the filename only.'.format(profile_file=profile_file))
            return [directory]
        return [self._pgo_profile_directory]

    def prepare_pgo_profile_collection(self):
        self._pgo_collection_active = True
        if os.environ.get('LLVM_PROFILE_FILE'):
            # Access (and thereby validate) the caller's path now so a bad LLVM_PROFILE_FILE fails
            # before the run. We neither create the dir (LLVM does, on first write) nor wipe it (could
            # be a real location like /home); collect() tolerates its absence if the run flushed nothing.
            assert self.pgo_profile_output_directories
            return
        # Driver-owned output: a fresh, unique dir per iteration -- mkdtemp so concurrent runs don't
        # collide, and each iteration starts clean so collect copies only its own .profraw.
        if self._pgo_profile_directory:
            force_remove(self._pgo_profile_directory)
        self._pgo_profile_directory = tempfile.mkdtemp(prefix='WebKitPGO-')

    def collect_pgo_profile(self, destination):
        _log.info('Copying PGO profiles from {sources} to {destination}'.format(
                  sources=self.pgo_profile_output_directories, destination=destination))
        for directory in self.pgo_profile_output_directories:
            if not os.path.isdir(directory):
                _log.warning('[pgo] no profile directory {directory}; the run flushed nothing'.format(directory=directory))
                continue
            shutil.copytree(directory, destination, dirs_exist_ok=True)

    def launch_url(self, url, options, browser_build_path, browser_path):
        if not self._default_browser_arguments:
            self._default_browser_arguments = [url]
        if not self.browser_args:
            self.browser_args = []
        exec_args = [self.process_name] + self.browser_args + self._default_browser_arguments
        _log.info('Executing: {browser_cmdline}'.format(browser_cmdline=' '.join(exec_args)))

        pass_fds = ()
        if os.environ.get("SYSPROF_CONTROL_FD"):
            try:
                control_fd = int(os.environ.get("SYSPROF_CONTROL_FD"))
                copy_fd = os.dup(control_fd)
                pass_fds += (copy_fd, )
                self._test_environ["SYSPROF_CONTROL_FD"] = str(copy_fd)
            except (ValueError):
                pass

        # The browser process is not expected to exit on its own, it gets killed when the benchmark ends
        # and at that time is already too late to read the stdour/stderr pipes when there is lot of text
        # because if the pipe gets full then the browser process gets frozen.
        # So do not capture stdout/stderr, let the subprocess flush it to the default stdout/stderr.
        self._browser_process = subprocess.Popen(
            exec_args, env=self._test_environ, pass_fds=pass_fds,
            encoding='utf-8',
        )

    def launch_webdriver(self, url, driver):
        try:
            driver.maximize_window()
        except Exception as error:
            _log.error('Failed to maximize {browser} window - Error: {error}'.format(browser=driver.name, error=error))
        _log.info('Launching "%s" with url "%s"' % (driver.name, url))
        driver.get(url)

    def diagnose_test_failure(self, diagnose_directory, error):
        # FIXME: store a screenshoot or some debug data for later analysis before closing the browser.
        self.close_browsers()

    def _get_first_executable_path_from_list(self, searchlist):
        searchpath = [os.path.curdir] + os.environ['PATH'].split(os.pathsep)
        for program in searchlist:
            for path in searchpath:
                fullpath = os.path.abspath(os.path.join(path, program))
                if  os.path.isfile(fullpath) and os.access(fullpath, os.X_OK):
                    return fullpath
        return None

    def _screen_size(self):
        # load_subclasses() from __init__.py will load this file to
        # check the platform defined. Do here a lazy import instead of
        # trying to import the Gtk module on the global scope of this
        # file to avoid ImportError errors on other platforms.
        # Python imports are cached and only run once, so this should be ok.
        import gi
        gi.require_version('Gtk', '3.0')
        gi.require_version('Gdk', '3.0')
        from gi.repository import Gtk, Gdk
        if Gtk.get_major_version() == 3 and Gtk.get_minor_version() < 22:
            screen = Gtk.Window().get_screen()
            return screen.get_monitor_geometry(screen.get_primary_monitor())
        else:
            display = Gdk.Display.get_default()
            monitor = display.get_primary_monitor()
            return monitor.get_geometry()

