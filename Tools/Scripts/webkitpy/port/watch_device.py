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

import logging

from webkitcorepy import Version

from webkitpy.common.memoized import memoized
from webkitpy.common.system.crashlogs import CrashLogs
from webkitpy.port.config import apple_additions
from webkitpy.port.watch import WatchPort

_log = logging.getLogger(__name__)


class WatchDevicePort(WatchPort):
    port_name = 'watchos-device'

    ARCHITECTURES = ['armv7k', 'arm64e', 'arm64_32']
    DEFAULT_ARCHITECTURE = 'armv7k'

    SDK = apple_additions().get_sdk('watchos') if apple_additions() else 'watchos'
    DEVICE_MANAGER = apple_additions().device_manager() if apple_additions() else None
    NO_ON_DEVICE_TESTING = 'On-device testing is not supported in this configuration'
    NO_DEVICE_MANAGER = NO_ON_DEVICE_TESTING

    def _driver_class(self):
        if apple_additions():
            return apple_additions().device_driver()
        return super(WatchDevicePort, self)._driver_class()

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        if port_name == cls.port_name:
            watchos_sdk_version = host.platform.xcode_sdk_version(cls.SDK)
            if not watchos_sdk_version:
                raise Exception("Please install the watchOS SDK.")
            port_name = port_name + '-' + str(watchos_sdk_version.major)
        return port_name

    def path_to_crash_logs(self):
        if not apple_additions():
            raise RuntimeError(self.NO_ON_DEVICE_TESTING)
        return apple_additions().device_crash_log_path()

    def _look_for_all_crash_logs_in_log_dir(self, newer_than):
        log_list = {}
        for device in self.devices():
            crash_log = CrashLogs(device, self.path_to_crash_logs(), crash_logs_to_skip=self._crash_logs_to_skip_for_host.get(device, []))
            log_list.update(crash_log.find_all_logs(newer_than=newer_than))
        return log_list

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than, time_fn=None, sleep_fn=None, wait_for_log=True, target_host=None):
        if target_host:
            return super(WatchDevicePort, self)._get_crash_log(name, pid, stdout, stderr, newer_than, time_fn=time_fn, sleep_fn=sleep_fn, wait_for_log=wait_for_log, target_host=target_host)

        # We need to search every device since one was not specified.
        for device in self.devices():
            stderr_out, crashlog = super(WatchDevicePort, self)._get_crash_log(name, pid, stdout, stderr, newer_than, time_fn=time_fn, sleep_fn=sleep_fn, wait_for_log=False, target_host=device)
            if crashlog:
                return (stderr, crashlog)
        return (stderr, None)

    @memoized
    def device_version(self):
        if self.get_option('version'):
            return Version.from_string(self.get_option('version'))

        if not self.DEVICE_MANAGER:
            raise RuntimeError(self.NO_ON_DEVICE_TESTING)

        if not self.DEVICE_MANAGER.available_devices(host=self.host):
            raise RuntimeError('No devices are available')
        version = None
        for device in self.DEVICE_MANAGER.available_devices(host=self.host):
            if not device.platform.is_watchos():
                continue
            if not version:
                version = device.platform.os_version
            else:
                if device.platform.os_version != version:
                    raise RuntimeError('Multiple connected devices have different watchOS versions')
        return version

    # FIXME: These need device implementations <rdar://problem/30497991>.
    def check_for_leaks(self, process_name, process_id):
        pass

    def operating_system(self):
        return 'watchos-device'

    def clean_up_test_run(self):
        super(WatchDevicePort, self).clean_up_test_run()
        apple_additions().device_clean_up_test_run(self, self._path_to_driver(), self._build_path())
