# Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
import logging
import multiprocessing
import queue
import time
import traceback

from abc import abstractmethod

from webkitpy.common.system.executive import ScriptError
from webkitpy.common.version_name_map import VersionNameMap, PUBLIC_TABLE, INTERNAL_TABLE
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.port.darwin import DarwinPort
from webkitpy.port.device_provisioning import DeviceProvisioning
from webkitpy.port.simulator_process import SimulatorProcess
from webkitpy.results.upload import Upload
from webkitpy.xcode.device_type import DeviceType
from webkitpy.xcode.simulated_device import DeviceRequest, SimulatedDeviceManager


_log = logging.getLogger(__name__)


class EmbeddedPort(DarwinPort):

    @property
    def _provisioning(self):
        """The device manager's provisioning interface.

        A manager which returns devices already able to run tests does not implement it, and gets the device-less
        defaults, which is what makes the port address its devices by position instead of claiming them."""
        manager = self.DEVICE_MANAGER
        if manager is None:
            return DeviceProvisioning
        # Managers are classes, except in tests where they are instances.
        if issubclass(manager if isinstance(manager, type) else type(manager), DeviceProvisioning):
            return manager
        return DeviceProvisioning


    DEVICE_MANAGER = None
    NO_DEVICE_MANAGER = 'No device manager found for port'

    DEVICE_WAIT_TIMEOUT = 300

    DEVICE_READY_TIMEOUT = SimulatedDeviceManager.SIMULATOR_BOOT_TIMEOUT

    @property
    @abstractmethod
    def SDK(self) -> str:
        """SDK name"""
        ...

    @abstractmethod
    def operating_system(self):
        """Platform name"""
        ...

    def __init__(self, *args, **kwargs):
        super(EmbeddedPort, self).__init__(*args, **kwargs)
        self._test_runner_process_constructor = SimulatorProcess
        self._printing_cmd_line = False
        self._claimed_device = None

    def is_simulator(self):
        if not self.DEVICE_MANAGER:
            raise RuntimeError(self.NO_DEVICE_MANAGER)

        return self.DEVICE_MANAGER == SimulatedDeviceManager

    def driver_cmd_line_for_logging(self):
        # Avoid creating/connecting to devices just for command line logging.
        self._printing_cmd_line = True
        result = super(EmbeddedPort, self).driver_cmd_line_for_logging()
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
        parent = super(EmbeddedPort, self).driver_name()
        if parent == 'WebKitTestRunner':
            return 'WebKitTestRunnerApp.app'
        return f'{parent}.app'

    # A device is the target host for a specific worker number.
    def target_host(self, worker_number=None):
        if self._printing_cmd_line or worker_number is None:
            return self.host
        if self.DEVICE_MANAGER is None:
            raise RuntimeError('No device manager for specified port')
        device = self._device_for_worker(worker_number)
        if device is None:
            raise RuntimeError('No initialized devices for testing')
        return device

    def _device_for_worker(self, worker_number):
        """The device this worker owns, claiming one from the device manager if it does not have it yet.

        Returns None when no device is free, which callers report rather than waiting on: a worker holding a shard
        while it waits keeps that shard away from the devices already testing."""
        if self._claimed_device is not None:
            return self._claimed_device

        if self._manager_hands_out_devices():
            device, sets_up = self._provisioning.claim_device(self.DEVICE_WAIT_TIMEOUT)
            if device is None:
                return None
            _log.debug(u'Worker {} claimed {}'.format(worker_number, device))
            self._claimed_device = device
            self._prepare_claimed_device(device, sets_up=sets_up)
            return device

        if not self.DEVICE_MANAGER.INITIALIZED_DEVICES:
            return None
        return self.DEVICE_MANAGER.INITIALIZED_DEVICES[worker_number % len(self.DEVICE_MANAGER.INITIALIZED_DEVICES)]

    def _manager_hands_out_devices(self):
        return bool(self._provisioning.DEVICE_QUEUE)

    def expects_more_devices(self):
        return self._provisioning.expects_more_devices()

    def provisioning_state(self):
        return self._provisioning.provisioning_state()

    def devices(self):
        if self.DEVICE_MANAGER is None:
            return []
        if self.DEVICE_MANAGER.INITIALIZED_DEVICES is None:
            return []
        return self.DEVICE_MANAGER.INITIALIZED_DEVICES

    def __getstate__(self):
        # A device this process claimed is not the claim of whichever process this port is pickled into.
        return dict(self.__dict__, _claimed_device=None)

    def any_ready_device(self):
        """A device fit to run something that belongs to no worker, such as listing tests.

        The first initialized device is not necessarily one of them: with devices handed over as each finishes booting,
        it may still be coming up while others are already testing."""
        ready = self._provisioning.READY_DEVICES
        if ready:
            return ready[0]
        devices = self.devices()
        return devices[0] if devices else None

    @staticmethod
    def _device_is_present(device, force_update=False):
        """Whether a device is still up, without launching anything on it.

        `is_usable` would be the fuller answer, but it decides by launching an app and terminating it, which steals
        the foreground from the driver and fails whichever test runs next."""
        platform_device = device.platform_device
        try:
            if hasattr(platform_device, 'is_booted_or_booting'):
                return bool(platform_device.is_booted_or_booting(force_update=force_update))
            if hasattr(platform_device, 'available'):
                return bool(platform_device.available())
        except Exception as error:
            _log.warning(u'Could not determine whether {} is still there, assuming it is not: {}'.format(device, error))
            return False
        return True

    def target_host_is_usable(self, worker_number=None, force_update=False):
        """Whether this worker's device is still there to run tests on.

        Asking simctl costs a fifth of a second, so only a caller which already suspects the device forces a fresh
        answer; the rest accept a recent one."""
        if worker_number is None or self.DEVICE_MANAGER is None:
            return True

        device = self._device_for_worker(worker_number)
        if device is None:
            return False
        return self._device_is_present(device, force_update=force_update)

    def has_usable_device(self):
        """Whether any device is still there.

        Asked by the process handing out shards, which must not claim a device to find out: claiming would take one
        away from the workers and block while it waited for it."""
        if self.DEVICE_MANAGER is None:
            return True
        return any(self._device_is_present(device) for device in self.devices())

    # Despite their names, these flags do not actually get passed all the way down to webkit-build.
    def _build_driver_flags(self):
        return ['--sdk', self.SDK] + (['ARCHS=%s' % self.architecture()] if self.architecture() else [])

    def _install_on(self, device):
        if not self.get_option('install'):
            _log.debug('Skipping installation')
            return

        _log.debug(u'Installing to {}'.format(device))
        # Without passing DYLD_LIBRARY_PATH, libWebCoreTestSupport cannot be loaded and DRT/WKTR will crash pre-launch,
        # leaving a crash log which will be picked up in results. DYLD_FRAMEWORK_PATH is needed to prevent an early crash.
        if not device.install_app(self._path_to_driver(), {'DYLD_LIBRARY_PATH': self._build_path(), 'DYLD_FRAMEWORK_PATH': self._build_path()}):
            raise RuntimeError('Failed to install app {} on device {}'.format(self._path_to_driver(), device.udid))
        if not device.install_dylibs(self._build_path()):
            raise RuntimeError('Failed to install dylibs at {} on device {}'.format(self._build_path(), device.udid))

    def _device_type_with_version(self, device_type=None):
        device_type = device_type if device_type else self.DEVICE_TYPE
        return DeviceType(
            hardware_family=device_type.hardware_family,
            hardware_type=device_type.hardware_type,
            software_version=self.device_version(),
            software_variant=device_type.software_variant,
        )

    def default_child_processes(self, device_type=None):
        if not self.DEVICE_MANAGER:
            raise RuntimeError(self.NO_DEVICE_MANAGER)

        device_type = self._device_type_with_version(device_type)
        if device_type not in self.DEVICE_TYPE:
            return 0

        if self.get_option('force'):
            device_type.hardware_family = None
            device_type.hardware_type = None

        return self.DEVICE_MANAGER.device_count_for_type(
            self._device_type_with_version(device_type),
            host=self.host,
            use_booted_simulator=not self.get_option('dedicated_simulators', False),
        )

    def max_child_processes(self, device_type=None):
        result = self.default_child_processes(device_type=device_type)
        if result and self.is_simulator():
            return super(EmbeddedPort, self).max_child_processes(device_type=None)
        return result

    def get_available_simulators_of_type_from_xcrun(self) -> list[DeviceType]:
        '''Determines usable simulator types from available runtimes.'''

        if not self.host.platform.is_mac():
            return []

        os_name = self.DEVICE_TYPE.software_variant
        try:
            runtimes = json.loads(
                self.host.executive.run_command(
                    [SimulatedDeviceManager.xcrun, 'simctl', 'list', 'runtimes', os_name, '-j']
                )
            )['runtimes']
        except (ValueError, ScriptError):
            _log.error('Failed to decode json output')
            return []

        usable_devices = []
        for runtime in runtimes:
            for device in runtime['supportedDeviceTypes']:
                usable_devices.append(DeviceType(
                    hardware_family=device['productFamily'],
                    hardware_type=device['name'][len(device['productFamily']):].strip(),
                    software_variant=os_name
                ))

        return usable_devices

    def supported_device_types(self):
        types = set()
        for device in self.DEVICE_MANAGER.available_devices(host=self.host, udids=self.get_option('udids', None)):
            if self.is_simulator() and not device.platform_device.is_booted_or_booting():
                continue
            if device.device_type in self.DEVICE_TYPE:
                types.add(device.device_type)

        if not types and self.is_simulator():
            available_sim_types = self.get_available_simulators_of_type_from_xcrun()
            for default_device in self.DEFAULT_DEVICE_TYPES:
                if default_device in available_sim_types and default_device in self.DEVICE_TYPE:
                    types.add(default_device)
            if not types:
                if self.DEVICE_TYPE.hardware_family:
                    for sim_type in available_sim_types:
                        if self.DEVICE_TYPE.hardware_family == sim_type.hardware_family:
                            types.add(sim_type)
                            break
                else:
                    for dt in self.DEFAULT_DEVICE_TYPES:
                        for sim_type in available_sim_types:
                            if dt.hardware_family == sim_type.hardware_family:
                                types.add(sim_type)
                                break

        if types and not self.get_option('dedicated_simulators', False):

            def sorted_by_default_device_type(type):
                try:
                    return self.DEFAULT_DEVICE_TYPES.index(type)
                except ValueError:
                    return len(self.DEFAULT_DEVICE_TYPES)

            return sorted(types, key=sorted_by_default_device_type)

        return self.DEFAULT_DEVICE_TYPES or [self.DEVICE_TYPE]

    def setup_test_run(self, device_type=None, workers_per_device=1):
        if not self.DEVICE_MANAGER:
            raise RuntimeError(self.NO_DEVICE_MANAGER)

        device_type = self._device_type_with_version(device_type)
        _log.debug(u'\nCreating devices for {}'.format(device_type))

        request = DeviceRequest(
            device_type,
            use_booted_simulator=not self.get_option('dedicated_simulators', False),
            use_existing_simulator=False,
            allow_incomplete_match=self.get_option('force'),
        )
        # Creating devices belongs to the manager. Only waiting for them to become ready is optional.
        create_devices = getattr(self.DEVICE_MANAGER, 'create_devices', self.DEVICE_MANAGER.initialize_devices)
        create_devices(
            [request] * self.child_processes(),
            self.host,
            layout_test_dir=self.layout_tests_dir(),
            pin=self.get_option('pin', None),
            use_nfs=self.get_option('use_nfs', True),
            reboot=self.get_option('reboot', False),
            udids=self.get_option('udids', None),
        )

        if not self.devices():
            raise RuntimeError('No devices are available for testing')
        if len(self.DEVICE_MANAGER.INITIALIZED_DEVICES) < self.child_processes():
            raise RuntimeError('To few connected devices for {} processes'.format(self.child_processes()))

        # Recorded before any device is handed over, in the process that collects crash logs at the end of the run.
        for device in self.devices():
            self._crash_logs_to_skip_for_host[device] = device.filesystem.files_under(self.path_to_crash_logs())

        self._provisioning.begin_provisioning(timeout=self.DEVICE_READY_TIMEOUT, slots_per_device=workers_per_device)

        if not self._provisioning.wait_for_first_ready_device(timeout=self.DEVICE_READY_TIMEOUT):
            raise RuntimeError('No device became ready for testing')
        if self._provisioning.PENDING_DEVICES:
            _log.debug(u'Starting on {} of {} devices; the rest join as they are ready'.format(
                len(self._provisioning.READY_DEVICES), len(self.devices())))

    def advance_provisioning(self):
        """Offers the workers any device that has finished booting.

        Called from the process handing out shards, so it must not block: while it runs, no results are collected and
        no shards are dispatched, which stalls every worker including those already testing."""
        self._provisioning.advance_provisioning()

    def _prepare_claimed_device(self, device, sets_up=True):
        """Everything a device needs before it can run tests, done by the worker that claimed it.

        Not by the process handing out shards: waiting for a device to be able to run something, and copying the
        driver onto it, both block for a long time, and a worker that has just claimed a device has nothing else to
        do meanwhile. Only the first worker on a device does that part, since the second would install the same driver
        onto the same device and ask a device already proven usable whether it is usable."""
        if sets_up:
            # Having finished booting is not quite the same as being able to run something, so this is still asked.
            self._provisioning.block_on_ready([device], timeout=self.DEVICE_READY_TIMEOUT)
            self._install_on(device)

        # Binds a socket which must belong to the process running the driver, so every worker sharing a device does it.
        device.prepare_for_testing(
            self.ports_to_forward(),
            self.app_identifier_from_bundle(self._path_to_driver()),
            self.layout_tests_dir(),
        )

    def clean_up_test_run(self):
        # A run sets up and tears down once per device type it tests, and a device torn down by one is not ready for
        # the next.
        self._claimed_device = None
        self._provisioning.end_provisioning()
        super(EmbeddedPort, self).clean_up_test_run()

        # Best effort to let every device teardown before throwing any exceptions here.
        # Failure to teardown devices can leave things in a bad state.
        # Iterates the devices rather than the workers: ownership belongs to whichever worker claimed a device, so
        # this process cannot ask for one by worker number.
        exception_list = []
        for device in self.devices():
            if not device:
                continue
            try:
                device.finished_testing()
            except BaseException as e:
                trace = traceback.format_exc()
                if isinstance(e, Exception):
                    exception_list.append([e, trace])
                else:
                    exception_list.append([Exception(u'Exception while tearing down {}'.format(device)), trace])

        if len(exception_list) == 1:
            # Raise the failure itself: a bare raise here has no exception to re-raise and loses it.
            raise exception_list[0][0]
        if len(exception_list) > 1:
            print('\n')
            for exception in exception_list:
                _log.error('{} raised: {}'.format(exception[0].__class__.__name__, exception[0]))
                _log.error(exception[1])
                _log.error('--------------------------------------------------')

            raise RuntimeError('Multiple failures when teardown devices')

    def did_spawn_worker(self, worker_number):
        super(EmbeddedPort, self).did_spawn_worker(worker_number)

        device = self._device_for_worker(worker_number)
        if device is None:
            _log.debug(u'Worker {} has no device yet; its share of the tests will run elsewhere'.format(worker_number))
            return
        device.release_worker_resources()

    def prepare_devices_for_workers(self, workers_per_device=1):
        """Offers the ready devices to the pool of workers about to start.

        Call this before every pool: a worker holds its device until it exits, so a pool leaves nothing behind it for
        the next one to claim."""
        return self._provisioning.offer_ready_devices(slots_per_device=workers_per_device)

    def setup_environ_for_server(self, server_name=None):
        env = super(EmbeddedPort, self).setup_environ_for_server(server_name)
        if server_name == self.driver_name() and self.get_option('guard_malloc'):
            self._append_value_colon_separated(env, 'DYLD_INSERT_LIBRARIES', '/usr/lib/libgmalloc.dylib')
            self._append_value_colon_separated(env, '__XPC_DYLD_INSERT_LIBRARIES', '/usr/lib/libgmalloc.dylib')
        env['XML_CATALOG_FILES'] = ''  # work around missing /etc/catalog <rdar://problem/4292995>
        return env

    @abstractmethod
    def device_version(self):
        raise NotImplementedError

    def configuration_for_upload(self, host=None):
        configuration = self.test_configuration()

        device_type = host.device_type if host else self.DEVICE_TYPE
        model = device_type.hardware_family
        if model and device_type.hardware_type:
            model += u' {}'.format(device_type.hardware_type)

        version = self.device_version()
        for table in [INTERNAL_TABLE, PUBLIC_TABLE]:
            version_name = VersionNameMap.map(self.host.platform).to_name(version, platform=device_type.software_variant.lower(), table=table)
            if version_name:
                break

        if self.get_option('guard_malloc'):
            style = 'guard-malloc'
        elif self._config.asan:
            style = 'asan'
        else:
            style = configuration.build_type

        return Upload.create_configuration(
            platform=device_type.software_variant.lower(),
            is_simulator=self.is_simulator(),
            version=str(version),
            version_name=version_name,
            architecture=configuration.architecture,
            style=style,
            model=model,
            flavor=self.get_option('result_report_flavor'),
            sdk=host.build_version if host else None,
        )
