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

from linux_browser_driver import LinuxBrowserDriver


class GTKMiniBrowserDriver(LinuxBrowserDriver):
    browser_name = 'minibrowser-gtk'
    process_search_list = ['Tools/Scripts/run-minibrowser', 'MiniBrowser']

    def launch_url(self, url, options, browser_build_path, browser_path):
        self._browser_arguments = []
        if self.process_name.endswith('run-minibrowser'):
            self._browser_arguments.append('--gtk')
        self._browser_arguments.append('--geometry={w}x{h}'.format(w=self._screen_size().width, h=self._screen_size().height))
        self._browser_arguments.append(url)
        super(GTKMiniBrowserDriver, self).launch_url(url, options, browser_build_path, browser_path)

    def launch_driver(self, url, options, browser_build_path):
        raise ValueError("Browser {browser} is not available with webdriver".format(browser=self.browser_name))
