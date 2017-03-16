# Copyright (C) 2015 Apple Inc. All rights reserved.
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
from webkitpy.xcode.simulator import Simulator


class SimulatorTest(unittest.TestCase):

    def setUp(self):
        """ Set up method for SimulatorTest """
        self._host = MockHost()

    def _set_expected_xcrun_simctl_list(self, lines):
        self._host.platform.expected_xcode_simctl_list = (line for line in lines.splitlines())

    def test_simulator_device_types(self):
        """ Tests that valid `xcrun simctl list` output is parsed as expected """
        self._set_expected_xcrun_simctl_list('''== Device Types ==
iPhone 4s (com.apple.CoreSimulator.SimDeviceType.iPhone-4s)
iPhone 5 (com.apple.CoreSimulator.SimDeviceType.iPhone-5)
iPhone 5s (com.apple.CoreSimulator.SimDeviceType.iPhone-5s)
iPhone 6 Plus (com.apple.CoreSimulator.SimDeviceType.iPhone-6-Plus)
iPhone 6 (com.apple.CoreSimulator.SimDeviceType.iPhone-6)
iPad 2 (com.apple.CoreSimulator.SimDeviceType.iPad-2)
iPad Retina (com.apple.CoreSimulator.SimDeviceType.iPad-Retina)
iPad Air (com.apple.CoreSimulator.SimDeviceType.iPad-Air)
iPad Pro (9.7-inch) (com.apple.CoreSimulator.SimDeviceType.iPad-Pro--9-7-inch-)
Apple TV 1080p (com.apple.CoreSimulator.SimDeviceType.Apple-TV-1080p)
Apple Watch - 38mm (com.apple.CoreSimulator.SimDeviceType.Apple-Watch-38mm)
Apple Watch - 42mm (com.apple.CoreSimulator.SimDeviceType.Apple-Watch-42mm)
== Runtimes ==
iOS 8.0 (8.0 - 12A465) (com.apple.CoreSimulator.SimRuntime.iOS-8-0)
iOS 8.0 Internal (8.0 - Unknown) (com.apple.CoreSimulator.SimRuntime.iOS-8-0-Internal) (unavailable, runtime path not found)
iOS 8.4 (8.4.1 - 12H321) (com.apple.CoreSimulator.SimRuntime.iOS-8-4)
tvOS 9.0 (9.0 - 13T5347l) (com.apple.CoreSimulator.SimRuntime.tvOS-9-0)
watchOS 2.0 (2.0 - 13S343) (com.apple.CoreSimulator.SimRuntime.watchOS-2-0)
== Devices ==
-- iOS 8.0 --
    iPhone 4s (68D9A792-E3A9-462B-B211-762C6A5D3779) (Shutdown)
    iPhone 5 (1C3767FF-C268-4961-B6DA-F4F75E99EF5D) (Shutdown)
    iPhone 5s (2A1CB363-9A09-4438-B9BE-9C42BD208F72) (Shutdown)
    iPhone 5s WebKit Tester (79BA9206-E0B6-4D0E-B0E8-A88E2D45515D) (Booted)
    iPhone 6 Plus (7F8039BE-D4A0-4245-9D56-AF94413FD6F5) (Shutdown)
    iPhone 6 (7BF9F835-0CEA-4EE3-BD15-A62BD9F4B691) (Shutdown)
    iPad 2 (2967C54F-A499-4043-A82C-8C1F5ADBB4A9) (Shutdown)
    iPad Retina (733FC71E-22F4-4077-BF79-25C27EA881FC) (Shutdown)
    iPad Air (67266841-82F3-4545-AED6-568B117E41A8) (Shutdown)
    iPad Pro (9.7 inch) (DEBD50B8-0566-4D7A-BCDB-4134CFE5DA40) (Shutdown)
    iPhone 7 (2AE58BBE-0995-4326-9B65-2F413E2E33DC) (Shutdown) (unavailable, runtime profile not found)
-- iOS 8.0 Internal --
-- tvOS 9.0 --
Apple TV 1080p (55281ABE-9C27-438B-AD50-C540D7BC4BAC) (Shutdown)
-- watchOS 2.0 --
    Apple Watch - 38mm (00138CD2-D30C-4380-A30E-A70B88E1A3C5) (Shutdown)
    Apple Watch - 42mm (186AD85E-9BE5-4734-BC33-DF50484AAFF0) (Shutdown)
''')
        simulator = Simulator(host=self._host)
        self.assertEqual(12, len(simulator.device_types))

        device_type_iphone_4s = simulator.device_types[0]
        self.assertEqual('iPhone 4s', device_type_iphone_4s.name)
        self.assertEqual('com.apple.CoreSimulator.SimDeviceType.iPhone-4s', device_type_iphone_4s.identifier)

        device_type_iphone_5 = simulator.device_types[1]
        self.assertEqual('iPhone 5', device_type_iphone_5.name)
        self.assertEqual('com.apple.CoreSimulator.SimDeviceType.iPhone-5', device_type_iphone_5.identifier)

        device_type_iphone_5s = simulator.device_types[2]
        self.assertEqual('iPhone 5s', device_type_iphone_5s.name)
        self.assertEqual('com.apple.CoreSimulator.SimDeviceType.iPhone-5s', device_type_iphone_5s.identifier)

        device_type_iphone_6_plus = simulator.device_types[3]
        self.assertEqual('iPhone 6 Plus', device_type_iphone_6_plus.name)
        self.assertEqual('com.apple.CoreSimulator.SimDeviceType.iPhone-6-Plus', device_type_iphone_6_plus.identifier)

        device_type_iphone_6 = simulator.device_types[4]
        self.assertEqual('iPhone 6', device_type_iphone_6.name)
        self.assertEqual('com.apple.CoreSimulator.SimDeviceType.iPhone-6', device_type_iphone_6.identifier)

        device_type_ipad_2 = simulator.device_types[5]
        self.assertEqual('iPad 2', device_type_ipad_2.name)
        self.assertEqual('com.apple.CoreSimulator.SimDeviceType.iPad-2', device_type_ipad_2.identifier)

        device_type_ipad_retina = simulator.device_types[6]
        self.assertEqual('iPad Retina', device_type_ipad_retina.name)
        self.assertEqual('com.apple.CoreSimulator.SimDeviceType.iPad-Retina', device_type_ipad_retina.identifier)

        device_type_ipad_air = simulator.device_types[7]
        self.assertEqual('iPad Air', device_type_ipad_air.name)
        self.assertEqual('com.apple.CoreSimulator.SimDeviceType.iPad-Air', device_type_ipad_air.identifier)

        device_type_ipad_pro = simulator.device_types[8]
        self.assertEqual('iPad Pro (9.7-inch)', device_type_ipad_pro.name)
        self.assertEqual('com.apple.CoreSimulator.SimDeviceType.iPad-Pro--9-7-inch-', device_type_ipad_pro.identifier)

        device_type_apple_tv_1080p = simulator.device_types[9]
        self.assertEqual('Apple TV 1080p', device_type_apple_tv_1080p.name)
        self.assertEqual('com.apple.CoreSimulator.SimDeviceType.Apple-TV-1080p', device_type_apple_tv_1080p.identifier)

        device_type_apple_watch_38mm = simulator.device_types[10]
        self.assertEqual('Apple Watch - 38mm', device_type_apple_watch_38mm.name)
        self.assertEqual('com.apple.CoreSimulator.SimDeviceType.Apple-Watch-38mm', device_type_apple_watch_38mm.identifier)

        device_type_apple_watch_42mm = simulator.device_types[11]
        self.assertEqual('Apple Watch - 42mm', device_type_apple_watch_42mm.name)
        self.assertEqual('com.apple.CoreSimulator.SimDeviceType.Apple-Watch-42mm', device_type_apple_watch_42mm.identifier)

        self.assertEqual(5, len(simulator.runtimes))

        runtime_ios_8 = simulator.runtimes[0]
        self.assertEqual('com.apple.CoreSimulator.SimRuntime.iOS-8-0', runtime_ios_8.identifier)
        self.assertEqual(True, runtime_ios_8.available)
        self.assertEqual(False, runtime_ios_8.is_internal_runtime)
        self.assertEqual(tuple([8, 0]), runtime_ios_8.version)
        self.assertEqual(11, len(runtime_ios_8.devices))

        device_iphone_4s = runtime_ios_8.devices[0]
        self.assertEqual('iPhone 4s', device_iphone_4s.name)
        self.assertEqual('68D9A792-E3A9-462B-B211-762C6A5D3779', device_iphone_4s.udid)
        self.assertEqual(True, device_iphone_4s.available)
        self.assertEqual(runtime_ios_8, device_iphone_4s.runtime)

        device_iphone_5 = runtime_ios_8.devices[1]
        self.assertEqual('iPhone 5', device_iphone_5.name)
        self.assertEqual('1C3767FF-C268-4961-B6DA-F4F75E99EF5D', device_iphone_5.udid)
        self.assertEqual(True, device_iphone_5.available)
        self.assertEqual(runtime_ios_8, device_iphone_5.runtime)

        device_iphone_5s = runtime_ios_8.devices[2]
        self.assertEqual('iPhone 5s', device_iphone_5s.name)
        self.assertEqual('2A1CB363-9A09-4438-B9BE-9C42BD208F72', device_iphone_5s.udid)
        self.assertEqual(True, device_iphone_5s.available)
        self.assertEqual(runtime_ios_8, device_iphone_5s.runtime)

        device_iphone_5s_webkit_tester = runtime_ios_8.devices[3]
        self.assertEqual('iPhone 5s WebKit Tester', device_iphone_5s_webkit_tester.name)
        self.assertEqual('79BA9206-E0B6-4D0E-B0E8-A88E2D45515D', device_iphone_5s_webkit_tester.udid)
        self.assertEqual(True, device_iphone_5s_webkit_tester.available)
        self.assertEqual(runtime_ios_8, device_iphone_5s_webkit_tester.runtime)

        device_iphone_6_plus = runtime_ios_8.devices[4]
        self.assertEqual('iPhone 6 Plus', device_iphone_6_plus.name)
        self.assertEqual('7F8039BE-D4A0-4245-9D56-AF94413FD6F5', device_iphone_6_plus.udid)
        self.assertEqual(True, device_iphone_6_plus.available)
        self.assertEqual(runtime_ios_8, device_iphone_6_plus.runtime)

        device_iphone_6 = runtime_ios_8.devices[5]
        self.assertEqual('iPhone 6', device_iphone_6.name)
        self.assertEqual('7BF9F835-0CEA-4EE3-BD15-A62BD9F4B691', device_iphone_6.udid)
        self.assertEqual(True, device_iphone_6.available)
        self.assertEqual(runtime_ios_8, device_iphone_6.runtime)

        device_ipad_2 = runtime_ios_8.devices[6]
        self.assertEqual('iPad 2', device_ipad_2.name)
        self.assertEqual('2967C54F-A499-4043-A82C-8C1F5ADBB4A9', device_ipad_2.udid)
        self.assertEqual(True, device_ipad_2.available)
        self.assertEqual(runtime_ios_8, device_ipad_2.runtime)

        device_ipad_retina = runtime_ios_8.devices[7]
        self.assertEqual('iPad Retina', device_ipad_retina.name)
        self.assertEqual('733FC71E-22F4-4077-BF79-25C27EA881FC', device_ipad_retina.udid)
        self.assertEqual(True, device_ipad_retina.available)
        self.assertEqual(runtime_ios_8, device_ipad_retina.runtime)

        device_ipad_air = runtime_ios_8.devices[8]
        self.assertEqual('iPad Air', device_ipad_air.name)
        self.assertEqual('67266841-82F3-4545-AED6-568B117E41A8', device_ipad_air.udid)
        self.assertEqual(True, device_ipad_air.available)
        self.assertEqual(runtime_ios_8, device_ipad_air.runtime)

        device_ipad_pro = runtime_ios_8.devices[9]
        self.assertEqual('iPad Pro (9.7 inch)', device_ipad_pro.name)
        self.assertEqual('DEBD50B8-0566-4D7A-BCDB-4134CFE5DA40', device_ipad_pro.udid)
        self.assertEqual(True, device_ipad_pro.available)
        self.assertEqual(runtime_ios_8, device_ipad_pro.runtime)

        device_iphone_7 = runtime_ios_8.devices[10]
        self.assertEqual('iPhone 7', device_iphone_7.name)
        self.assertEqual('2AE58BBE-0995-4326-9B65-2F413E2E33DC', device_iphone_7.udid)
        self.assertEqual(False, device_iphone_7.available)
        self.assertEqual(runtime_ios_8, device_iphone_7.runtime)

        runtime_ios_8_internal = simulator.runtimes[1]
        self.assertEqual('com.apple.CoreSimulator.SimRuntime.iOS-8-0-Internal', runtime_ios_8_internal.identifier)
        self.assertEqual(False, runtime_ios_8_internal.available)
        self.assertEqual(True, runtime_ios_8_internal.is_internal_runtime)
        self.assertEqual(tuple([8, 0]), runtime_ios_8_internal.version)
        self.assertEqual(0, len(runtime_ios_8_internal.devices))

        runtime_ios_8_4 = simulator.runtimes[2]
        self.assertEqual('com.apple.CoreSimulator.SimRuntime.iOS-8-4', runtime_ios_8_4.identifier)
        self.assertEqual(True, runtime_ios_8_4.available)
        self.assertEqual(False, runtime_ios_8_4.is_internal_runtime)
        self.assertEqual(tuple([8, 4]), runtime_ios_8_4.version)
        self.assertEqual(0, len(runtime_ios_8_4.devices))

        runtime_tvos_9 = simulator.runtimes[3]
        self.assertEqual('com.apple.CoreSimulator.SimRuntime.tvOS-9-0', runtime_tvos_9.identifier)
        self.assertEqual(True, runtime_tvos_9.available)
        self.assertEqual(False, runtime_tvos_9.is_internal_runtime)
        self.assertEqual(tuple([9, 0]), runtime_tvos_9.version)
        self.assertEqual(1, len(runtime_tvos_9.devices))

        device_apple_tv_1080p = runtime_tvos_9.devices[0]
        self.assertEqual('Apple TV 1080p', device_apple_tv_1080p.name)
        self.assertEqual('55281ABE-9C27-438B-AD50-C540D7BC4BAC', device_apple_tv_1080p.udid)
        self.assertEqual(True, device_apple_tv_1080p.available)
        self.assertEqual(runtime_tvos_9, device_apple_tv_1080p.runtime)

        runtime_watchos_2 = simulator.runtimes[4]
        self.assertEqual('com.apple.CoreSimulator.SimRuntime.watchOS-2-0', runtime_watchos_2.identifier)
        self.assertEqual(True, runtime_watchos_2.available)
        self.assertEqual(False, runtime_watchos_2.is_internal_runtime)
        self.assertEqual(tuple([2, 0]), runtime_watchos_2.version)
        self.assertEqual(2, len(runtime_watchos_2.devices))

        device_apple_watch_38mm = runtime_watchos_2.devices[0]
        self.assertEqual('Apple Watch - 38mm', device_apple_watch_38mm.name)
        self.assertEqual('00138CD2-D30C-4380-A30E-A70B88E1A3C5', device_apple_watch_38mm.udid)
        self.assertEqual(True, device_apple_watch_38mm.available)
        self.assertEqual(runtime_watchos_2, device_apple_watch_38mm.runtime)

        device_apple_watch_42mm = runtime_watchos_2.devices[1]
        self.assertEqual('Apple Watch - 42mm', device_apple_watch_42mm.name)
        self.assertEqual('186AD85E-9BE5-4734-BC33-DF50484AAFF0', device_apple_watch_42mm.udid)
        self.assertEqual(True, device_apple_watch_42mm.available)
        self.assertEqual(runtime_watchos_2, device_apple_watch_42mm.runtime)

    def test_invalid_device_types_header(self):
        """ Tests that an invalid Device Types header throws an exception """
        self._set_expected_xcrun_simctl_list('''XX Device Types XX
''')
        with self.assertRaises(RuntimeError) as cm:
            Simulator(host=self._host)
        self.assertEqual('Expected == Device Types == header but got: "XX Device Types XX"', cm.exception.message)

    def test_invalid_runtimes_header(self):
        """ Tests that an invalid Runtimes header throws an exception """
        self._set_expected_xcrun_simctl_list('''== Device Types ==
iPhone 4s (com.apple.CoreSimulator.SimDeviceType.iPhone-4s)
XX Runtimes XX
''')
        with self.assertRaises(RuntimeError) as cm:
            Simulator(host=self._host)
        self.assertEqual('Expected == Runtimes == header but got: "XX Runtimes XX"', cm.exception.message)

    def test_invalid_devices_header(self):
        """ Tests that an invalid Devices header throws an exception """
        self._set_expected_xcrun_simctl_list('''== Device Types ==
iPhone 4s (com.apple.CoreSimulator.SimDeviceType.iPhone-4s)
== Runtimes ==
iOS 8.0 (8.0 - 12A465) (com.apple.CoreSimulator.SimRuntime.iOS-8-0)
XX Devices XX
''')
        with self.assertRaises(RuntimeError) as cm:
            Simulator(host=self._host)
        self.assertEqual('Expected == Devices == header but got: "XX Devices XX"', cm.exception.message)

    def test_unavailable_devices(self):
        """ Tests that unavailable devices are ignored """
        self._set_expected_xcrun_simctl_list('''== Device Types ==
iPhone 4s (com.apple.CoreSimulator.SimDeviceType.iPhone-4s)
== Runtimes ==
iOS 8.0 (8.0 - 12A465) (com.apple.CoreSimulator.SimRuntime.iOS-8-0)
iOS 8.0 Internal (8.0 - Unknown) (com.apple.CoreSimulator.SimRuntime.iOS-8-0-Internal) (unavailable, runtime path not found)
== Devices ==
-- iOS 8.0 --
    iPhone 4s (271BBEAC-1826-4CE1-B3AF-83F35CDD1D82) (Shutdown)
-- iOS 8.0 Internal --
-- Unavailable: com.apple.CoreSimulator.SimRuntime.iOS-8-1 --
    iPhone 4s (08C0542B-65F7-46E8-B203-CB4055207BC8) (Shutdown) (unavailable, runtime profile not found)
-- Unavailable: com.apple.CoreSimulator.SimRuntime.iOS-8-2 --
    iPhone 4s (A36F7432-0AF5-49C4-A261-C44383992597) (Shutdown) (unavailable, runtime profile not found)
''')
        simulator = Simulator(host=self._host)
        self.assertEqual(1, len(simulator.device_types))

        device_type_iphone_4s = simulator.device_types[0]
        self.assertEqual('iPhone 4s', device_type_iphone_4s.name)
        self.assertEqual('com.apple.CoreSimulator.SimDeviceType.iPhone-4s', device_type_iphone_4s.identifier)

        self.assertEqual(2, len(simulator.runtimes))

        runtime_ios_8 = simulator.runtimes[0]
        self.assertEqual('com.apple.CoreSimulator.SimRuntime.iOS-8-0', runtime_ios_8.identifier)
        self.assertEqual(True, runtime_ios_8.available)
        self.assertEqual(False, runtime_ios_8.is_internal_runtime)
        self.assertEqual(tuple([8, 0]), runtime_ios_8.version)
        self.assertEqual(1, len(runtime_ios_8.devices))

        device_iphone_4s = runtime_ios_8.devices[0]
        self.assertEqual('iPhone 4s', device_iphone_4s.name)
        self.assertEqual('271BBEAC-1826-4CE1-B3AF-83F35CDD1D82', device_iphone_4s.udid)
        self.assertEqual(True, device_iphone_4s.available)
        self.assertEqual(runtime_ios_8, device_iphone_4s.runtime)

        runtime_ios_8_internal = simulator.runtimes[1]
        self.assertEqual('com.apple.CoreSimulator.SimRuntime.iOS-8-0-Internal', runtime_ios_8_internal.identifier)
        self.assertEqual(False, runtime_ios_8_internal.available)
        self.assertEqual(True, runtime_ios_8_internal.is_internal_runtime)
        self.assertEqual(tuple([8, 0]), runtime_ios_8_internal.version)
        self.assertEqual(0, len(runtime_ios_8_internal.devices))

    def test_device_pairs(self):
        """ Tests that Device Pairs header does not cause parsing exception """
        self._set_expected_xcrun_simctl_list('''== Device Types ==
iPhone 4s (com.apple.CoreSimulator.SimDeviceType.iPhone-4s)
== Runtimes ==
iOS 8.0 (8.0 - 12A465) (com.apple.CoreSimulator.SimRuntime.iOS-8-0)
iOS 8.0 Internal (8.0 - Unknown) (com.apple.CoreSimulator.SimRuntime.iOS-8-0-Internal) (unavailable, runtime path not found)
== Devices ==
-- iOS 8.0 --
    iPhone 4s (271BBEAC-1826-4CE1-B3AF-83F35CDD1D82) (Shutdown)
== Device Pairs ==
''')
        simulator = Simulator(host=self._host)
        self.assertEqual(1, len(simulator.device_types))

        device_type_iphone_4s = simulator.device_types[0]
        self.assertEqual('iPhone 4s', device_type_iphone_4s.name)
        self.assertEqual('com.apple.CoreSimulator.SimDeviceType.iPhone-4s', device_type_iphone_4s.identifier)

        self.assertEqual(2, len(simulator.runtimes))

        runtime_ios_8 = simulator.runtimes[0]
        self.assertEqual('com.apple.CoreSimulator.SimRuntime.iOS-8-0', runtime_ios_8.identifier)
        self.assertEqual(True, runtime_ios_8.available)
        self.assertEqual(False, runtime_ios_8.is_internal_runtime)
        self.assertEqual(tuple([8, 0]), runtime_ios_8.version)
        self.assertEqual(1, len(runtime_ios_8.devices))

        device_iphone_4s = runtime_ios_8.devices[0]
        self.assertEqual('iPhone 4s', device_iphone_4s.name)
        self.assertEqual('271BBEAC-1826-4CE1-B3AF-83F35CDD1D82', device_iphone_4s.udid)
        self.assertEqual(True, device_iphone_4s.available)
        self.assertEqual(runtime_ios_8, device_iphone_4s.runtime)
