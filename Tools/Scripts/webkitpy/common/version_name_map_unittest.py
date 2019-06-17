# Copyright (C) 2017 Apple Inc. All rights reserved.
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

import unittest

from webkitpy.common.system.systemhost import SystemHost
from webkitpy.common.version import Version
from version_name_map import VersionNameMap


class VersionMapTestCase(unittest.TestCase):

    def test_default_system_platform(self):
        host = SystemHost()
        map = VersionNameMap(platform=host.platform)
        self.assertEqual(map.default_system_platform, host.platform.os_name)

    def test_mac_version_by_name(self):
        map = VersionNameMap()
        self.assertEqual(('mac', Version(10, 15)), map.from_name('Catalina'))
        self.assertEqual(('mac', Version(10, 15)), map.from_name('catalina'))
        self.assertEqual(('mac', Version(10, 14)), map.from_name('Mojave'))
        self.assertEqual(('mac', Version(10, 14)), map.from_name('mojave'))
        self.assertEqual(('mac', Version(10, 13)), map.from_name('High Sierra'))
        self.assertEqual(('mac', Version(10, 13)), map.from_name('high sierra'))
        self.assertEqual(('mac', Version(10, 13)), map.from_name('highsierra'))
        self.assertEqual(('mac', Version(10, 12)), map.from_name('Sierra'))
        self.assertEqual(('mac', Version(10, 12)), map.from_name('sierra'))
        self.assertEqual(('mac', Version(10, 11)), map.from_name('El Capitan'))
        self.assertEqual(('mac', Version(10, 11)), map.from_name('elcapitan'))
        self.assertEqual(('mac', Version(10, 11)), map.from_name('el Capitan'))

    def test_ios_version_by_name(self):
        map = VersionNameMap()
        self.assertEqual(('ios', Version(11)), map.from_name('iOS 11'))
        self.assertEqual(('ios', Version(11)), map.from_name('ios11'))
        self.assertEqual(('ios', Version(11)), map.from_name('iOS 11.2'))
        self.assertEqual(('ios', Version(11)), map.from_name('ios11.2'))
        self.assertEqual(('ios', Version(11)), map.from_name('iOS11.2'))

    def test_mac_name_by_version(self):
        map = VersionNameMap()
        self.assertEqual('Catalina', map.to_name(version=Version(10, 15), platform='mac'))
        self.assertEqual('Mojave', map.to_name(version=Version(10, 14), platform='mac'))
        self.assertEqual('High Sierra', map.to_name(version=Version(10, 13), platform='mac'))
        self.assertEqual('High Sierra', map.to_name(version=Version(10, 13, 3), platform='mac'))
        self.assertEqual('Sierra', map.to_name(version=Version(10, 12), platform='mac'))
        self.assertEqual('El Capitan', map.to_name(version=Version(10, 11), platform='mac'))
        self.assertEqual('El Capitan', map.to_name(version=Version(10, 11, 3), platform='mac'))

    def test_ios_name_by_version(self):
        map = VersionNameMap()
        self.assertEqual('iOS 13', map.to_name(version=Version(13), platform='ios'))
        self.assertEqual('iOS 12', map.to_name(version=Version(12), platform='ios'))
        self.assertEqual('iOS 11', map.to_name(version=Version(11), platform='ios'))
        self.assertEqual('iOS 10', map.to_name(version=Version(10), platform='ios'))
        self.assertEqual('iOS 10', map.to_name(version=Version(10, 3), platform='ios'))
