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

from webkitpy.common.memoized import memoized
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

    # FIXME: This is only exposed so that SimulatorProcess can use it.
    def device_for_worker_number(self, number):
        if self._printing_cmd_line:
            return None
        # When using simulated devices, this means webkitpy is managing the devices.
        if self.using_multiple_devices():
            return self._testing_device(number)
        return self._current_device

    def _create_devices(self, device_class):
        raise NotImplementedError

    def setup_test_run(self, device_class=None):
        self._create_devices(device_class)

        for i in xrange(self.child_processes()):
            device = self.device_for_worker_number(i)
            _log.debug('Installing to {}'.format(device))
            # Without passing DYLD_LIBRARY_PATH, libWebCoreTestSupport cannot be loaded and DRT/WKTR will crash pre-launch,
            # leaving a crash log which will be picked up in results.  No DYLD_FRAMEWORK_PATH will also cause the DRT/WKTR to
            # crash, but this crash will occur post-launch, after install_app has already killed the process.
            if not device.install_app(self._path_to_driver(), {'DYLD_LIBRARY_PATH': self._build_path()}):
                raise RuntimeError('Failed to install app {} on device {}'.format(self._path_to_driver(), device.udid))
