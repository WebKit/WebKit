# Copyright (C) 2025 Igalia S.L. All rights reserved.
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

import logging
import re
import subprocess

from webkitpy.benchmark_runner.browser_driver.linux_browser_driver import LinuxBrowserDriver
from webkitpy.benchmark_runner.utils import get_path_from_project_root


_log = logging.getLogger(__name__)


class ServoBrowserDriver(LinuxBrowserDriver):
    browser_name = 'servo'
    process_search_list = ['servo', 'servoshell']

    def launch_driver(self, url, options, browser_build_path):
        raise ValueError("Browser {browser} is not available with webdriver".format(browser=self.browser_name))

    def _screen_size(self):
        print_screen_size_path = get_path_from_project_root('../../../../Tools/gtk/print-screen-size')
        try:
            output = subprocess.check_output([print_screen_size_path], env=self._test_environ, stderr=subprocess.PIPE, timeout=10)
        except (OSError, subprocess.SubprocessError) as error:
            _log.warning(f'Unable to determine the screen size with {print_screen_size_path}: {error}')
            return None
        screen_size = output.decode('utf-8', errors='ignore').strip()
        if not re.match(r'^[0-9]+x[0-9]+$', screen_size):
            _log.warning(f'Unable to parse the screen size returned by {print_screen_size_path}: "{screen_size}"')
            return None
        return screen_size

    def launch_url(self, url, options, browser_build_path, browser_path):
        self._default_browser_arguments = ['--no-native-titlebar']
        screen_size = self._screen_size()
        if screen_size:
            self._default_browser_arguments.append(f'--window-size={screen_size}')
        self._default_browser_arguments.append(url)
        super().launch_url(url, options, browser_build_path, browser_path)

    def browser_version(self):
        version_cmd = [self.process_name, '--version']
        version_output = subprocess.check_output(version_cmd, timeout=3).decode('utf-8', errors='ignore').strip()
        m = re.search(r'Servo\s+([0-9][^\s]*)', version_output)
        if m:
            return m.group(1)
        version_cmd = ' '.join(version_cmd)
        raise ValueError(f'Unable to parse browser version. Command "{version_cmd}" returned "{version_output}"')
