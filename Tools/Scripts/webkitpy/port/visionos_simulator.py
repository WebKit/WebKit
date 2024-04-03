# Copyright (C) 2024 Apple Inc. All rights reserved.
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
from webkitpy.port.config import apple_additions
from webkitpy.port.visionos import VisionOSPort
from webkitpy.xcode.device_type import DeviceType
from webkitpy.xcode.simulated_device import SimulatedDeviceManager

_log = logging.getLogger(__name__)


class VisionOSSimulatorPort(VisionOSPort):
    port_name = 'visionos-simulator'

    ARCHITECTURES = ['arm64']
    DEFAULT_ARCHITECTURE = 'arm64'

    DEVICE_MANAGER = SimulatedDeviceManager

    DEFAULT_DEVICE_TYPES = [DeviceType(software_variant='visionOS', hardware_family='Vision', hardware_type='Pro')]
    SDK = apple_additions().get_sdk('xrsimulator') if apple_additions() else 'xrsimulator'

    def architecture(self):
        result = self.get_option('architecture') or self.host.platform.architecture()
        if result in ('arm64', 'arm64e'):
            return 'arm64'
        return self.DEFAULT_ARCHITECTURE

    @staticmethod
    def _version_from_name(name):
        if len(name.split('-')) > 2 and name.split('-')[2].isdigit():
            return Version.from_string(name.split('-')[2])
        return None

    @memoized
    def device_version(self):
        if self.get_option('version'):
            return Version.from_string(self.get_option('version'))
        return VisionOSSimulatorPort._version_from_name(self._name) if VisionOSSimulatorPort._version_from_name(self._name) else self.host.platform.xcode_sdk_version('xrsimulator')

    def environment_for_api_tests(self):
        inherited_env = super(VisionOSSimulatorPort, self).environment_for_api_tests()
        new_environment = {}
        SIMCTL_ENV_PREFIX = 'SIMCTL_CHILD_'
        for value in inherited_env:
            if not value.startswith(SIMCTL_ENV_PREFIX):
                new_environment[SIMCTL_ENV_PREFIX + value] = inherited_env[value]
            else:
                new_environment[value] = inherited_env[value]
        return new_environment

    def operating_system(self):
        return 'visionos-simulator'

    def setup_environ_for_server(self, server_name=None):
        _log.debug('Setting up environment for server on {}'.format(self.operating_system()))
        env = super(VisionOSSimulatorPort, self).setup_environ_for_server(server_name)
        if server_name == self.driver_name() and self.get_option('leaks'):
            env['MallocStackLogging'] = '1'
            env['__XPC_MallocStackLogging'] = '1'
            env['MallocScribble'] = '1'
            env['__XPC_MallocScribble'] = '1'
        return env

    def reset_preferences(self):
        SimulatedDeviceManager.tear_down(self.host)

    @property
    @memoized
    def developer_dir(self):
        return self._executive.run_command(['xcode-select', '--print-path']).rstrip()
