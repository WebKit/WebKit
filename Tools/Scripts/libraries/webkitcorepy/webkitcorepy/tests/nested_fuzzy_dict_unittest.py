# Copyright (C) 2021 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
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

from webkitcorepy import NestedFuzzyDict


class TestNestedFuzzyDict(unittest.TestCase):
    def test_constructor(self):
        d = NestedFuzzyDict(value_a=1, value_b=2, other_value=3)
        self.assertEqual(sorted(list(d.values())), sorted((1, 2, 3)))
        self.assertEqual(sorted(list(d.keys())), sorted(('value_a', 'value_b', 'other_value')))
        self.assertEqual(
            sorted(list(d.items())),
            sorted((('value_a', 1), ('value_b', 2), ('other_value', 3))),
        )

    def test_index(self):
        d = NestedFuzzyDict(value_a=1, value_b=2, other_value=3)
        self.assertEqual(d['value_a'], 1)
        self.assertEqual(d['value_b'], 2)
        self.assertEqual(d['other_'], 3)

        with self.assertRaises(KeyError):
            self.assertEqual(d['nothing'], None)

        with self.assertRaises(KeyError):
            self.assertEqual(d['value'], None)

    def test_get(self):
        d = NestedFuzzyDict(value_a=1, value_b=2, other_value=3)
        self.assertEqual(d.get('value_a'), 1)
        self.assertEqual(d.get('value_b'), 2)
        self.assertEqual(d.get('other_'), 3)
        self.assertEqual(d.get('nothing'), None)

        with self.assertRaises(KeyError):
            self.assertEqual(d.get('value_'), None)

    def test_getitem(self):
        d = NestedFuzzyDict(value_a=1, value_b=2, other_value=3)
        self.assertEqual(d.getitem('value_a'), ('value_a', 1))
        self.assertEqual(d.getitem('value_b'), ('value_b', 2))
        self.assertEqual(d.getitem('other_'), ('other_value', 3))
        self.assertEqual(d.getitem('nothing'), (None, None))

        with self.assertRaises(KeyError):
            self.assertEqual(d.getitem('value_'), (None, None))

    def test_set(self):
        d = NestedFuzzyDict()
        self.assertEqual(d.get('somelongvalue'), None)
        d['somelongvalue'] = 1
        self.assertEqual(d.get('somelongvalue'), 1)
        self.assertEqual(d.get('somelong'), 1)

    def test_delitem(self):
        d = NestedFuzzyDict(value_a=1, value_b=2, other_value=3)
        del d['other_value']
        self.assertEqual(len(d), 2)
        del d['value_']
        self.assertEqual(len(d), 0)

    def test_contains(self):
        d = NestedFuzzyDict(value_a=1, value_b=2, other_value=3)
        self.assertTrue('value_a' in d)
        self.assertTrue('value_' in d)
        self.assertFalse('content' in d)

    def test_len(self):
        self.assertEqual(len(NestedFuzzyDict(value_a=1, value_b=2, other_value=3)), 3)

    def test_dict(self):
        d = NestedFuzzyDict(value_a=1, value_b=2, other_value=3)
        self.assertDictEqual(d.dict(), dict(value_a=1, value_b=2, other_value=3))
