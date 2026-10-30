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

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.tool.mocktool import MockOptions
from webkitpy.webdriver_tests.webdriver_driver import create_driver


class WebDriverMacTest(unittest.TestCase):

    def browser_options(self, **options):
        port = MockHost().port_factory.get('mac', MockOptions(**options))
        return create_driver(port).capabilities()['webkit:browserOptions']

    def test_site_isolation(self):
        self.assertEqual(self.browser_options(site_isolation=True), {'args': ['--force-site-isolation', 'YES']})

    def test_no_site_isolation(self):
        # Turned off explicitly, whatever WebKit's default is.
        self.assertEqual(self.browser_options(site_isolation=False), {'args': ['--force-site-isolation', 'NO']})
        self.assertEqual(self.browser_options(), {'args': ['--force-site-isolation', 'NO']})
