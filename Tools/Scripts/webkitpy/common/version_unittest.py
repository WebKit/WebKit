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

from version import Version


class VersionTestCase(unittest.TestCase):

    def test_string_constructor(self):
        v = Version.from_string('1.2.3.4.5')
        self.assertEqual(v.major, 1)
        self.assertEqual(v.minor, 2)
        self.assertEqual(v.tiny, 3)
        self.assertEqual(v.micro, 4)
        self.assertEqual(v.nano, 5)

    def test_from_list(self):
        v = Version.from_iterable([1, 2, 3, 4, 5])
        self.assertEqual(v.major, 1)
        self.assertEqual(v.minor, 2)
        self.assertEqual(v.tiny, 3)
        self.assertEqual(v.micro, 4)
        self.assertEqual(v.nano, 5)

    def test_from_tuple(self):
        v = v = Version.from_iterable((1, 2, 3))
        self.assertEqual(v.major, 1)
        self.assertEqual(v.minor, 2)
        self.assertEqual(v.tiny, 3)

    def test_int_constructor(self):
        v = Version(1)
        self.assertEqual(v.major, 1)
        self.assertEqual(v.minor, 0)
        self.assertEqual(v.tiny, 0)
        self.assertEqual(v.micro, 0)
        self.assertEqual(v.nano, 0)

    def test_len(self):
        self.assertEqual(len(Version(1, 2, 3, 4, 5)), 5)
        self.assertEqual(len(Version()), 5)

    def test_set_by_int(self):
        v = Version()
        v[0] = 1
        self.assertEqual(v.major, 1)
        v[1] = 2
        self.assertEqual(v.minor, 2)
        v[2] = 3
        self.assertEqual(v.tiny, 3)
        v[3] = 4
        self.assertEqual(v.micro, 4)
        v[4] = 5
        self.assertEqual(v.nano, 5)

    def test_set_by_string(self):
        v = Version()
        v['major'] = 1
        self.assertEqual(v.major, 1)
        v['minor'] = 2
        self.assertEqual(v.minor, 2)
        v['tiny'] = 3
        self.assertEqual(v.tiny, 3)
        v['micro'] = 4
        self.assertEqual(v.micro, 4)
        v['nano'] = 5
        self.assertEqual(v.nano, 5)

    def test_get_by_int(self):
        v = Version(1, 2, 3, 4, 5)
        self.assertEqual(v[0], v.major)
        self.assertEqual(v[1], v.minor)
        self.assertEqual(v[2], v.tiny)
        self.assertEqual(v[3], v.micro)
        self.assertEqual(v[4], v.nano)

    def test_get_by_string(self):
        v = Version(1, 2, 3, 4, 5)
        self.assertEqual(v['major'], v.major)
        self.assertEqual(v['minor'], v.minor)
        self.assertEqual(v['tiny'], v.tiny)
        self.assertEqual(v['micro'], v.micro)
        self.assertEqual(v['nano'], v.nano)

    def test_string(self):
        self.assertEqual(str(Version(1, 2, 3)), '1.2.3')
        self.assertEqual(str(Version(1, 2, 0)), '1.2')
        self.assertEqual(str(Version(1, 2)), '1.2')
        self.assertEqual(str(Version(0, 0, 3)), '0.0.3')

    def test_contained_in(self):
        self.assertTrue(Version(11, 1) in Version(11))
        self.assertTrue(Version(11, 1, 2) in Version(11, 1))
        self.assertFalse(Version(11) in Version(11, 1))
        self.assertFalse(Version(11) in Version(11, 1, 2))
        self.assertFalse(Version(11, 1) in Version(11, 1, 2))
        self.assertTrue(Version(11) in Version(11))
        self.assertTrue(Version(11, 1) in Version(11, 1))
        self.assertTrue(Version(11, 1, 2) in Version(11, 1, 2))
        self.assertTrue(Version(11) in Version(11, 0))
        self.assertTrue(Version(11, 0) in Version(11))
        self.assertTrue(Version(11) in Version(11, 0, 0))
        self.assertTrue(Version(11, 0, 0) in Version(11))
        self.assertTrue(Version(11, 1) in Version(11, 1, 0))
        self.assertTrue(Version(11, 1, 0) in Version(11, 1))

    def test_compare_versions(self):
        self.assertEqual(Version(1, 2, 3), Version(1, 2, 3))
        self.assertGreater(Version(1, 2, 4), Version(1, 2, 3))
        self.assertGreater(Version(1, 3, 2), Version(1, 2, 3))
        self.assertGreater(Version(2, 1, 1), Version(1, 2, 3))
        self.assertNotEqual(Version(1, 2, 3), None)
