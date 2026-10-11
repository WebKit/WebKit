# Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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
from webkitpy.port.driver import DriverInput
from webkitpy.port.embedded_port import EmbeddedPort
from webkitpy.xcode.simulated_device import SimulatedDeviceManager

_log = logging.getLogger(__name__)


class EmbeddedSimulatorPort(EmbeddedPort):
    """Base class for simulator ports (iOS, watchOS, visionOS simulators)."""

    DEVICE_MANAGER = SimulatedDeviceManager

    @staticmethod
    def _version_from_name(name):
        if len(name.split('-')) > 2 and name.split('-')[2].isdigit():
            return Version.from_string(name.split('-')[2])
        return None

    @memoized
    def device_version(self):
        if self.get_option('version'):
            return Version.from_string(self.get_option('version'))
        return self._version_from_name(self._name) if self._version_from_name(self._name) else self.host.platform.xcode_sdk_version(self.SDK)

    def environment_for_api_tests(self):
        inherited_env = super().environment_for_api_tests()
        new_environment = {}
        SIMCTL_ENV_PREFIX = 'SIMCTL_CHILD_'
        for value in inherited_env:
            if not value.startswith(SIMCTL_ENV_PREFIX):
                new_environment[SIMCTL_ENV_PREFIX + value] = inherited_env[value]
            else:
                new_environment[value] = inherited_env[value]
        return new_environment

    def setup_environ_for_server(self, server_name=None):
        _log.debug('Setting up environment for server on {}'.format(self.operating_system()))
        env = super().setup_environ_for_server(server_name)
        if server_name == self.driver_name() and self.get_option('leaks'):
            env['MallocStackLogging'] = '1'
            env['__XPC_MallocStackLogging'] = '1'
            env['MallocScribble'] = '1'
            env['__XPC_MallocScribble'] = '1'
        return env

    def setup_test_run(self, device_type=None):
        super().setup_test_run(device_type)
        if not self.get_option('install') or self.get_option('no_timeout'):
            return

        # The first launch of the runner on a simulator booted for this run can outlast a test's timeout.
        for worker_number in range(self.child_processes()):
            device = self.target_host(worker_number)
            if not device.platform_device.booted_by_script:
                continue
            _log.debug('Warming up the runner on {} ...'.format(device))
            warmup_driver = self.create_driver(worker_number)
            warmup_driver.run_test(DriverInput('file:///warmup-does-not-exist', 60000., None, should_run_pixel_test=False), stop_when_done=True)

    def reset_preferences(self):
        SimulatedDeviceManager.tear_down(self.host)

    @property
    @memoized
    def developer_dir(self):
        return self._executive.run_command(['xcode-select', '--print-path']).rstrip()
