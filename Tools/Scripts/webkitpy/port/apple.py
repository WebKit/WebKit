# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the Google name nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging

from webkitpy.common.memoized import memoized
from webkitpy.common.version_name_map import VersionNameMap
from webkitpy.port.base import Port
from webkitpy.port.config import apple_additions
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.common.version_name_map import PUBLIC_TABLE, INTERNAL_TABLE


_log = logging.getLogger(__name__)


class ApplePort(Port):
    """Shared logic between all of Apple's ports."""

    # overridden in subclasses
    ARCHITECTURES = []
    _crash_logs_to_skip_for_host = {}

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        options = options or {}
        if port_name in (cls.port_name, cls.port_name + '-wk2'):

            # Since IOS simulator run on mac, they need a special check
            if host.platform.os_name == 'mac' and 'ios-simulator' in port_name:
                return port_name

            version_name_addition = ('-' + host.platform.os_version_name().lower().replace(' ', '')) if host.platform.os_version_name() else ''

            # If the port_name matches the (badly named) cls.port_name, that
            # means that they passed 'mac' or 'win' and didn't specify a version.
            # That convention means that we're supposed to use the version currently
            # being run, so this won't work if you're not on mac or win (respectively).
            # If you're not on the o/s in question, you must specify a full version (cf. above).
            if port_name == cls.port_name and not getattr(options, 'webkit_test_runner', False):
                port_name = cls.port_name + version_name_addition
            else:
                port_name = cls.port_name + version_name_addition + '-wk2'
        elif getattr(options, 'webkit_test_runner', False) and  '-wk2' not in port_name:
            port_name += '-wk2'

        return port_name

    def _strip_port_name_prefix(self, port_name):
        # Callers treat this return value as the "version", which only works
        # because Apple ports use a simple name-version port_name scheme.
        # FIXME: This parsing wouldn't be needed if port_name handling was moved to factory.py
        # instead of the individual port constructors.
        return port_name[len(self.port_name + '-'):]

    def __init__(self, host, port_name, **kwargs):
        super(ApplePort, self).__init__(host, port_name, **kwargs)

        port_name = port_name.replace('-wk2', '')
        self._version = self._strip_port_name_prefix(port_name)

    def setup_test_run(self, device_type=None):
        self._crash_logs_to_skip_for_host[self.host] = self.host.filesystem.files_under(self.path_to_crash_logs())

    def default_timeout_ms(self):
        if self.get_option('guard_malloc'):
            return 350 * 1000
        return super(ApplePort, self).default_timeout_ms()

    def should_retry_crashes(self):
        return True

    @memoized
    def _allowed_versions(self):
        versions = set()
        for table in [PUBLIC_TABLE, INTERNAL_TABLE]:
            versions.update(VersionNameMap.map(self.host.platform).mapping_for_platform(platform=self.port_name.split('-')[0], table=table).values())
        return sorted(versions)

    def _generate_all_test_configurations(self):
        configurations = []
        for version in self._allowed_versions():
            for build_type in self.ALL_BUILD_TYPES:
                for architecture in self.ARCHITECTURES:
                    for table in [PUBLIC_TABLE, INTERNAL_TABLE]:
                        version_name = VersionNameMap.map(self.host.platform).to_name(version, platform=self.port_name.split('-')[0], table=table)
                        if version_name:
                            configurations.append(TestConfiguration(version=version_name, architecture=architecture, build_type=build_type))
        return configurations

    def _apple_baseline_path(self, platform):
        return self._filesystem.join(apple_additions().layout_tests_path(), platform)

    def _path_to_helper(self):
        binary_name = 'LayoutTestHelper'
        return self._build_path(binary_name)
