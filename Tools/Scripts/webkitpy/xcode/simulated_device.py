# Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
import re
import time

from webkitcorepy import Version, Timeout

from webkitpy.common.memoized import memoized
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system.systemhost import SystemHost
from webkitpy.port.device import Device
from webkitpy.xcode.device_type import DeviceType

try:
    from plistlib import load as readPlist
except ImportError:
    from plistlib import readPlist

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

    # FIXME: Switch this back to 6GB (or maybe lower?) once webkit.org/b/217392 is resolved.
    MEMORY_ESTIMATE_PER_SIMULATOR_INSTANCE = 8 * (1024 ** 3)  # 8GB a simulator.
    PROCESS_COUNT_ESTIMATE_PER_SIMULATOR_INSTANCE = 125

    # Testing on iMac Pros has indicated that more than 12 simulators, even if we seem to have enough resources for them,
    # results in diminishing returns.
    MAX_NUMBER_OF_SIMULATORS = 12

    xcrun = '/usr/bin/xcrun'
    simulator_device_path = '~/Library/Developer/CoreSimulator/Devices'
    simulator_bundle_id = 'com.apple.iphonesimulator'
    _device_identifier_to_name = {}
    _managing_simulator_app = False
    _last_updated_state = 0

    @staticmethod
    def _create_runtimes(runtimes):
        result = []
        for runtime in runtimes:
            if runtime.get('availability') != '(available)' and runtime.get('isAvailable') != 'YES' and runtime.get('isAvailable') != True:
                continue
            try:
                result.append(SimulatedDeviceManager.Runtime(runtime))
            except (ValueError, AssertionError):
                continue
        return result

    @staticmethod
    def _create_device_with_runtime(host, runtime, device_info):
        if device_info.get('availability') != '(available)' and device_info.get('isAvailable') != 'YES' and device_info.get('isAvailable') != True:
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
            device_type_string = SimulatedDeviceManager._device_identifier_to_name[readPlist(host.filesystem.open_binary_file_for_reading(device_plist))['deviceType']]
            device_type = DeviceType.from_string(device_type_string, runtime.version)
            device_type.software_variant = runtime.os_variant
        except (ValueError, AssertionError):
            return None

        result = Device(SimulatedDevice(
            name=device_info['name'],
            udid=device_info['udid'],
            host=host,
            device_type=device_type,
            build_version=runtime.build_version,
        ))
        SimulatedDeviceManager.AVAILABLE_DEVICES.append(result)
        return result

    @staticmethod
    def populate_available_devices(host=None):
        host = host or SystemHost.get_default()
        if not host.platform.is_mac():
            return

        try:
            simctl_json = json.loads(host.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'list', '--json'], decode_output=False, return_stderr=False))
        except (ValueError, ScriptError):
            _log.error('Failed to decode json output')
            return

        SimulatedDeviceManager._device_identifier_to_name = {device['identifier']: device['name'] for device in simctl_json['devicetypes']}
        SimulatedDeviceManager.AVAILABLE_RUNTIMES = SimulatedDeviceManager._create_runtimes(simctl_json['runtimes'])

        SimulatedDeviceManager._last_updated_state = time.time()
        for runtime in SimulatedDeviceManager.AVAILABLE_RUNTIMES:
            # Needed for <rdar://problem/47122965>
            devices = []
            if isinstance(simctl_json['devices'], list):
                for devices_for_runtime in simctl_json['devices']:
                    if devices_for_runtime['name'] == runtime.name:
                        devices = devices_for_runtime['devices']
                        break
            else:
                devices = simctl_json['devices'].get(runtime.name, None) or simctl_json['devices'].get(runtime.identifier, [])

            for device_json in devices:
                device = SimulatedDeviceManager._create_device_with_runtime(host, runtime, device_json)
                if not device:
                    continue

                # Update device state from simctl output.
                device.platform_device._state = SimulatedDevice.NAME_FOR_STATE.index(device_json['state'].upper())
        return

    @staticmethod
    def available_devices(host=None):
        host = host or SystemHost.get_default()
        if SimulatedDeviceManager.AVAILABLE_DEVICES == []:
            SimulatedDeviceManager.populate_available_devices(host)
        return SimulatedDeviceManager.AVAILABLE_DEVICES

    @staticmethod
    def device_by_filter(filter, host=None):
        host = host or SystemHost.get_default()
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
            if device and request.device_type == device.device_type:
                return device
        return None

    @staticmethod
    def _find_available_name(name_base):
        created_index = 0
        while True:
            name = u'{} {}'.format(name_base, created_index)
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
        # Search for an available runtime that best matches the provided device type
        candidate = None
        for runtime in SimulatedDeviceManager.AVAILABLE_RUNTIMES:
            if runtime.os_variant != device_type.software_variant:
                continue
            if device_type.software_version and runtime.version.major != device_type.software_version.major:
                continue
            if device_type.software_version and runtime.version < device_type.software_version:
                continue
            if not candidate or runtime.version < candidate.version:
                candidate = runtime
        return candidate

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
                if device.device_type == full_device_type:
                    full_device_type.hardware_family = device.device_type.hardware_family
                    break

        if full_device_type.hardware_type is None:
            # Again, we use the existing devices to determine a legal hardware type
            for device in SimulatedDeviceManager.AVAILABLE_DEVICES:
                if device.device_type == full_device_type:
                    full_device_type.hardware_type = device.device_type.hardware_type
                    break

        if not full_device_type.hardware_family or not full_device_type.hardware_type:
            # If we couldn't define a device with existing devices, pick the newest matching device type
            for _, type_name in reversed(SimulatedDeviceManager._device_identifier_to_name.items()):
                candidate = DeviceType.from_string(type_name)
                if candidate == full_device_type:
                    full_device_type.hardware_family = candidate.hardware_family
                    full_device_type.hardware_type = candidate.hardware_type
                    break

        full_device_type.check_consistency()
        return full_device_type

    @staticmethod
    def _get_device_identifier_for_type(device_type):
        type_name_for_request = u'{}{}'.format(
            device_type.hardware_family.lower(),
            ' {}'.format(device_type.standardized_hardware_type.lower()) if device_type.standardized_hardware_type else '',
        )
        for type_id, type_name in SimulatedDeviceManager._device_identifier_to_name.items():
            if type_name.lower() == type_name_for_request:
                return type_id
            if type_name.lower().endswith(DeviceType.FIRST_GENERATION) and type_name.lower()[:-len(DeviceType.FIRST_GENERATION)] == type_name_for_request:
                return type_id
        return None

    @staticmethod
    def _create_or_find_device_for_request(request, host=None, name_base='Managed'):
        assert isinstance(request, DeviceRequest)
        host = host or SystemHost.get_default()

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

        _log.debug(u"Creating device '{}', of type {}".format(name, device_type))
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
            if request.device_type == device.device_type:
                _log.debug(u"The request for '{}' matched {} exactly".format(request.device_type, device))
                return request

        # Contained-in match.
        for request in requests:
            if not request.use_booted_simulator:
                continue
            if device.device_type in request.device_type:
                _log.debug(u"The request for '{}' fuzzy-matched {}".format(request.device_type, device))
                return request

        # DeviceRequests are compared by reference
        requests_copy = [request for request in requests]

        # Check for an incomplete match
        # This is usually used when we don't want to take the time to start a simulator and would
        # rather use the one the user has already started, even if it isn't quite what we're looking for.
        for request in requests_copy:
            if not request.use_booted_simulator or not request.allow_incomplete_match:
                continue
            if request.device_type.software_variant == device.device_type.software_variant:
                _log.warn(u"The request for '{}' incomplete-matched {}".format(request.device_type, device))
                _log.warn(u"This may cause unexpected behavior in code that expected the device type {}".format(request.device_type))
                return request
        return None

    @staticmethod
    def _wait_until_device_in_state(device, state, deadline):
        while device.platform_device.state(force_update=True) != state:
            _log.debug(u'Waiting on {} to enter state {}...'.format(device, SimulatedDevice.NAME_FOR_STATE[state]))
            time.sleep(1)
            if time.time() > deadline:
                raise RuntimeError('Timed out while waiting for all devices to boot')

    @staticmethod
    def _wait_until_device_is_usable(device, deadline):
        _log.debug(u'Waiting until {} is usable'.format(device))
        while not device.platform_device.is_usable(force_update=True):
            if time.time() > deadline:
                raise RuntimeError(u'Timed out while waiting for {} to become usable'.format(device))
            time.sleep(1)

    @staticmethod
    def _boot_device(device, host=None):
        host = host or SystemHost.get_default()
        _log.debug(u"Booting device '{}'".format(device.udid))
        device.platform_device.booted_by_script = True
        host.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'boot', device.udid])
        SimulatedDeviceManager.INITIALIZED_DEVICES.append(device)
        # FIXME: Remove this delay once rdar://77234240 is resolved.
        time.sleep(15)

    @staticmethod
    def device_count_for_type(device_type, host=None, use_booted_simulator=True, **kwargs):
        host = host or SystemHost.get_default()
        if not host.platform.is_mac():
            return 0

        if SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.is_booted_or_booting(), host=host) and use_booted_simulator:
            filter = lambda device: device.platform_device.is_booted_or_booting() and device.device_type in device_type
            return len(SimulatedDeviceManager.device_by_filter(filter, host=host))

        for name in SimulatedDeviceManager._device_identifier_to_name.values():
            if DeviceType.from_string(name) in device_type:
                return SimulatedDeviceManager.max_supported_simulators(host)
        return 0

    @staticmethod
    def initialize_devices(requests, host=None, name_base='Managed', simulator_ui=True, timeout=SIMULATOR_BOOT_TIMEOUT, **kwargs):
        host = host or SystemHost.get_default()
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
            _log.debug(u'Attached to running simulator {}'.format(device))
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
                if request.device_type != device.device_type and not request.allow_incomplete_match:
                    continue
                if request.device_type.software_variant != device.device_type.software_variant:
                    continue
                requests.remove(request)

        for request in requests:
            device = SimulatedDeviceManager._create_or_find_device_for_request(request, host, name_base)
            assert device is not None

            SimulatedDeviceManager._boot_device(device, host)

        if simulator_ui and host.executive.run_command(['killall', '-0', 'Simulator.app'], return_exit_code=True) != 0:
            SimulatedDeviceManager._managing_simulator_app = not host.executive.run_command(['open', '-g', '-b', SimulatedDeviceManager.simulator_bundle_id, '--args', '-PasteboardAutomaticSync', '0'], return_exit_code=True)

        deadline = time.time() + timeout
        for device in SimulatedDeviceManager.INITIALIZED_DEVICES:
            SimulatedDeviceManager._wait_until_device_is_usable(device, deadline)

        return SimulatedDeviceManager.INITIALIZED_DEVICES

    @staticmethod
    @memoized
    def max_supported_simulators(host=None):
        host = host or SystemHost.get_default()
        if not host.platform.is_mac():
            return 0

        try:
            system_process_count_limit = int(host.executive.run_command(['/usr/bin/ulimit', '-u']).strip())
            current_process_count = len(host.executive.run_command(['/bin/ps', 'aux']).strip().split('\n'))
            _log.debug(u'Process limit: {}, current #processes: {}'.format(system_process_count_limit, current_process_count))
        except (ValueError, ScriptError):
            return 0

        max_supported_simulators_for_hardware = min(
            host.executive.cpu_count() // 2,
            host.platform.total_bytes_memory() // SimulatedDeviceManager.MEMORY_ESTIMATE_PER_SIMULATOR_INSTANCE,
            SimulatedDeviceManager.MAX_NUMBER_OF_SIMULATORS,
        )
        max_supported_simulators_locally = (system_process_count_limit - current_process_count) // SimulatedDeviceManager.PROCESS_COUNT_ESTIMATE_PER_SIMULATOR_INSTANCE

        if (max_supported_simulators_locally < max_supported_simulators_for_hardware):
            _log.warn(u'This machine could support {} simulators, but is only configured for {}.'.format(max_supported_simulators_for_hardware, max_supported_simulators_locally))
            _log.warn('Please see <https://trac.webkit.org/wiki/IncreasingKernelLimits>.')

        if max_supported_simulators_locally == 0:
            max_supported_simulators_locally = 1

        return min(max_supported_simulators_locally, max_supported_simulators_for_hardware)

    @staticmethod
    def swap(device, request, host=None, name_base='Managed', timeout=SIMULATOR_BOOT_TIMEOUT):
        host = host or SystemHost.get_default()
        if SimulatedDeviceManager.INITIALIZED_DEVICES is None:
            raise RuntimeError('Cannot swap when there are no initialized devices')
        if device not in SimulatedDeviceManager.INITIALIZED_DEVICES:
            raise RuntimeError(u'{} is not initialized, cannot swap it'.format(device))

        index = SimulatedDeviceManager.INITIALIZED_DEVICES.index(device)
        SimulatedDeviceManager.INITIALIZED_DEVICES[index] = None
        device.platform_device._tear_down()

        device = SimulatedDeviceManager._create_or_find_device_for_request(request, host, name_base)
        assert device

        if not device.platform_device.is_booted_or_booting(force_update=True):
            device.platform_device.booted_by_script = True
            _log.debug(u"Booting device '{}'".format(device.udid))
            host.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'boot', device.udid])
        SimulatedDeviceManager.INITIALIZED_DEVICES[index] = device

        deadline = time.time() + timeout
        SimulatedDeviceManager._wait_until_device_is_usable(device, max(0, deadline - time.time()))

    @staticmethod
    def tear_down(host=None, timeout=SIMULATOR_BOOT_TIMEOUT):
        host = host or SystemHost.get_default()
        if SimulatedDeviceManager._managing_simulator_app:
            host.executive.run_command(['killall', '-9', 'Simulator.app'], return_exit_code=True)
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

        # If we were managing the simulator, there are some cache files we need to remove
        for directory in host.filesystem.glob('/tmp/com.apple.CoreSimulator.SimDevice.*'):
            host.filesystem.rmtree(directory)
        core_simulator_directory = host.filesystem.expanduser(host.filesystem.join('~', 'Library', 'Developer', 'CoreSimulator'))
        host.filesystem.rmtree(host.filesystem.join(core_simulator_directory, 'Caches'))
        host.filesystem.rmtree(host.filesystem.join(core_simulator_directory, 'Temp'))


class SimulatedDevice(object):
    class DeviceState:
        CREATING = 0
        SHUT_DOWN = 1
        BOOTING = 2
        BOOTED = 3
        SHUTTING_DOWN = 4

    NUM_INSTALL_RETRIES = 5
    NAME_FOR_STATE = [
        'CREATING',
        'SHUTDOWN',
        'BOOTING',
        'BOOTED',
        'SHUTTING DOWN',
    ]

    UI_MANAGER_SERVICE = {
        'iOS': 'com.apple.SpringBoard',
        'watchOS': 'com.apple.Carousel',
    }

    def __init__(self, name, udid, host, device_type, build_version):
        assert device_type.software_version

        self.name = name
        self.udid = udid
        self.device_type = device_type
        self.build_version = build_version
        self._state = SimulatedDevice.DeviceState.SHUTTING_DOWN

        self.executive = host.executive
        self.filesystem = host.filesystem
        self.platform = host.platform

        # Determine tear down behavior
        self.booted_by_script = False
        self.managed_by_script = False

    def state(self, force_update=False):
        # Don't allow state to get stale
        if not force_update and time.time() < SimulatedDeviceManager._last_updated_state + 10:
            return self._state

        try:
            SimulatedDeviceManager._last_updated_state = time.time()
            simctl_json = json.loads(self.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'list', '--json'], decode_output=False, return_stderr=False))
            state_map = {}
            for devices in simctl_json['devices'].values():
                for device in devices:
                    if device.get('udid') and device.get('state'):
                        state_map[device.get('udid')] = device.get('state')
            for device in SimulatedDeviceManager.AVAILABLE_DEVICES:
                device.platform_device._state = SimulatedDevice.NAME_FOR_STATE.index(state_map.get(device.platform_device.udid, 'SHUTDOWN').upper())
        except (ValueError, ScriptError):
            _log.error("Failed to decode 'simctl list' json output")
            self._state = SimulatedDevice.DeviceState.SHUTTING_DOWN

        return self._state

    def is_booted_or_booting(self, force_update=False):
        if self.state(force_update=force_update) == SimulatedDevice.DeviceState.BOOTING or self.state() == SimulatedDevice.DeviceState.BOOTED:
            return True
        return False

    def is_usable(self, force_update=False):
        if self.state(force_update=force_update) != SimulatedDevice.DeviceState.BOOTED:
            return False

        service = self.UI_MANAGER_SERVICE.get(self.device_type.software_variant)
        if not service:
            _log.debug(u'{} has no service to check if the device is usable'.format(self.device_type.software_variant))
            return True

        exit_code = self.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'spawn', self.udid, 'launchctl', 'list', service], return_exit_code=True)
        if exit_code == 0:
            return True
        return False

    def _shut_down(self, timeout=30.0):
        deadline = time.time() + timeout

        # Either shutdown is successful, or the device was already shutdown when we attempted to shut it down.
        exit_code = self.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'shutdown', self.udid], return_exit_code=True)
        if exit_code != 0 and self.state() != SimulatedDevice.DeviceState.SHUT_DOWN:
            raise RuntimeError(u'Failed to shutdown {} with exit code {}'.format(self.udid, exit_code))

        while self.state(force_update=True) != SimulatedDevice.DeviceState.SHUT_DOWN:
            time.sleep(.5)
            if time.time() > deadline:
                raise RuntimeError(u'Timed out while waiting for {} to shut down'.format(self.udid))

    def _delete(self, timeout=10.0):
        deadline = time.time() + timeout
        self._shut_down(deadline - time.time())
        _log.debug(u"Removing device '{}'".format(self.name))
        if self.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'delete', self.udid], return_exit_code=True):
            _log.error(u"Failed to remove '{},' error is not fatal, continuing".format(self.name))

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
        # Even after carousel is running, it takes a few seconds for watchOS to allow installs.
        for i in range(self.NUM_INSTALL_RETRIES):
            exit_code = self.executive.run_command(['xcrun', 'simctl', 'install', self.udid, app_path], return_exit_code=True)
            if exit_code == 0:
                return True

            # Return code 204 indicates that the device is booting, a retry may be successful.
            if exit_code == 204:
                time.sleep(5)
                continue
            return False
        return False

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

        with Timeout(timeout, handler=RuntimeError(u'Timed out waiting for process to open {} on {}'.format(bundle_id, self.udid)), patch=False):
            while True:
                output = self.executive.run_command(
                    ['xcrun', 'simctl', 'launch', self.udid, bundle_id] + args,
                    env=environment_to_use,
                    error_handler=_log_debug_error,
                    return_stderr=False,
                )
                match = re.match(r'(?P<bundle>[^:]+): (?P<pid>\d+)\n', output)
                # FIXME: We shouldn't need to check the PID <rdar://problem/31154075>.
                if match and self.executive.check_running_pid(int(match.group('pid'))):
                    break
                if match:
                    _log.debug(u'simctl launch reported pid {}, but this process is not running'.format(match.group('pid')))
                else:
                    _log.debug('simctl launch did not report a pid')

        if match.group('bundle') != bundle_id:
            raise RuntimeError(u'Failed to find process id for {}: {}'.format(bundle_id, output))
        _log.debug(u'Returning pid {} of launched process'.format(match.group('pid')))
        return int(match.group('pid'))

    def __eq__(self, other):
        return self.udid == other.udid

    def __ne__(self, other):
        return not self.__eq__(other)

    def __repr__(self):
        return u'<Device "{name}": {udid}. State: {state}. Type: {type}>'.format(
            name=self.name,
            udid=self.udid,
            state=SimulatedDevice.NAME_FOR_STATE[self.state()],
            type=self.device_type)
