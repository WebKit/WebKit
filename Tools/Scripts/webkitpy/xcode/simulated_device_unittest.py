# Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

import json
import multiprocessing
import plistlib
import time
import unittest

from webkitcorepy import Version

from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.xcode.device_type import DeviceType
from webkitpy.xcode import simulated_device
from webkitpy.xcode.simulated_device import DeviceRequest, SimulatedDeviceManager, SimulatedDevice
from webkitpy.xcode.simulator_daemons import disabled_launchd_jobs

simctl_json_output = """{
 "devicetypes" : [
   {
     "name" : "iPhone 4s",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPhone-4s"
   },
   {
     "name" : "iPhone 5",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPhone-5"
   },
   {
     "name" : "iPhone 5s",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPhone-5s"
   },
   {
     "name" : "iPhone 6",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPhone-6"
   },
   {
     "name" : "iPhone 6 Plus",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPhone-6-Plus"
   },
   {
     "name" : "iPhone 6s",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPhone-6s"
   },
   {
     "name" : "iPhone 6s Plus",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPhone-6s-Plus"
   },
   {
     "name" : "iPhone 7",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPhone-7"
   },
   {
     "name" : "iPhone 7 Plus",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPhone-7-Plus"
   },
   {
     "name" : "iPhone 8",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPhone-8"
   },
   {
     "name" : "iPhone 8 Plus",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPhone-8-Plus"
   },
   {
     "name" : "iPhone SE (1st generation)",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPhone-SE"
   },
   {
     "name" : "iPhone X",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPhone-X"
   },
   {
     "name" : "iPhone Xs",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPhone-Xs"
   },
   {
     "name" : "iPad 2",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPad-2"
   },
   {
     "name" : "iPad Retina",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPad-Retina"
   },
   {
     "name" : "iPad Air",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPad-Air"
   },
   {
     "name" : "iPad Air 2",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPad-Air-2"
   },
   {
     "name" : "iPad (9th generation)",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPad--9th-generation-"
   },
   {
     "name" : "iPad Pro (9.7-inch)",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPad-Pro--9-7-inch-"
   },
   {
     "name" : "iPad Pro (12.9-inch)",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPad-Pro"
   },
   {
     "name" : "iPad Pro (12.9-inch) (2nd generation)",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPad-Pro--12-9-inch---2nd-generation-"
   },
   {
     "name" : "iPad Pro (10.5-inch)",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPad-Pro--10-5-inch-"
   },
   {
     "name" : "Apple TV",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.Apple-TV-1080p"
   },
   {
     "name" : "Apple TV 4K",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.Apple-TV-4K-4K"
   },
   {
     "name" : "Apple TV 4K (at 1080p)",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.Apple-TV-4K-1080p"
   },
   {
     "name" : "Apple Watch - 38mm",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.Apple-Watch-38mm"
   },
   {
     "name" : "Apple Watch - 42mm",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.Apple-Watch-42mm"
   },
   {
     "name" : "Apple Watch Series 2 - 38mm",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.Apple-Watch-Series-2-38mm"
   },
   {
     "name" : "Apple Watch Series 2 - 42mm",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.Apple-Watch-Series-2-42mm"
   },
   {
     "name" : "Watch2017 - 38mm",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.Apple-Watch-Series-3-38mm"
   },
   {
     "name" : "Watch2017 - 42mm",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.Apple-Watch-Series-3-42mm"
   }
 ],
 "runtimes" : [
   {
     "runtimeRoot" : "/path/to/RuntimeRoot",
     "buildversion" : "13E233",
     "availability" : "(available)",
     "name" : "iOS 9.3",
     "identifier" : "com.apple.CoreSimulator.SimRuntime.iOS-9-3",
     "version" : "9.3"
   },
   {
     "runtimeRoot" : "/path/to/RuntimeRoot",
     "buildversion" : "15A8401",
     "availability" : "(available)",
     "name" : "iOS 11.0",
     "identifier" : "com.apple.CoreSimulator.SimRuntime.iOS-11-0",
     "version" : "11.0.1"
   },
   {
     "runtimeRoot" : "/path/to/RuntimeRoot",
     "buildversion" : "15J380",
     "availability" : "(available)",
     "name" : "tvOS 11.0",
     "identifier" : "com.apple.CoreSimulator.SimRuntime.tvOS-11-0",
     "version" : "11.0"
   },
   {
     "runtimeRoot" : "/path/to/RuntimeRoot",
     "buildversion" : "15R372",
     "availability" : "(available)",
     "name" : "watchOS 4.0",
     "identifier" : "com.apple.CoreSimulator.SimRuntime.watchOS-4-0",
     "version" : "4.0"
   },
   {
     "runtimeRoot" : "/path/to/RuntimeRoot",
     "buildversion" : "16A367",
     "isAvailable" : "YES",
     "name" : "iOS 12.0",
     "identifier" : "com.apple.CoreSimulator.SimRuntime.iOS-12-0",
     "version" : "12.0"
   }
 ],
 "devices" : {
   "watchOS 4.0" : [
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "Apple Watch - 38mm",
       "udid" : "ACCA529B-DAED-4684-ACE5-0BB3A6245064"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "Apple Watch - 42mm",
       "udid" : "46948CF4-B5E3-485B-87CA-DD303FFA7F9B"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "Apple Watch Series 2 - 38mm",
       "udid" : "A0A989D0-C5B8-432D-869F-54640FD6739D"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "Apple Watch Series 2 - 42mm",
       "udid" : "AB05A1C2-1049-4087-BEDB-326B42D58CDD"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "Watch2017 - 38mm",
       "udid" : "8EFAD24B-2EB3-48AF-9484-F97AA418C5D6"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "Watch2017 - 42mm",
       "udid" : "3B477D07-65AD-481A-9506-3776817A6293"
     }
   ],
   "iOS 9.3" : [
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 4s",
       "udid" : "837FF579-72A0-4D30-B95B-956E89CE6CDC"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 5",
       "udid" : "46C5F828-1394-4F98-83CA-3CE18020DA5B"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 5s",
       "udid" : "7760B62D-26D9-4E1E-B429-18CED8CC71E4"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 6",
       "udid" : "19135EEB-2792-4ED6-82AD-374C3F1F5DAC"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 6 Plus",
       "udid" : "60691334-2C32-4366-B489-F13FA3579066"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 6s",
       "udid" : "696BE729-5C61-42FE-9502-E183DB7222C5"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 6s Plus",
       "udid" : "32CBF7F4-36E4-417B-929C-9C3863E6C7FD"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPad 2",
       "udid" : "4043B3B9-8FDE-4ABA-A942-7C7C7126E9AC"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPad Retina",
       "udid" : "35FCFEEC-577F-46C1-8389-5195D17D9D76"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPad Air",
       "udid" : "867884CE-1B74-4912-B216-8E750BF15699"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPad Air 2",
       "udid" : "AB7731B7-9BC5-4EA4-B9C1-3DA6E826D7CC"
     }
   ],
   "tvOS 11.0" : [
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "Apple TV",
       "udid" : "7BC43B9B-EF0E-4A0A-A3CD-6040688C1D64"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "Apple TV 4K",
       "udid" : "7C6B05C9-2E4E-4C4A-A1B0-FF842DFD686F"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "Apple TV 4K (at 1080p)",
       "udid" : "DB10825C-04DD-4A50-8C37-E96C7148076A"
     }
   ],
   "iOS 11.0" : [
     {
       "state" : "Booted",
       "availability" : "(available)",
       "name" : "iPhone 5s",
       "udid" : "34FB476C-6FA0-43C8-8945-1BD7A4EBF0DE"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 6",
       "udid" : "0045E516-F2E1-484E-B95D-73E8AA7663A4"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 6 Plus",
       "udid" : "4A518A18-508B-4160-8BF8-EB96F3769834"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 6s",
       "udid" : "9E4697DC-1166-4C49-A4EB-36DEAA14BA55"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 6s Plus",
       "udid" : "BE8E0A96-F456-4504-BCD2-D8AD9D9267BA"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 7",
       "udid" : "B5E3E0D2-FFED-44CD-AF8D-AFCB3EBA59DA"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 7 Plus",
       "udid" : "CD9A6D80-9013-4782-8CC7-F111309DB0E6"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 8",
       "udid" : "17104B4F-E77D-4019-98E6-621FE3CC3653"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone 8 Plus",
       "udid" : "51B74402-D1D9-496E-93F5-161D31C83CCD"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone SE (1st generation)",
       "udid" : "DB46D0DB-510E-4928-BDB4-1A0192ED4A38"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPhone X",
       "udid" : "4E6E7393-C4E3-4323-AA8B-4A42A45AE7B8"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPad Air",
       "udid" : "CC6E7B6D-1A88-4F24-9009-DB8A7B28D234"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPad Air 2",
       "udid" : "116F49B6-4ED4-4F8E-B736-226E6915A580"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPad (9th generation)",
       "udid" : "1805162F-861B-40CA-8468-8B7DC0922D62"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPad Pro (9.7-inch)",
       "udid" : "5B77D232-EF20-48FE-BC73-2D500F3DF162"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPad Pro (12.9-inch)",
       "udid" : "5C46CD8C-07AD-4F41-8314-226CB5D62C30"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPad Pro (12.9-inch) (2nd generation)",
       "udid" : "56DFB13A-D2FC-48CD-8D97-B90441999208"
     },
     {
       "state" : "Shutdown",
       "availability" : "(available)",
       "name" : "iPad Pro (10.5-inch)",
       "udid" : "C92DDBB6-14AE-4B19-B9E5-4365FADE66E0"
     }
   ],
   "iOS 12.0" : [
     {
       "state" : "Shutdown",
       "isAvailable" : "YES",
       "name" : "iPhone Xs",
       "udid" : "450C587D-70B1-54D9-9A56-2BD7B5FC01EF"
     }
   ]
 },
 "pairs" : {
   "6C37B862-BBB1-4A2F-9D64-174CC38C7A9D" : {
     "watch" : {
       "name" : "Apple Watch Series 3 - 38mm",
       "udid" : "8EFAD24B-2EB3-48AF-9484-F97AA418C5D6",
       "state" : "Shutdown"
     },
     "phone" : {
       "name" : "iPhone 8",
       "udid" : "17104B4F-E77D-4019-98E6-621FE3CC3653",
       "state" : "Shutdown"
     },
     "state" : "(active, disconnected)"
   },
   "018AB114-B3E5-45FE-8598-6524870D8D5E" : {
     "watch" : {
       "name" : "Apple Watch Series 2 - 38mm",
       "udid" : "A0A989D0-C5B8-432D-869F-54640FD6739D",
       "state" : "Shutdown"
     },
     "phone" : {
       "name" : "iPhone 7",
       "udid" : "B5E3E0D2-FFED-44CD-AF8D-AFCB3EBA59DA",
       "state" : "Shutdown"
     },
     "state" : "(active, disconnected)"
   },
   "540DD594-89E9-4DFA-87AC-7AD7DDCB9DE8" : {
     "watch" : {
       "name" : "Apple Watch Series 2 - 42mm",
       "udid" : "AB05A1C2-1049-4087-BEDB-326B42D58CDD",
       "state" : "Shutdown"
     },
     "phone" : {
       "name" : "iPhone 7 Plus",
       "udid" : "CD9A6D80-9013-4782-8CC7-F111309DB0E6",
       "state" : "Shutdown"
     },
     "state" : "(active, disconnected)"
   },
   "2DFF3E54-84B0-4D2F-BF4A-40F3273E44F1" : {
     "watch" : {
       "name" : "Apple Watch Series 3 - 42mm",
       "udid" : "3B477D07-65AD-481A-9506-3776817A6293",
       "state" : "Shutdown"
     },
     "phone" : {
       "name" : "iPhone 8 Plus",
       "udid" : "51B74402-D1D9-496E-93F5-161D31C83CCD",
       "state" : "Shutdown"
     },
     "state" : "(active, disconnected)"
   }
 },
 "services" : [
   "This triggers the bail-out logic in SimulatedDevice.is_usable()",
   "com.apple.springboard.services",
   "com.apple.carousel.sessionservice"
 ]
}"""

class SimulatedDeviceTest(unittest.TestCase):

    @staticmethod
    def reset_simulated_device_manager():
        SimulatedDeviceManager.AVAILABLE_RUNTIMES = []
        SimulatedDeviceManager.AVAILABLE_DEVICES = []
        SimulatedDeviceManager.INITIALIZED_DEVICES = None
        SimulatedDeviceManager._device_identifier_to_name = {}
        SimulatedDeviceManager._managing_simulator_app = False

    def tearDown(self):
        SimulatedDeviceTest.reset_simulated_device_manager()

    @staticmethod
    def mock_host_for_simctl():
        simctl_json = json.loads(simctl_json_output)  # Construct enough of a filesystem for all our simctl code to work.
        filesystem_map = {}
        runtime_name_to_id = {}

        # Runtime mapping
        for runtime_group in simctl_json['runtimes']:
            runtime_name_to_id[runtime_group['name']] = runtime_group['identifier']

        # Device type mapping
        device_type_name_to_id = {}
        for device_type in simctl_json['devicetypes']:
            device_type_name_to_id[device_type['name']] = device_type['identifier']

        for runtime, device_groups in simctl_json['devices'].items():
            for device in device_groups:
                file_path = '/Users/mock' + SimulatedDeviceManager.simulator_device_path[1:] + '/' + device['udid'] + '/device.plist'
                # We're taking advantage the fact that the names of the devices match the names of their runtimes in the
                # provided JSON ouput. This is not generally true, which is why we're only using this fact to build up
                # a mock filesystem that is used by the actual simctl parsing code.
                filesystem_map[file_path] = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist SYSTEM "{}">
<plist version="1.0">
<dict>
    <key>UDID</key>
    <string>{}</string>
    <key>deviceType</key>
    <string>{}</string>
    <key>name</key>
    <string>{}</string>
    <key>runtime</key>
    <string>{}</string>
    <key>state</key>
    <integer>{}</integer>
</dict>
</plist>""".format(file_path, device['udid'], device_type_name_to_id[device['name']], device['name'], runtime_name_to_id[runtime], SimulatedDevice.NAME_FOR_STATE.index(device['state'].upper()))

        return MockSystemHost(
            executive=MockExecutive2(output=simctl_json_output),
            filesystem=MockFileSystem(files=filesystem_map),
        )

    def test_available_devices(self):
        SimulatedDeviceTest.reset_simulated_device_manager()
        host = SimulatedDeviceTest.mock_host_for_simctl()
        SimulatedDeviceManager.available_devices(host)

        # There should only be 1 iPhone X, iPhone 8 and iPhone SE
        self.assertEqual(1, len(SimulatedDeviceManager.device_by_filter(lambda device: device.device_type == DeviceType.from_string('iPhone X'), host)))
        self.assertEqual(1, len(SimulatedDeviceManager.device_by_filter(lambda device: device.device_type == DeviceType.from_string('iPhone 8'), host)))

        # There should be 2 5s and 6s
        self.assertEqual(2, len(SimulatedDeviceManager.device_by_filter(lambda device: device.device_type == DeviceType.from_string('iPhone 5s'), host)))
        self.assertEqual(2, len(SimulatedDeviceManager.device_by_filter(lambda device: device.device_type == DeviceType.from_string('iPhone 6s'), host)))

        # 19 iPhones
        self.assertEqual(19, len(SimulatedDeviceManager.device_by_filter(lambda device: device.device_type == DeviceType.from_string('iPhone'), host)))

        # 11 iPads
        self.assertEqual(11, len(SimulatedDeviceManager.device_by_filter(lambda device: device.device_type == DeviceType.from_string('iPad'), host)))

        # 18 Apple watches
        self.assertEqual(6, len(SimulatedDeviceManager.device_by_filter(lambda device: device.device_type == DeviceType.from_string('Apple Watch'), host)))

        # 3 Apple TVs
        self.assertEqual(3, len(SimulatedDeviceManager.device_by_filter(lambda device: device.device_type == DeviceType.from_string('Apple TV'), host)))

        # 18 devices running iOS 11.0
        self.assertEqual(18, len(SimulatedDeviceManager.device_by_filter(lambda device: device.device_type == DeviceType(software_variant='iOS', software_version=Version(11, 0, 1)), host)))

        # 11 iPhones running iOS 11.0
        self.assertEqual(11, len(SimulatedDeviceManager.device_by_filter(lambda device: device.device_type == DeviceType(hardware_family='iPhone', software_version=Version(11, 0, 1)), host)))

        # 1 device running iOS 12
        self.assertEqual(1, len(SimulatedDeviceManager.device_by_filter(lambda device: device.device_type == DeviceType(software_variant='iOS', software_version=Version(12, 0, 0)), host)))

    def test_existing_simulator(self):
        SimulatedDeviceTest.reset_simulated_device_manager()
        host = SimulatedDeviceTest.mock_host_for_simctl()
        SimulatedDeviceManager.available_devices(host)

        SimulatedDeviceManager.initialize_devices(DeviceRequest(DeviceType.from_string('iPhone', Version(11))), host=host)

        self.assertEqual(1, len(SimulatedDeviceManager.INITIALIZED_DEVICES))
        self.assertEqual('34FB476C-6FA0-43C8-8945-1BD7A4EBF0DE', SimulatedDeviceManager.INITIALIZED_DEVICES[0].udid)
        self.assertEqual('15A8401', SimulatedDeviceManager.INITIALIZED_DEVICES[0].build_version)
        self.assertEqual(SimulatedDevice.DeviceState.BOOTED, SimulatedDeviceManager.INITIALIZED_DEVICES[0].platform_device.state())

        SimulatedDeviceManager.tear_down(host)
        self.assertIsNone(SimulatedDeviceManager.INITIALIZED_DEVICES)

    def test_lower_case_device_type(self):
        SimulatedDeviceTest.reset_simulated_device_manager()
        host = SimulatedDeviceTest.mock_host_for_simctl()
        SimulatedDeviceManager.available_devices(host)

        SimulatedDeviceManager.initialize_devices(DeviceRequest(DeviceType.from_string('iphone 5s', Version(11))), host=host)

        self.assertEqual(1, len(SimulatedDeviceManager.INITIALIZED_DEVICES))
        self.assertEqual('34FB476C-6FA0-43C8-8945-1BD7A4EBF0DE', SimulatedDeviceManager.INITIALIZED_DEVICES[0].udid)
        self.assertEqual(SimulatedDevice.DeviceState.BOOTED, SimulatedDeviceManager.INITIALIZED_DEVICES[0].platform_device.state())

        SimulatedDeviceManager.tear_down(host)
        self.assertIsNone(SimulatedDeviceManager.INITIALIZED_DEVICES)

    def test_matching_up_success(self):
        SimulatedDeviceTest.reset_simulated_device_manager()
        host = SimulatedDeviceTest.mock_host_for_simctl()
        SimulatedDeviceManager.available_devices(host)

        runtime = SimulatedDeviceManager.get_runtime_for_device_type(DeviceType.from_string('iphone 5s', Version(9, 2)))
        self.assertEqual(runtime.os_variant, 'iOS')
        self.assertEqual(runtime.version, Version(9, 3))

    def test_matching_up_failure(self):
        SimulatedDeviceTest.reset_simulated_device_manager()
        host = SimulatedDeviceTest.mock_host_for_simctl()
        SimulatedDeviceManager.available_devices(host)

        runtime = SimulatedDeviceManager.get_runtime_for_device_type(DeviceType.from_string('iphone 5s', Version(9, 4)))
        self.assertEqual(runtime, None)

    def test_no_state_files(self):
        SimulatedDeviceTest.reset_simulated_device_manager()
        host = SimulatedDeviceTest.mock_host_for_simctl()
        host.filesystem = MockFileSystem()
        devices = SimulatedDeviceManager.available_devices(host)

        for device in devices:
            self.assertEqual(SimulatedDevice.DeviceState.SHUT_DOWN, device.state(force_update=True))


class LaunchdConfigurationBeforeBootTest(unittest.TestCase):
    """Configuration a device needs launchd to have is written before it boots, so it applies to the boot itself.
    webkitpy defines the configuration; ports add to it through apple_additions."""

    class FakeDevice(object):
        udid = 'UDID-1'

        def __init__(self, configuration):
            self.platform_device = self
            self.launchd_configuration = configuration

    def _write(self, configuration, files=None):
        host = MockSystemHost(filesystem=MockFileSystem(files=files or {}))
        SimulatedDeviceManager._configure_launchd_before_booting(self.FakeDevice(configuration), host)
        return host

    def _path(self, name='example.plist'):
        return '/private/var/tmp/com.apple.CoreSimulator.SimDevice.UDID-1/' + name

    def test_configuration_is_written_before_boot(self):
        host = self._write({'example.plist': {'first': True, 'second': False}})
        self.assertEqual(
            plistlib.loads(host.filesystem.read_binary_file(self._path())),
            {'first': True, 'second': False})

    def test_every_named_file_is_written(self):
        host = self._write({'one.plist': {'a': True}, 'two.plist': {'b': True}})
        self.assertEqual(plistlib.loads(host.filesystem.read_binary_file(self._path('one.plist'))), {'a': True})
        self.assertEqual(plistlib.loads(host.filesystem.read_binary_file(self._path('two.plist'))), {'b': True})

    def test_nothing_written_without_configuration(self):
        host = self._write({})
        self.assertFalse(host.filesystem.exists(self._path()))

    def test_existing_keys_are_kept(self):
        existing = plistlib.dumps({'keep': False})
        host = self._write({'example.plist': {'first': True}}, files={self._path(): existing})
        self.assertEqual(
            plistlib.loads(host.filesystem.read_binary_file(self._path())),
            {'keep': False, 'first': True})

    def test_unreadable_file_is_replaced_rather_than_raising(self):
        host = self._write({'example.plist': {'first': True}}, files={self._path(): b'not a plist'})
        self.assertEqual(plistlib.loads(host.filesystem.read_binary_file(self._path())), {'first': True})

    def test_failure_to_write_is_not_fatal(self):
        host = MockSystemHost(filesystem=MockFileSystem())

        def refuse(path, contents):
            raise IOError('read-only')

        host.filesystem.write_binary_file = refuse
        # A device that boots unconfigured is better than a run that cannot start.
        SimulatedDeviceManager._configure_launchd_before_booting(
            self.FakeDevice({'example.plist': {'first': True}}), host)


class LaunchdStateCleanupTest(unittest.TestCase):
    """The configuration we write lives in the device's temp directory, so it goes away when the device does."""

    def test_state_directory_is_removed_on_tear_down(self):
        SimulatedDeviceTest.reset_simulated_device_manager()
        host = SimulatedDeviceTest.mock_host_for_simctl()
        devices = SimulatedDeviceManager.available_devices(host)
        self.assertTrue(devices)

        device = devices[0]
        SimulatedDeviceManager.INITIALIZED_DEVICES = [device]
        path = host.filesystem.join(
            SimulatedDeviceManager.launchd_state_path.format(device.udid), 'disabled.plist')
        host.filesystem.maybe_make_directory(host.filesystem.dirname(path))
        host.filesystem.write_binary_file(path, b'contents')
        self.assertTrue(host.filesystem.exists(path))

        SimulatedDeviceManager.tear_down(host)
        self.assertFalse(host.filesystem.exists(path), 'the plist we wrote should not outlive the device')


class LaunchdConfigurationDefaultTest(unittest.TestCase):
    """webkitpy defines the configuration a simulator boots with; apple_additions only adds to it."""

    def test_unneeded_daemons_are_disabled_by_default(self):
        host = SimulatedDeviceTest.mock_host_for_simctl()
        SimulatedDeviceTest.reset_simulated_device_manager()
        devices = SimulatedDeviceManager.available_devices(host)
        self.assertTrue(devices)
        self.assertEqual(
            devices[0].platform_device.launchd_configuration.get('disabled.plist'),
            disabled_launchd_jobs())

    def test_daemons_testing_needs_are_not_disabled(self):
        for label in ('com.apple.sharingd', 'com.apple.eligibilityd', 'com.apple.sleepd'):
            self.assertNotIn(label, disabled_launchd_jobs())


class FakeBootWait(object):
    """Stands in for the process waiting on a device to finish booting."""

    def __init__(self, polls_until_done=0, returncode=0):
        self._remaining = polls_until_done
        self._final = returncode
        self.returncode = None

    def poll(self):
        if self._remaining > 0:
            self._remaining -= 1
            return None
        self.returncode = self._final
        return self._final


class FakeSimulatedDevice(object):
    def __init__(self, name, booted=True):
        self.name = name
        self.udid = 'udid-' + name
        self.platform_device = self
        self.booted = booted

    def is_booted_or_booting(self, force_update=False):
        return self.booted

    def __repr__(self):
        return self.name


class WaitForFirstReadyDeviceTest(unittest.TestCase):
    """Waiting for the first device belongs to the manager, so the port does not run a polling loop of its own."""

    def setUp(self):
        self._saved = (SimulatedDeviceManager.INITIALIZED_DEVICES, SimulatedDeviceManager.READY_DEVICES,
                       SimulatedDeviceManager.PENDING_DEVICES, SimulatedDeviceManager.DEVICE_QUEUE,
                       SimulatedDeviceManager.PROVISIONING_DONE, SimulatedDeviceManager._boot_waits_by_udid)

    def tearDown(self):
        SimulatedDeviceManager.end_provisioning()
        (SimulatedDeviceManager.INITIALIZED_DEVICES, SimulatedDeviceManager.READY_DEVICES,
         SimulatedDeviceManager.PENDING_DEVICES, SimulatedDeviceManager.DEVICE_QUEUE,
         SimulatedDeviceManager.PROVISIONING_DONE, SimulatedDeviceManager._boot_waits_by_udid) = self._saved

    def test_returns_once_the_first_device_is_ready(self):
        slow, quick = FakeSimulatedDevice('slow'), FakeSimulatedDevice('quick')
        SimulatedDeviceManager.INITIALIZED_DEVICES = [slow, quick]
        SimulatedDeviceManager.start_waiting_for_boot = staticmethod(
            lambda device: FakeBootWait(0 if device.name == 'quick' else 500))
        SimulatedDeviceManager.begin_provisioning(timeout=30)

        self.assertTrue(SimulatedDeviceManager.wait_for_first_ready_device(timeout=30))
        self.assertEqual([device.name for device in SimulatedDeviceManager.READY_DEVICES], ['quick'])
        self.assertEqual([device.name for device in SimulatedDeviceManager.PENDING_DEVICES], ['slow'])

    def test_reports_no_device_when_none_becomes_ready(self):
        SimulatedDeviceManager.INITIALIZED_DEVICES = [FakeSimulatedDevice('broken')]
        SimulatedDeviceManager.start_waiting_for_boot = staticmethod(lambda device: FakeBootWait(0, returncode=1))
        SimulatedDeviceManager.begin_provisioning(timeout=1)

        self.assertFalse(SimulatedDeviceManager.wait_for_first_ready_device(timeout=1))


class DeviceSetupOnceTest(unittest.TestCase):
    """Installing the driver and proving a device usable belong to the device, so only the first worker sharing it is
    told to do that work."""

    def setUp(self):
        self._saved = (SimulatedDeviceManager.INITIALIZED_DEVICES, SimulatedDeviceManager.READY_DEVICES,
                       SimulatedDeviceManager.PENDING_DEVICES, SimulatedDeviceManager.DEVICE_QUEUE,
                       SimulatedDeviceManager.PROVISIONING_DONE, SimulatedDeviceManager._slots_per_device)

    def tearDown(self):
        SimulatedDeviceManager.end_provisioning()
        (SimulatedDeviceManager.INITIALIZED_DEVICES, SimulatedDeviceManager.READY_DEVICES,
         SimulatedDeviceManager.PENDING_DEVICES, SimulatedDeviceManager.DEVICE_QUEUE,
         SimulatedDeviceManager.PROVISIONING_DONE, SimulatedDeviceManager._slots_per_device) = self._saved

    def _claim_all(self, devices, slots):
        SimulatedDeviceManager.INITIALIZED_DEVICES = list(devices)
        SimulatedDeviceManager.READY_DEVICES = list(devices)
        SimulatedDeviceManager.PENDING_DEVICES = []
        SimulatedDeviceManager.DEVICE_QUEUE = multiprocessing.Queue()
        SimulatedDeviceManager.offer_ready_devices(slots_per_device=slots)
        claimed = []
        for _ in range(len(devices) * slots):
            claimed.append(SimulatedDeviceManager.claim_device(1))
        return claimed

    def test_one_worker_a_device_always_sets_up(self):
        claimed = self._claim_all([FakeSimulatedDevice('a'), FakeSimulatedDevice('b')], 1)
        self.assertEqual([sets_up for _, sets_up in claimed], [True, True])

    def test_only_the_first_worker_sharing_a_device_sets_it_up(self):
        devices = [FakeSimulatedDevice('a'), FakeSimulatedDevice('b')]
        claimed = self._claim_all(devices, 3)
        by_device = {}
        for device, sets_up in claimed:
            by_device.setdefault(device.udid, []).append(sets_up)
        self.assertEqual(len(by_device), 2)
        for udid, flags in by_device.items():
            self.assertEqual(sum(1 for f in flags if f), 1, 'exactly one worker sets up {}'.format(udid))
            self.assertEqual(len(flags), 3)


class SlotStarvationTest(unittest.TestCase):
    """Every slot offered belongs to a worker. Anything the process handing out shards takes for itself leaves a
    worker with no device, and that worker hands its shards back for the whole run."""

    def setUp(self):
        self._saved = (SimulatedDeviceManager.INITIALIZED_DEVICES, SimulatedDeviceManager.READY_DEVICES,
                       SimulatedDeviceManager.PENDING_DEVICES, SimulatedDeviceManager.DEVICE_QUEUE,
                       SimulatedDeviceManager.PROVISIONING_DONE, SimulatedDeviceManager._slots_per_device)

    def tearDown(self):
        SimulatedDeviceManager.end_provisioning()
        (SimulatedDeviceManager.INITIALIZED_DEVICES, SimulatedDeviceManager.READY_DEVICES,
         SimulatedDeviceManager.PENDING_DEVICES, SimulatedDeviceManager.DEVICE_QUEUE,
         SimulatedDeviceManager.PROVISIONING_DONE, SimulatedDeviceManager._slots_per_device) = self._saved

    def test_one_worker_a_device_offers_exactly_one_slot_each(self):
        devices = [FakeSimulatedDevice(name) for name in ('a', 'b', 'c')]
        SimulatedDeviceManager.INITIALIZED_DEVICES = list(devices)
        SimulatedDeviceManager.READY_DEVICES = list(devices)
        SimulatedDeviceManager.PENDING_DEVICES = []
        SimulatedDeviceManager.DEVICE_QUEUE = multiprocessing.Queue()
        SimulatedDeviceManager.offer_ready_devices(slots_per_device=1)

        claimed = []
        for _ in devices:
            device, _sets_up = SimulatedDeviceManager.claim_device(1)
            claimed.append(device)
        self.assertEqual(len([c for c in claimed if c]), len(devices))
        self.assertEqual(len({c.udid for c in claimed if c}), len(devices))


class SlotsPerDeviceTest(unittest.TestCase):
    """A device is offered once for every worker meant to share it, so stacking does not wait on provisioning."""

    def setUp(self):
        self._saved = (SimulatedDeviceManager.INITIALIZED_DEVICES, SimulatedDeviceManager.READY_DEVICES,
                       SimulatedDeviceManager.PENDING_DEVICES, SimulatedDeviceManager.DEVICE_QUEUE,
                       SimulatedDeviceManager.PROVISIONING_DONE, SimulatedDeviceManager._slots_per_device)

    def tearDown(self):
        SimulatedDeviceManager.end_provisioning()
        (SimulatedDeviceManager.INITIALIZED_DEVICES, SimulatedDeviceManager.READY_DEVICES,
         SimulatedDeviceManager.PENDING_DEVICES, SimulatedDeviceManager.DEVICE_QUEUE,
         SimulatedDeviceManager.PROVISIONING_DONE, SimulatedDeviceManager._slots_per_device) = self._saved

    def _drain(self):
        claimed = []
        while True:
            try:
                device, _sets_up = SimulatedDeviceManager.DEVICE_QUEUE.get(timeout=0.3)
                claimed.append(device)
            except Exception:
                break
        return claimed

    def _offer(self, devices, slots):
        SimulatedDeviceManager.INITIALIZED_DEVICES = list(devices)
        SimulatedDeviceManager.READY_DEVICES = list(devices)
        SimulatedDeviceManager.PENDING_DEVICES = []
        SimulatedDeviceManager.DEVICE_QUEUE = multiprocessing.Queue()
        SimulatedDeviceManager.offer_ready_devices(slots_per_device=slots)
        return self._drain()

    def test_one_slot_a_device_gives_each_worker_its_own(self):
        devices = [FakeSimulatedDevice('a'), FakeSimulatedDevice('b')]
        claimed = self._offer(devices, 1)
        self.assertEqual(len(claimed), 2)
        self.assertEqual(len({device.udid for device in claimed}), 2)

    def test_three_slots_a_device_lets_three_workers_share_it(self):
        devices = [FakeSimulatedDevice('a'), FakeSimulatedDevice('b')]
        claimed = self._offer(devices, 3)
        self.assertEqual(len(claimed), 6)
        for device in devices:
            self.assertEqual(len([c for c in claimed if c.udid == device.udid]), 3)

    def test_a_single_device_still_offers_every_slot(self):
        claimed = self._offer([FakeSimulatedDevice('only')], 4)
        self.assertEqual(len(claimed), 4)

    def test_slots_below_one_are_treated_as_one(self):
        claimed = self._offer([FakeSimulatedDevice('a')], 0)
        self.assertEqual(len(claimed), 1)


class BlockOnReadyTest(unittest.TestCase):
    """Booting a device and waiting for it to be ready are separate steps."""

    def setUp(self):
        self._initialized = SimulatedDeviceManager.INITIALIZED_DEVICES
        self._usable = SimulatedDeviceManager._wait_until_devices_are_usable
        self._extras = SimulatedDeviceManager._set_up_environment_extras
        self.waited_on = []
        self.extras_for = []
        SimulatedDeviceManager._wait_until_devices_are_usable = lambda devices, deadline, on_usable=None: (
            self.waited_on.append(list(devices)), on_usable(list(devices)) if on_usable else None)
        SimulatedDeviceManager._set_up_environment_extras = lambda devices: self.extras_for.append(list(devices))

    def tearDown(self):
        SimulatedDeviceManager.INITIALIZED_DEVICES = self._initialized
        SimulatedDeviceManager._wait_until_devices_are_usable = self._usable
        SimulatedDeviceManager._set_up_environment_extras = self._extras

    def test_accepts_one_device(self):
        device = FakeSimulatedDevice('one')
        SimulatedDeviceManager.block_on_ready([device])
        self.assertEqual(self.waited_on, [[device]])
        self.assertEqual(self.extras_for, [[device]])

    def test_accepts_multiple_devices(self):
        devices = [FakeSimulatedDevice('one'), FakeSimulatedDevice('two')]
        SimulatedDeviceManager.block_on_ready(devices)
        self.assertEqual(self.waited_on, [devices])

    def test_defaults_to_every_initialized_device(self):
        devices = [FakeSimulatedDevice('one'), FakeSimulatedDevice('two')]
        SimulatedDeviceManager.INITIALIZED_DEVICES = devices
        SimulatedDeviceManager.block_on_ready()
        self.assertEqual(self.waited_on, [devices])

    def test_nothing_to_wait_on(self):
        SimulatedDeviceManager.INITIALIZED_DEVICES = []
        SimulatedDeviceManager.block_on_ready()
        SimulatedDeviceManager.block_on_ready([])
        self.assertEqual(self.waited_on, [])
        self.assertEqual(self.extras_for, [])

    def test_devices_are_waited_on_as_a_group(self):
        devices = [FakeSimulatedDevice('one'), FakeSimulatedDevice('two'), FakeSimulatedDevice('three')]
        SimulatedDeviceManager.block_on_ready(devices)
        self.assertEqual(len(self.waited_on), 1)


class UsabilityCheckBudgetTest(unittest.TestCase):
    """A round of usability checks never runs past the deadline the caller was promised."""

    class FakeExecutive(object):
        def __init__(self, recorder):
            self._recorder = recorder
            self.PIPE = -1

        def popen(self, command, **kwargs):
            return None

    def _budgets_for(self, device_count, deadline_in):
        budgets = []
        devices = []
        for index in range(device_count):
            device = FakeSimulatedDevice('device-{}'.format(index))
            device.device_type = DeviceType.from_string('iPhone 11')
            device.UI_MANAGER_SERVICE = {'iOS': 'com.apple.Preferences'}
            device.executive = self.FakeExecutive(budgets)
            device.state = lambda force_update=False: SimulatedDevice.DeviceState.BOOTED
            devices.append(device)

        saved = simulated_device.run_all
        simulated_device.run_all = lambda commands, timeout=None, popen=None: (
            budgets.append(timeout) or [(0, b'', b'')] * len(commands))
        try:
            SimulatedDeviceManager._devices_not_yet_usable(devices, time.monotonic() + deadline_in)
        finally:
            simulated_device.run_all = saved
        return budgets

    def test_budget_scales_with_device_count(self):
        budget = self._budgets_for(3, deadline_in=10000)[0]
        self.assertEqual(budget, SimulatedDeviceManager.USABILITY_CHECK_TIMEOUT * 3)

    def test_budget_is_capped_by_the_deadline(self):
        budget = self._budgets_for(12, deadline_in=60)[0]
        self.assertLessEqual(budget, 60)

    def test_budget_stays_positive_past_the_deadline(self):
        for budget in self._budgets_for(2, deadline_in=-30):
            self.assertGreater(budget, 0)


class ProvisioningTest(unittest.TestCase):
    """Devices are offered to the workers as each one finishes booting, and asking never blocks."""

    def setUp(self):
        self._saved = (SimulatedDeviceManager.INITIALIZED_DEVICES, SimulatedDeviceManager.READY_DEVICES,
                       SimulatedDeviceManager.PENDING_DEVICES, SimulatedDeviceManager.DEVICE_QUEUE,
                       SimulatedDeviceManager.PROVISIONING_DONE)

    def tearDown(self):
        SimulatedDeviceManager.end_provisioning()
        (SimulatedDeviceManager.INITIALIZED_DEVICES, SimulatedDeviceManager.READY_DEVICES,
         SimulatedDeviceManager.PENDING_DEVICES, SimulatedDeviceManager.DEVICE_QUEUE,
         SimulatedDeviceManager.PROVISIONING_DONE) = self._saved

    def _begin(self, waits):
        devices = [FakeSimulatedDevice(name) for name in waits]
        by_udid = {device.udid: waits[device.name] for device in devices}
        SimulatedDeviceManager.INITIALIZED_DEVICES = devices
        SimulatedDeviceManager.start_waiting_for_boot = staticmethod(lambda device: by_udid[device.udid])
        SimulatedDeviceManager.begin_provisioning(timeout=60)
        return devices

    def test_a_device_is_offered_as_soon_as_its_own_boot_finishes(self):
        self._begin({'quick': FakeBootWait(0), 'slow': FakeBootWait(5)})
        SimulatedDeviceManager.advance_provisioning()

        self.assertEqual([str(d) for d in SimulatedDeviceManager.READY_DEVICES], ['quick'])
        self.assertEqual([str(d) for d in SimulatedDeviceManager.PENDING_DEVICES], ['slow'])
        self.assertTrue(SimulatedDeviceManager.expects_more_devices())

    def test_the_slow_device_joins_once_its_boot_finishes(self):
        self._begin({'quick': FakeBootWait(0), 'slow': FakeBootWait(2)})
        for _ in range(5):
            SimulatedDeviceManager.advance_provisioning()

        self.assertEqual([str(d) for d in SimulatedDeviceManager.READY_DEVICES], ['quick', 'slow'])
        self.assertFalse(SimulatedDeviceManager.expects_more_devices())

    def test_asking_never_blocks(self):
        self._begin({'never': FakeBootWait(10 ** 6)})

        started = time.monotonic()
        for _ in range(5):
            SimulatedDeviceManager.advance_provisioning()
        self.assertLess(time.monotonic() - started, 1,
                        'the process handing out shards must not block on a device that is still booting')

    def test_a_device_that_fails_to_boot_is_left_behind(self):
        self._begin({'fine': FakeBootWait(0), 'broken': FakeBootWait(0, returncode=1)})
        SimulatedDeviceManager.advance_provisioning()

        self.assertEqual([str(d) for d in SimulatedDeviceManager.READY_DEVICES], ['fine'])
        self.assertEqual(SimulatedDeviceManager.PENDING_DEVICES, [])
        self.assertFalse(SimulatedDeviceManager.expects_more_devices(),
                         'workers must not keep waiting for a device that is never arriving')

    def test_devices_still_booting_are_given_up_on_at_the_deadline(self):
        self._begin({'never': FakeBootWait(10 ** 6)})
        SimulatedDeviceManager._provisioning_deadline = time.monotonic() - 1

        SimulatedDeviceManager.advance_provisioning()

        self.assertEqual(SimulatedDeviceManager.PENDING_DEVICES, [])
        self.assertFalse(SimulatedDeviceManager.expects_more_devices())

    def test_a_claimed_device_is_the_copy_this_process_holds(self):
        # A device handed over through a queue is a copy carrying the state it had when it was sent.
        devices = self._begin({'only': FakeBootWait(0)})
        SimulatedDeviceManager.advance_provisioning()
        sent = FakeSimulatedDevice('only')
        SimulatedDeviceManager.DEVICE_QUEUE.put((sent, True))
        time.sleep(0.2)

        SimulatedDeviceManager.DEVICE_QUEUE.get()  # the real one
        claimed, _sets_up = SimulatedDeviceManager.claim_device(1)

        self.assertIs(claimed, devices[0], 'the worker should get the copy whose state it can refresh')

    def test_a_worker_gives_up_once_nothing_is_coming(self):
        self._begin({'broken': FakeBootWait(0, returncode=1)})
        SimulatedDeviceManager.advance_provisioning()

        self.assertIsNone(SimulatedDeviceManager.claim_device(0.1)[0])

    def test_teardown_forgets_a_runs_devices(self):
        self._begin({'only': FakeBootWait(0)})
        SimulatedDeviceManager.advance_provisioning()

        SimulatedDeviceManager.end_provisioning()

        self.assertEqual(SimulatedDeviceManager.READY_DEVICES, [],
                         'a device from the last run must not be offered to the next')
        self.assertIsNone(SimulatedDeviceManager.DEVICE_QUEUE)
        self.assertFalse(SimulatedDeviceManager.expects_more_devices())
