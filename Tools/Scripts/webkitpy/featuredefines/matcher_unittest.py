# Copyright (C) 2022 Sony Interactive Entertainment
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Tests for matcher.py."""


from __future__ import print_function

import unittest

from webkitpy.featuredefines.matcher import flag_matcher, usage_matcher, idl_usage_matcher, cmake_options_matcher, declaration_matcher


class TestFeatureDefineMatcher(unittest.TestCase):
    def test_usage_matcher(self):
        matcher = usage_matcher()

        self._not_a_match(matcher, 'ENABLE_FOO')
        self._not_a_match(matcher, '#define ENABLE_FOO 1')
        self._not_a_match(matcher, '[Conditional=FOO]')
        self._not_a_match(matcher, 'ENABLE_FOO =')
        self._not_a_match(matcher, 'WEBKIT_OPTION_DEFINE(ENABLE_FOO "Info" PUBLIC ON)')
        self._not_a_match(matcher, 'WEBKIT_OPTION_DEFINE(ENABLE_FOO "Info" PRIVATE ON)')

        self._not_a_match(matcher, 'USE(FOO)')

        self._is_a_match(matcher, 'ENABLE(FOO)', 'ENABLE_FOO')
        self._is_a_match(matcher, 'ENABLE(BAR)', 'ENABLE_BAR')

        # Test with different prefix
        matcher = usage_matcher('USE')

        self._not_a_match(matcher, 'ENABLE(FOO)')

        self._is_a_match(matcher, 'USE(FOO)', 'USE_FOO')
        self._is_a_match(matcher, 'USE(BAR)', 'USE_BAR')

    def test_declaration_matcher(self):
        matcher = declaration_matcher()

        self._not_a_match(matcher, 'ENABLE(FOO)')
        self._not_a_match(matcher, 'ENABLE_FOO')
        self._not_a_match(matcher, '[Conditional=FOO]')
        self._not_a_match(matcher, 'ENABLE_FOO =')
        self._not_a_match(matcher, 'WEBKIT_OPTION_DEFINE(ENABLE_FOO "Info" PUBLIC ON)')
        self._not_a_match(matcher, 'WEBKIT_OPTION_DEFINE(ENABLE_FOO "Info" PRIVATE ON)')

        self._not_a_match(matcher, '#define USE_FOO 1')

        self._is_a_match(matcher, '#define ENABLE_FOO 1', 'ENABLE_FOO', value=True)
        self._is_a_match(matcher, '#define ENABLE_BAR 0', 'ENABLE_BAR', value=False)

        # Test with different prefix
        matcher = declaration_matcher('USE')

        self._not_a_match(matcher, '#define ENABLE_FOO 1')

        self._is_a_match(matcher, '#define USE_FOO 1', 'USE_FOO', value=True)
        self._is_a_match(matcher, '#define USE_BAR 0', 'USE_BAR', value=False)

    def test_flag_matcher(self):
        matcher = flag_matcher()

        self._not_a_match(matcher, 'ENABLE(FOO)')
        self._not_a_match(matcher, '[Conditional=FOO]')

        self._not_a_match(matcher, 'USE_FOO')

        self._is_a_match(matcher, 'ENABLE_FOO', 'ENABLE_FOO')
        self._is_a_match(matcher, 'ENABLE_BAR', 'ENABLE_BAR')

        # Still would match these
        self._is_a_match(matcher, '#define ENABLE_FOO 1', 'ENABLE_FOO')
        self._is_a_match(matcher, 'ENABLE_FOO =', 'ENABLE_FOO')
        self._is_a_match(matcher, 'WEBKIT_OPTION_DEFINE(ENABLE_FOO "Info" PUBLIC ON)', 'ENABLE_FOO')
        self._is_a_match(matcher, 'WEBKIT_OPTION_DEFINE(ENABLE_FOO "Info" PRIVATE ON)', 'ENABLE_FOO')

        # Test with different prefix
        matcher = flag_matcher('USE')

        self._not_a_match(matcher, 'ENABLE_FOO')

        self._is_a_match(matcher, 'USE_FOO', 'USE_FOO')
        self._is_a_match(matcher, 'USE_BAR', 'USE_BAR')

    def test_idl_matcher(self):
        matcher = idl_usage_matcher()

        self._not_a_match(matcher, 'ENABLE(FOO)')
        self._not_a_match(matcher, 'ENABLE_FOO')
        self._not_a_match(matcher, '#define ENABLE_FOO 1')
        self._not_a_match(matcher, 'ENABLE_FOO =')
        self._not_a_match(matcher, 'WEBKIT_OPTION_DEFINE(ENABLE_FOO "Info" PUBLIC ON)')
        self._not_a_match(matcher, 'WEBKIT_OPTION_DEFINE(ENABLE_FOO "Info" PRIVATE ON)')

        self._is_a_match(matcher, '[Conditional=FOO]', 'ENABLE_FOO')
        self._is_a_match(matcher, '[Conditional=BAR]', 'ENABLE_BAR')

    def test_cmake_options_matcher(self):
        matcher = cmake_options_matcher()

        self._not_a_match(matcher, 'ENABLE(FOO)')
        self._not_a_match(matcher, 'ENABLE_FOO')
        self._not_a_match(matcher, '#define ENABLE_FOO 1')
        self._not_a_match(matcher, 'ENABLE_FOO =')

        self._not_a_match(matcher, 'WEBKIT_OPTION_DEFINE(USE_FOO "Info" PRIVATE ON)')

        self._is_a_match(matcher, 'WEBKIT_OPTION_DEFINE(ENABLE_FOO "Info" PUBLIC ON)', 'ENABLE_FOO', value=True, description='Info')
        self._is_a_match(matcher, 'WEBKIT_OPTION_DEFINE(ENABLE_BAR "Mesg" PUBLIC OFF)', 'ENABLE_BAR', value=False, description='Mesg')
        self._is_a_match(matcher, 'WEBKIT_OPTION_DEFINE(ENABLE_FOO "Info" PRIVATE ON)', 'ENABLE_FOO', value=True, description='Info')
        self._is_a_match(matcher, 'WEBKIT_OPTION_DEFINE(ENABLE_BAR "Mesg" PRIVATE OFF)', 'ENABLE_BAR', value=False, description='Mesg')

        # Test with different prefix
        matcher = cmake_options_matcher('USE')

        self._not_a_match(matcher, 'WEBKIT_OPTION_DEFINE(ENABLE_FOO "Info" PUBLIC ON)')
        self._not_a_match(matcher, 'WEBKIT_OPTION_DEFINE(ENABLE_FOO "Info" PRIVATE ON)')

        self._is_a_match(matcher, 'WEBKIT_OPTION_DEFINE(USE_FOO "Info" PRIVATE ON)', 'USE_FOO', value=True, description='Info')
        self._is_a_match(matcher, 'WEBKIT_OPTION_DEFINE(USE_BAR "Mesg" PRIVATE OFF)', 'USE_BAR', value=False, description='Mesg')

    def _not_a_match(self, matcher, line):
        self.assertIsNone(matcher(line))

    def _is_a_match(self, matcher, line, flag, value=False, description=""):
        match = matcher(line)

        self.assertIsNotNone(match)
        self.assertEqual(match.flag, flag)
        self.assertEqual(match.value, value)
        self.assertEqual(match.description, description)


if __name__ == '__main__':
    unittest.main()
