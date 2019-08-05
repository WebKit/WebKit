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

from webkitpy.common.version_name_map import VersionNameMap, INTERNAL_TABLE
from webkitpy.port.config import apple_additions


# This class is designed to match device types. Because it is used for matching, 'None' is treated as a wild-card.
class DeviceType(object):

    @classmethod
    def from_string(cls, device_string, version=None):
        """
        Converts a string into a DeviceType object. These strings should be of the form
        '<hardware_family> <hardware_type>', where <hardware_family> is mandatory and
        <hardware_family> is optional.

        Example input + output:
        ('iPhone 6 Plus') -> DeviceType(hardware_family='iPhone', hardware_type='6 Plus', software_version=None)
        ('iPhone', Version(11)) -> DeviceType(hardware_family='iPhone', hardware_type=None, software_version=Version(11))
        ('Apple TV 4K') -> DeviceType(hardware_family='TV', hardware_type='4K', software_version=None)

        :param device_string: String representing a device.
        :type device_string: str
        :param version: Version object of software run on the device.
        :type version: Version
        :returns: DeviceType object
        :rtype: DeviceType
        """
        split_str = device_string.split(' ')
        if len(split_str) == 1:
            return cls(hardware_family=device_string, software_version=version)
        family_index = 0
        if split_str[family_index].lower() == 'apple':
            family_index = 1
        return cls(
            hardware_family=split_str[family_index],
            hardware_type=' '.join(split_str[family_index + 1:]) if len(split_str) > family_index + 1 else None,
            software_version=version)

    def _define_software_variant_from_hardware_family(self):
        if self.hardware_family is None:
            return
        if self.software_variant:
            return

        self.software_variant = 'iOS'
        if self.hardware_family.lower().split(' ')[-1].startswith('watch'):
            self.hardware_family = 'Apple Watch'
            self.software_variant = 'watchOS'
        elif self.hardware_family.lower().split(' ')[-1].startswith('tv'):
            self.hardware_family = 'Apple TV'
            self.software_variant = 'tvOS'

    def check_consistency(self):
        if self.hardware_family is not None:
            assert self.software_variant is not None
            if self.hardware_family == 'Apple Watch':
                assert self.software_variant is 'watchOS'
            elif self.hardware_family == 'Apple TV':
                assert self.software_variant == 'tvOS'
            else:
                assert self.software_variant == 'iOS'

        if self.hardware_type is not None:
            assert self.hardware_family is not None
        if self.software_version:
            assert self.software_variant is not None

    def __init__(self, hardware_family=None, hardware_type=None, software_version=None, software_variant=None):
        """
        :param hardware_family: iPhone, iPad, Apple Watch and Apple TV are all examples.
        :type hardware_family: str
        :param hardware_type: 6s, Series 2 - 42mm, 4k are all examples
        :type hardware_type: str
        :param software_version: Version object representing software the device is running.
        :type software_version: Version
        :param software_variant: Groups together hardware families which share an OS, like iPad and iPhone. iOS, tvOS and watchOS are examples.
        :type software_variant: str
        """
        if hardware_family is None and hardware_type is None and software_version is None and software_variant is None:
            raise ValueError('Cannot instantiate DeviceType with no arguments')

        self.hardware_family = hardware_family
        self.hardware_type = hardware_type
        self.software_version = software_version
        self.software_variant = software_variant

        self._define_software_variant_from_hardware_family()
        self.check_consistency()

    def __str__(self):
        version = None
        if self.software_version and apple_additions():
            version = VersionNameMap.map().to_name(self.software_version, platform=self.software_variant.lower(), table=INTERNAL_TABLE)
        elif self.software_version:
            version = VersionNameMap.map().to_name(self.software_version, platform=self.software_variant.lower())

        return u'{hardware_family}{hardware_type} running {version}'.format(
            hardware_family=self.hardware_family if self.hardware_family else 'Device',
            hardware_type=u' {}'.format(self.hardware_type) if self.hardware_type else '',
            version=version or self.software_variant,
        )

    # This technique of matching treats 'None' a wild-card.
    def __eq__(self, other):
        assert isinstance(other, DeviceType)
        if self.hardware_family is not None and other.hardware_family is not None and self.hardware_family.lower() != other.hardware_family.lower():
            return False
        if self.hardware_type is not None and other.hardware_type is not None and self.hardware_type.lower() != other.hardware_type.lower():
            return False
        if self.software_variant is not None and other.software_variant is not None and self.software_variant.lower() != other.software_variant.lower():
            return False
        if self.software_version is not None and other.software_version is not None and self.software_version != other.software_version:
            return False
        return True

    def __contains__(self, other):
        assert isinstance(other, DeviceType)
        if self.hardware_family is not None and (not other.hardware_family or self.hardware_family.lower() != other.hardware_family.lower()):
            return False
        if self.hardware_type is not None and (not other.hardware_type or self.hardware_type.lower() != other.hardware_type.lower()):
            return False
        if self.software_variant is not None and (not other.software_variant or self.software_variant.lower() != other.software_variant.lower()):
            return False
        if self.software_version is not None and other.software_version is not None and not other.software_version in self.software_version:
            return False
        return True

    def __hash__(self):
        return hash((self.hardware_family, self.hardware_type, self.software_variant, self.software_version))
