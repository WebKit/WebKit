# Copyright (C) 2018 Igalia S.L.
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

import json
import os

from webkitpy.common.host_mock import MockHost
from webkitpy.common.test_expectations import TestExpectations
from webkitpy.common.webkit_finder import WebKitFinder


class MockTestExpectations(TestExpectations):

    def __init__(self, port, expectations, build_type='Release'):
        host = MockHost()
        port = host.port_factory.get(port)
        self._port_name = port.name()
        self._build_type = build_type
        self._expectations = json.loads(expectations)

    def is_skip(self, test, subtest):
        if test in self.skipped_tests():
            return True
        return subtest in self.skipped_subtests(test)


class ExpectationsTest(unittest.TestCase):

    BASIC = """
{
    "imported/w3c/webdriver/tests/test1.py": {
        "subtests": {
            "test1_one": {
                "expected": {"all": {"status": ["FAIL"], "bug": "1234"}}
            },
            "test1_two": {
                "expected": {
                    "all": {"status": ["FAIL"], "bug": "1234"},
                    "gtk": {"status": ["PASS"]}
                }
            }
        }
    },
    "imported/w3c/webdriver/tests/test2.py": {
        "expected": {"all": {"status": ["FAIL"], "bug": "1234"}}
    },
    "imported/w3c/webdriver/tests/test3.py": {
        "expected": {"all": {"status": ["FAIL"], "bug": "1234"}},
        "subtests": {
            "test3_one": {
                "expected": {"all": {"status": ["PASS"]}}
            }
        }
    }
}"""

    SKIP = """
{
    "imported/w3c/webdriver/tests/test1.py": {
        "expected": {"all": {"status": ["SKIP"], "bug": "1234"}}
    },
    "imported/w3c/webdriver/tests/test2.py": {
        "expected": {"gtk": {"status": ["SKIP"], "bug": "1234"}}
    },
    "imported/w3c/webdriver/tests/test3.py": {
        "expected": {"all": {"status": ["SKIP"], "bug": "1234"}},
        "subtests": {
            "test3_one": {
                "expected": {"all": {"status": ["FAIL"], "bug": "1234"}}
            }
        }
    },
    "imported/w3c/webdriver/tests/test4.py": {
        "subtests": {
            "test4_one": {
                "expected": {"all": {"status": ["SKIP"], "bug": "1234"}}
            }
        }
    }
}"""

    FLAKY = """
{
    "TestCookieManager": {
        "subtests": {
            "/webkit2/WebKitCookieManager/persistent-storage": {
                "expected": {"gtk": {"status": ["FAIL", "PASS"], "bug": "1234"}}
            }
        }
    },
    "TestWebKit": {
        "subtests": {
            "WebKit.MouseMoveAfterCrash": {
                "expected": {"all": {"status": ["CRASH", "PASS"], "bug": "1234"}}
            },
            "WebKit.WKConnection": {
                "expected": {"wpe": {"status": ["FAIL", "TIMEOUT"], "bug": "1234"}}
            }
        }
    }
}"""

    BUILD_TYPE = """
{
    "TestCookieManager": {
        "subtests": {
            "/webkit2/WebKitCookieManager/persistent-storage": {
                "expected": {"all": {"status": ["FAIL"], "bug": "1234"}}
            }
        }
    },
    "TestWebKit": {
        "subtests": {
            "WebKit.MouseMoveAfterCrash": {
                "expected": {"all@Debug": {"status": ["CRASH"], "bug": "1234"}}
            },
            "WebKit.WKConnection": {
                "expected": {"gtk@Release": {"status": ["FAIL"], "bug": "1234"},
                             "gtk@Debug": {"status": ["CRASH"], "bug": "1234"}}
            }
        }
    },
    "TestWebCore": {
        "expected": {"all@Debug": {"status": ["CRASH"]}},
        "subtests": {
            "ComplexTextControllerTest.InitialAdvanceWithLeftRunInRTL": {
                "expected": {"all": {"status": ["PASS"], "bug": "1234"}}
            },
            "FileMonitorTest.DetectChange": {
                "expected": {"all@Release": {"status": ["FAIL"], "bug": "1234"}}
            }
        }
    },
    "TestWebViewEditor": {
        "expected": {"all@Release": {"status": ["SKIP"]},
                     "wpe@Debug": {"status": ["SKIP"]}},
        "subtests": {
            "/webkit2/WebKitWebView/editable/editable": {
                "expected": {"gtk": {"status": ["FAIL"], "bug": "1234"}}
            }
        }
    }
}"""

    SLOW = """
{
    "TestCookieManager": {
        "expected": {"all": {"slow": true}},
        "subtests": {
            "/webkit2/WebKitCookieManager/persistent-storage": {
                "expected": {"wpe": {"status": ["FAIL"], "slow": false, "bug": "1234"}}
            }
        }
    },
    "TestWebKit": {
        "subtests": {
            "WebKit.MouseMoveAfterCrash": {
                "expected": {"all": {"status": ["FAIL"], "slow": true, "bug": "1234"}}
            },
            "WebKit.WKConnection": {
                "expected": {"gtk": {"status": ["CRASH"], "bug": "1234"}}
            }
        }
    }
}"""

    def assert_exp(self, test, subtest, result):
        self.assertIn(result, self.expectations.get_expectation(test, subtest))

    def assert_not_exp(self, test, subtest, result):
        self.assertNotIn(result, self.expectations.get_expectation(test, subtest))

    def assert_bad_exp(self, test):
        self.assertRaises(AssertionError, self.expectations.get_expectation(test))

    def assert_skip(self, test, subtest, result):
        self.assertEqual(self.expectations.is_skip(test, subtest), result)

    def assert_slow(self, test, subtest, result):
        self.assertEqual(self.expectations.is_slow(test, subtest), result)

    def test_basic(self):
        self.expectations = MockTestExpectations('gtk', self.BASIC)
        self.assert_exp('imported/w3c/webdriver/tests/test5.py', 'test5_two', 'PASS')

        self.assert_exp('imported/w3c/webdriver/tests/test1.py', 'test1_five', 'PASS')
        self.assert_exp('imported/w3c/webdriver/tests/test1.py', 'test1_one', 'FAIL')
        self.assert_exp('imported/w3c/webdriver/tests/test1.py', 'test1_two', 'PASS')

        self.assert_exp('imported/w3c/webdriver/tests/test2.py', 'test2_one', 'FAIL')

        self.assert_exp('imported/w3c/webdriver/tests/test3.py', 'test3_two', 'FAIL')
        self.assert_exp('imported/w3c/webdriver/tests/test3.py', 'test3_one', 'PASS')

        self.expectations = MockTestExpectations('wpe', self.BASIC)
        self.assert_exp('imported/w3c/webdriver/tests/test1.py', 'test1_two', 'FAIL')

    def test_skip(self):
        self.expectations = MockTestExpectations('gtk', self.SKIP)
        self.assert_skip('imported/w3c/webdriver/tests/test5.py', None, False)
        self.assert_skip('imported/w3c/webdriver/tests/test1.py', None, True)
        self.assert_skip('imported/w3c/webdriver/tests/test2.py', None, True)
        self.assert_skip('imported/w3c/webdriver/tests/test3.py', None, True)
        self.assert_skip('imported/w3c/webdriver/tests/test3.py', 'test3_one', True)
        self.assert_skip('imported/w3c/webdriver/tests/test4.py', None, False)
        self.assert_skip('imported/w3c/webdriver/tests/test4.py', 'test4_one', True)
        self.assert_exp('imported/w3c/webdriver/tests/test4.py', 'test4_one', 'SKIP')

        self.expectations = MockTestExpectations('wpe', self.SKIP)
        self.assert_skip('imported/w3c/webdriver/tests/test2.py', None, False)

    def test_flaky(self):
        self.expectations = MockTestExpectations('gtk', self.FLAKY)
        self.assert_exp('TestCookieManager', '/webkit2/WebKitCookieManager/persistent-storage', 'PASS')
        self.assert_exp('TestCookieManager', '/webkit2/WebKitCookieManager/persistent-storage', 'FAIL')
        self.assert_exp('TestWebKit', 'WebKit.MouseMoveAfterCrash', 'CRASH')
        self.assert_exp('TestWebKit', 'WebKit.MouseMoveAfterCrash', 'PASS')
        self.assert_exp('TestWebKit', 'WebKit.WKConnection', 'PASS')
        self.assert_not_exp('TestWebKit', 'WebKit.WKConnection', 'FAIL')
        self.assert_not_exp('TestWebKit', 'WebKit.WKConnection', 'TIMEOUT')

        self.expectations = MockTestExpectations('wpe', self.FLAKY)
        self.assert_exp('TestCookieManager', '/webkit2/WebKitCookieManager/persistent-storage', 'PASS')
        self.assert_not_exp('TestCookieManager', '/webkit2/WebKitCookieManager/persistent-storage', 'FAIL')
        self.assert_exp('TestWebKit', 'WebKit.MouseMoveAfterCrash', 'CRASH')
        self.assert_exp('TestWebKit', 'WebKit.MouseMoveAfterCrash', 'PASS')
        self.assert_exp('TestWebKit', 'WebKit.WKConnection', 'FAIL')
        self.assert_exp('TestWebKit', 'WebKit.WKConnection', 'TIMEOUT')

    def test_build_type(self):
        self.expectations = MockTestExpectations('gtk', self.BUILD_TYPE, 'Debug')
        self.assert_exp('TestCookieManager', '/webkit2/WebKitCookieManager/persistent-storage', 'FAIL')
        self.assert_exp('TestWebKit', 'WebKit.MouseMoveAfterCrash', 'CRASH')
        self.assert_exp('TestWebKit', 'WebKit.WKConnection', 'CRASH')
        self.assert_exp('TestWebCore', 'ComplexTextControllerTest.InitialAdvanceWithLeftRunInRTL', 'PASS')
        self.assert_exp('TestWebCore', 'FileMonitorTest.DetectChange', 'CRASH')
        self.assert_exp('TestWebCore', 'FileSystemTest.MappingMissingFile', 'CRASH')
        self.assert_skip('TestWebViewEditor', None, False)
        self.assert_skip('TestWebViewEditor', '/webkit2/WebKitWebView/editable/editable', False)
        self.assert_exp('TestWebViewEditor', '/webkit2/WebKitWebView/editable/editable', 'FAIL')

        self.expectations = MockTestExpectations('gtk', self.BUILD_TYPE, 'Release')
        self.assert_exp('TestCookieManager', '/webkit2/WebKitCookieManager/persistent-storage', 'FAIL')
        self.assert_exp('TestWebKit', 'WebKit.MouseMoveAfterCrash', 'PASS')
        self.assert_exp('TestWebKit', 'WebKit.WKConnection', 'FAIL')
        self.assert_exp('TestWebCore', 'ComplexTextControllerTest.InitialAdvanceWithLeftRunInRTL', 'PASS')
        self.assert_exp('TestWebCore', 'FileMonitorTest.DetectChange', 'FAIL')
        self.assert_exp('TestWebCore', 'FileSystemTest.MappingMissingFile', 'PASS')
        self.assert_skip('TestWebViewEditor', None, True)
        self.assert_skip('TestWebViewEditor', '/webkit2/WebKitWebView/editable/editable', True)

        self.expectations = MockTestExpectations('wpe', self.BUILD_TYPE, 'Release')
        self.assert_skip('TestWebViewEditor', None, True)
        self.assert_skip('TestWebViewEditor', '/webkit2/WebKitWebView/editable/editable', True)

        self.expectations = MockTestExpectations('wpe', self.BUILD_TYPE, 'Debug')
        self.assert_skip('TestWebViewEditor', None, True)
        self.assert_skip('TestWebViewEditor', '/webkit2/WebKitWebView/editable/editable', True)

    def test_slow(self):
        self.expectations = MockTestExpectations('gtk', self.SLOW)
        self.assert_slow('TestCookieManager', '/webkit2/WebKitCookieManager/basic', True)
        self.assert_slow('TestCookieManager', '/webkit2/WebKitCookieManager/persistent-storage', True)
        self.assert_slow('TestWebKit', 'WebKit.WKView', False)
        self.assert_slow('TestWebKit', 'WebKit.MouseMoveAfterCrash', True)
        self.assert_exp('TestWebKit', 'WebKit.MouseMoveAfterCrash', 'FAIL')
        self.assert_slow('TestWebKit', 'WebKit.WKConnection', False)

        self.expectations = MockTestExpectations('wpe', self.SLOW)
        self.assert_slow('TestCookieManager', '/webkit2/WebKitCookieManager/basic', True)
        self.assert_slow('TestCookieManager', '/webkit2/WebKitCookieManager/persistent-storage', False)
        self.assert_slow('TestWebKit', 'WebKit.WKView', False)
        self.assert_slow('TestWebKit', 'WebKit.MouseMoveAfterCrash', True)
        self.assert_slow('TestWebKit', 'WebKit.WKConnection', False)
