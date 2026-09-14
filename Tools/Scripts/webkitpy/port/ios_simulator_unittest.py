# Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

from webkitcorepy import Version

import multiprocessing
import queue
import time
import unittest

from unittest.mock import Mock

from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.port import embedded_port
from webkitpy.port.device_provisioning import DeviceProvisioning
from webkitpy.port.ios_simulator import IOSSimulatorPort
from webkitpy.port.mac import MacPort
from webkitpy.port import ios_testcase
from webkitpy.port import port_testcase
from webkitpy.tool.mocktool import MockOptions
from webkitpy.common.system.executive_mock import MockExecutive2, ScriptError
from webkitpy.xcode.device_type import DeviceType
from webkitpy.xcode.simulated_device import SimulatedDeviceManager

from webkitcorepy import OutputCapture


class IOSSimulatorTest(ios_testcase.IOSTest):
    # FIXME: https://bugs.webkit.org/show_bug.cgi?id=173107
    os_name = 'mac'
    os_version = None
    port_name = 'ios-simulator'
    port_maker = IOSSimulatorPort

    def make_port(self, host=None, port_name=None, options=None, os_name=None, os_version=IOSSimulatorPort.CURRENT_VERSION, **kwargs):
        port = super(IOSSimulatorTest, self).make_port(host=host, port_name=port_name, options=options, os_name=os_name, os_version=os_version, kwargs=kwargs)
        port.set_option('child_processes', 1)
        return port

    def test_setup_environ_for_server(self):
        port = self.make_port(options=MockOptions(leaks=True, guard_malloc=True))
        env = port.setup_environ_for_server(port.driver_name())
        self.assertEqual(env['MallocStackLogging'], '1')
        self.assertEqual(env['MallocScribble'], '1')
        self.assertEqual(env['DYLD_INSERT_LIBRARIES'], '/usr/lib/libgmalloc.dylib')

    def test_operating_system(self):
        self.assertEqual('ios-simulator', self.make_port().operating_system())

    def test_32bit(self):
        port = self.make_port(options=MockOptions(architecture='i386'))

        def run_script(script, args=None, env=None):
            self.args = args

        port._run_script = run_script
        self.assertEqual(port.architecture(), 'i386')
        port._build_driver()
        self.assertEqual(self.args, ['--sdk', 'iphonesimulator', 'ARCHS=i386'])

    def test_64bit(self):
        # Apple Mac port is 64-bit by default
        port = self.make_port()
        self.assertEqual(port.architecture(), 'x86_64')

        def run_script(script, args=None, env=None):
            self.args = args

        port._run_script = run_script
        port._build_driver()
        self.assertEqual(self.args, ['--sdk', 'iphonesimulator', 'ARCHS=x86_64'])

    def test_sdk_name(self):
        port = self.make_port()
        self.assertEqual(port.SDK, 'iphonesimulator')

    def test_layout_test_searchpath_with_apple_additions(self):
        with port_testcase.bind_mock_apple_additions():
            port = self.make_port()
            major_os_version = port._options.version.split('.')[0]
            search_path = port.default_baseline_search_path()

        self.assertEqual(search_path, [
            f'/additional_testing_path/ios-simulator-add-ios{major_os_version}-wk2',
            f'/mock-checkout/LayoutTests/platform/ios-simulator-{major_os_version}-wk2',
            f'/additional_testing_path/ios-simulator-add-ios{major_os_version}',
            f'/mock-checkout/LayoutTests/platform/ios-simulator-{major_os_version}',
            '/additional_testing_path/ios-simulator-wk2',
            '/mock-checkout/LayoutTests/platform/ios-simulator-wk2',
            '/additional_testing_path/ios-simulator',
            '/mock-checkout/LayoutTests/platform/ios-simulator',
            f'/additional_testing_path/ios-add-ios{major_os_version}-wk2',
            f'/mock-checkout/LayoutTests/platform/ios-{major_os_version}-wk2',
            f'/additional_testing_path/ios-add-ios{major_os_version}',
            f'/mock-checkout/LayoutTests/platform/ios-{major_os_version}',
            '/additional_testing_path/ios-wk2',
            '/mock-checkout/LayoutTests/platform/ios-wk2',
            '/additional_testing_path/ios',
            '/mock-checkout/LayoutTests/platform/ios',
            '/mock-checkout/LayoutTests/platform/wk2',
        ])

    def test_layout_test_searchpath_without_apple_additions(self):
        port = self.make_port(port_name='ios-simulator-wk2')
        major_os_version = port._options.version.split('.')[0]
        search_path = port.default_baseline_search_path()

        self.assertEqual(search_path, [
            f'/mock-checkout/LayoutTests/platform/ios-simulator-{major_os_version}-wk2',
            f'/mock-checkout/LayoutTests/platform/ios-simulator-{major_os_version}',
            '/mock-checkout/LayoutTests/platform/ios-simulator-wk2',
            '/mock-checkout/LayoutTests/platform/ios-simulator',
            f'/mock-checkout/LayoutTests/platform/ios-{major_os_version}-wk2',
            f'/mock-checkout/LayoutTests/platform/ios-{major_os_version}',
            '/mock-checkout/LayoutTests/platform/ios-wk2',
            '/mock-checkout/LayoutTests/platform/ios',
            '/mock-checkout/LayoutTests/platform/wk2',
        ])

    def test_layout_searchpath_wih_device_type(self):
        port = self.make_port(port_name='ios-simulator-wk2')
        major_os_version = port._options.version.split('.')[0]
        search_path = port.default_baseline_search_path(DeviceType.from_string('iPhone SE'))

        self.assertEqual(search_path, [
            f'/mock-checkout/LayoutTests/platform/iphone-se-simulator-{major_os_version}-wk2',
            f'/mock-checkout/LayoutTests/platform/iphone-se-simulator-{major_os_version}',
            '/mock-checkout/LayoutTests/platform/iphone-se-simulator-wk2',
            '/mock-checkout/LayoutTests/platform/iphone-se-simulator',
            f'/mock-checkout/LayoutTests/platform/iphone-simulator-{major_os_version}-wk2',
            f'/mock-checkout/LayoutTests/platform/iphone-simulator-{major_os_version}',
            '/mock-checkout/LayoutTests/platform/iphone-simulator-wk2',
            '/mock-checkout/LayoutTests/platform/iphone-simulator',
            f'/mock-checkout/LayoutTests/platform/ios-simulator-{major_os_version}-wk2',
            f'/mock-checkout/LayoutTests/platform/ios-simulator-{major_os_version}',
            '/mock-checkout/LayoutTests/platform/ios-simulator-wk2',
            '/mock-checkout/LayoutTests/platform/ios-simulator',
            f'/mock-checkout/LayoutTests/platform/iphone-se-{major_os_version}-wk2',
            f'/mock-checkout/LayoutTests/platform/iphone-se-{major_os_version}',
            '/mock-checkout/LayoutTests/platform/iphone-se-wk2',
            '/mock-checkout/LayoutTests/platform/iphone-se',
            f'/mock-checkout/LayoutTests/platform/iphone-{major_os_version}-wk2',
            f'/mock-checkout/LayoutTests/platform/iphone-{major_os_version}',
            '/mock-checkout/LayoutTests/platform/iphone-wk2',
            '/mock-checkout/LayoutTests/platform/iphone',
            f'/mock-checkout/LayoutTests/platform/ios-{major_os_version}-wk2',
            f'/mock-checkout/LayoutTests/platform/ios-{major_os_version}',
            '/mock-checkout/LayoutTests/platform/ios-wk2',
            '/mock-checkout/LayoutTests/platform/ios',
            '/mock-checkout/LayoutTests/platform/wk2',
        ])

    def test_max_child_processes(self):
        port = self.make_port()
        self.assertEqual(port.max_child_processes(DeviceType.from_string('Apple Watch')), 0)

    def test_default_upload_configuration(self):
        port = self.make_port()
        configuration = port.configuration_for_upload()
        self.assertEqual(configuration['architecture'], port.architecture())
        self.assertEqual(configuration['is_simulator'], True)
        self.assertEqual(configuration['platform'], 'ios')
        self.assertEqual(configuration['style'], 'release')
        self.assertEqual(configuration['version_name'], 'iOS {}'.format(port.device_version()))


class FakeClaimableDevice(object):
    def __init__(self, name):
        self.name = name
        self.udid = 'udid-' + name
        self.prepared = 0

    def prepare_for_testing(self, ports_to_forward, test_app_bundle_id, layout_test_directory):
        self.prepared += 1

    def finished_testing(self):
        self.prepared = 0

    def __repr__(self):
        return self.name


class FakeBootingDevice(object):
    def __init__(self, name):
        self.name = name
        self.udid = 'udid-' + name

    def __repr__(self):
        return self.name


class FakeBootWait(object):
    """Stands in for the process waiting on a device to finish booting."""

    def __init__(self, polls_until_done=0, returncode=0):
        self._remaining = polls_until_done
        self._final = returncode
        self.returncode = None
        self.polls = 0

    def poll(self):
        self.polls += 1
        if self._remaining > 0:
            self._remaining -= 1
            return None
        self.returncode = self._final
        return self._final


class FakeDeviceManager(object):
    AVAILABLE_DEVICES = []
    INITIALIZED_DEVICES = None


def _fake_port(devices, child_processes=None):
    port = IOSSimulatorPort(MockSystemHost(os_name='mac'), 'ios-simulator', options=MockOptions(
        configuration='Release', architecture='x86_64',
        child_processes=child_processes or len(devices)))
    manager = FakeDeviceManager()
    manager.INITIALIZED_DEVICES = list(devices)
    port.DEVICE_MANAGER = manager
    port.ports_to_forward = lambda: []
    port._path_to_driver = lambda: '/mock/Driver.app'
    port.app_identifier_from_bundle = lambda path: 'com.example'
    port.layout_tests_dir = lambda: '/mock/LayoutTests'
    port._install_on = lambda device: None
    return port


class InheritedClaimTest(unittest.TestCase):
    """A worker must not run on the device the process which handed out its shard claimed, and the port reaches a
    worker by being pickled."""

    def test_a_claim_does_not_survive_pickling(self):
        port = IOSSimulatorPort(MockSystemHost(), 'ios-simulator-wk2')
        port._claimed_device = 'a device this process claimed'
        self.assertIsNone(port.__getstate__()['_claimed_device'])

    def test_pickling_keeps_everything_else(self):
        port = IOSSimulatorPort(MockSystemHost(), 'ios-simulator-wk2')
        port._claimed_device = 'a device this process claimed'
        state = port.__getstate__()
        self.assertEqual(set(state), set(port.__dict__))
        self.assertEqual(state['_printing_cmd_line'], port._printing_cmd_line)


class CollectionDeviceTest(unittest.TestCase):
    """Work that belongs to no worker runs on a device which is ready, not on whichever was created first."""

    class FakeDevice(object):
        def __init__(self, name):
            self.name = name
            self.udid = 'udid-' + name

        def __repr__(self):
            return self.name

    def setUp(self):
        self._saved = (SimulatedDeviceManager.INITIALIZED_DEVICES, SimulatedDeviceManager.READY_DEVICES)
        self.port = IOSSimulatorPort(MockSystemHost(), 'ios-simulator-wk2')

    def tearDown(self):
        SimulatedDeviceManager.INITIALIZED_DEVICES, SimulatedDeviceManager.READY_DEVICES = self._saved

    def test_the_first_created_device_is_skipped_while_it_is_still_booting(self):
        booting, ready = self.FakeDevice('Managed 0'), self.FakeDevice('Managed 1')
        SimulatedDeviceManager.INITIALIZED_DEVICES = [booting, ready]
        SimulatedDeviceManager.READY_DEVICES = [ready]
        self.assertIs(self.port.any_ready_device(), ready)

    def test_the_first_created_device_is_used_once_it_is_ready(self):
        first, second = self.FakeDevice('Managed 0'), self.FakeDevice('Managed 1')
        SimulatedDeviceManager.INITIALIZED_DEVICES = [first, second]
        SimulatedDeviceManager.READY_DEVICES = [first, second]
        self.assertIs(self.port.any_ready_device(), first)

    def test_falls_back_when_nothing_is_ready_yet(self):
        first = self.FakeDevice('Managed 0')
        SimulatedDeviceManager.INITIALIZED_DEVICES = [first]
        SimulatedDeviceManager.READY_DEVICES = []
        self.assertIs(self.port.any_ready_device(), first)

    def test_no_devices_at_all(self):
        SimulatedDeviceManager.INITIALIZED_DEVICES = []
        SimulatedDeviceManager.READY_DEVICES = []
        self.assertIsNone(self.port.any_ready_device())


class DeviceLivenessFreshnessTest(unittest.TestCase):
    """Only a caller which already suspects the device pays for a fresh answer from simctl."""

    def _asked_with(self, **kwargs):
        platform_device = Mock()
        platform_device.is_booted_or_booting.return_value = True
        port = IOSSimulatorPort(MockSystemHost(), 'ios-simulator-wk2')
        port._claimed_device = Mock(platform_device=platform_device)
        port.target_host_is_usable(0, **kwargs)
        return [call.kwargs['force_update'] for call in platform_device.is_booted_or_booting.call_args_list]

    def test_default_accepts_a_recent_answer(self):
        self.assertEqual(self._asked_with(), [False])

    def test_a_suspicious_caller_forces_a_fresh_answer(self):
        self.assertEqual(self._asked_with(force_update=True), [True])


class IOSSimulatorClaimTest(unittest.TestCase):
    """The port owns the one device it claimed; the device manager owns the rest."""

    class FakeManager(DeviceProvisioning):
        AVAILABLE_DEVICES = []
        INITIALIZED_DEVICES = None
        DEVICE_QUEUE = None

        def __init__(self, devices, hands_out=True):
            self.INITIALIZED_DEVICES = list(devices)
            self.DEVICE_QUEUE = object() if hands_out else None
            self.handed_out = list(devices)
            self.ended = False

        def claim_device(self, wait_timeout):
            # Matches the real manager: the device, and whether this worker is the one to set it up.
            return (self.handed_out.pop(0), True) if self.handed_out else (None, False)

        def expects_more_devices(self):
            return bool(self.handed_out)

        def end_provisioning(self):
            self.ended = True

    def _port(self, devices, hands_out=True):
        port = IOSSimulatorPort(MockSystemHost(os_name='mac'), 'ios-simulator', options=MockOptions(
            configuration='Release', architecture='x86_64', child_processes=max(1, len(devices))))
        port.DEVICE_MANAGER = self.FakeManager(devices, hands_out=hands_out)
        port.ports_to_forward = lambda: []
        port._path_to_driver = lambda: '/mock/Driver.app'
        port.app_identifier_from_bundle = lambda path: 'com.example'
        port.layout_tests_dir = lambda: '/mock/LayoutTests'
        port._install_on = lambda device: None
        return port

    def test_the_port_claims_from_the_device_manager(self):
        port = self._port([FakeClaimableDevice('device-0'), FakeClaimableDevice('device-1')])

        claimed = port._device_for_worker(0)

        self.assertEqual(str(claimed), 'device-0')
        self.assertEqual(claimed.prepared, 1, 'the claiming worker prepares its own device')

    def test_a_worker_keeps_the_device_it_claimed(self):
        port = self._port([FakeClaimableDevice('device-0'), FakeClaimableDevice('device-1')])

        claimed = port._device_for_worker(0)
        self.assertIs(port._device_for_worker(0), claimed, 'the port owns one device, not a device per worker')
        self.assertIs(port.target_host(0), claimed)

    def test_a_worker_without_a_device_reports_itself_unusable(self):
        port = self._port([])
        self.assertIsNone(port._device_for_worker(0))
        self.assertFalse(port.target_host_is_usable(0))

    def test_a_manager_that_hands_back_ready_devices_is_addressed_by_position(self):
        port = self._port([FakeClaimableDevice('device-0'), FakeClaimableDevice('device-1')], hands_out=False)
        self.assertEqual(str(port._device_for_worker(1)), 'device-1')

    def test_teardown_hands_the_devices_back_to_the_manager(self):
        port = self._port([FakeClaimableDevice('device-0')])
        port._device_for_worker(0)

        port.clean_up_test_run()

        self.assertIsNone(port._claimed_device)
        self.assertTrue(port.DEVICE_MANAGER.ended)
