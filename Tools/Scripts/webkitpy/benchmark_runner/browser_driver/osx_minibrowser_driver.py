# Copyright (C) 2024 Igalia S.L. All rights reserved.
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
import os

from webkitpy.benchmark_runner.browser_driver.osx_browser_driver import OSXBrowserDriver


_log = logging.getLogger(__name__)


class OSXMiniBrowserDriver(OSXBrowserDriver):
    process_name = 'MiniBrowser'
    browser_name = 'minibrowser'
    app_name = 'MiniBrowser.app'
    bundle_id = 'org.webkit.MiniBrowser'

    # FIXME: handle self._browser_path.
    def launch_args_with_url(self, url):
        return ['--args', '--url', url]

    def launch_url(self, url, options, browser_build_path, browser_path):
        self._launch_process(build_dir=browser_build_path, app_name=self.app_name, url=url, args=self.launch_args_with_url(url))

    def launch_driver(self, url, options, browser_build_path):
        raise ValueError("Browser {browser} is not available with webdriver".format(browser=self.browser_name))

    def set_binary_location_impl(options, browser_build_path, app_name, process_name):
        if not browser_build_path:
            return
        app_path = os.path.join(browser_build_path, app_name)
        binary_path = os.path.join(app_path, "Contents/MacOS", process_name)
        options.binary_location = binary_path
