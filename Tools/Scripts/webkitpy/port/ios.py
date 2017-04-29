# Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

from webkitpy.common.memoized import memoized
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.port.darwin import DarwinPort
from webkitpy.port.simulator_process import SimulatorProcess

_log = logging.getLogger(__name__)


class IOSPort(DarwinPort):
    port_name = "ios"

    def __init__(self, host, port_name, **kwargs):
        super(IOSPort, self).__init__(host, port_name, **kwargs)
        self._test_runner_process_constructor = SimulatorProcess
        self._printing_cmd_line = False
        self._current_device = None

    def _device_for_worker_number_map(self):
        raise NotImplementedError

    def driver_cmd_line_for_logging(self):
        # Avoid creating/connecting to devices just for logging the commandline.
        self._printing_cmd_line = True
        result = super(IOSPort, self).driver_cmd_line_for_logging()
        self._printing_cmd_line = False
        return result

    def driver_name(self):
        if self.get_option('driver_name'):
            return self.get_option('driver_name')
        if self.get_option('webkit_test_runner'):
            return 'WebKitTestRunnerApp.app'
        return 'DumpRenderTree.app'

    def _generate_all_test_configurations(self):
        configurations = []
        for build_type in self.ALL_BUILD_TYPES:
            for architecture in self.ARCHITECTURES:
                configurations.append(TestConfiguration(version=self._version, architecture=architecture, build_type=build_type))
        return configurations

    @memoized
    def child_processes(self):
        return int(self.get_option('child_processes'))

    def using_multiple_devices(self):
        return False

    def _testing_device(self, number):
        device = self._device_for_worker_number_map()[number]
        if not device:
            raise RuntimeError('Device at {} could not be found'.format(number))
        return device

    # A device is the target host for a specific worker number.
    def target_host(self, worker_number=None):
        if self._printing_cmd_line or worker_number is None:
            return self.host
        # When using simulated devices, this means webkitpy is managing the devices.
        if self.using_multiple_devices():
            return self._testing_device(worker_number)
        return self._current_device

    def default_baseline_search_path(self):
        wk_string = 'wk1'
        if self.get_option('webkit_test_runner'):
            wk_string = 'wk2'
        fallback_names = [
            '{}-{}'.format(self.port_name, wk_string),
            self.port_name,
            '{}-{}'.format(IOSPort.port_name, wk_string),
            IOSPort.port_name,
        ]
        if self.get_option('webkit_test_runner'):
            fallback_names.append('wk2')

        return map(self._webkit_baseline_path, fallback_names)

    def test_expectations_file_position(self):
        return 3

    def _create_devices(self, device_class):
        raise NotImplementedError

    def setup_test_run(self, device_class=None):
        self._create_devices(device_class)

        if self.get_option('install'):
            for i in xrange(self.child_processes()):
                device = self.target_host(i)
                _log.debug('Installing to {}'.format(device))
                # Without passing DYLD_LIBRARY_PATH, libWebCoreTestSupport cannot be loaded and DRT/WKTR will crash pre-launch,
                # leaving a crash log which will be picked up in results. DYLD_FRAMEWORK_PATH is needed to prevent an early crash.
                if not device.install_app(self._path_to_driver(), {'DYLD_LIBRARY_PATH': self._build_path(), 'DYLD_FRAMEWORK_PATH': self._build_path()}):
                    raise RuntimeError('Failed to install app {} on device {}'.format(self._path_to_driver(), device.udid))
                if not device.install_dylibs(self._build_path()):
                    raise RuntimeError('Failed to install dylibs at {} on device {}'.format(self._build_path(), device.udid))
        else:
            _log.debug('Skipping installation')

        for i in xrange(self.child_processes()):
            self.target_host(i).prepare_for_testing(
                self.ports_to_forward(),
                self.app_identifier_from_bundle(self._path_to_driver()),
                self.layout_tests_dir(),
            )

    def clean_up_test_run(self):
        super(IOSPort, self).clean_up_test_run()

        # Best effort to let every device teardown before throwing any exceptions here.
        # Failure to teardown devices can leave things in a bad state.
        exception_list = []
        for i in xrange(self.child_processes()):
            device = self.target_host(i)
            try:
                if device:
                    device.finished_testing()
            except BaseException as e:
                trace = traceback.format_exc()
                if isinstance(e, Exception):
                    exception_list.append([e, trace])
                else:
                    exception_list.append([Exception('Exception tearing down {}'.format(device)), trace])
        if len(exception_list) == 1:
            raise
        elif len(exception_list) > 1:
            print '\n'
            for exception in exception_list:
                _log.error('{} raised: {}'.format(exception[0].__class__.__name__, exception[0]))
                _log.error(exception[1])
                _log.error('--------------------------------------------------')

            raise RuntimeError('Multiple failures when teardown devices')
