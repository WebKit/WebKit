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

import atexit
import json
import logging
import plistlib
import re
import time

from webkitpy.common.memoized import memoized
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system.systemhost import SystemHost
from webkitpy.common.timeout_context import Timeout
from webkitpy.common.version import Version
from webkitpy.port.device import Device
from webkitpy.xcode.device_type import DeviceType

_log = logging.getLogger(__name__)


class DeviceRequest(object):

    def __init__(self, device_type, use_booted_simulator=True, use_existing_simulator=True, allow_incomplete_match=False, merge_requests=False):
        self.device_type = device_type
        self.use_booted_simulator = use_booted_simulator
        self.use_existing_simulator = use_existing_simulator
        self.allow_incomplete_match = allow_incomplete_match  # When matching booted simulators, only force the software_variant to match.
        self.merge_requests = merge_requests  # Allow a single booted simulator to fullfil multiple requests.


class SimulatedDeviceManager(object):
    class Runtime(object):
        def __init__(self, runtime_dict):
            self.build_version = runtime_dict['buildversion']
            self.os_variant = runtime_dict['name'].split(' ')[0]
            self.version = Version.from_string(runtime_dict['version'])
            self.identifier = runtime_dict['identifier']
            self.name = runtime_dict['name']

    AVAILABLE_RUNTIMES = []
    AVAILABLE_DEVICES = []
    INITIALIZED_DEVICES = None

    SIMULATOR_BOOT_TIMEOUT = 600

    # FIXME: Simulators should only take up 2GB, but because of <rdar://problem/39393590> something in the OS thinks they're taking closer to 6GB
    MEMORY_ESTIMATE_PER_SIMULATOR_INSTANCE = 6 * (1024 ** 3)  # 6GB a simulator.
    PROCESS_COUNT_ESTIMATE_PER_SIMULATOR_INSTANCE = 125

    xcrun = '/usr/bin/xcrun'
    simulator_device_path = '~/Library/Developer/CoreSimulator/Devices'
    simulator_bundle_id = 'com.apple.iphonesimulator'
    _device_identifier_to_name = {}
    _managing_simulator_app = False

    @staticmethod
    def _create_runtimes(runtimes):
        result = []
        for runtime in runtimes:
            if runtime.get('availability') != '(available)' and runtime.get('isAvailable') != 'YES':
                continue
            try:
                result.append(SimulatedDeviceManager.Runtime(runtime))
            except (ValueError, AssertionError):
                continue
        return result

    @staticmethod
    def _create_device_with_runtime(host, runtime, device_info):
        if device_info.get('availability') != '(available)' and device_info.get('isAvailable') != 'YES':
            return None

        # Check existing devices.
        for device in SimulatedDeviceManager.AVAILABLE_DEVICES:
            if device.udid == device_info['udid']:
                return device

        # Check that the device.plist exists
        device_plist = host.filesystem.expanduser(host.filesystem.join(SimulatedDeviceManager.simulator_device_path, device_info['udid'], 'device.plist'))
        if not host.filesystem.isfile(device_plist):
            return None

        # Find device type. If we can't parse the device type, ignore this device.
        try:
            device_type_string = SimulatedDeviceManager._device_identifier_to_name[plistlib.readPlist(host.filesystem.open_binary_file_for_reading(device_plist))['deviceType']]
            device_type = DeviceType.from_string(device_type_string, runtime.version)
            assert device_type.software_variant == runtime.os_variant
        except (ValueError, AssertionError):
            return None

        result = Device(SimulatedDevice(
            name=device_info['name'],
            udid=device_info['udid'],
            host=host,
            device_type=device_type,
        ))
        SimulatedDeviceManager.AVAILABLE_DEVICES.append(result)
        return result

    @staticmethod
    def populate_available_devices(host=SystemHost()):
        if not host.platform.is_mac():
            return

        try:
            simctl_json = json.loads(host.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'list', '--json']))
        except (ValueError, ScriptError):
            return

        SimulatedDeviceManager._device_identifier_to_name = {device['identifier']: device['name'] for device in simctl_json['devicetypes']}
        SimulatedDeviceManager.AVAILABLE_RUNTIMES = SimulatedDeviceManager._create_runtimes(simctl_json['runtimes'])

        for runtime in SimulatedDeviceManager.AVAILABLE_RUNTIMES:
            for device_json in simctl_json['devices'][runtime.name]:
                device = SimulatedDeviceManager._create_device_with_runtime(host, runtime, device_json)
                if not device:
                    continue

                # Update device state from simctl output.
                device.platform_device._state = SimulatedDevice.NAME_FOR_STATE.index(device_json['state'].upper())
                device.platform_device._last_updated_state = time.time()
        return

    @staticmethod
    def available_devices(host=SystemHost()):
        if SimulatedDeviceManager.AVAILABLE_DEVICES == []:
            SimulatedDeviceManager.populate_available_devices(host)
        return SimulatedDeviceManager.AVAILABLE_DEVICES

    @staticmethod
    def device_by_filter(filter, host=SystemHost()):
        result = []
        for device in SimulatedDeviceManager.available_devices(host):
            if filter(device):
                result.append(device)
        return result

    @staticmethod
    def _find_exisiting_device_for_request(request):
        if not request.use_existing_simulator:
            return None
        for device in SimulatedDeviceManager.AVAILABLE_DEVICES:
            # One of the INITIALIZED_DEVICES may be None, so we can't just use __eq__
            for initialized_device in SimulatedDeviceManager.INITIALIZED_DEVICES:
                if isinstance(initialized_device, Device) and device == initialized_device:
                    device = None
                    break
            if device and request.device_type == device.platform_device.device_type:
                return device
        return None

    @staticmethod
    def _find_available_name(name_base):
        created_index = 0
        while True:
            name = '{} {}'.format(name_base, created_index)
            created_index += 1
            for device in SimulatedDeviceManager.INITIALIZED_DEVICES:
                if device is None:
                    continue
                if device.platform_device.name == name:
                    break
            else:
                return name

    @staticmethod
    def get_runtime_for_device_type(device_type):
        for runtime in SimulatedDeviceManager.AVAILABLE_RUNTIMES:
            if runtime.os_variant == device_type.software_variant and (device_type.software_version is None or device_type.software_version == runtime.version):
                return runtime

        # Allow for a partial version match.
        for runtime in SimulatedDeviceManager.AVAILABLE_RUNTIMES:
            if runtime.os_variant == device_type.software_variant and runtime.version in device_type.software_version:
                return runtime
        return None

    @staticmethod
    def _disambiguate_device_type(device_type):
        # Copy by value since we do not want to modify the DeviceType passed in.
        full_device_type = DeviceType(
            hardware_family=device_type.hardware_family,
            hardware_type=device_type.hardware_type,
            software_version=device_type.software_version,
            software_variant=device_type.software_variant)

        runtime = SimulatedDeviceManager.get_runtime_for_device_type(full_device_type)
        assert runtime is not None
        full_device_type.software_version = runtime.version

        if full_device_type.hardware_family is None:
            # We use the existing devices to determine a legal family if no family is specified
            for device in SimulatedDeviceManager.AVAILABLE_DEVICES:
                if device.platform_device.device_type == full_device_type:
                    full_device_type.hardware_family = device.platform_device.device_type.hardware_family
                    break

        if full_device_type.hardware_type is None:
            # Again, we use the existing devices to determine a legal hardware type
            for device in SimulatedDeviceManager.AVAILABLE_DEVICES:
                if device.platform_device.device_type == full_device_type:
                    full_device_type.hardware_type = device.platform_device.device_type.hardware_type
                    break

        full_device_type.check_consistency()
        return full_device_type

    @staticmethod
    def _get_device_identifier_for_type(device_type):
        for type_id, type_name in SimulatedDeviceManager._device_identifier_to_name.iteritems():
            if type_name == '{} {}'.format(device_type.hardware_family, device_type.hardware_type):
                return type_id
        return None

    @staticmethod
    def _create_or_find_device_for_request(request, host=SystemHost(), name_base='Managed'):
        assert isinstance(request, DeviceRequest)

        device = SimulatedDeviceManager._find_exisiting_device_for_request(request)
        if device:
            return device

        name = SimulatedDeviceManager._find_available_name(name_base)
        device_type = SimulatedDeviceManager._disambiguate_device_type(request.device_type)
        runtime = SimulatedDeviceManager.get_runtime_for_device_type(device_type)
        device_identifier = SimulatedDeviceManager._get_device_identifier_for_type(device_type)

        assert runtime is not None
        assert device_identifier is not None

        for device in SimulatedDeviceManager.available_devices(host):
            if device.platform_device.name == name:
                device.platform_device._delete()
                break

        _log.debug("Creating device '{}', of type {}".format(name, device_type))
        host.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'create', name, device_identifier, runtime.identifier])

        # We just added a device, so our list of _available_devices needs to be re-synced.
        SimulatedDeviceManager.populate_available_devices(host)
        for device in SimulatedDeviceManager.available_devices(host):
            if device.platform_device.name == name:
                device.platform_device.managed_by_script = True
                return device
        return None

    @staticmethod
    def _does_fulfill_request(device, requests):
        if not device.platform_device.is_booted_or_booting():
            return None

        # Exact match.
        for request in requests:
            if not request.use_booted_simulator:
                continue
            if request.device_type == device.platform_device.device_type:
                _log.debug("The request for '{}' matched {} exactly".format(request.device_type, device))
                return request

        # Contained-in match.
        for request in requests:
            if not request.use_booted_simulator:
                continue
            if device.platform_device.device_type in request.device_type:
                _log.debug("The request for '{}' fuzzy-matched {}".format(request.device_type, device))
                return request

        # DeviceRequests are compared by reference
        requests_copy = [request for request in requests]

        # Check for an incomplete match
        # This is usually used when we don't want to take the time to start a simulator and would
        # rather use the one the user has already started, even if it isn't quite what we're looking for.
        for request in requests_copy:
            if not request.use_booted_simulator or not request.allow_incomplete_match:
                continue
            if request.device_type.software_variant == device.platform_device.device_type.software_variant:
                _log.warn("The request for '{}' incomplete-matched {}".format(request.device_type, device))
                _log.warn("This may cause unexpected behavior in code that expected the device type {}".format(request.device_type))
                return request
        return None

    @staticmethod
    def _wait_until_device_in_state(device, state, deadline):
        while device.platform_device.state(force_update=True) != state:
            _log.debug('Waiting on {} to enter state {}...'.format(device, SimulatedDevice.NAME_FOR_STATE[state]))
            time.sleep(1)
            if time.time() > deadline:
                raise RuntimeError('Timed out while waiting for all devices to boot')

    @staticmethod
    def _boot_device(device, host=SystemHost()):
        _log.debug("Booting device '{}'".format(device.udid))
        device.platform_device.booted_by_script = True
        host.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'boot', device.udid])
        SimulatedDeviceManager.INITIALIZED_DEVICES.append(device)

    @staticmethod
    def initialize_devices(requests, host=SystemHost(), name_base='Managed', simulator_ui=True, timeout=SIMULATOR_BOOT_TIMEOUT, **kwargs):
        if SimulatedDeviceManager.INITIALIZED_DEVICES is not None:
            return SimulatedDeviceManager.INITIALIZED_DEVICES

        if not host.platform.is_mac():
            return None

        SimulatedDeviceManager.INITIALIZED_DEVICES = []
        atexit.register(SimulatedDeviceManager.tear_down)

        # Convert to iterable type
        if not hasattr(requests, '__iter__'):
            requests = [requests]

        # Check running sims
        for device in SimulatedDeviceManager.available_devices(host):
            matched_request = SimulatedDeviceManager._does_fulfill_request(device, requests)
            if matched_request is None:
                continue
            requests.remove(matched_request)
            _log.debug('Attached to running simulator {}'.format(device))
            SimulatedDeviceManager.INITIALIZED_DEVICES.append(device)

            # DeviceRequests are compared by reference
            requests_copy = [request for request in requests]

            # Merging requests means that if 4 devices are requested, but only one is running, these
            # 4 requests will be fulfilled by the 1 running device.
            for request in requests_copy:
                if not request.merge_requests:
                    continue
                if not request.use_booted_simulator:
                    continue
                if request.device_type != device.platform_device.device_type and not request.allow_incomplete_match:
                    continue
                if request.device_type.software_variant != device.platform_device.device_type.software_variant:
                    continue
                requests.remove(request)

        for request in requests:
            device = SimulatedDeviceManager._create_or_find_device_for_request(request, host, name_base)
            assert device is not None

            SimulatedDeviceManager._boot_device(device, host)

        if simulator_ui and host.executive.run_command(['killall', '-0', 'Simulator'], return_exit_code=True) != 0:
            SimulatedDeviceManager._managing_simulator_app = not host.executive.run_command(['open', '-g', '-b', SimulatedDeviceManager.simulator_bundle_id], return_exit_code=True)

        deadline = time.time() + timeout
        for device in SimulatedDeviceManager.INITIALIZED_DEVICES:
            SimulatedDeviceManager._wait_until_device_in_state(device, SimulatedDevice.DeviceState.BOOTED, deadline)
        SimulatedDeviceManager.wait_until_data_migration_is_done(host, max(0, deadline - time.time()))

        return SimulatedDeviceManager.INITIALIZED_DEVICES

    @staticmethod
    @memoized
    def max_supported_simulators(host=SystemHost()):
        if not host.platform.is_mac():
            return 0

        try:
            system_process_count_limit = int(host.executive.run_command(['/usr/bin/ulimit', '-u']).strip())
            current_process_count = len(host.executive.run_command(['/bin/ps', 'aux']).strip().split('\n'))
            _log.debug('Process limit: {}, current #processes: {}'.format(system_process_count_limit, current_process_count))
        except (ValueError, ScriptError):
            return 0

        max_supported_simulators_for_hardware = min(host.executive.cpu_count() / 2, host.platform.total_bytes_memory() // SimulatedDeviceManager.MEMORY_ESTIMATE_PER_SIMULATOR_INSTANCE)
        max_supported_simulators_locally = (system_process_count_limit - current_process_count) // SimulatedDeviceManager.PROCESS_COUNT_ESTIMATE_PER_SIMULATOR_INSTANCE

        if (max_supported_simulators_locally < max_supported_simulators_for_hardware):
            _log.warn('This machine could support {} simulators, but is only configured for {}.'.format(max_supported_simulators_for_hardware, max_supported_simulators_locally))
            _log.warn('Please see <https://trac.webkit.org/wiki/IncreasingKernelLimits>.')

        if max_supported_simulators_locally == 0:
            max_supported_simulators_locally = 1

        return min(max_supported_simulators_locally, max_supported_simulators_for_hardware)

    @staticmethod
    def swap(device, request, host=SystemHost(), name_base='Managed', timeout=SIMULATOR_BOOT_TIMEOUT):
        if SimulatedDeviceManager.INITIALIZED_DEVICES is None:
            raise RuntimeError('Cannot swap when there are no initialized devices')
        if device not in SimulatedDeviceManager.INITIALIZED_DEVICES:
            raise RuntimeError('{} is not initialized, cannot swap it'.format(device))

        index = SimulatedDeviceManager.INITIALIZED_DEVICES.index(device)
        SimulatedDeviceManager.INITIALIZED_DEVICES[index] = None
        device.platform_device._tear_down()

        device = SimulatedDeviceManager._create_or_find_device_for_request(request, host, name_base)
        assert device

        if not device.platform_device.is_booted_or_booting(force_update=True):
            device.platform_device.booted_by_script = True
            _log.debug("Booting device '{}'".format(device.udid))
            host.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'boot', device.udid])
        SimulatedDeviceManager.INITIALIZED_DEVICES[index] = device

        deadline = time.time() + timeout
        SimulatedDeviceManager._wait_until_device_in_state(device, SimulatedDevice.DeviceState.BOOTED, deadline)
        SimulatedDeviceManager.wait_until_data_migration_is_done(host, max(0, deadline - time.time()))

    @staticmethod
    def wait_until_data_migration_is_done(host, timeout=SIMULATOR_BOOT_TIMEOUT):
        # The existence of a datamigrator process means that simulators are still booting.
        deadline = time.time() + timeout
        _log.debug('Waiting until no com.apple.datamigrator processes are found')
        while host.executive.running_pids(lambda process_name: 'com.apple.datamigrator' in process_name):
            if time.time() > deadline:
                raise RuntimeError('Timed out while waiting for data migration')
            time.sleep(1)

    @staticmethod
    def tear_down(host=SystemHost(), timeout=60):
        if SimulatedDeviceManager._managing_simulator_app:
            host.executive.run_command(['killall', '-9', 'Simulator'], return_exit_code=True)
            SimulatedDeviceManager._managing_simulator_app = False

        if SimulatedDeviceManager.INITIALIZED_DEVICES is None:
            return

        deadline = time.time() + timeout
        while SimulatedDeviceManager.INITIALIZED_DEVICES:
            device = SimulatedDeviceManager.INITIALIZED_DEVICES[0]
            if device is None:
                SimulatedDeviceManager.INITIALIZED_DEVICES.remove(None)
                continue
            device.platform_device._tear_down(deadline - time.time())

        SimulatedDeviceManager.INITIALIZED_DEVICES = None


class SimulatedDevice(object):
    class DeviceState:
        CREATING = 0
        SHUT_DOWN = 1
        BOOTING = 2
        BOOTED = 3
        SHUTTING_DOWN = 4

    NAME_FOR_STATE = [
        'CREATING',
        'SHUTDOWN',
        'BOOTING',
        'BOOTED',
        'SHUTTING DOWN',
    ]

    def __init__(self, name, udid, host, device_type):
        assert device_type.software_version

        self.name = name
        self.udid = udid
        self.device_type = device_type
        self._state = SimulatedDevice.DeviceState.SHUTTING_DOWN
        self._last_updated_state = time.time()

        self.executive = host.executive
        self.filesystem = host.filesystem
        self.platform = host.platform

        # Determine tear down behavior
        self.booted_by_script = False
        self.managed_by_script = False

    def state(self, force_update=False):
        # Don't allow state to get stale
        if not force_update and time.time() < self._last_updated_state + 1:
            return self._state

        device_plist = self.filesystem.expanduser(self.filesystem.join(SimulatedDeviceManager.simulator_device_path, self.udid, 'device.plist'))
        self._state = int(plistlib.readPlist(self.filesystem.open_binary_file_for_reading(device_plist))['state'])
        self._last_updated_state = time.time()
        return self._state

    def is_booted_or_booting(self, force_update=False):
        if self.state(force_update=force_update) == SimulatedDevice.DeviceState.BOOTING or self.state() == SimulatedDevice.DeviceState.BOOTED:
            return True
        return False

    def _shut_down(self, timeout=10.0):
        deadline = time.time() + timeout

        if self.state(force_update=True) != SimulatedDevice.DeviceState.SHUT_DOWN and self.state != SimulatedDevice.DeviceState.SHUTTING_DOWN:
            self.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'shutdown', self.udid])

        while self.state(force_update=True) != SimulatedDevice.DeviceState.SHUT_DOWN:
            time.sleep(.5)
            if time.time() > deadline:
                raise RuntimeError('Timed out while waiting for {} to shut down'.format(self.udid))

    def _delete(self, timeout=10.0):
        deadline = time.time() + timeout
        self._shut_down(deadline - time.time())
        _log.debug("Removing device '{}'".format(self.name))
        self.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'delete', self.udid])

        # This will (by design) fail if run more than once on the same SimulatedDevice
        SimulatedDeviceManager.AVAILABLE_DEVICES.remove(self)

    def _tear_down(self, timeout=10.0):
        deadline = time.time() + timeout
        if self.booted_by_script:
            self._shut_down(deadline - time.time())
        if self.managed_by_script:
            self._delete(deadline - time.time())

        # One of the INITIALIZED_DEVICES may be None, so we can't just use __eq__
        for device in SimulatedDeviceManager.INITIALIZED_DEVICES:
            if isinstance(device, Device) and device.platform_device == self:
                SimulatedDeviceManager.INITIALIZED_DEVICES.remove(device)

    def install_app(self, app_path, env=None):
        return not self.executive.run_command(['xcrun', 'simctl', 'install', self.udid, app_path], return_exit_code=True)

    # FIXME: Increase timeout for <rdar://problem/31331576>
    def launch_app(self, bundle_id, args, env=None, timeout=300):
        environment_to_use = {}
        SIMCTL_ENV_PREFIX = 'SIMCTL_CHILD_'
        for value in (env or {}):
            if not value.startswith(SIMCTL_ENV_PREFIX):
                environment_to_use[SIMCTL_ENV_PREFIX + value] = env[value]
            else:
                environment_to_use[value] = env[value]

        # FIXME: This is a workaround for <rdar://problem/30172453>.
        def _log_debug_error(error):
            _log.debug(error.message_with_output())

        output = None

        with Timeout(timeout, RuntimeError('Timed out waiting for process to open {} on {}'.format(bundle_id, self.udid))):
            while True:
                output = self.executive.run_command(
                    ['xcrun', 'simctl', 'launch', self.udid, bundle_id] + args,
                    env=environment_to_use,
                    error_handler=_log_debug_error,
                )
                match = re.match(r'(?P<bundle>[^:]+): (?P<pid>\d+)\n', output)
                # FIXME: We shouldn't need to check the PID <rdar://problem/31154075>.
                if match and self.executive.check_running_pid(int(match.group('pid'))):
                    break
                if match:
                    _log.debug('simctl launch reported pid {}, but this process is not running'.format(match.group('pid')))
                else:
                    _log.debug('simctl launch did not report a pid')

        if match.group('bundle') != bundle_id:
            raise RuntimeError('Failed to find process id for {}: {}'.format(bundle_id, output))
        _log.debug('Returning pid {} of launched process'.format(match.group('pid')))
        return int(match.group('pid'))

    def __eq__(self, other):
        return self.udid == other.udid

    def __ne__(self, other):
        return not self.__eq__(other)

    def __repr__(self):
        return '<Device "{name}": {udid}. State: {state}. Type: {type}>'.format(
            name=self.name,
            udid=self.udid,
            state=SimulatedDevice.NAME_FOR_STATE[self.state()],
            type=self.device_type)
