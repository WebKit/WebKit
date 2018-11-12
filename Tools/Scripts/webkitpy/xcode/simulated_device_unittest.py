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
     "name" : "iPhone SE",
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
     "name" : "iPad (5th generation)",
     "identifier" : "com.apple.CoreSimulator.SimDeviceType.iPad--5th-generation-"
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
     "buildversion" : "13E233",
     "availability" : "(available)",
     "name" : "iOS 9.3",
     "identifier" : "com.apple.CoreSimulator.SimRuntime.iOS-9-3",
     "version" : "9.3"
   },
   {
     "buildversion" : "15A8401",
     "availability" : "(available)",
     "name" : "iOS 11.0",
     "identifier" : "com.apple.CoreSimulator.SimRuntime.iOS-11-0",
     "version" : "11.0.1"
   },
   {
     "buildversion" : "15J380",
     "availability" : "(available)",
     "name" : "tvOS 11.0",
     "identifier" : "com.apple.CoreSimulator.SimRuntime.tvOS-11-0",
     "version" : "11.0"
   },
   {
     "buildversion" : "15R372",
     "availability" : "(available)",
     "name" : "watchOS 4.0",
     "identifier" : "com.apple.CoreSimulator.SimRuntime.watchOS-4-0",
     "version" : "4.0"
   },
   {
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
       "name" : "iPhone SE",
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
       "name" : "iPad (5th generation)",
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

import json
import unittest

from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.common.version import Version
from webkitpy.xcode.device_type import DeviceType
from webkitpy.xcode.simulated_device import DeviceRequest, SimulatedDeviceManager, SimulatedDevice


class SimulatedDeviceTest(unittest.TestCase):

    @staticmethod
    def reset_simulated_device_manager():
        SimulatedDeviceManager.AVAILABLE_RUNTIMES = []
        SimulatedDeviceManager.AVAILABLE_DEVICES = []
        SimulatedDeviceManager.INITIALIZED_DEVICES = None
        SimulatedDeviceManager._device_identifier_to_name = False
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

        for runtime, device_groups in simctl_json['devices'].iteritems():
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
        self.assertEquals(1, len(SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.device_type == DeviceType.from_string('iPhone X'), host)))
        self.assertEquals(1, len(SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.device_type == DeviceType.from_string('iPhone 8'), host)))

        # There should be 2 5s and 6s
        self.assertEquals(2, len(SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.device_type == DeviceType.from_string('iPhone 5s'), host)))
        self.assertEquals(2, len(SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.device_type == DeviceType.from_string('iPhone 6s'), host)))

        # 19 iPhones
        self.assertEquals(19, len(SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.device_type == DeviceType.from_string('iPhone'), host)))

        # 11 iPads
        self.assertEquals(11, len(SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.device_type == DeviceType.from_string('iPad'), host)))

        # 18 Apple watches
        self.assertEquals(6, len(SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.device_type == DeviceType.from_string('Apple Watch'), host)))

        # 3 Apple TVs
        self.assertEquals(3, len(SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.device_type == DeviceType.from_string('Apple TV'), host)))

        # 18 devices running iOS 11.0
        self.assertEquals(18, len(SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.device_type == DeviceType(software_variant='iOS', software_version=Version(11, 0, 1)), host)))

        # 11 iPhones running iOS 11.0
        self.assertEquals(11, len(SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.device_type == DeviceType(hardware_family='iPhone', software_version=Version(11, 0, 1)), host)))

        # 1 device running iOS 12
        self.assertEquals(1, len(SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.device_type == DeviceType(software_variant='iOS', software_version=Version(12, 0, 0)), host)))

    def test_existing_simulator(self):
        SimulatedDeviceTest.reset_simulated_device_manager()
        host = SimulatedDeviceTest.mock_host_for_simctl()
        SimulatedDeviceManager.available_devices(host)

        SimulatedDeviceManager.initialize_devices(DeviceRequest(DeviceType.from_string('iPhone', Version(11))), host=host)

        self.assertEquals(1, len(SimulatedDeviceManager.INITIALIZED_DEVICES))
        self.assertEquals('34FB476C-6FA0-43C8-8945-1BD7A4EBF0DE', SimulatedDeviceManager.INITIALIZED_DEVICES[0].udid)
        self.assertEquals(SimulatedDevice.DeviceState.BOOTED, SimulatedDeviceManager.INITIALIZED_DEVICES[0].platform_device.state())

        SimulatedDeviceManager.tear_down(host)
        self.assertIsNone(SimulatedDeviceManager.INITIALIZED_DEVICES)

    @staticmethod
    def change_state_to(device, state):
        assert isinstance(state, int)

        # Reaching into device.plist to change device state. Note that this will not change the initial state of the device
        # as determined from the .json output.
        device_plist = device.filesystem.expanduser(device.filesystem.join(SimulatedDeviceManager.simulator_device_path, device.udid, 'device.plist'))
        index_position = device.filesystem.files[device_plist].index('</integer>') - 1
        device.filesystem.files[device_plist] = device.filesystem.files[device_plist][:index_position] + str(state) + device.filesystem.files[device_plist][index_position + 1:]

    def test_swapping_devices(self):
        SimulatedDeviceTest.reset_simulated_device_manager()
        host = SimulatedDeviceTest.mock_host_for_simctl()
        SimulatedDeviceManager.available_devices(host)

        # We won't test the creation and deletion of simulators, only managing existing sims
        SimulatedDeviceTest.change_state_to(SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.device_type == DeviceType.from_string('iPhone 8'), host)[0], SimulatedDevice.DeviceState.BOOTED)
        SimulatedDeviceTest.change_state_to(SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.device_type == DeviceType.from_string('iPhone X'), host)[0], SimulatedDevice.DeviceState.BOOTED)

        SimulatedDeviceManager.initialize_devices(DeviceRequest(DeviceType.from_string('iPhone 8')), host=host)

        self.assertEquals(1, len(SimulatedDeviceManager.INITIALIZED_DEVICES))
        self.assertEquals('17104B4F-E77D-4019-98E6-621FE3CC3653', SimulatedDeviceManager.INITIALIZED_DEVICES[0].udid)
        self.assertEquals(SimulatedDevice.DeviceState.BOOTED, SimulatedDeviceManager.INITIALIZED_DEVICES[0].platform_device.state())

        # Now swap for the X
        SimulatedDeviceTest.change_state_to(SimulatedDeviceManager.INITIALIZED_DEVICES[0], SimulatedDevice.DeviceState.SHUT_DOWN)
        SimulatedDeviceManager.swap(SimulatedDeviceManager.INITIALIZED_DEVICES[0], DeviceRequest(DeviceType.from_string('iPhone X')), host)

        self.assertEquals(1, len(SimulatedDeviceManager.INITIALIZED_DEVICES))
        self.assertEquals('4E6E7393-C4E3-4323-AA8B-4A42A45AE7B8', SimulatedDeviceManager.INITIALIZED_DEVICES[0].udid)
        self.assertEquals(SimulatedDevice.DeviceState.BOOTED,  SimulatedDeviceManager.INITIALIZED_DEVICES[0].platform_device.state())

        SimulatedDeviceTest.change_state_to(SimulatedDeviceManager.INITIALIZED_DEVICES[0], SimulatedDevice.DeviceState.SHUT_DOWN)
        SimulatedDeviceManager.tear_down(host)
        self.assertIsNone(SimulatedDeviceManager.INITIALIZED_DEVICES)
