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
import multiprocessing
import os
import plistlib
import queue
import re
import subprocess
import time

from dataclasses import dataclass

from webkitcorepy import Version, Timeout, string_utils
from webkitcorepy.subprocess_utils import run_all, wait_until_exit

from webkitpy.common.memoized import memoized
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system.systemhost import SystemHost
from webkitpy.port.config import apple_additions
from webkitpy.port.device import Device
from webkitpy.port.device_provisioning import DeviceProvisioning
from webkitpy.xcode.device_type import DeviceType
from webkitpy.xcode.simulator_daemons import disabled_launchd_jobs

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


@dataclass
class _UsabilityCheck:
    """One device's usability check: launch its UI service, then terminate it."""

    device: object
    service: str

    @property
    def launch_command(self):
        return [SimulatedDeviceManager.xcrun, 'simctl', 'launch', self.device.udid, self.service]

    @property
    def terminate_command(self):
        return [SimulatedDeviceManager.xcrun, 'simctl', 'terminate', self.device.udid, self.service]


class SimulatedDeviceManager(DeviceProvisioning):
    class Runtime(object):
        def __init__(self, runtime_dict):
            self.root = runtime_dict['runtimeRoot']
            self.build_version = runtime_dict['buildversion']
            self.os_variant = runtime_dict['name'].split(' ')[0]
            self.version = Version.from_string(runtime_dict['version'])
            self.identifier = runtime_dict['identifier']
            self.name = runtime_dict['name']

    AVAILABLE_RUNTIMES = []
    AVAILABLE_DEVICES = []
    INITIALIZED_DEVICES = None

    DEVICE_QUEUE = None
    PROVISIONING_DONE = None

    SIMULATOR_BOOT_TIMEOUT = 600

    MEMORY_ESTIMATE_PER_SIMULATOR_INSTANCE = 2 * (1024 ** 3)  # 2GB a simulator.
    CPU_ESTIMATE_PER_SIMULATOR_INSTANCE = 1
    PROCESS_COUNT_ESTIMATE_PER_SIMULATOR_INSTANCE = 125

    # Testing on iMac Pros has indicated that more than 12 simulators, even if we seem to have enough resources for them,
    # results in diminishing returns.
    MAX_NUMBER_OF_SIMULATORS = 12

    USABILITY_CHECK_TIMEOUT = 120
    ENVIRONMENT_EXTRAS_TIMEOUT = 600
    INSTALL_TIMEOUT = 600
    SHUTDOWN_TIMEOUT = 30

    xcrun = '/usr/bin/xcrun'
    simulator_device_path = '~/Library/Developer/CoreSimulator/Devices'
    simulator_bundle_id = 'com.apple.iphonesimulator'
    launchd_state_path = '/private/var/tmp/com.apple.CoreSimulator.SimDevice.{}'
    _device_identifier_to_name = {}
    _managing_simulator_app = False
    _last_updated_state = 0
    _boot_waits_by_udid = {}
    _provisioning_deadline = 0
    _slots_per_device = 1

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
            runtime_root=runtime.root,
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
    def available_devices(host=None, udids=None):
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
    def _find_existing_uninitialized_device_for_request(request):
        # type: (DeviceRequest) -> DeviceRequest | None
        '''Finds an existing, eligible, and uninitialized device that satisfies the passed request.

        Arguments:
            request (`DeviceRequest`): The details of the device request.
        '''

        if not request.use_existing_simulator:
            return None
        for device in SimulatedDeviceManager.AVAILABLE_DEVICES:
            # One of the INITIALIZED_DEVICES may be None, so we can't just use __eq__
            for initialized_device in SimulatedDeviceManager.INITIALIZED_DEVICES:
                if isinstance(initialized_device, Device) and device == initialized_device:
                    device = None
                    break
            if device and request.device_type == device.device_type and not device.platform_device.is_booted_or_booting():
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
            if DeviceType.standardize_hardware_type(type_name).lower() == type_name_for_request:
                return type_id
        return None

    @classmethod
    def _create_or_find_device_for_request(cls, request, host=None, name_base='Managed'):
        assert isinstance(request, DeviceRequest)
        host = host or SystemHost.get_default()

        device = cls._find_existing_uninitialized_device_for_request(request)
        if device:
            return device

        name = cls._find_available_name(name_base)
        device_type = cls._disambiguate_device_type(request.device_type)
        runtime = cls.get_runtime_for_device_type(device_type)
        device_identifier = cls._get_device_identifier_for_type(device_type)

        assert runtime is not None
        assert device_identifier is not None

        for device in cls.available_devices(host):
            if device.platform_device.name == name and device.platform_device.device_type == device_type:
                device.platform_device._delete()
                break

        _log.debug(u"Creating device '{}', of type {}".format(name, device_type))
        host.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'create', name, device_identifier, runtime.identifier])

        # We just added a device, so our list of _available_devices needs to be re-synced.
        cls.populate_available_devices(host)
        for device in cls.available_devices(host):
            if device.platform_device.name == name and device.platform_device.device_type == device_type:
                device.platform_device.managed_by_script = True
                return device
        return None

    @staticmethod
    def _does_fulfill_request(device, requests, allow_shutdown_devices=False):
        if not allow_shutdown_devices and not device.platform_device.is_booted_or_booting():
            return None

        # Exact match.
        for request in requests:
            if not request.use_booted_simulator and not allow_shutdown_devices:
                continue
            if request.device_type == device.device_type:
                _log.debug(u"The request for '{}' matched {} exactly".format(request.device_type, device))
                return request

        # Contained-in match.
        for request in requests:
            if not request.use_booted_simulator and not allow_shutdown_devices:
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
            if (not request.use_booted_simulator and not allow_shutdown_devices) or not request.allow_incomplete_match:
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
    def start_waiting_for_boot(device):
        """Starts a wait for a device to finish booting, to be polled rather than blocked on.

        `simctl bootstatus` blocks in a process of its own, so polling it costs nothing and, unlike the usability
        check, nothing is launched on the device to find out."""
        executive = device.platform_device.executive
        return executive.popen(
            [SimulatedDeviceManager.xcrun, 'simctl', 'bootstatus', device.udid, '-b'],
            stdout=executive.PIPE, stderr=executive.PIPE,
        )

    @staticmethod
    def _configure_launchd_before_booting(device, host):
        """Puts the device's launchd configuration in place before the simulator starts.

        launchd_sim reads its configuration as it starts, so what is written here applies to the boot itself rather
        than having to be applied to a running device afterwards. The directory is named after the UDID, which is
        known before the device boots.

        On a key this configuration sets, its value wins; other pre-existing keys the runtime shipped are preserved."""
        configuration = getattr(device.platform_device, 'launchd_configuration', None)
        if not configuration:
            return

        directory = SimulatedDeviceManager.launchd_state_path.format(device.udid)
        for name, entries in configuration.items():
            path = host.filesystem.join(directory, name)
            contents = {}
            if host.filesystem.exists(path):
                try:
                    contents = plistlib.loads(host.filesystem.read_binary_file(path))
                except Exception as error:
                    _log.warning(u'Could not read {}, replacing it: {}'.format(path, error))
            contents.update(entries)
            try:
                host.filesystem.maybe_make_directory(directory)
                host.filesystem.write_binary_file(path, plistlib.dumps(contents))
                _log.debug(u'Wrote {} for {} before booting'.format(name, device.udid))
            except Exception as error:
                # Not fatal: the device still boots, just without this configuration.
                _log.warning(u'Could not write {} for {} before booting: {}'.format(name, device.udid, error))

    @staticmethod
    def _wait_until_devices_are_usable(devices, deadline, on_usable=None):
        """Waits for every device to answer its usability check, testing them as a group.

        `on_usable` is handed the devices which have just answered, so a caller can act on each one as soon as it is
        ready rather than after the slowest has caught up."""
        waiting_on = list(devices)
        for device in waiting_on:
            _log.debug(u'Waiting until {} is usable'.format(device))

        while waiting_on:
            still_waiting = SimulatedDeviceManager._devices_not_yet_usable(waiting_on, deadline)
            if on_usable:
                on_usable([device for device in waiting_on if device not in still_waiting])
            waiting_on = still_waiting
            if not waiting_on:
                return
            if time.monotonic() > deadline:
                raise RuntimeError(u'Timed out while waiting for {} to become usable'.format(
                    ', '.join(str(device) for device in waiting_on)))
            time.sleep(1)

    @staticmethod
    def _devices_not_yet_usable(devices, deadline):
        """Runs one round of the usability check against every device at once, returning those not ready yet."""
        checks = []
        not_ready = []
        for device in devices:
            platform_device = device.platform_device
            if platform_device.state(force_update=True) != SimulatedDevice.DeviceState.BOOTED:
                not_ready.append(device)
                continue
            service = platform_device.UI_MANAGER_SERVICE.get(platform_device.device_type.software_variant)
            if not service:
                _log.debug(u'{} has no service to check if the device is usable'.format(platform_device.device_type.software_variant))
                continue
            checks.append(_UsabilityCheck(device=device, service=service))
        if not checks:
            return not_ready

        def budget_for(count):
            return max(1, min(SimulatedDeviceManager.USABILITY_CHECK_TIMEOUT * count, deadline - time.monotonic()))

        # Every simulator in a run is created against the same host, so they share one executive to launch through.
        popen = checks[0].device.platform_device.executive.popen

        launched = []
        for check, (returncode, _stdout, _stderr) in zip(checks, run_all(
                [check.launch_command for check in checks], timeout=budget_for(len(checks)), popen=popen)):
            if returncode:
                not_ready.append(check.device)
            else:
                launched.append(check)

        if launched:
            time.sleep(.7)
            for check, (returncode, _stdout, _stderr) in zip(launched, run_all(
                    [check.terminate_command for check in launched], timeout=budget_for(len(launched)), popen=popen)):
                if returncode:
                    not_ready.append(check.device)
        return not_ready

    @staticmethod
    def _boot_devices(devices, host=None):
        """Boots simulators together rather than one after another.

        Booting one after another pays the rdar://77234240 sleep once a simulator, where booting together pays it once.
        A simulator that will not boot is dropped from INITIALIZED_DEVICES rather than raising, so every device gets its
        turn."""
        host = host or SystemHost.get_default()
        if not devices:
            return

        for device in devices:
            # FIXME: remove this workaround after rdar://129789675 has been resolved.
            host.executive.run_command(['sh', '-c', "mkdir -m 700 -p " + "~/Library/Developer/CoreSimulator/Devices/" + device.udid + "/data/private/var/db"])
            SimulatedDeviceManager._configure_launchd_before_booting(device, host)

        processes = []
        for device in devices:
            _log.debug(u"Booting device '{}'".format(device.udid))
            device.platform_device.booted_by_script = True
            processes.append(host.executive.popen(
                [SimulatedDeviceManager.xcrun, 'simctl', 'boot', device.udid],
                stdout=host.executive.PIPE, stderr=host.executive.PIPE,
            ))

        deadline = time.monotonic() + SimulatedDeviceManager.SIMULATOR_BOOT_TIMEOUT
        for device, process in zip(devices, processes):
            returncode, _, stderr = wait_until_exit(process, timeout=max(1, deadline - time.monotonic()))
            if returncode:
                device.platform_device.booted_by_script = False
                _log.error(u'Failed to boot {}: {}'.format(
                    device.udid,
                    string_utils.decode(stderr, target_type=str).strip() if stderr else 'exit code {}'.format(returncode)))
                if device in SimulatedDeviceManager.INITIALIZED_DEVICES:
                    SimulatedDeviceManager.INITIALIZED_DEVICES.remove(device)
                SimulatedDeviceManager._shut_down_device(device, host)
                continue
            if device not in SimulatedDeviceManager.INITIALIZED_DEVICES:
                SimulatedDeviceManager.INITIALIZED_DEVICES.append(device)

        # FIXME: Remove this delay once rdar://77234240 is resolved.
        time.sleep(15)

    @staticmethod
    def _set_up_environment_extras(devices):
        """Runs every device's environment setup at once.

        The budget is generous because killing this partway leaves a device half configured."""
        commands = []
        executive = None
        for device in devices:
            platform_device = device.platform_device
            for command in getattr(platform_device, 'environment_extras', None) or []:
                commands.append(command)
                executive = platform_device.executive
        if not commands:
            return

        for returncode, _stdout, _stderr in run_all(
                commands, timeout=SimulatedDeviceManager.ENVIRONMENT_EXTRAS_TIMEOUT, popen=executive.popen):
            if returncode:
                _log.warning('Environment setup command failed with exit code {}'.format(returncode))

    @staticmethod
    def _shut_down_device(device, host):
        wait_until_exit(host.executive.popen(
            [SimulatedDeviceManager.xcrun, 'simctl', 'shutdown', device.udid],
            stdout=host.executive.PIPE, stderr=host.executive.PIPE,
        ), timeout=SimulatedDeviceManager.SHUTDOWN_TIMEOUT)

    @classmethod
    def begin_provisioning(cls, timeout=SIMULATOR_BOOT_TIMEOUT, slots_per_device=1):
        """Starts waiting on every initialized device, so each can be handed over as it becomes ready.

        A device is offered slots_per_device times, so that many workers share it. Offering as each device becomes
        ready keeps testing starting on the first one rather than waiting for the slowest."""
        cls._slots_per_device = max(1, slots_per_device)
        cls.DEVICE_QUEUE = multiprocessing.Queue()
        cls.PROVISIONING_DONE = multiprocessing.Event()
        cls.PENDING_DEVICES = list(cls.INITIALIZED_DEVICES or [])
        cls.READY_DEVICES = []
        cls._boot_waits_by_udid = {}
        cls._provisioning_deadline = time.monotonic() + timeout
        cls.advance_provisioning()

    @classmethod
    def advance_provisioning(cls):
        """Hands over any device that has finished booting. Never blocks, so it is safe to call while dispatching."""
        for device in list(cls.PENDING_DEVICES):
            waiting = cls._boot_waits_by_udid.get(device.udid)
            if waiting is None:
                cls._boot_waits_by_udid[device.udid] = cls.start_waiting_for_boot(device)
                continue
            if waiting.poll() is None:
                continue

            cls.PENDING_DEVICES.remove(device)
            del cls._boot_waits_by_udid[device.udid]
            if waiting.returncode:
                _log.error(u'{} never finished booting, continuing without it'.format(device))
                continue
            _log.debug(u'{} is ready to test on'.format(device))
            cls.READY_DEVICES.append(device)
            for slot in range(cls._slots_per_device):
                cls.DEVICE_QUEUE.put((device, slot == 0))

        if cls.PENDING_DEVICES and time.monotonic() > cls._provisioning_deadline:
            _log.error(u'Gave up waiting for {} to become ready; continuing without them'.format(
                ', '.join(str(device) for device in cls.PENDING_DEVICES)))
            cls.PENDING_DEVICES = []

        if not cls.PENDING_DEVICES and cls.PROVISIONING_DONE:
            cls.PROVISIONING_DONE.set()

    @classmethod
    def wait_for_first_ready_device(cls, timeout=None):
        """Blocks until one device can be handed to a worker, and returns whether any can.

        Polled rather than waited on: `Timeout` is built on SIGALRM and only works on the main thread, so the boot
        waits cannot be joined from anywhere else."""
        deadline = time.monotonic() + (timeout if timeout else cls.SIMULATOR_BOOT_TIMEOUT)
        while cls.expects_more_devices() and not cls.READY_DEVICES:
            if time.monotonic() > deadline:
                break
            time.sleep(0.5)
            cls.advance_provisioning()
        return bool(cls.READY_DEVICES)

    @classmethod
    def offer_ready_devices(cls, slots_per_device=None):
        """Puts every ready device back on the queue, for a pool of workers which has not claimed them yet."""
        if cls.DEVICE_QUEUE is None:
            return None
        while True:
            try:
                cls.DEVICE_QUEUE.get(block=False)
            except queue.Empty:
                break
        if slots_per_device is not None:
            cls._slots_per_device = max(1, slots_per_device)
        for device in cls.READY_DEVICES:
            for slot in range(cls._slots_per_device):
                cls.DEVICE_QUEUE.put((device, slot == 0))
        return cls.DEVICE_QUEUE

    @classmethod
    def claim_device(cls, wait_timeout):
        """Waits for a device to hand over, returning it and whether this worker is the one to set it up.

        A device can take many minutes to come up, and a worker which gives up early hands its shard back, costing
        that shard its place in the biggest-first order. So each time the wait runs out this asks whether anything is
        still coming, and gives up only once nothing is. Where several workers share a device, only the first is told
        to set it up: installing the driver and waiting for the device to be usable belong to the device, not to
        each worker."""
        while True:
            try:
                claimed, first = cls.DEVICE_QUEUE.get(timeout=wait_timeout)
            except queue.Empty:
                if not cls.expects_more_devices():
                    return None, False
                if not any(device.platform_device.is_booted_or_booting(force_update=True)
                           for device in cls.PENDING_DEVICES):
                    _log.warning('No device is still coming up; giving this shard back')
                    return None, False
                continue
            return (cls.device_for(claimed) or claimed), first

    @classmethod
    def device_for(cls, other):
        """This process's own copy of a device.

        A device handed over through a queue is a copy carrying the state it had when it was sent, and refreshing
        state only reaches the copies held here."""
        for known in cls.INITIALIZED_DEVICES or []:
            if known and known.udid == other.udid:
                return known
        return None

    @classmethod
    def expects_more_devices(cls):
        return bool(cls.PROVISIONING_DONE) and not cls.PROVISIONING_DONE.is_set()

    @classmethod
    def has_ready_device(cls):
        return any(device.platform_device.is_booted_or_booting(force_update=True) for device in cls.READY_DEVICES)

    @classmethod
    def end_provisioning(cls):
        """Forgets a run's devices. A device torn down by one run is not ready for the next."""
        if cls.PROVISIONING_DONE:
            cls.PROVISIONING_DONE.set()
        if cls.DEVICE_QUEUE is not None:
            # Dropping the queue with devices still in it leaves its feeder thread writing to a pipe nobody reads.
            while True:
                try:
                    cls.DEVICE_QUEUE.get_nowait()
                except (queue.Empty, OSError, ValueError):
                    break
            cls.DEVICE_QUEUE.cancel_join_thread()
        cls.READY_DEVICES = []
        cls.PENDING_DEVICES = []
        cls.DEVICE_QUEUE = None
        cls.PROVISIONING_DONE = None
        cls._boot_waits_by_udid = {}

    @classmethod
    def provisioning_state(cls):
        """What a worker needs in order to claim a device."""
        return dict(device_queue=cls.DEVICE_QUEUE, provisioning_done=cls.PROVISIONING_DONE,
                    pending_devices=cls.PENDING_DEVICES, ready_devices=cls.READY_DEVICES)

    @classmethod
    def adopt_provisioning_state(cls, state):
        cls.DEVICE_QUEUE = state.get('device_queue')
        cls.PROVISIONING_DONE = state.get('provisioning_done')
        cls.PENDING_DEVICES = state.get('pending_devices') or []
        cls.READY_DEVICES = state.get('ready_devices') or []

    @staticmethod
    def device_count_for_type(device_type, host=None, use_booted_simulator=True, **kwargs):
        host = host or SystemHost.get_default()
        if not host.platform.is_mac():
            return 0

        if SimulatedDeviceManager.device_by_filter(lambda device: device.platform_device.is_booted_or_booting(), host=host) and use_booted_simulator:
            def is_booted_device_of_type(device):
                return device.platform_device.is_booted_or_booting() and device.device_type in device_type

            return len(SimulatedDeviceManager.device_by_filter(is_booted_device_of_type, host=host))

        for name in SimulatedDeviceManager._device_identifier_to_name.values():
            if DeviceType.from_string(name) in device_type:
                return SimulatedDeviceManager.max_supported_simulators(host)
        return 0

    @staticmethod
    def _validate_running_device_against_requests(requests: list[Device], device: Device):
        '''Reduce device requests based on request options.'''
        for request in requests:
            if not request.merge_requests:
                # If multiple devices are requested but only 1 is running, all requests will be fulfilled with the 1 running device.
                continue
            if not request.use_booted_simulator:
                continue
            if request.device_type != device.device_type and not request.allow_incomplete_match:
                continue
            if request.device_type.software_variant != device.device_type.software_variant:
                continue
            requests.remove(request)
        return requests

    @classmethod
    def initialize_devices(cls, requests, host=None, timeout=SIMULATOR_BOOT_TIMEOUT, **kwargs):
        devices = cls.create_devices(requests, host=host, **kwargs)
        cls.block_on_ready(devices, timeout=timeout)
        return devices

    @classmethod
    def block_on_ready(cls, devices=None, timeout=SIMULATOR_BOOT_TIMEOUT):
        """Waits until every given device can run tests, defaulting to all of them.

        They are polled as a group so they become usable alongside each other, and each one's environment is set up as
        soon as it is usable rather than after the slowest has caught up."""
        devices = list(cls.INITIALIZED_DEVICES or []) if devices is None else list(devices)
        if not devices:
            return

        cls._wait_until_devices_are_usable(devices, time.monotonic() + timeout,
                                           on_usable=cls._set_up_environment_extras)

    @classmethod
    def create_devices(cls, requests, host=None, name_base='Managed', simulator_ui=True, keep_alive=False, udids=None, **kwargs):
        """Finds or creates the devices a request needs and boots them.

        Booting is not the same as being ready to run tests, which block_on_ready waits for."""
        host = host or SystemHost.get_default()
        if SimulatedDeviceManager.INITIALIZED_DEVICES is not None:
            return SimulatedDeviceManager.INITIALIZED_DEVICES

        if not host.platform.is_mac():
            return None

        SimulatedDeviceManager.INITIALIZED_DEVICES = []

        if not keep_alive:
            atexit.register(SimulatedDeviceManager.tear_down)

        # Convert to iterable type
        if not hasattr(requests, '__iter__'):
            requests = [requests]

        # Parse user-specified UDIDs
        udids = udids or []
        if type(udids) is str:
            udids = udids.split(',')

        # Check for running simulators.
        deferred_booted_devices = []
        for device in cls.available_devices(host):
            matched_request = cls._does_fulfill_request(device, requests, True)
            if matched_request is None:
                continue

            device_is_booted = device.platform_device.is_booted_or_booting()
            if device.platform_device.udid not in udids:
                if device_is_booted:  # Defer booted devices that weren't requested, resorting to non-booted if we still need them later
                    deferred_booted_devices.append((matched_request, device))
            else:
                # For specified UDIDs, either use or boot them immediately
                cls._boot_devices([device], host) if not device_is_booted else SimulatedDeviceManager.INITIALIZED_DEVICES.append(device)
                _log.debug(u'Attached to requested simulator {}'.format(device))
                requests.remove(matched_request)
                requests = cls._validate_running_device_against_requests(requests, device)

        # Check for matches among remaining booted simulators.
        if len(deferred_booted_devices) and len(requests):
            for matched_request, device in deferred_booted_devices:
                _log.debug(u'Attached to running simulator {}'.format(device))
                requests.remove(matched_request)
                SimulatedDeviceManager.INITIALIZED_DEVICES.append(device)

                requests = cls._validate_running_device_against_requests(requests, device)
                if not len(requests):
                    break

        if len(requests):
            _log.debug(f'Running{"/specified" if udids else ""} simulators did not satisfy request. Finding matching non-booted ones, and/or creating new ones to satisfy the request.')

        # Check for any other matching simulators that can satisfy the request.
        # If none are found, we create and boot new ones.
        devices_to_boot = []
        for request in requests:
            device = cls._create_or_find_device_for_request(request, host, name_base)
            assert device is not None
            # Registered before booting rather than after: naming and reuse both look at INITIALIZED_DEVICES, so a
            # device that is not in it yet gets the same name as the next one, which then deletes it.
            SimulatedDeviceManager.INITIALIZED_DEVICES.append(device)
            devices_to_boot.append(device)

        cls._boot_devices(devices_to_boot, host)

        if simulator_ui and host.executive.run_command(['killall', '-0', 'Simulator.app'], return_exit_code=True) != 0:
            SimulatedDeviceManager._managing_simulator_app = not host.executive.run_command(['open', '-g', '-b', SimulatedDeviceManager.simulator_bundle_id, '--args', '-PasteboardAutomaticSync', '0'], return_exit_code=True)

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
            host.executive.cpu_count() // SimulatedDeviceManager.CPU_ESTIMATE_PER_SIMULATOR_INSTANCE,
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
    def tear_down(host=None, timeout=SIMULATOR_BOOT_TIMEOUT):
        host = host or SystemHost.get_default()
        if SimulatedDeviceManager._managing_simulator_app:
            host.executive.run_command(['killall', '-9', 'Simulator.app'], return_exit_code=True)
            SimulatedDeviceManager._managing_simulator_app = False

        if SimulatedDeviceManager.INITIALIZED_DEVICES is None:
            return

        deadline = time.time() + timeout
        launchd_state_directories = [
            SimulatedDeviceManager.launchd_state_path.format(device.udid)
            for device in SimulatedDeviceManager.INITIALIZED_DEVICES if device
        ]
        while SimulatedDeviceManager.INITIALIZED_DEVICES:
            device = SimulatedDeviceManager.INITIALIZED_DEVICES[0]
            if device is None:
                SimulatedDeviceManager.INITIALIZED_DEVICES.remove(None)
                continue
            device.platform_device._tear_down(deadline - time.time())

        SimulatedDeviceManager.INITIALIZED_DEVICES = None

        for directory in launchd_state_directories:
            host.filesystem.rmtree(directory)

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
        'iOS': 'com.apple.Preferences',
        'watchOS': 'com.apple.NanoSettings',
    }

    def __init__(self, name, udid, host, device_type, build_version, runtime_root):
        assert device_type.software_version

        self.name = name
        self.udid = udid
        self.device_type = device_type
        self.build_version = build_version
        self.runtime_root = runtime_root
        self._state = SimulatedDevice.DeviceState.SHUTTING_DOWN

        self.executive = host.executive
        self.filesystem = host.filesystem
        self.platform = host.platform

        self.launchd_configuration = {
            'disabled.plist': disabled_launchd_jobs(),
        }

        self.environment_extras = []

        if apple_additions():
            self.environment_extras.extend(apple_additions().environment_extras(udid))
            additional = getattr(apple_additions(), 'launchd_configuration', lambda: {})() or {}
            for plist_name, entries in additional.items():
                self.launchd_configuration.setdefault(plist_name, {}).update(entries)

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

        exit_code = 1
        try:
            with Timeout(seconds=30, patch=False):
                exit_code = self.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'launch', self.udid, service], return_exit_code=True)
        except Timeout.Exception:
            _log.error("Exceeded timeout while attempting to launch usability test")
            return False

        time.sleep(.7)

        try:
            with Timeout(seconds=30, patch=False):
                exit_code |= self.executive.run_command([SimulatedDeviceManager.xcrun, 'simctl', 'terminate', self.udid, service], return_exit_code=True)
        except Timeout.Exception:
            _log.error("Exceeded timeout while running simctl terminate, not simulator frames executing")
            return False

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
            # FIXME: remove this workaround when rdar://129789675 has been resolved.
            eligibility_util = os.path.join(os.path.dirname(app_path), "WebKitEligibilityUtil")
            exit_code, _, _ = wait_until_exit(
                self.executive.popen(['xcrun', 'simctl', 'spawn', self.udid, eligibility_util],
                                     stdout=self.executive.PIPE, stderr=self.executive.PIPE),
                timeout=SimulatedDeviceManager.INSTALL_TIMEOUT)
            _log.debug(u'WebKitEligibilityUtil returned {}'.format(exit_code))

            exit_code, _, _ = wait_until_exit(
                self.executive.popen(['xcrun', 'simctl', 'install', self.udid, app_path],
                                     stdout=self.executive.PIPE, stderr=self.executive.PIPE),
                timeout=SimulatedDeviceManager.INSTALL_TIMEOUT)
            if exit_code == 0:
                return True

            # Return code 18 indicates that the app is not compatible with the current device, which can
            # happen under load and may not occur on retry.
            # Return code 204 indicates that the device is booting, a retry may be successful.
            if exit_code in (18, 204):
                time.sleep(15)
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

    def set_up_environment_extras(self):
        if len(self.environment_extras) == 0:
            return
        _log.debug(u'Running extra environment setup commands.')
        for command in self.environment_extras:
            self.executive.run_command(command)

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
