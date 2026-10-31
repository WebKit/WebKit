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
from webkitpy.webdriver_tests.webdriver_test_expectations import WebDriverTestExpectations, expectations_port_names


class MockWebDriverTestExpectations(WebDriverTestExpectations):
    def __init__(self, port_name, expectations, build_type='Release', architecture=None, options=None):
        super().__init__(MockHost().port_factory.get(port_name, options), '', build_type, architecture)
        self._expectations = self._load_expectation_string(expectations)


class WebDriverTestExpectationsTest(unittest.TestCase):

    EXPECTATIONS = """
{
    "imported/w3c/webdriver/tests/test1.py": {
        "expected": {"all": {"status": ["FAIL"], "bug": "1234"}},
        "subtests": {
            "test1_one": {
                "expected": {"glib": {"status": ["PASS"]}, "mac@Debug:arm64": {"status": ["TIMEOUT"], "slow": true}}
            },
            "notes": "Not a subtest"
        }
    },
    "imported/w3c/webdriver/tests/test2.py": {
        "expected": {"cocoa": {"status": ["SKIP"], "bug": "1234"}, "wpe": {"status": ["FAIL", "PASS"]}}
    },
    "imported/w3c/webdriver/tests/test3.py": {
        "expected": {"mac": {"status": ["FAIL"]}},
        "subtests": {
            "test3_one": {
                "expected": {"mac-site-isolation@Debug": {"status": ["TIMEOUT"]}}
            },
            "test3_two": {
                "expected": {"mac-site-isolation": {"status": ["PASS"]}}
            }
        }
    }
}"""

    def test_port_names(self):
        host = MockHost()
        self.assertEqual(expectations_port_names(host.port_factory.get('gtk')), ['gtk', 'glib'])
        self.assertEqual(expectations_port_names(host.port_factory.get('wpe')), ['wpe', 'glib'])
        self.assertEqual(expectations_port_names(host.port_factory.get('mac-tahoe-wk2')), ['mac-tahoe-wk2', 'mac', 'cocoa'])
        self.assertEqual(expectations_port_names(host.port_factory.get('mac-tahoe-wk2', MockOptions(site_isolation=True))), ['mac-site-isolation', 'mac-tahoe-wk2', 'mac', 'cocoa'])

    def assert_expected(self, port_name, build_type, test, subtest, status, slow=False, site_isolation=False):
        expectations = MockWebDriverTestExpectations(port_name, self.EXPECTATIONS, build_type, 'arm64', MockOptions(site_isolation=site_isolation))
        self.assertEqual(expectations.get_expectation(test, subtest), status)
        self.assertEqual(expectations.is_slow(test, subtest), slow)

    def test_lookup(self):
        test1 = 'imported/w3c/webdriver/tests/test1.py'
        test2 = 'imported/w3c/webdriver/tests/test2.py'
        for port_name in ['gtk', 'wpe']:
            self.assert_expected(port_name, 'Release', test1, None, ['FAIL'])
            # The group's entry applies to its ports.
            self.assert_expected(port_name, 'Release', test1, 'test1_one', ['PASS'])
            # A subtest without its own entry gets its test file's.
            self.assert_expected(port_name, 'Release', test1, 'test1_two', ['FAIL'])
        self.assert_expected('gtk', 'Release', test2, None, ['PASS'])
        self.assert_expected('wpe', 'Release', test2, None, ['FAIL', 'PASS'])

        # Mac isn't in the glib group, and its own entry is for Debug only.
        self.assert_expected('mac-tahoe-wk2', 'Release', test1, 'test1_one', ['FAIL'])
        self.assert_expected('mac-tahoe-wk2', 'Debug', test1, 'test1_one', ['TIMEOUT'], slow=True)
        self.assert_expected('mac-tahoe-wk2', 'Release', test2, None, ['SKIP'])

    def test_site_isolation_lookup(self):
        test1 = 'imported/w3c/webdriver/tests/test1.py'
        test3 = 'imported/w3c/webdriver/tests/test3.py'
        # Without an entry for Site Isolation, the port's other entries apply.
        self.assert_expected('mac-tahoe-wk2', 'Debug', test1, 'test1_one', ['TIMEOUT'], slow=True, site_isolation=True)
        self.assert_expected('mac-tahoe-wk2', 'Release', test3, 'test3_one', ['FAIL'], site_isolation=True)
        self.assert_expected('mac-tahoe-wk2', 'Debug', test3, 'test3_one', ['TIMEOUT'], site_isolation=True)
        self.assert_expected('mac-tahoe-wk2', 'Release', test3, 'test3_two', ['PASS'], site_isolation=True)
        # Entries for Site Isolation are only used with it.
        self.assert_expected('mac-tahoe-wk2', 'Debug', test3, 'test3_one', ['FAIL'])
        self.assert_expected('mac-tahoe-wk2', 'Release', test3, 'test3_two', ['FAIL'])
