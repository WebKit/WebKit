# Copyright (C) 2025 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import tempfile
from pathlib import Path
from unittest import TestCase

from .allow import AllowList, AllowedSPI, PermanentlyAllowedReason

Toml = b'''
[key1."rdar://123456789"]
symbols = ["_TemporarilyAllowedSymbol"]
selectors = ["_initWithTemporarilyAllowedData:"]
classes = ["NSTemporarilyAllowed"]

[key2.not-web-essential]
symbols = ["_Permanent1", "_Permanent2"]
requires = ["ENABLE_FOO", "!ENABLE_BAR"]
'''

A1 = AllowedSPI(key='key1', bug='rdar://123456789',
                symbols=['_TemporarilyAllowedSymbol'],
                selectors=['_initWithTemporarilyAllowedData:'],
                classes=['NSTemporarilyAllowed'])
A2 = AllowedSPI(key='key2', bug=PermanentlyAllowedReason.NOT_WEB_ESSENTIAL,
                symbols=['_Permanent1', '_Permanent2'],
                selectors=[], classes=[], requires=['ENABLE_FOO', '!ENABLE_BAR'])


class TestAllowList(TestCase):
    def setUp(self):
        self.tempfile = tempfile.NamedTemporaryFile(prefix='TestAllowList-')
        self.tempfile.write(Toml)
        self.tempfile.flush()

        self.file = Path(self.tempfile.name)

    def tearDown(self):
        self.tempfile.close()

    def test_parse(self):
        # When parsing the allowlist fixture...
        allowlist = AllowList.from_file(self.file)
        # It should load the two allowances:
        self.assertIn(A1, allowlist.allowed_spi)
        self.assertIn(A2, allowlist.allowed_spi)

    def test_allowed_reasons(self):
        # It supports the permanent exception categories:
        AllowList.from_dict({'key1': {'legacy': {'classes': ['Foo']}}})
        AllowList.from_dict({'key1': {'not-web-essential':
                                      {'classes': ['Foo']}}})
        AllowList.from_dict({'key1': {'equivalent-api': {'classes': ['Foo']}}})

        # It supports temporary exceptions from bugzilla URLs:
        AllowList.from_dict({'key1':
                             {'https://bugs.webkit.org/show_bug.cgi?id=12345':
                              {'classes': ['Foo']}}})
        AllowList.from_dict({'key1': {'https://webkit.org/b/12345':
                                      {'classes': ['Foo']}}})

        # It rejects made up category names:
        with self.assertRaisesRegex(ValueError, 'category-that-doesnt-exist'):
            AllowList.from_dict({'key1': {'category-that-doesnt-exist':
                                          {'classes': ['Foo']}}})

    def test_no_repetition(self):
        with self.assertRaisesRegex(ValueError, 'already mentioned in '
                                    'allowlist at "key1"."rdar://1"'):
            AllowList.from_dict(
                {'key1': {'rdar://1': {'classes': ['Foo']}},
                 'key2': {'rdar://2': {'classes': ['Foo']}}}
            )

    def test_repetition_allowed_with_requires(self):
        AllowList.from_dict(
            {'key1': {'rdar://1': {'classes': ['Foo'], 'requires': ['A']}},
             'key2': {'rdar://2': {'classes': ['Foo'], 'requires': ['B']}}}
        )

    def test_repeated_requirements(self):
        AllowList.from_dict(
            {'key1': {'rdar://1': {'classes': ['Foo'], 'requires': ['A', 'B']}},
             'key2': {'rdar://2': {'classes': ['Bar'], 'requires': ['A', 'B']}}}
        )
        with self.assertRaisesRegex(ValueError, 'already mentioned in '
                                    'allowlist at "key1"."rdar://1"'):
            AllowList.from_dict(
                {'key1': {'rdar://1': {'classes': ['Foo'],
                                       'requires': ['A', 'B', 'A']}}}
            )


    def test_no_string(self):
        with self.assertRaisesRegex(ValueError, '"Foo" in allowlist is a '
                                    'string, expected a list'):
            AllowList.from_dict(
                {'key1': {'rdar://1': {'classes': 'Foo'}}},
            )
