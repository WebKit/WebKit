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
    ConfigurationCategory, get_token_category, matches_platform,
)


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
