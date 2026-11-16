# Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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

import logging

from webkitpy.port.config import apple_additions, Config
from webkitpy.port.embedded_simulator import EmbeddedSimulatorPort
from webkitpy.port.ios import IOSPort
from webkitpy.xcode.device_type import DeviceType
from webkitpy.xcode.simulated_device import SimulatedDeviceManager

_log = logging.getLogger(__name__)


class IOSSimulatorPort(EmbeddedSimulatorPort, IOSPort):
    port_name = "ios-simulator"

    FUTURE_VERSION = 'future'
    ARCHITECTURES = ['x86_64', 'i386', 'arm64']
    DEFAULT_ARCHITECTURE = 'x86_64'

    DEFAULT_DEVICE_TYPES = [
        DeviceType(hardware_family='iPhone', hardware_type='12'),
        DeviceType(hardware_family='iPad', hardware_type='(9th generation)'),
    ]
    SDK = apple_additions().get_sdk('iphonesimulator') if apple_additions() else 'iphonesimulator'

    def architecture(self):
        result = self.get_option('architecture') or self.host.platform.architecture()
        if result == 'arm64e':
            return 'arm64'
        return result

    def clean_up_test_run(self):
        super(IOSSimulatorPort, self).clean_up_test_run()
        _log.debug("clean_up_test_run")

        SimulatedDeviceManager.tear_down(self.host)

    def operating_system(self):
        return 'ios-simulator'


class IPhoneSimulatorPort(IOSSimulatorPort):
    port_name = 'iphone-simulator'

    # DEFAULT_DEVICE_TYPES should match webkitdirs.pm.
    DEVICE_TYPE = DeviceType(hardware_family='iPhone')
    DEFAULT_DEVICE_TYPES = [
        DeviceType(hardware_family='iPhone', hardware_type='12'),
    ]

    def __init__(self, *args, **kwargs):
        super(IPhoneSimulatorPort, self).__init__(*args, **kwargs)
        self._config = Config(self._executive, self._filesystem, IOSSimulatorPort.port_name)


class IPadSimulatorPort(IOSSimulatorPort):
    port_name = 'ipad-simulator'

    # DEFAULT_DEVICE_TYPES should match webkitdirs.pm.
    DEVICE_TYPE = DeviceType(hardware_family='iPad')
    DEFAULT_DEVICE_TYPES = [DeviceType(hardware_family='iPad', hardware_type='(9th generation)')]

    def __init__(self, *args, **kwargs):
        super(IPadSimulatorPort, self).__init__(*args, **kwargs)
        self._config = Config(self._executive, self._filesystem, IOSSimulatorPort.port_name)
