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
import sys
import tempfile
import logging
import subprocess
try:
    import psutil
except ImportError:
    pass
from webkitpy.benchmark_runner.utils import force_remove
from browser_driver import BrowserDriver


_log = logging.getLogger(__name__)


class LinuxBrowserDriver(BrowserDriver):
    browser_name = None
    process_search_list = []
    platform = 'linux'

    def __init__(self):
        self.process_name = self._get_first_executable_path_from_list(self.process_search_list)
        if self.process_name is None:
            raise ValueError('Cant find executable for browser {browser_name}. Searched list: {browser_process_list}'.format(
                              browser_name=self.browser_name, browser_process_list=self.process_search_list))

    def prepare_env(self, config):
        self._browser_process = None
        self._browser_arguments = None
        self._temp_profiledir = tempfile.mkdtemp()
        self._test_environ = dict(os.environ)
        self._test_environ['HOME'] = self._temp_profiledir

    def prepare_initial_env(self, config):
        pass

    def restore_env(self):
        force_remove(self._temp_profiledir)

    def restore_env_after_all_testing(self):
        pass

    def close_browsers(self):
        if self._browser_process:
            if self._browser_process.poll() is None:  # still running
                if 'psutil' in sys.modules:
                    main_browser_process = psutil.Process(self._browser_process.pid)
                    browser_children = main_browser_process.children(recursive=True)
                    _log.info('Killing browser {browser_name} with pid {browser_pid} and cmd: {browser_cmd}'.format(
                               browser_name=self.browser_name, browser_pid=self._browser_process.pid,
                               browser_cmd=' '.join(main_browser_process.cmdline()).strip() or main_browser_process.name()))
                    main_browser_process.kill()
                    for browser_child in browser_children:
                        if browser_child.is_running():
                            _log.info('Killing still alive {browser_name} child with pid {browser_pid} and cmd: {browser_cmd}'.format(
                                       browser_name=self.browser_name, browser_pid=browser_child.pid,
                                       browser_cmd=' '.join(browser_child.cmdline()).strip() or browser_child.name()))
                            browser_child.kill()
                else:
                    _log.info('Killing browser {browser_name} with pid {browser_pid}'.format(
                               browser_name=self.browser_name, browser_pid=self._browser_process.pid))
                    self._browser_process.kill()
                    _log.warning('python psutil not found, cant check for '
                                 'still-alive browser childs to kill.')
            else:
                _log.error('Browser {browser_name} with pid {browser_pid} ended prematurely with return code {browser_retcode}.'.format(
                            browser_name=self.browser_name, browser_pid=self._browser_process.pid,
                            browser_retcode=self._browser_process.returncode))

    def launch_url(self, url, options, browser_build_path, browser_path):
        if not self._browser_arguments:
            self._browser_arguments = [url]
        exec_args = [self.process_name] + self._browser_arguments
        _log.info('Executing: {browser_cmdline}'.format(browser_cmdline=' '.join(exec_args)))
        self._browser_process = subprocess.Popen(exec_args, env=self._test_environ,
                                                 stdout=subprocess.PIPE,
                                                 stderr=subprocess.STDOUT)

    def launch_webdriver(self, url, driver):
        try:
            driver.maximize_window()
        except Exception as error:
            _log.error('Failed to maximize {browser} window - Error: {error}'.format(browser=driver.name, error=error))
        _log.info('Launching "%s" with url "%s"' % (driver.name, url))
        driver.get(url)

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
