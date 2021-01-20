# Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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

from webkitcorepy import Version

from webkitpy.common.memoized import memoized
from webkitpy.port.config import apple_additions, Config
from webkitpy.port.ios import IOSPort
from webkitpy.xcode.device_type import DeviceType
from webkitpy.xcode.simulated_device import SimulatedDeviceManager


_log = logging.getLogger(__name__)


class IOSSimulatorPort(IOSPort):
    port_name = "ios-simulator"

    FUTURE_VERSION = 'future'
    ARCHITECTURES = ['x86_64', 'i386']
    DEFAULT_ARCHITECTURE = 'x86_64'

    DEVICE_MANAGER = SimulatedDeviceManager

    DEFAULT_DEVICE_TYPES = [
        DeviceType(hardware_family='iPhone', hardware_type='SE'),
        DeviceType(hardware_family='iPad', hardware_type='(5th generation)'),
        DeviceType(hardware_family='iPhone', hardware_type='7'),
    ]
    SDK = apple_additions().get_sdk('iphonesimulator') if apple_additions() else 'iphonesimulator'

    @staticmethod
    def _version_from_name(name):
        if len(name.split('-')) > 2 and name.split('-')[2].isdigit():
            return Version.from_string(name.split('-')[2])
        return None

    @memoized
    def device_version(self):
        if self.get_option('version'):
            return Version.from_string(self.get_option('version'))
        return IOSSimulatorPort._version_from_name(self._name) if IOSSimulatorPort._version_from_name(self._name) else self.host.platform.xcode_sdk_version('iphonesimulator')

    def clean_up_test_run(self):
        super(IOSSimulatorPort, self).clean_up_test_run()
        _log.debug("clean_up_test_run")

        SimulatedDeviceManager.tear_down(self.host)

    def environment_for_api_tests(self):
        no_prefix = super(IOSSimulatorPort, self).environment_for_api_tests()
        result = {}
        SIMCTL_ENV_PREFIX = 'SIMCTL_CHILD_'
        for value in no_prefix:
            if not value.startswith(SIMCTL_ENV_PREFIX):
                result[SIMCTL_ENV_PREFIX + value] = no_prefix[value]
            else:
                result[value] = no_prefix[value]
        return result

    def setup_environ_for_server(self, server_name=None):
        env = super(IOSSimulatorPort, self).setup_environ_for_server(server_name)
        if server_name == self.driver_name():
            if self.get_option('leaks'):
                env['MallocStackLogging'] = '1'
                env['__XPC_MallocStackLogging'] = '1'
                env['MallocScribble'] = '1'
                env['__XPC_MallocScribble'] = '1'
        return env

    def operating_system(self):
        return 'ios-simulator'

    def reset_preferences(self):
        _log.debug("reset_preferences")
        SimulatedDeviceManager.tear_down(self.host)

    @property
    @memoized
    def developer_dir(self):
        return self._executive.run_command(['xcode-select', '--print-path']).rstrip()

    def logging_patterns_to_strip(self):
        return []

    def stderr_patterns_to_strip(self):
        return []


class IPhoneSimulatorPort(IOSSimulatorPort):
    port_name = 'iphone-simulator'

    DEVICE_TYPE = DeviceType(hardware_family='iPhone')
    DEFAULT_DEVICE_TYPES = [
        DeviceType(hardware_family='iPhone', hardware_type='SE'),
        DeviceType(hardware_family='iPhone', hardware_type='7'),
    ]

    def __init__(self, *args, **kwargs):
        super(IPhoneSimulatorPort, self).__init__(*args, **kwargs)
        self._config = Config(self._executive, self._filesystem, IOSSimulatorPort.port_name)


class IPadSimulatorPort(IOSSimulatorPort):
    port_name = 'ipad-simulator'

    DEVICE_TYPE = DeviceType(hardware_family='iPad')
    DEFAULT_DEVICE_TYPES = [DeviceType(hardware_family='iPad', hardware_type='(5th generation)')]

    def __init__(self, *args, **kwargs):
        super(IPadSimulatorPort, self).__init__(*args, **kwargs)
        self._config = Config(self._executive, self._filesystem, IOSSimulatorPort.port_name)
