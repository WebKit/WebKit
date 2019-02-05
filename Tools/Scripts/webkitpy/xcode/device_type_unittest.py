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

from webkitpy.common.version import Version
from webkitpy.xcode.device_type import DeviceType


class DeviceTypeTest(unittest.TestCase):

    def test_iphone_initialization(self):
        type = DeviceType(hardware_family='iPhone')
        self.assertEquals('iPhone', type.hardware_family)
        self.assertEquals(None, type.hardware_type)
        self.assertEquals('iOS', type.software_variant)
        self.assertEquals(None, type.software_version)
        self.assertEqual('iPhone running iOS', str(type))

        type = DeviceType('iPhone', '6s', Version(11))
        self.assertEquals('iPhone', type.hardware_family)
        self.assertEquals('6s', type.hardware_type)
        self.assertEquals('iOS', type.software_variant)
        self.assertEquals(Version(11), type.software_version)
        self.assertEqual('iPhone 6s running iOS 11', str(type))

    def test_ipad_initialization(self):
        type = DeviceType(hardware_family='iPad')
        self.assertEquals('iPad', type.hardware_family)
        self.assertEquals(None, type.hardware_type)
        self.assertEquals('iOS', type.software_variant)
        self.assertEquals(None, type.software_version)
        self.assertEqual('iPad running iOS', str(type))

        type = DeviceType('iPad', 'Air 2', Version(11))
        self.assertEquals('iPad', type.hardware_family)
        self.assertEquals('Air 2', type.hardware_type)
        self.assertEquals('iOS', type.software_variant)
        self.assertEquals(Version(11), type.software_version)
        self.assertEqual('iPad Air 2 running iOS 11', str(type))

    def test_generic_ios_device(self):
        type = DeviceType(software_variant='iOS')
        self.assertEquals(None, type.hardware_family)
        self.assertEquals(None, type.hardware_type)
        self.assertEquals('iOS', type.software_variant)
        self.assertEquals(None, type.software_version)
        self.assertEqual('Device running iOS', str(type))

    def test_watch_initialization(self):
        type = DeviceType(hardware_family='Watch')
        self.assertEquals('Apple Watch', type.hardware_family)
        self.assertEquals(None, type.hardware_type)
        self.assertEquals('watchOS', type.software_variant)
        self.assertEquals(None, type.software_version)
        self.assertEqual('Apple Watch running watchOS', str(type))

        type = DeviceType('Apple Watch', 'Series 2 - 42mm', Version(4))
        self.assertEquals('Apple Watch', type.hardware_family)
        self.assertEquals('Series 2 - 42mm', type.hardware_type)
        self.assertEquals('watchOS', type.software_variant)
        self.assertEquals(Version(4), type.software_version)
        self.assertEqual('Apple Watch Series 2 - 42mm running watchOS 4', str(type))

    def test_tv_initialization(self):
        type = DeviceType(hardware_family='TV')
        self.assertEquals('Apple TV', type.hardware_family)
        self.assertEquals(None, type.hardware_type)
        self.assertEquals('tvOS', type.software_variant)
        self.assertEquals(None, type.software_version)
        self.assertEqual('Apple TV running tvOS', str(type))

        type = DeviceType('Apple TV', '4K', Version(11))
        self.assertEquals('Apple TV', type.hardware_family)
        self.assertEquals('4K', type.hardware_type)
        self.assertEquals('tvOS', type.software_variant)
        self.assertEquals(Version(11), type.software_version)
        self.assertEqual('Apple TV 4K running tvOS 11', str(type))

    def test_from_string(self):
        self.assertEqual('iPhone 6s running iOS', str(DeviceType.from_string('iPhone 6s')))
        self.assertEqual('iPhone 6 Plus running iOS', str(DeviceType.from_string('iPhone 6 Plus')))
        self.assertEqual('iPhone running iOS 11', str(DeviceType.from_string('iPhone', Version(11))))
        self.assertEqual('iPad Air 2 running iOS', str(DeviceType.from_string('iPad Air 2')))
        self.assertEqual('Apple Watch Series 2 - 42mm running watchOS', str(DeviceType.from_string('Apple Watch Series 2 - 42mm')))
        self.assertEqual('Apple TV 4K running tvOS', str(DeviceType.from_string('Apple TV 4K')))
        self.assertEqual('Device running iOS', str(DeviceType.from_string('')))
        self.assertEqual('Apple Watch running watchOS', str(DeviceType.from_string('Apple Watch')))

    def test_comparison(self):
        # iPhone comparisons
        self.assertEqual(DeviceType.from_string('iPhone 6s'), DeviceType.from_string('iPhone'))
        self.assertEqual(DeviceType.from_string('iPhone X'), DeviceType.from_string('iPhone'))
        self.assertNotEqual(DeviceType.from_string('iPhone 6s'), DeviceType.from_string('iPhone X'))

        # iPad comparisons
        self.assertEqual(DeviceType.from_string('iPad Air 2'), DeviceType.from_string('iPad'))
        self.assertEqual(DeviceType.from_string('iPad Pro (12.9-inch)'), DeviceType.from_string('iPad'))
        self.assertNotEqual(DeviceType.from_string('iPad Air 2'), DeviceType.from_string('iPad Pro (12.9-inch)'))

        # Apple Watch comparisons
        self.assertEqual(DeviceType.from_string('Apple Watch Series 2 - 38mm'), DeviceType.from_string('Apple Watch'))
        self.assertEqual(DeviceType.from_string('Apple Watch Series 2 - 42mm'), DeviceType.from_string('Apple Watch'))
        self.assertNotEqual(DeviceType.from_string('Apple Watch Series 2 - 38mm'), DeviceType.from_string('Apple Watch Series 2 - 42mm'))

        # Apple TV comparisons
        self.assertEqual(DeviceType.from_string('Apple TV 4K'), DeviceType.from_string('Apple TV'))
        self.assertEqual(DeviceType.from_string('Apple TV 4K (at 1080p)'), DeviceType.from_string('Apple TV'))
        self.assertNotEqual(DeviceType.from_string('Apple TV 4K'), DeviceType.from_string('Apple TV 4K (at 1080p)'))

        # Match by software_variant
        self.assertEqual(DeviceType.from_string('iPhone 6s'), DeviceType(software_variant='iOS'))
        self.assertEqual(DeviceType.from_string('iPad Air 2'), DeviceType(software_variant='iOS'))
        self.assertNotEqual(DeviceType.from_string('Apple Watch Series 2 - 42mm'), DeviceType(software_variant='iOS'))
        self.assertNotEqual(DeviceType.from_string('Apple TV 4K'), DeviceType(software_variant='iOS'))

        # Cross-device comparisons
        self.assertNotEqual(DeviceType.from_string('iPad'), DeviceType.from_string('iPhone'))
        self.assertNotEqual(DeviceType.from_string('Apple Watch'), DeviceType.from_string('iPhone'))
        self.assertNotEqual(DeviceType.from_string('Apple Watch'), DeviceType.from_string('Apple TV'))

    def test_contained_in(self):
        self.assertTrue(DeviceType.from_string('iPhone 6s') in DeviceType.from_string('iPhone'))
        self.assertFalse(DeviceType.from_string('iPhone') in DeviceType.from_string('iPhone 6s'))
        self.assertTrue(DeviceType.from_string('iPhone', Version(11, 1)) in DeviceType.from_string('iPhone', Version(11)))
        self.assertFalse(DeviceType.from_string('iPhone', Version(11)) in DeviceType.from_string('iPhone', Version(11, 1)))

    def test_comparison_lower_case(self):
        self.assertEqual(DeviceType.from_string('iphone X'), DeviceType.from_string('iPhone'))
        self.assertEqual(DeviceType.from_string('iphone'), DeviceType.from_string('iPhone X'))
        self.assertEqual(DeviceType.from_string('iPhone X'), DeviceType.from_string('iphone'))
        self.assertEqual(DeviceType.from_string('iPhone'), DeviceType.from_string('iphone X'))
        self.assertEqual(DeviceType.from_string('iphone X'), DeviceType.from_string('iphone'))
        self.assertEqual(DeviceType.from_string('iphone'), DeviceType.from_string('iphone X'))

        self.assertTrue(DeviceType.from_string('iphone 6s') in DeviceType.from_string('iPhone'))
        self.assertTrue(DeviceType.from_string('iPhone 6s') in DeviceType.from_string('iphone'))
        self.assertTrue(DeviceType.from_string('iphone 6s') in DeviceType.from_string('iphone'))

    def test_unmapped_version(self):
        self.assertEqual('iPhone running iOS', str(DeviceType.from_string('iPhone', Version(9))))
