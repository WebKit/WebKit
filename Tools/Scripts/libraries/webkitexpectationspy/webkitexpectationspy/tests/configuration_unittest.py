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

"""Tests for configuration token classification."""

import unittest

from webkitexpectationspy.configuration import (
    ConfigurationCategory, ConfigurationSpecifier, canonical_token, get_token_category, matches_platform,
)
from webkitexpectationspy.version_specifier import VersionSpecifier


class PlatformClassificationTest(unittest.TestCase):
    """Platform recognition covers both the explicit Platform enum and the
    ``*os`` shape regex, so a future platform like ``xros`` is recognized
    without amending the enum."""

    def test_explicit_platform_tokens_classify_as_platform(self):
        for token in ('mac', 'ios', 'watchos', 'tvos', 'visionos', 'linux', 'win', 'gtk', 'wpe'):
            self.assertEqual(
                get_token_category(token), ConfigurationCategory.PLATFORM,
                msg='{} should classify as PLATFORM'.format(token))

    def test_unknown_os_shape_classifies_as_platform(self):
        # xros is not in the Platform enum but matches the *os shape, so
        # the library should still treat it as a platform.
        self.assertEqual(get_token_category('xros'), ConfigurationCategory.PLATFORM)
        self.assertTrue(matches_platform('xros'))

    def test_unknown_non_os_token_does_not_classify_as_platform(self):
        # "foobar" is not a platform shape; it falls through to FLAVOR.
        self.assertEqual(get_token_category('foobar'), ConfigurationCategory.FLAVOR)
        self.assertFalse(matches_platform('foobar'))


class TokenCategoryTest(unittest.TestCase):

    def test_style_tokens(self):
        for token in ('debug', 'release', 'production'):
            self.assertEqual(get_token_category(token), ConfigurationCategory.STYLE)

    def test_architecture_tokens(self):
        for token in ('arm64', 'x86_64'):
            self.assertEqual(get_token_category(token), ConfigurationCategory.ARCHITECTURE)

    def test_hardware_tokens(self):
        for token in ('simulator', 'device', 'iphone', 'ipad'):
            self.assertEqual(get_token_category(token), ConfigurationCategory.HARDWARE)

    def test_version_specifier_tokens(self):
        self.assertEqual(get_token_category('sonoma+'), ConfigurationCategory.VERSION)
        self.assertEqual(get_token_category('ventura-sequoia'), ConfigurationCategory.VERSION)

    def test_guard_malloc_is_style(self):
        self.assertEqual(get_token_category('Guard-Malloc', {'sonoma'}), ConfigurationCategory.STYLE)
        self.assertEqual(canonical_token('Guard-Malloc'), 'guardmalloc')

    def test_version_shape_requires_known_versions(self):
        versions = {'sonoma', 'tahoe'}
        self.assertEqual(get_token_category('Tahoe+', versions), ConfigurationCategory.VERSION)
        self.assertEqual(get_token_category('sonoma-tahoe', versions), ConfigurationCategory.VERSION)
        self.assertEqual(get_token_category('15.0+', versions), ConfigurationCategory.VERSION)
        self.assertEqual(get_token_category('bogus+', versions, frozenset()), None)

    def test_bare_version_name(self):
        self.assertEqual(get_token_category('Tahoe', {'tahoe'}), ConfigurationCategory.VERSION)

    def test_unknown_flavor_is_rejected_when_flavors_are_declared(self):
        self.assertEqual(get_token_category('wk2', flavor_tokens=frozenset({'wk2'})), ConfigurationCategory.FLAVOR)
        self.assertIsNone(get_token_category('wk3', flavor_tokens=frozenset({'wk2'})))

    def test_any_identifier_is_a_flavor_without_declared_flavors(self):
        self.assertEqual(get_token_category('wk3'), ConfigurationCategory.FLAVOR)


class ConfigurationSpecifierMatchTest(unittest.TestCase):

    def test_tokens_in_one_category_match_any(self):
        spec = ConfigurationSpecifier.from_tokens({'mac', 'ios'})
        self.assertTrue(spec.matches({'mac'}))
        self.assertTrue(spec.matches({'ios'}))
        self.assertFalse(spec.matches({'gtk'}))

    def test_categories_must_all_match(self):
        spec = ConfigurationSpecifier.from_tokens({'mac', 'debug', 'release'})
        self.assertTrue(spec.matches({'mac', 'release'}))
        self.assertFalse(spec.matches({'ios', 'release'}))
        self.assertFalse(spec.matches({'mac', 'guardmalloc'}))

    def test_flavors_must_all_match(self):
        spec = ConfigurationSpecifier.from_tokens({'wk2', 'siteisolation'})
        self.assertTrue(spec.matches({'wk2', 'siteisolation'}))
        self.assertFalse(spec.matches({'wk2'}))

    def test_version_specifiers_match_any(self):
        spec = ConfigurationSpecifier.from_tokens(set(), (VersionSpecifier('sonoma'), VersionSpecifier('sequoia')))
        order = ['sonoma', 'sequoia', 'tahoe']
        self.assertTrue(spec.matches(set(), 'sonoma', order))
        self.assertTrue(spec.matches(set(), 'sequoia', order))
        self.assertFalse(spec.matches(set(), 'tahoe', order))

    def test_version_alias_matches_by_number(self):
        spec = ConfigurationSpecifier.from_tokens(set(), (VersionSpecifier.parse('Cheer+'),))
        version_name_map = {'sequoia': (15,), 'tahoe': (26,), 'cheer': (26,)}
        self.assertTrue(spec.matches(set(), 'tahoe', version_name_map=version_name_map))
        self.assertFalse(spec.matches(set(), 'sequoia', version_name_map=version_name_map))

    def test_version_from_another_platform_does_not_match(self):
        spec = ConfigurationSpecifier.from_tokens(set(), (VersionSpecifier.parse('Tahoe+'),))
        self.assertFalse(spec.matches(set(), 'ios26', ['ios26'], {'ios26': (26,)}))

    def test_version_without_current_version_does_not_match(self):
        spec = ConfigurationSpecifier.from_tokens(set(), (VersionSpecifier('tahoe'),))
        self.assertFalse(spec.matches(set(), None, ['tahoe']))

    def test_specificity_counts_categories_not_tokens(self):
        self.assertEqual(ConfigurationSpecifier.from_tokens(set()).specificity, 0)
        self.assertEqual(ConfigurationSpecifier.from_tokens({'debug', 'release'}).specificity, 1)
        self.assertEqual(ConfigurationSpecifier.from_tokens({'mac', 'debug', 'wk2'}).specificity, 3)
