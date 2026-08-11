# Copyright (C) 2019 Igalia S.L. All rights reserved.
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

from webkitpy.benchmark_runner.browser_driver.linux_browser_driver import LinuxBrowserDriver


class WPEMiniBrowserBaseDriver(LinuxBrowserDriver):
    process_search_list = ['Tools/Scripts/run-minibrowser', 'MiniBrowser']

    def _extra_wpe_minibrowser_args(self):
        return []

    def launch_url(self, url, options, browser_build_path, browser_path):
        self._default_browser_arguments = []
        if self.process_name.endswith('run-minibrowser'):
            self._default_browser_arguments.extend(['--wpe', '--'])
        self._default_browser_arguments.extend(self._extra_wpe_minibrowser_args())
        if self.browser_args:
            self._default_browser_arguments.extend(self.browser_args)
            self.browser_args = []
        self._default_browser_arguments.append(url)
        super().launch_url(url, options, browser_build_path, browser_path)

    def launch_driver(self, url, options, browser_build_path):
        raise ValueError("Browser {browser} is not available with webdriver".format(browser=self.browser_name))

    def prepare_env(self, config):
        super().prepare_env(config)
        self._test_environ['WPE_BROWSER'] = 'minibrowser'


class WPEMiniBrowserDriver(WPEMiniBrowserBaseDriver):
    browser_name = 'minibrowser-wpe'

    def _extra_wpe_minibrowser_args(self):
        return ['--fullscreen']


class WPEMiniBrowserSkiaCPUDriver(WPEMiniBrowserDriver):
    browser_name = 'minibrowser-wpe-skiacpu'

    def prepare_env(self, config):
        super().prepare_env(config)
        self._test_environ['WEBKIT_SKIA_ENABLE_CPU_RENDERING'] = '1'


class WPEMiniBrowserLegacyAPIDriver(WPEMiniBrowserBaseDriver):
    browser_name = 'minibrowser-wpe-legacy'

    def _extra_wpe_minibrowser_args(self):
        return ['--use-legacy-api', '--fullscreen']
