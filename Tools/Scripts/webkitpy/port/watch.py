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

from webkitpy.common.memoized import memoized
from webkitpy.common.version import Version
from webkitpy.common.version_name_map import VersionNameMap, INTERNAL_TABLE
from webkitpy.port.config import apple_additions
from webkitpy.port.device_port import DevicePort
from webkitpy.xcode.device_type import DeviceType


_log = logging.getLogger(__name__)


class WatchPort(DevicePort):
    port_name = 'watchos'

    CURRENT_VERSION = Version(5)
    DEFAULT_DEVICE_TYPE = DeviceType(software_variant='watchOS')

    def __init__(self, *args, **kwargs):
        super(WatchPort, self).__init__(*args, **kwargs)

        if self.get_option('webkit_test_runner', False) == False:
            raise RuntimeError('DumpRenderTree is not supported on this platform.')

    def driver_name(self):
        if self.get_option('driver_name'):
            return self.get_option('driver_name')
        return 'WebKitTestRunnerApp.app'

    def version_name(self):
        if self._os_version is None:
            return None
        return VersionNameMap.map(self.host.platform).to_name(self._os_version, platform=WatchPort.port_name)

    def test_expectations_file_position(self):
        return 4

    @memoized
    def default_baseline_search_path(self):
        versions_to_fallback = []
        if self.device_version() == self.CURRENT_VERSION:
            versions_to_fallback = [self.CURRENT_VERSION]
        elif self.device_version():
            temp_version = Version(self.device_version().major)
            while temp_version.major != self.CURRENT_VERSION.major:
                versions_to_fallback.append(Version.from_iterable(temp_version))
                if temp_version < self.CURRENT_VERSION:
                    temp_version.major += 1
                else:
                    temp_version.major -= 1

        expectations = []
        for version in versions_to_fallback:
            if apple_additions():
                apple_name = VersionNameMap.map(self.host.platform).to_name(version, platform=WatchPort.port_name, table=INTERNAL_TABLE)
                expectations.append(self._apple_baseline_path('{}-{}'.format(self.port_name, apple_name.lower().replace(' ', ''))))
            expectations.append(self._webkit_baseline_path('{}-{}'.format(self.port_name, version.major)))

        if apple_additions():
            expectations.append(self._apple_baseline_path(self.port_name))
        expectations.append(self._webkit_baseline_path(self.port_name))

        for version in versions_to_fallback:
            apple_name = None
            if apple_additions():
                apple_name = VersionNameMap.map(self.host.platform).to_name(version, platform=WatchPort.port_name, table=INTERNAL_TABLE)
            if apple_name:
                expectations.append(
                    self._apple_baseline_path('{}-{}'.format(WatchPort.port_name, apple_name.lower().replace(' ', ''))))
            expectations.append(self._webkit_baseline_path('{}-{}'.format(WatchPort.port_name, version.major)))

        if apple_additions():
            expectations.append(self._apple_baseline_path(WatchPort.port_name))
        expectations.append(self._webkit_baseline_path(WatchPort.port_name))

        expectations.append(self._webkit_baseline_path('wk2'))

        return expectations
