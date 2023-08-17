# Copyright (C) 2020-2023 Apple Inc. All rights reserved.
# Copyright (C) 2021 Igalia S.L.
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

from .events import CommitClassifier


class TestCommitClassifier(unittest.TestCase):
    def test_regex_header_filter(self):
        self.assertTrue(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('Versioning.'))
        self.assertTrue(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('versioning.'))
        self.assertTrue(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('Versioning'))

        self.assertFalse(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('Bumped versioning'))
        self.assertFalse(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('Versioning bumped.'))

    def test_fuzzy_header_filter(self):
        self.assertTrue(CommitClassifier.LineFilter({"value": "gardening", "ratio": 85})('[Gardening] Skip n tests'))
        self.assertTrue(CommitClassifier.LineFilter({"value": "gardening", "ratio": 85})('[girdening] Skip n tests'))

        self.assertFalse(CommitClassifier.LineFilter({"value": "gardening", "ratio": 85})('[gdening] Skip n tests'))

    def test_commit_class_gardening(self):
        c = CommitClassifier(
            name='Gardening',
            pickable=False,
            headers=[dict(value='gardening', ratio=85)],
            paths=[
                'LayoutTests/',
                'Tools/TestWebKitAPI',
            ],
        )
        self.assertEqual(c.name, 'Gardening')
        self.assertFalse(c.pickable)
        self.assertTrue(c.matches('[Gardening] Skip n tests', [], ['LayoutTests/TestExpectations']))

        self.assertFalse(c.matches('[Gardening] Skip n tests', [], []))
        self.assertFalse(c.matches('[gdening] Skip n tests', [], ['LayoutTests/TestExpectations']))
        self.assertFalse(c.matches('[Gardening] Skip n tests', [], ['Makefile', 'LayoutTests/TestExpectations']))

    def test_commit_class_cherry_pick(self):
        c = CommitClassifier(
            name='Cherry-pick',
            pickable=False,
            headers=['^[Cc]herry[- ][Pp]ick'],
            trailers=['^[Oo]riginally[- ]landed[- ]as:'],
        )
        self.assertEqual(c.name, 'Cherry-pick')
        self.assertFalse(c.pickable)
        self.assertTrue(c.matches('Cherry-pick abcde', [], ['somefile']))
        self.assertTrue(c.matches('cherry-pick abcde', [], ['otherfile']))
        self.assertTrue(c.matches('Cherry-Pick abcde', [], []))
        self.assertTrue(c.matches('cherry pick abcde', [], []))
        self.assertTrue(c.matches('[Component] Some change', ['Originally-landed-as: 1234.1@branch (abcde)'], []))

        self.assertFalse(c.matches('Partial cherry-pick', [], []))
        self.assertFalse(c.matches('Took cherry-pick', [], []))

    def test_commit_class_tools(self):
        c = CommitClassifier(
            name='Tools',
            pickable=True,
            paths=['Tools', 'LayoutTests', 'metadata'],
        )
        self.assertEqual(c.name, 'Tools')
        self.assertTrue(c.pickable)
        self.assertTrue(c.matches('Some change', [], ['Tools/Scripts/git-webkit']))
        self.assertTrue(c.matches('', [], ['LayoutTests/some/test.html']))
        self.assertTrue(c.matches('', [], ['metadata/contributors.json']))
        self.assertTrue(c.matches('', [], ['Tools/Scripts/run-webkit-tests', 'metadata/commit_classes.json']))

        self.assertFalse(c.matches('', [], []))
        self.assertFalse(c.matches('', [], ['metadata/contributors.json', 'Makefile']))
