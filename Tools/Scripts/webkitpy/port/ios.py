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
from webkitpy.port.device_port import DevicePort
from webkitpy.port.simulator_process import SimulatorProcess

_log = logging.getLogger(__name__)


class IOSPort(DevicePort):
    port_name = "ios"

    CURRENT_VERSION = Version(12)

    def __init__(self, host, port_name, **kwargs):
        super(IOSPort, self).__init__(host, port_name, **kwargs)
        self._test_runner_process_constructor = SimulatorProcess
        self._printing_cmd_line = False

    def version_name(self):
        if self._os_version is None:
            return None
        return VersionNameMap.map(self.host.platform).to_name(self._os_version, platform=IOSPort.port_name)

    @memoized
    def default_baseline_search_path(self):
        wk_string = 'wk1'
        if self.get_option('webkit_test_runner'):
            wk_string = 'wk2'

        versions_to_fallback = []
        if self.device_version().major == self.CURRENT_VERSION.major:
            versions_to_fallback = [self.CURRENT_VERSION]
        elif self.device_version():
            temp_version = Version(self.device_version().major)
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
