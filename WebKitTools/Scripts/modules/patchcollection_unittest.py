#!/usr/bin/env python
# Copyright (c) 2009, Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from modules.patchcollection import PatchCollection

def test_filter(patch):
    return not patch == MockBugzilla.patch_3

class MockBugzilla:
    patch_1 = ("patch", 1)
    patch_2 = ("patch", 2)
    patch_3 = ("patch", 3)
    patch_4 = ("patch", 4)

    def fetch_attachment(self, patch_id):
        return self.patch_1

    def fetch_patches_from_bug(self, bug_id):
        return [self.patch_2, self.patch_3, self.patch_4]


class MockEmptyBugzilla:
    def fetch_attachment(self, patch_id):
        return None

    def fetch_patches_from_bug(self, bug_id):
        return []


class PatchCollectionTest(unittest.TestCase):
    def test_basic(self):
        bugs = MockBugzilla()
        patches = PatchCollection(bugs, filter=test_filter)
        self.assertEqual(len(patches), 0)
        patches.add(42)
        self.assertEqual(len(patches), 1)
        patch = patches.next()
        self.assertEqual(patch, MockBugzilla.patch_1)
        self.assertEqual(len(patches), 0)
        patches.add_all_from_bug(38)
        # Notice that one of the patches gets filtered out.
        self.assertEqual(len(patches), 2)
        patch = patches.next()
        self.assertEqual(patch, MockBugzilla.patch_2)
        self.assertEqual(len(patches), 1)
        patch = patches.next()
        self.assertEqual(patch, MockBugzilla.patch_4)
        self.assertEqual(len(patches), 0)

    def test_no_patch(self):
        bugs = MockEmptyBugzilla()
        patches = PatchCollection(bugs, filter=test_filter)
        self.assertEqual(len(patches), 0)
        patches.add(42)
        self.assertEqual(len(patches), 0)
        patches.add_all_from_bug(38)
        self.assertEqual(len(patches), 0)

if __name__ == '__main__':
    unittest.main()
