# Copyright (C) 2024-2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Tests for version specifier handling."""

import unittest

from webkitexpectationspy.version_specifier import VersionSpecifier


class VersionSpecifierParseTest(unittest.TestCase):

    def test_parse_exact_version_returns_none(self):
        spec = VersionSpecifier.parse('Sonoma')
        self.assertIsNone(spec)

    def test_parse_and_later(self):
        spec = VersionSpecifier.parse('Sonoma+')
        self.assertIsNotNone(spec)
        self.assertEqual(spec.base_version, 'sonoma')
        self.assertEqual(spec.type, VersionSpecifier.Type.AND_LATER)
        self.assertIsNone(spec.end_version)

    def test_parse_and_earlier(self):
        spec = VersionSpecifier.parse('Ventura-')
        self.assertIsNotNone(spec)
        self.assertEqual(spec.base_version, 'ventura')
        self.assertEqual(spec.type, VersionSpecifier.Type.AND_EARLIER)

    def test_parse_range(self):
        spec = VersionSpecifier.parse('Ventura-Sequoia')
        self.assertIsNotNone(spec)
        self.assertEqual(spec.base_version, 'ventura')
        self.assertEqual(spec.end_version, 'sequoia')
        self.assertEqual(spec.type, VersionSpecifier.Type.RANGE)

    def test_parse_numeric_version_and_later(self):
        spec = VersionSpecifier.parse('15.0+')
        self.assertIsNotNone(spec)
        self.assertEqual(spec.base_version, '15.0')
        self.assertEqual(spec.type, VersionSpecifier.Type.AND_LATER)

    def test_parse_numeric_range(self):
        spec = VersionSpecifier.parse('13.0-15.0')
        self.assertIsNotNone(spec)
        self.assertEqual(spec.base_version, '13.0')
        self.assertEqual(spec.end_version, '15.0')
        self.assertEqual(spec.type, VersionSpecifier.Type.RANGE)


class VersionSpecifierMatchTest(unittest.TestCase):

    def setUp(self):
        self.version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']

    def test_matches_exact(self):
        spec = VersionSpecifier('sonoma', specifier_type=VersionSpecifier.Type.EXACT)
        self.assertTrue(spec.matches('sonoma', self.version_order))
        self.assertFalse(spec.matches('ventura', self.version_order))

    def test_matches_and_later(self):
        spec = VersionSpecifier('ventura', specifier_type=VersionSpecifier.Type.AND_LATER)
        self.assertFalse(spec.matches('monterey', self.version_order))
        self.assertTrue(spec.matches('ventura', self.version_order))
        self.assertTrue(spec.matches('sonoma', self.version_order))
        self.assertTrue(spec.matches('sequoia', self.version_order))

    def test_matches_and_earlier(self):
        spec = VersionSpecifier('ventura', specifier_type=VersionSpecifier.Type.AND_EARLIER)
        self.assertTrue(spec.matches('monterey', self.version_order))
        self.assertTrue(spec.matches('ventura', self.version_order))
        self.assertFalse(spec.matches('sonoma', self.version_order))

    def test_matches_range(self):
        spec = VersionSpecifier('ventura', 'sonoma', VersionSpecifier.Type.RANGE)
        self.assertFalse(spec.matches('monterey', self.version_order))
        self.assertTrue(spec.matches('ventura', self.version_order))
        self.assertTrue(spec.matches('sonoma', self.version_order))
        self.assertFalse(spec.matches('sequoia', self.version_order))

    def test_matches_unknown_version(self):
        spec = VersionSpecifier('unknown', specifier_type=VersionSpecifier.Type.AND_LATER)
        self.assertFalse(spec.matches('sonoma', self.version_order))

    def test_matches_case_insensitive(self):
        spec = VersionSpecifier('Sonoma', specifier_type=VersionSpecifier.Type.EXACT)
        self.assertTrue(spec.matches('SONOMA', self.version_order))
        self.assertTrue(spec.matches('sonoma', self.version_order))


class VersionSpecifierNumericTest(unittest.TestCase):

    def setUp(self):
        self.version_order = ['monterey', 'ventura', 'sonoma', 'sequoia']
        self.version_name_map = {
            'monterey': (12,),
            'ventura': (13,),
            'sonoma': (14,),
            'sequoia': (15,),
        }

    def test_numeric_exact(self):
        spec = VersionSpecifier('sonoma', specifier_type=VersionSpecifier.Type.EXACT)
        self.assertTrue(spec.matches('sonoma', self.version_order, self.version_name_map))
        self.assertFalse(spec.matches('ventura', self.version_order, self.version_name_map))

    def test_numeric_and_later(self):
        spec = VersionSpecifier('ventura', specifier_type=VersionSpecifier.Type.AND_LATER)
        self.assertFalse(spec.matches('monterey', self.version_order, self.version_name_map))
        self.assertTrue(spec.matches('ventura', self.version_order, self.version_name_map))
        self.assertTrue(spec.matches('sonoma', self.version_order, self.version_name_map))

    def test_numeric_range(self):
        spec = VersionSpecifier('ventura', 'sonoma', VersionSpecifier.Type.RANGE)
        self.assertFalse(spec.matches('monterey', self.version_order, self.version_name_map))
        self.assertTrue(spec.matches('ventura', self.version_order, self.version_name_map))
        self.assertTrue(spec.matches('sonoma', self.version_order, self.version_name_map))
        self.assertFalse(spec.matches('sequoia', self.version_order, self.version_name_map))

    def test_raw_version_number_matching(self):
        spec = VersionSpecifier('14.0', specifier_type=VersionSpecifier.Type.AND_LATER)
        # '14.0' matches via raw number even though it's not in version_order
        self.assertTrue(spec.matches('sonoma', ['monterey', 'ventura', 'sonoma'],
                                     self.version_name_map))
        self.assertFalse(spec.matches('monterey', ['monterey', 'ventura', 'sonoma'],
                                      self.version_name_map))


if __name__ == '__main__':
    unittest.main()
