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

import re

from webkitpy.common.memoized import memoized
from webkitpy.common.version import Version


PUBLIC_TABLE = 'public'
INTERNAL_TABLE = 'internal'


class VersionNameMap(object):

    # Allows apple_additions to define a custom mapping
    @staticmethod
    @memoized
    def map(platform=None):
        from webkitpy.port.config import apple_additions
        if apple_additions():
            return apple_additions().version_name_mapping(platform)
        return VersionNameMap(platform=platform)

    def __init__(self, platform=None):
        if platform is None:
            from webkitpy.common.system.systemhost import SystemHost
            platform = SystemHost().platform
        self.mapping = {}

        self.default_system_platform = platform.os_name
        self.mapping[PUBLIC_TABLE] = {
            'mac': {
                'Leopard': Version(10, 5),
                'Snow Leopard': Version(10, 6),
                'Lion': Version(10, 7),
                'Mountain Lion': Version(10, 8),
                'Mavericks': Version(10, 9),
                'Yosemite': Version(10, 10),
                'El Capitan': Version(10, 11),
                'Sierra': Version(10, 12),
                'High Sierra': Version(10, 13),
                'Mojave': Version(10, 14),
                'Catalina': Version(10, 15),
                'Big Sur': Version(11, 0),
            },
            'ios': self._automap_to_major_version('iOS', minimum=Version(10), maximum=Version(14)),
            'tvos': self._automap_to_major_version('tvOS', minimum=Version(10), maximum=Version(14)),
            'watchos': self._automap_to_major_version('watchOS', minimum=Version(1), maximum=Version(7)),
            'win': {
                'Win10': Version(10),
                '8.1': Version(6, 3),
                '8': Version(6, 2),
                '7sp0': Version(6, 1, 7600),
                'Vista': Version(6),
                'XP': Version(5, 1),
            },

            # This entry avoids hitting the assert in mapping_for_platform() on Linux,
            # but otherwise shouldn't contain any useful key-value pairs.
            'linux': {},
        }

        # wincairo uses the same versions as Windows
        self.mapping[PUBLIC_TABLE]['wincairo'] = self.mapping[PUBLIC_TABLE]['win']

        # FTW uses the same versions as Windows
        self.mapping[PUBLIC_TABLE]['ftw'] = self.mapping[PUBLIC_TABLE]['win']


    @classmethod
    def _automap_to_major_version(cls, prefix, minimum=Version(1), maximum=Version(1)):
        result = {}
        assert minimum <= maximum
        for i in range((maximum.major + 1) - minimum.major):
            result['{} {}'.format(prefix, str(Version(minimum.major + i)))] = Version(minimum.major + i)
        return result

    def to_name(self, version, platform=None, table=PUBLIC_TABLE):
        closest_match = (None, None)
        for os_name, os_version in self.mapping_for_platform(platform, table).items():
            if version == os_version:
                return os_name
            elif version in os_version:
                if closest_match[1] and closest_match[1] in os_version:
                    continue
                closest_match = (os_name, os_version)
        return closest_match[0]

    @staticmethod
    def strip_name_formatting(name):
        # <OS> major.minor.tiny should map to <OS> major
        if ' ' in name:
            try:
                name = '{}{}'.format(''.join(name.split(' ')[:-1]), Version.from_string(name.split(' ')[-1]).major)
            except ValueError:
                pass
        else:
            try:
                split = re.split(r'\d', name)
                name = '{}{}'.format(split[0], Version.from_string(name[(len(split) - 1):]).major)
            except ValueError:
                pass

        # Strip out any spaces, make everything lower-case
        result = name.replace(' ', '').lower()
        return result

    def from_name(self, name):
        # Exact match
        for _, map in self.mapping.items():
            for os_name, os_map in map.items():
                if name in os_map:
                    return (os_name, os_map[name])

        # It's not an exact match, let's try unifying formatting
        unformatted = self.strip_name_formatting(name)
        for _, map in self.mapping.items():
            for os_name, os_map in map.items():
                for version_name, version in os_map.items():
                    if self.strip_name_formatting(version_name) == unformatted:
                        return (os_name, version)
        return (None, None)

    def names(self, platform=None, table=PUBLIC_TABLE):
        """return list of os_name for platform"""
        mapping = self.mapping_for_platform(platform, table)
        names = [os_name for os_name in mapping]
        return sorted(names, key=lambda os_name: mapping[os_name])

    def mapping_for_platform(self, platform=None, table=PUBLIC_TABLE):
        """return proper os_name: os_version mapping for platform"""
        platform = self.default_system_platform if platform is None else platform
        return self.mapping.get(table, {}).get(platform, {})
