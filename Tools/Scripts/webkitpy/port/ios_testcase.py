# Copyright (C) 2017 Apple Inc. All rights reserved.
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


class IOSTest(darwin_testcase.DarwinTest):
    disable_setup = True

    def make_port(self, host=None, port_name=None, options=None, os_name=None, os_version=Version(13), **kwargs):
        port = super(IOSTest, self).make_port(host=host, port_name=port_name, options=options, os_name=os_name, os_version=None, kwargs=kwargs)
        port.set_option('version', str(os_version))
        return port

    def test_driver_name(self):
        self.assertEqual(self.make_port().driver_name(), 'DumpRenderTree.app')

    def test_baseline_searchpath(self):
        search_path = self.make_port().default_baseline_search_path()
        self.assertEqual(search_path[-1], '/mock-checkout/LayoutTests/platform/ios')
        self.assertEqual(search_path[-2], '/mock-checkout/LayoutTests/platform/ios-wk1')
