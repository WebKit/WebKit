# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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
from webkitpy.results.configuration import API_TESTS, LAYOUT_TESTS, ExpectationLine, expectation_line_for, expectation_lines_for, port_for_configuration
from webkitpy.xcode.device_type import DeviceType


def mac_configuration(**kwargs):
    configuration = dict(platform='mac', is_simulator=False, version=15007003, version_name='Sequoia', architecture='arm64', model='Mac16,11', style='release', flavor='wk2')
    configuration.update(kwargs)
    return configuration


def ios_configuration(**kwargs):
    configuration = dict(platform='ios', is_simulator=True, version=26005000, version_name='iOS 26', architecture='arm64', model='iPhone 12', style='release', flavor='wk2')
    configuration.update(kwargs)
    return configuration


class PortForConfigurationTest(unittest.TestCase):
    def test_mac_wk2(self):
        port, device_type = port_for_configuration(mac_configuration(), LAYOUT_TESTS, host=MockHost())
        self.assertEqual(port.name(), 'mac-sequoia-wk2')
        self.assertEqual(str(port.test_configuration()), '<sequoia, arm64, release>')
        self.assertIsNone(device_type)

    def test_mac_wk1(self):
        port, _ = port_for_configuration(mac_configuration(version='26.5', style='debug', flavor='wk1'), LAYOUT_TESTS, host=MockHost())
        self.assertEqual(port.name(), 'mac-tahoe')
        self.assertTrue(port.is_webkitlegacy())
        self.assertEqual(str(port.test_configuration()), '<tahoe, arm64, debug>')

    def test_styles(self):
        for style in ('guard-malloc', 'asan'):
            port, _ = port_for_configuration(mac_configuration(style=style), LAYOUT_TESTS, host=MockHost())
            self.assertEqual(port.test_configuration().build_type, style)
            self.assertEqual(port.get_option('configuration'), 'Release')

    def test_local_asan_build(self):
        host = MockHost()
        port, _ = port_for_configuration(mac_configuration(), LAYOUT_TESTS, host=host)
        host.filesystem.write_text_file(host.filesystem.join(port._config.build_directory(None), 'ASan'), '')
        port, _ = port_for_configuration(mac_configuration(), LAYOUT_TESTS, host=host)
        self.assertEqual(port.test_configuration().build_type, 'release')

    def test_mac_site_isolation(self):
        port, _ = port_for_configuration(mac_configuration(flavor='site-isolation'), LAYOUT_TESTS, host=MockHost())
        self.assertEqual(port.name(), 'mac-sequoia-wk2')
        self.assertTrue(port.get_option('site_isolation_enabled_by_default'))
        self.assertEqual(port.get_option('additional_expectations'), ['/mock-checkout/LayoutTests/platform/mac-site-isolation/TestExpectations'])
        self.assertEqual(port.get_option('additional_platform_directory'), ['/mock-checkout/LayoutTests/platform/mac-site-isolation'])
        self.assertEqual(port.get_option('result_report_flavor'), 'site-isolation')

    def test_accessibility_isolated_tree(self):
        port, _ = port_for_configuration(mac_configuration(flavor='accessibility-isolated-tree'), LAYOUT_TESTS, host=MockHost())
        self.assertEqual(port.get_option('additional_expectations'), ['/mock-checkout/LayoutTests/accessibility-isolated-tree/TestExpectations'])
        self.assertEqual(port.get_option('result_report_flavor'), 'accessibility-isolated-tree')

    def test_iphone(self):
        port, device_type = port_for_configuration(ios_configuration(style='debug'), LAYOUT_TESTS, host=MockHost())
        self.assertEqual(port.port_name, 'iphone-simulator')
        self.assertEqual(str(port.device_version()), '26.5')
        self.assertEqual(port.test_configuration().build_type, 'debug')
        self.assertEqual(device_type, DeviceType.from_string('iPhone 12'))

    def test_ipad_site_isolation(self):
        port, device_type = port_for_configuration(ios_configuration(model='iPad (9th generation)', flavor='site-isolation'), LAYOUT_TESTS, host=MockHost())
        self.assertEqual(port.port_name, 'ipad-simulator')
        self.assertEqual(device_type, DeviceType.from_string('iPad (9th generation)'))
        self.assertEqual(port.get_option('additional_expectations'), ['/mock-checkout/LayoutTests/platform/ios-site-isolation/TestExpectations'])

    def test_ios_without_model(self):
        port, device_type = port_for_configuration(ios_configuration(version='26.5', model=None), LAYOUT_TESTS, host=MockHost())
        self.assertEqual(port.port_name, 'ios-simulator')
        self.assertIsNone(device_type)

    def test_ios_device(self):
        port, _ = port_for_configuration(ios_configuration(is_simulator=False, flavor=None), API_TESTS, host=MockHost())
        self.assertEqual(port.port_name, 'ios-device')

    def test_visionos(self):
        port, device_type = port_for_configuration(ios_configuration(platform='visionos', model='Vision Pro'), LAYOUT_TESTS, host=MockHost())
        self.assertEqual(port.port_name, 'visionos-simulator')
        self.assertEqual(device_type, DeviceType.from_string('Vision Pro'))

    def test_api_tests_site_isolation(self):
        port, _ = port_for_configuration(ios_configuration(flavor='site-isolation'), API_TESTS, host=MockHost())
        self.assertEqual(port.api_test_current_configuration(), {
            'platform': 'ios', 'version': 'ios26', 'style': 'release', 'architecture': 'arm64',
            'flavor': 'siteisolation', 'hardware': 'simulator', 'hardware_type': 'iphone',
        })

    def test_unsupported(self):
        for configuration, suite in (
            (mac_configuration(flavor='unknown'), LAYOUT_TESTS),
            (mac_configuration(flavor='wk2'), API_TESTS),
            (mac_configuration(), 'javascriptcore-tests'),
            (mac_configuration(style='unknown'), LAYOUT_TESTS),
            (mac_configuration(version=None), LAYOUT_TESTS),
            (mac_configuration(version='99.0'), LAYOUT_TESTS),
            (mac_configuration(platform='GTK'), LAYOUT_TESTS),
        ):
            with self.assertRaises(ValueError):
                port_for_configuration(configuration, suite, host=MockHost())


class ExpectationLinesTest(unittest.TestCase):
    def setUp(self):
        self.host = MockHost()
        for test in ('fast/a.html', 'fast/dir/b.html', 'fast/c.html'):
            self.host.filesystem.write_text_file('/mock-checkout/LayoutTests/{}'.format(test), '')
        self.host.filesystem.write_text_file('/mock-checkout/LayoutTests/TestExpectations', 'webkit.org/b/1001 fast/a.html [ Failure ]\nfast/dir [ Skip ]\n')
        self.host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/mac-site-isolation/TestExpectations', 'fast/c.html [ Pass Timeout ] # webkit.org/b/1002\n')
        self.host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/ipad/TestExpectations', 'webkit.org/b/1003 fast/c.html [ ImageOnlyFailure ]\n')
        self.host.filesystem.write_text_file('/mock-checkout/TestExpectations/apitests', '\n'.join([
            'webkit.org/b/2001 TestWebKitAPI.Suite.Flaky [ Pass Fail ]',
            '[ siteisolation ] TestWebKitAPI.Suite.SiteIsolation [ Skip ] # webkit.org/b/2002',
            '',
        ]))

    def test_layout_tests(self):
        self.assertEqual(expectation_lines_for(['fast/a.html', 'fast/dir/b.html', 'fast/c.html'], mac_configuration(), LAYOUT_TESTS, host=self.host), {
            'fast/a.html': ExpectationLine('/mock-checkout/LayoutTests/TestExpectations', 1, ('webkit.org/b/1001',), ('FAIL',)),
            'fast/dir/b.html': ExpectationLine('/mock-checkout/LayoutTests/TestExpectations', 2, (), ('SKIP',)),
        })

    def test_layout_tests_flavor(self):
        self.assertEqual(
            expectation_line_for('fast/c.html', mac_configuration(flavor='site-isolation'), LAYOUT_TESTS, host=self.host),
            ExpectationLine('/mock-checkout/LayoutTests/platform/mac-site-isolation/TestExpectations', 1, ('webkit.org/b/1002',), ('PASS', 'TIMEOUT')),
        )

    def test_layout_tests_device_type(self):
        self.assertIsNone(expectation_line_for('fast/c.html', ios_configuration(), LAYOUT_TESTS, host=self.host))
        self.assertEqual(
            expectation_line_for('fast/c.html', ios_configuration(model='iPad (9th generation)'), LAYOUT_TESTS, host=self.host),
            ExpectationLine('/mock-checkout/LayoutTests/platform/ipad/TestExpectations', 1, ('webkit.org/b/1003',), ('IMAGE',)),
        )

    def test_api_tests(self):
        self.assertEqual(expectation_lines_for(['TestWebKitAPI.Suite.Flaky', 'TestWebKitAPI.Suite.SiteIsolation'], mac_configuration(flavor=None), API_TESTS, host=self.host), {
            'TestWebKitAPI.Suite.Flaky': ExpectationLine('/mock-checkout/TestExpectations/apitests', 1, ('webkit.org/b/2001',), ('PASS', 'FAIL')),
        })

    def test_api_tests_flavor(self):
        self.assertEqual(
            expectation_line_for('TestWebKitAPI.Suite.SiteIsolation', mac_configuration(flavor='site-isolation'), API_TESTS, host=self.host),
            ExpectationLine('/mock-checkout/TestExpectations/apitests', 2, ('webkit.org/b/2002',), ('SKIP',)),
        )
