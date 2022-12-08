# Copyright (C) 2017 Igalia S.L.
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


class WebDriverWPE(WebDriver):

    def __init__(self, port):
        super(WebDriverWPE, self).__init__(port)

    def binary_path(self):
        return self._port._build_path('bin', 'WPEWebDriver')

    def browser_name(self):
        return self._port.browser_name()

    def browser_path(self):
        if self._port.browser_name() == "cog":
            return self._port.cog_path_to('launcher', 'cog')
        return self._port._build_path('bin', 'MiniBrowser')

    def browser_args(self):
        args = ['--automation']
        if self.browser_name() == "cog":
            if self._port._display_server == 'headless':
                args.append('--platform=headless')
            else:
                args.append("--platform=gtk4")
        elif self._port._display_server == 'headless':
            args.append('--headless')
        return args

    def browser_env(self):
        return self._port.setup_environ_for_minibrowser()

    def capabilities(self):
        return {'wpe:browserOptions': {
            'binary': self.browser_path(),
            'args': self.browser_args()}}

    def selenium_name(self):
        return 'WPEWebKit'


register_driver('wpe', WebDriverWPE)
