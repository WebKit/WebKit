# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from webkitpy.webdriver_tests.webdriver_driver import WebDriver, register_driver


class WebDriverMac(WebDriver):

    def __init__(self, port):
        super(WebDriverMac, self).__init__(port)

    def binary_path(self):
        return self._port._build_path('minidriver')

    def browser_name(self):
        return 'MiniBrowser'

    def browser_args(self):
        # Always say whether to use Site Isolation, so that a run doesn't change with WebKit's default.
        return ['--force-site-isolation', 'YES' if self._port.get_option('site_isolation') else 'NO']

    def capabilities(self):
        # minidriver always launches the MiniBrowser in its own build directory, and doesn't accept a binary.
        # The GLib ports trust the test server's certificate through WEBKIT_TLS_CAFILE_PEM instead.
        return {
            'acceptInsecureCerts': True,
            'webkit:browserOptions': {'args': self.browser_args()},
        }


register_driver('mac', WebDriverMac)
