# Copyright (C) 2011 Google Inc. All rights reserved.
# Copyright (c) 2017 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

from webkitcorepy import Version

from webkitpy.common.version_name_map import PUBLIC_TABLE, INTERNAL_TABLE, VersionNameMap


class MockPlatformInfo(object):
    def __init__(self, os_name='mac', os_version=Version.from_name('High Sierra'), architecture=None):
        assert isinstance(os_version, Version)
        self.os_name = os_name
        self.os_version = os_version
        self.expected_xcode_simctl_list = None
        self._architecture = architecture or dict(
            mac='x86_64',
            ios='arm64',
        ).get(self.os_name, 'x86')

    def is_mac(self):
        return self.os_name == 'mac'

    def is_ios(self):
        return self.os_name == 'ios'

    def is_linux(self):
        return self.os_name == 'linux'

    def is_win(self):
        return self.os_name == 'win'

    def is_native_win(self):
        return self.is_win() and not self.is_cygwin()

    def is_cygwin(self):
        return self.os_name == 'cygwin'

    def is_freebsd(self):
        return self.os_name == 'freebsd'

    def architecture(self):
        return self._architecture

    def display_name(self):
        return "MockPlatform 1.0"

    def os_version_name(self, table=None):
        if not self.os_version:
            return None
        if table:
            return VersionNameMap.map(self).to_name(self.os_version, table=table)
        version_name = VersionNameMap.map(self).to_name(self.os_version, table=PUBLIC_TABLE)
        if not version_name:
            version_name = VersionNameMap.map(self).to_name(self.os_version, table=INTERNAL_TABLE)
        return version_name

    def total_bytes_memory(self):
        return 3 * 1024 * 1024 * 1024  # 3GB is a reasonable amount of ram to mock.

    def terminal_width(self):
        return 80

    def build_version(self):
        if self.is_mac():
            return '17A405'
        return None

    def xcode_sdk_version(self, sdk_name):
        return Version(8, 1)

    def xcode_version(self):
        return Version(8, 0)

    def xcode_simctl_list(self):
        return self.expected_xcode_simctl_list
