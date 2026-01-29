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

from webkitcorepy import Version

from webkitpy.common.version_name_map import VersionNameMap, INTERNAL_TABLE
from webkitpy.port.config import apple_additions
from webkitpy.port.embedded_port import EmbeddedPort
from webkitpy.port.simulator_process import SimulatorProcess
from webkitpy.xcode.device_type import DeviceType

_log = logging.getLogger(__name__)


class IOSPort(EmbeddedPort):
    port_name = "ios"

    DEVICE_TYPE = DeviceType(software_variant='iOS')
    DRIVER_NAMES = ('WebKitTestRunner', 'DumpRenderTree')

    def __init__(self, host, port_name, **kwargs):
        super(IOSPort, self).__init__(host, port_name, **kwargs)
        self._test_runner_process_constructor = SimulatorProcess
        self._printing_cmd_line = False

    def version_name(self):
        if self._os_version is None:
            return None
        return VersionNameMap.map(self.host.platform).to_name(self._os_version, platform=IOSPort.port_name)

    def default_baseline_search_path(self, device_type=None):
        if device_type is None:
            device_type = self.DEVICE_TYPE

        wk_string = 'wk1'
        if not self.is_webkitlegacy():
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
            base_variants.append(u'{}-{}-{}'.format(hardware_family, hardware_type, runtime_type))
        if hardware_family:
            base_variants.append(u'{}-{}'.format(hardware_family, runtime_type))
        base_variants.append(u'{}-{}'.format(IOSPort.port_name, runtime_type))
        if hardware_family and hardware_type:
            base_variants.append(u'{}-{}'.format(hardware_family, hardware_type))
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
                    expectations.append(self._apple_baseline_path(u'{}-{}-{}'.format(variant, apple_name.lower().replace(' ', ''), wk_string)))
                expectations.append(self._webkit_baseline_path(u'{}-{}-{}'.format(variant, version.major, wk_string)))
                if apple_name:
                    expectations.append(self._apple_baseline_path(u'{}-{}'.format(variant, apple_name.lower().replace(' ', ''))))
                expectations.append(self._webkit_baseline_path(u'{}-{}'.format(variant, version.major)))

            if apple_additions():
                expectations.append(self._apple_baseline_path(u'{}-{}'.format(variant, wk_string)))
            expectations.append(self._webkit_baseline_path(u'{}-{}'.format(variant, wk_string)))
            if apple_additions():
                expectations.append(self._apple_baseline_path(variant))
            expectations.append(self._webkit_baseline_path(variant))

        if not self.is_webkitlegacy():
            expectations.append(self._webkit_baseline_path('wk2'))

        return expectations

    def _api_test_platform_cascade(self):
        """Return platform directories for API test expectations cascade."""
        cascade = []
        version_name_map = VersionNameMap.map(self.host.platform)

        internal_version_name = None
        if apple_additions():
            internal_version_name = version_name_map.to_name(self._os_version, platform=IOSPort.port_name, table=INTERNAL_TABLE)
            if internal_version_name:
                internal_version_name = internal_version_name.lower().replace(' ', '')

        internal_name = 'ios-{}'.format(internal_version_name) if internal_version_name else None
        cascade.append(('ios', internal_name))

        if self._os_version:
            public_name = 'ios-{}'.format(self._os_version.major)
            cascade.append((public_name, internal_name))

        if 'simulator' in self.SDK:
            internal_sim_name = 'ios-{}-simulator'.format(internal_version_name) if internal_version_name else None
            cascade.append(('ios-simulator', internal_sim_name))
            if self._os_version:
                public_sim_version = 'ios-simulator-{}'.format(self._os_version.major)
                cascade.append((public_sim_version, internal_sim_name))

        return cascade

    def api_test_version_tokens(self):
        """Return dict mapping version token (lowercase) to version order index."""
        tokens = {}
        version_name_map = VersionNameMap.map(self.host.platform)
        versions = self._allowed_versions()
        for idx, version in enumerate(versions):
            for table in [PUBLIC_TABLE, INTERNAL_TABLE]:
                version_name = version_name_map.to_name(version, platform=IOSPort.port_name, table=table)
                if version_name:
                    tokens[version_name.lower().replace(' ', '')] = idx
            # Also add numeric version like 'ios17', 'ios18'
            tokens['ios{}'.format(version.major)] = idx
        return tokens

    def api_test_version_order(self):
        """Return list of version names in order from oldest to newest."""
        version_order = []
        for version in self._allowed_versions():
            # Use numeric naming for iOS (iOS 17, iOS 18, etc.)
            version_order.append('ios{}'.format(version.major))
        return version_order

    def api_test_current_configuration(self):
        """Return the current runtime configuration for matching expectations."""
        config = super(IOSPort, self).api_test_current_configuration()
        config['platform'] = 'ios'

        # Add version
        if self._os_version:
            config['version'] = 'ios{}'.format(self._os_version.major)

        # Hardware: simulator vs device, and iphone/ipad
        if 'simulator' in self.SDK:
            config['hardware'] = 'simulator'
        else:
            config['hardware'] = 'device'

        # Device type: iphone/ipad
        if self.DEVICE_TYPE:
            hw_family = getattr(self.DEVICE_TYPE, 'hardware_family', None)
            if hw_family:
                hw_family_lower = hw_family.lower()
                if hw_family_lower == 'iphone':
                    config['hardware_type'] = 'iphone'
                elif hw_family_lower == 'ipad':
                    config['hardware_type'] = 'ipad'

        return config

    def port_adjust_environment_for_test_driver(self, env):
        env = super(IOSPort, self).port_adjust_environment_for_test_driver(env)
        env['CA_DISABLE_GENERIC_SHADERS'] = '1'
        env['__XPC_CA_DISABLE_GENERIC_SHADERS'] = '1'
        return env
