# Copyright (C) 2026 Igalia S.L. All rights reserved.
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
import socket

from webkitpy.benchmark_runner.browser_driver.browser_driver import BrowserDriver
_log = logging.getLogger(__name__)


def get_ip_default_route():
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("1.1.1.1", 80))
            return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"


# This is a manual browser that instead of executing the browser directly it prints the URL
# and waits for the user to connect the browser manually. This is useful, for example, when
# testing a browser from another device (like an embedded board) than where the server runs.
class ManualBrowserDriver(BrowserDriver):
    browser_name = 'manual'
    process_search_list = []
    platform = 'all'

    def __init__(self, browser_args):
        self._ip_default_route = get_ip_default_route()

    def _print_action_msg(self, msg):
        print(f'\n    [Manual Browser Testing]: {msg}\n')

    def close_browsers(self):
        self._print_action_msg('Close browser')

    def launch_url(self, url, options, browser_build_path, browser_path):
        url = url.replace('0.0.0.0', self._ip_default_route)
        self._print_action_msg(f'Open URL {url}')

    def launch_driver(self, *args, **kwargs):
        raise ValueError(f"Browser {self.browser_name} can't use webdriver. Please use --driver webserver")
