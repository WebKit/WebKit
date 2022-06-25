# Copyright (C) 2020 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from webkitcorepy import Version

from webkitpy.port import darwin_testcase
from webkitpy.tool.mocktool import MockOptions


class WatchTest(darwin_testcase.DarwinTest):
    disable_setup = True

    def make_port(self, host=None, port_name=None, options=None, os_name=None, os_version=Version(5), **kwargs):
        if options:
            options.architecture = 'x86'
            options.webkit_test_runner = True
        else:
            options = MockOptions(architecture='x86', webkit_test_runner=True, configuration='Release')
        port = super(WatchTest, self).make_port(host=host, port_name=port_name, options=options, os_name=os_name, os_version=None, kwargs=kwargs)
        port.set_option('version', str(os_version))
        return port

    def test_driver_name(self):
        self.assertEqual(self.make_port().driver_name(), 'WebKitTestRunnerApp.app')

    def test_baseline_searchpath(self):
        search_path = self.make_port().default_baseline_search_path()
        self.assertEqual(search_path[-1], '/mock-checkout/LayoutTests/platform/wk2')
        self.assertEqual(search_path[-2], '/mock-checkout/LayoutTests/platform/watchos')
