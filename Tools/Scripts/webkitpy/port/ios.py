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
from webkitpy.xcode.device_type import DeviceType

_log = logging.getLogger(__name__)


class IOSPort(DevicePort):
    port_name = "ios"

    CURRENT_VERSION = Version(12)
    # FIXME: This is not a clear way to do this (although it works) https://bugs.webkit.org/show_bug.cgi?id=192160
    DEFAULT_DEVICE_TYPE = DeviceType(software_variant='iOS')

    def __init__(self, host, port_name, **kwargs):
        super(IOSPort, self).__init__(host, port_name, **kwargs)
        self._test_runner_process_constructor = SimulatorProcess
        self._printing_cmd_line = False

    def version_name(self):
        if self._os_version is None:
            return None
        return VersionNameMap.map(self.host.platform).to_name(self._os_version, platform=IOSPort.port_name)

    @memoized
    def default_baseline_search_path(self, device_type=None):
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

        runtime_type = 'simulator' if 'simulator' in self.SDK else 'device'
        hardware_family = device_type.hardware_family.lower() if device_type and device_type.hardware_family else None
        hardware_type = device_type.hardware_type.lower() if device_type and device_type.hardware_type else None

        base_variants = []
        if hardware_family and hardware_type:
            base_variants.append('{}-{}-{}'.format(hardware_family, hardware_type, runtime_type))
        if hardware_family:
            base_variants.append('{}-{}'.format(hardware_family, runtime_type))
        base_variants.append('{}-{}'.format(IOSPort.port_name, runtime_type))
        if hardware_family and hardware_type:
            base_variants.append('{}-{}'.format(hardware_family, hardware_type))
        if hardware_family:
            base_variants.append(hardware_family)
        base_variants.append(IOSPort.port_name)

        expectations = []
        for variant in base_variants:
            for version in versions_to_fallback:
                apple_name = None
                if apple_additions():
                    apple_name = VersionNameMap.map(self.host.platform).to_name(version, platform=IOSPort.port_name, table=INTERNAL_TABLE)

                if apple_name:
                    expectations.append(self._apple_baseline_path('{}-{}-{}'.format(variant, apple_name.lower().replace(' ', ''), wk_string)))
                expectations.append(self._webkit_baseline_path('{}-{}-{}'.format(variant, version.major, wk_string)))
                if apple_name:
                    expectations.append(self._apple_baseline_path('{}-{}'.format(variant, apple_name.lower().replace(' ', ''))))
                expectations.append(self._webkit_baseline_path('{}-{}'.format(variant, version.major)))

            if apple_additions():
                expectations.append(self._apple_baseline_path('{}-{}'.format(variant, wk_string)))
            expectations.append(self._webkit_baseline_path('{}-{}'.format(variant, wk_string)))
            if apple_additions():
                expectations.append(self._apple_baseline_path(variant))
            expectations.append(self._webkit_baseline_path(variant))

        if self.get_option('webkit_test_runner'):
            expectations.append(self._webkit_baseline_path('wk2'))

        return expectations

    def test_expectations_file_position(self):
        return 5
