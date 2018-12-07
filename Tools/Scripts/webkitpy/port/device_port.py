# Copyright (C) 2018 Apple Inc. All rights reserved.
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
import traceback

from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.port.darwin import DarwinPort
from webkitpy.port.simulator_process import SimulatorProcess
from webkitpy.xcode.device_type import DeviceType
from webkitpy.xcode.simulated_device import DeviceRequest, SimulatedDeviceManager


_log = logging.getLogger(__name__)


class DevicePort(DarwinPort):

    DEVICE_MANAGER = None
    NO_DEVICE_MANAGER = 'No device manager found for port'

    def __init__(self, *args, **kwargs):
        super(DevicePort, self).__init__(*args, **kwargs)
        self._test_runner_process_constructor = SimulatorProcess
        self._printing_cmd_line = False

    def driver_cmd_line_for_logging(self):
        # Avoid creating/connecting to devices just for command line logging.
        self._printing_cmd_line = True
        result = super(DevicePort, self).driver_cmd_line_for_logging()
        self._printing_cmd_line = False
        return result

    def _generate_all_test_configurations(self):
        configurations = []
        for build_type in self.ALL_BUILD_TYPES:
            for architecture in self.ARCHITECTURES:
                configurations.append(TestConfiguration(version=self.version_name(), architecture=architecture, build_type=build_type))
        return configurations

    def child_processes(self):
        return int(self.get_option('child_processes'))

    def driver_name(self):
        if self.get_option('driver_name'):
            return self.get_option('driver_name')
        if self.get_option('webkit_test_runner'):
            return 'WebKitTestRunnerApp.app'
        return 'DumpRenderTree.app'

    # A device is the target host for a specific worker number.
    def target_host(self, worker_number=None):
        if self._printing_cmd_line or worker_number is None:
            return self.host
        if self.DEVICE_MANAGER is None:
            raise RuntimeError('No device manager for specified port')
        if self.DEVICE_MANAGER.INITIALIZED_DEVICES is None:
            raise RuntimeError('No initialized devices for testing')
        return self.DEVICE_MANAGER.INITIALIZED_DEVICES[worker_number]

    def devices(self):
        if self.DEVICE_MANAGER is None:
            return []
        if self.DEVICE_MANAGER.INITIALIZED_DEVICES is None:
            return []
        return self.DEVICE_MANAGER.INITIALIZED_DEVICES

    # Despite their names, these flags do not actually get passed all the way down to webkit-build.
    def _build_driver_flags(self):
        return ['--sdk', self.SDK] + (['ARCHS=%s' % self.architecture()] if self.architecture() else [])

    def _install(self):
        if not self.get_option('install'):
            _log.debug('Skipping installation')
            return

        for i in xrange(self.child_processes()):
            device = self.target_host(i)
            _log.debug('Installing to {}'.format(device))
            # Without passing DYLD_LIBRARY_PATH, libWebCoreTestSupport cannot be loaded and DRT/WKTR will crash pre-launch,
            # leaving a crash log which will be picked up in results. DYLD_FRAMEWORK_PATH is needed to prevent an early crash.
            if not device.install_app(self._path_to_driver(), {'DYLD_LIBRARY_PATH': self._build_path(), 'DYLD_FRAMEWORK_PATH': self._build_path()}):
                raise RuntimeError('Failed to install app {} on device {}'.format(self._path_to_driver(), device.udid))
            if not device.install_dylibs(self._build_path()):
                raise RuntimeError('Failed to install dylibs at {} on device {}'.format(self._build_path(), device.udid))

    def _device_type_with_version(self, device_type=None):
        device_type = device_type if device_type else self.DEFAULT_DEVICE_TYPE
        return DeviceType(
            hardware_family=device_type.hardware_family,
            hardware_type=device_type.hardware_type,
            software_version=self.device_version(),
            software_variant=device_type.software_variant,
        )

    def default_child_processes(self, device_type=None):
        if not self.DEVICE_MANAGER:
            raise RuntimeError(self.NO_DEVICE_MANAGER)

        # FIXME Checking software variant is important for simulators, otherwise an iOS port could boot a watchOS simulator.
        # Really, the DEFAULT_DEVICE_TYPE for simulators should be a general instead of specific type, then this code would
        # explicitly compare against device_type
        device_type = self._device_type_with_version(device_type)
        if device_type.software_variant and self.DEFAULT_DEVICE_TYPE.software_variant != device_type.software_variant:
            return 0

        return self.DEVICE_MANAGER.device_count_for_type(
            self._device_type_with_version(device_type),
            host=self.host,
            use_booted_simulator=not self.get_option('dedicated_simulators', False),
        )

    def max_child_processes(self, device_type=None):
        result = self.default_child_processes(device_type=device_type)
        if result and self.DEVICE_MANAGER == SimulatedDeviceManager:
            return super(DevicePort, self).max_child_processes(device_type=None)
        return result

    def setup_test_run(self, device_type=None):
        if not self.DEVICE_MANAGER:
            raise RuntimeError(self.NO_DEVICE_MANAGER)

        device_type = self._device_type_with_version(device_type)
        _log.debug('\nCreating devices for {}'.format(device_type))

        request = DeviceRequest(
            device_type,
            use_booted_simulator=not self.get_option('dedicated_simulators', False),
            use_existing_simulator=False,
            allow_incomplete_match=True,
        )
        self.DEVICE_MANAGER.initialize_devices(
            [request] * self.child_processes(),
            self.host,
            layout_test_dir=self.layout_tests_dir(),
            pin=self.get_option('pin', None),
            use_nfs=self.get_option('use_nfs', True),
            reboot=self.get_option('reboot', False),
        )

        if not self.devices():
            raise RuntimeError('No devices are available for testing')
        if len(self.DEVICE_MANAGER.INITIALIZED_DEVICES) < self.child_processes():
            raise RuntimeError('To few connected devices for {} processes'.format(self.child_processes()))

        self._install()

        for i in xrange(self.child_processes()):
            host = self.target_host(i)
            host.prepare_for_testing(
                self.ports_to_forward(),
                self.app_identifier_from_bundle(self._path_to_driver()),
                self.layout_tests_dir(),
            )
            self._crash_logs_to_skip_for_host[host] = host.filesystem.files_under(self.path_to_crash_logs())

    def clean_up_test_run(self):
        super(DevicePort, self).clean_up_test_run()

        # Best effort to let every device teardown before throwing any exceptions here.
        # Failure to teardown devices can leave things in a bad state.
        exception_list = []
        for i in xrange(self.child_processes()):
            device = self.target_host(i)
            if not device:
                continue
            try:
                device.finished_testing()
            except BaseException as e:
                trace = traceback.format_exc()
                if isinstance(e, Exception):
                    exception_list.append([e, trace])
                else:
                    exception_list.append([Exception('Exception while tearing down {}'.format(device)), trace])

        if len(exception_list) == 1:
            raise
        if len(exception_list) > 1:
            print('\n')
            for exception in exception_list:
                _log.error('{} raised: {}'.format(exception[0].__class__.__name__, exception[0]))
                _log.error(exception[1])
                _log.error('--------------------------------------------------')

            raise RuntimeError('Multiple failures when teardown devices')

    def did_spawn_worker(self, worker_number):
        super(DevicePort, self).did_spawn_worker(worker_number)

        self.target_host(worker_number).release_worker_resources()

    def setup_environ_for_server(self, server_name=None):
        env = super(DevicePort, self).setup_environ_for_server(server_name)
        if server_name == self.driver_name() and self.get_option('guard_malloc'):
            self._append_value_colon_separated(env, 'DYLD_INSERT_LIBRARIES', '/usr/lib/libgmalloc.dylib')
            self._append_value_colon_separated(env, '__XPC_DYLD_INSERT_LIBRARIES', '/usr/lib/libgmalloc.dylib')
        env['XML_CATALOG_FILES'] = ''  # work around missing /etc/catalog <rdar://problem/4292995>
        return env

    def device_version(self):
        raise NotImplementedError
