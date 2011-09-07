# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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

import unittest

from webkitpy.thirdparty.mock import Mock

from webkitpy.layout_tests.port import chromium_mac
from webkitpy.layout_tests.port import port_testcase


class ChromiumMacPortTest(port_testcase.PortTestCase):
    def port_maker(self, platform):
        if platform != 'darwin':
            return None
        return chromium_mac.ChromiumMacPort

    def test_check_wdiff(self):
        port = chromium_mac.ChromiumMacPort()
        self.assertTrue(port.check_wdiff())

    def assert_name(self, port_name, os_version_string, expected):
        port = chromium_mac.ChromiumMacPort(port_name=port_name,
                                            os_version_string=os_version_string)
        self.assertEquals(expected, port.name())

    def test_versions(self):
        port = chromium_mac.ChromiumMacPort()
        self.assertTrue(port.name() in ('chromium-cg-mac-leopard', 'chromium-cg-mac-snowleopard', 'chromium-cg-mac-future'))

        self.assert_name(None, '10.5.3', 'chromium-cg-mac-leopard')
        self.assert_name('chromium-cg-mac', '10.5.3', 'chromium-cg-mac-leopard')
        self.assert_name('chromium-cg-mac-leopard', '10.5.3', 'chromium-cg-mac-leopard')
        self.assert_name('chromium-cg-mac-leopard', '10.6.3', 'chromium-cg-mac-leopard')

        self.assert_name(None, '10.6.3', 'chromium-cg-mac-snowleopard')
        self.assert_name('chromium-cg-mac', '10.6.3', 'chromium-cg-mac-snowleopard')
        self.assert_name('chromium-cg-mac-snowleopard', '10.5.3', 'chromium-cg-mac-snowleopard')
        self.assert_name('chromium-cg-mac-snowleopard', '10.6.3', 'chromium-cg-mac-snowleopard')

        self.assert_name(None, '10.7', 'chromium-cg-mac-lion')
        self.assert_name(None, '10.7.3', 'chromium-cg-mac-lion')
        self.assert_name(None, '10.8', 'chromium-cg-mac-future')
        self.assert_name('chromium-cg-mac', '10.7.3', 'chromium-cg-mac-lion')
        self.assert_name('chromium-cg-mac-future', '10.4.3', 'chromium-cg-mac-future')
        self.assert_name('chromium-cg-mac-future', '10.5.3', 'chromium-cg-mac-future')
        self.assert_name('chromium-cg-mac-future', '10.6.3', 'chromium-cg-mac-future')
        self.assert_name('chromium-cg-mac-future', '10.7.3', 'chromium-cg-mac-future')

        self.assertRaises(AssertionError, self.assert_name, None, '10.4.1', 'should-raise-assertion-so-this-value-does-not-matter')

    def test_baseline_path(self):
        port = chromium_mac.ChromiumMacPort(port_name='chromium-cg-mac-leopard')
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-cg-mac-leopard'))

        port = chromium_mac.ChromiumMacPort(port_name='chromium-cg-mac-snowleopard')
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-cg-mac'))

    def test_graphics_type(self):
        port = chromium_mac.ChromiumMacPort(port_name='chromium-cg-mac')
        self.assertEquals('cpu-cg', port.graphics_type())
        port = chromium_mac.ChromiumMacPort(port_name='chromium-mac')
        self.assertEquals('cpu', port.graphics_type())
        # For now, Mac defaults to cpu-cg graphics type.
        port = chromium_mac.ChromiumMacPort()
        self.assertEquals('cpu-cg', port.graphics_type())


if __name__ == '__main__':
    port_testcase.main()
