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
from webkitpy.common.version import Version
from webkitpy.common.version_name_map import VersionNameMap, INTERNAL_TABLE
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.port.config import apple_additions
from webkitpy.port.darwin import DarwinPort
from webkitpy.port.simulator_process import SimulatorProcess

_log = logging.getLogger(__name__)


class IOSPort(DarwinPort):
    port_name = "ios"

    CURRENT_VERSION = Version(12)

    def __init__(self, host, port_name, **kwargs):
        super(IOSPort, self).__init__(host, port_name, **kwargs)
        self._test_runner_process_constructor = SimulatorProcess
        self._printing_cmd_line = False

    def _device_for_worker_number_map(self):
        raise NotImplementedError

    def version_name(self):
        if self._os_version is None:
            return None
        return VersionNameMap.map(self.host.platform).to_name(self._os_version, platform=IOSPort.port_name)

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
                configurations.append(TestConfiguration(version=self.version_name(), architecture=architecture, build_type=build_type))
        return configurations

    def child_processes(self):
        return int(self.get_option('child_processes'))

    def _testing_device(self, number):
        device = self._device_for_worker_number_map()[number]
        if not device:
            raise RuntimeError('Device at {} could not be found'.format(number))
        return device

    # A device is the target host for a specific worker number.
    def target_host(self, worker_number=None):
        if self._printing_cmd_line or worker_number is None:
            return self.host
        return self._testing_device(worker_number)

    @memoized
    def default_baseline_search_path(self):
        wk_string = 'wk1'
        if self.get_option('webkit_test_runner'):
            wk_string = 'wk2'

        versions_to_fallback = []
        if self.ios_version().major == self.CURRENT_VERSION.major:
            versions_to_fallback = [self.CURRENT_VERSION]
        elif self.ios_version():
            temp_version = Version(self.ios_version().major)
            while temp_version != self.CURRENT_VERSION:
                versions_to_fallback.append(Version.from_iterable(temp_version))
                if temp_version < self.CURRENT_VERSION:
                    temp_version.major += 1
                else:
                    temp_version.major -= 1

        expectations = []
        for version in versions_to_fallback:
            apple_name = None
            if apple_additions():
                apple_name = VersionNameMap.map(self.host.platform).to_name(version, platform=IOSPort.port_name, table=INTERNAL_TABLE)

            if apple_name:
                expectations.append(self._apple_baseline_path('{}-{}-{}'.format(self.port_name, apple_name.lower().replace(' ', ''), wk_string)))
            expectations.append(self._webkit_baseline_path('{}-{}-{}'.format(self.port_name, version.major, wk_string)))
            if apple_name:
                expectations.append(self._apple_baseline_path('{}-{}'.format(self.port_name, apple_name.lower().replace(' ', ''))))
            expectations.append(self._webkit_baseline_path('{}-{}'.format(self.port_name, version.major)))

        if apple_additions():
            expectations.append(self._apple_baseline_path('{}-{}'.format(self.port_name, wk_string)))
        expectations.append(self._webkit_baseline_path('{}-{}'.format(self.port_name, wk_string)))
        if apple_additions():
            expectations.append(self._apple_baseline_path(self.port_name))
        expectations.append(self._webkit_baseline_path(self.port_name))

        for version in versions_to_fallback:
            apple_name = None
            if apple_additions():
                apple_name = VersionNameMap.map(self.host.platform).to_name(version, platform=IOSPort.port_name, table=INTERNAL_TABLE)
            if apple_name:
                expectations.append(self._apple_baseline_path('{}-{}'.format(IOSPort.port_name, apple_name.lower().replace(' ', ''))))
            expectations.append(self._webkit_baseline_path('{}-{}'.format(IOSPort.port_name, version.major)))

        if apple_additions():
            expectations.append(self._apple_baseline_path('{}-{}'.format(IOSPort.port_name, wk_string)))
        expectations.append(self._webkit_baseline_path('{}-{}'.format(IOSPort.port_name, wk_string)))
        if apple_additions():
            expectations.append(self._apple_baseline_path(IOSPort.port_name))
        expectations.append(self._webkit_baseline_path(IOSPort.port_name))

        if self.get_option('webkit_test_runner'):
            expectations.append(self._webkit_baseline_path('wk2'))

        return expectations

    def test_expectations_file_position(self):
        return 4

    def ios_version(self):
        raise NotImplementedError

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
            host = self.target_host(i)
            host.prepare_for_testing(
                self.ports_to_forward(),
                self.app_identifier_from_bundle(self._path_to_driver()),
                self.layout_tests_dir(),
            )
            self._crash_logs_to_skip_for_host[host] = host.filesystem.files_under(self.path_to_crash_logs())

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
            print('\n')
            for exception in exception_list:
                _log.error('{} raised: {}'.format(exception[0].__class__.__name__, exception[0]))
                _log.error(exception[1])
                _log.error('--------------------------------------------------')

            raise RuntimeError('Multiple failures when teardown devices')

    def did_spawn_worker(self, worker_number):
        super(IOSPort, self).did_spawn_worker(worker_number)

        self.target_host(worker_number).release_worker_resources()
