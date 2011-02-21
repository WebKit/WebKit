#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import re
import sys
import testoutput
import unittest


class FakeFile(object):
    def __init__(self, filename, contents="fake contents"):
        self._filename = filename
        self._contents = contents

    def name(self):
        return self._filename

    def contents(self):
        return self._contents


class FakeTestOutput(testoutput.TestOutput):
    def __init__(self, platform, output_type, contents, is_expected=False):
        self._output_type = output_type
        self._contents = contents
        self._is_expected = is_expected
        actual = 'actual'
        if is_expected:
            actual = 'expected'
        test_name = 'anonymous-test-%s.txt' % actual
        file = FakeFile(test_name, contents)
        super(FakeTestOutput, self).__init__(platform, output_type, [file])

    def contents(self):
        return self._contents

    def retarget(self, platform):
        return FakeTestOutput(platform, self._output_type, self._contents, self._is_expected)


class TestOutputTest(unittest.TestCase):
    def _check_name(self, filename, expected_test_name):
        # FIXME: should consider using MockFileSystem here so as to not
        # have to worry about platform-specific path separators.
        if sys.platform == 'win32':
            filename = filename.replace('/', os.path.sep)
            expected_test_name = expected_test_name.replace('/', os.path.sep)
        r = testoutput.TextTestOutput(None, FakeFile(filename))
        self.assertEquals(expected_test_name, r.name())

    def _check_platform(self, filename, expected_platform):
        # FIXME: should consider using MockFileSystem here so as to not
        # have to worry about platform-specific path separators.
        if sys.platform == 'win32':
            filename = filename.replace('/', os.path.sep)
        r = testoutput.TextTestOutput(None, FakeFile(filename))
        self.assertEquals(expected_platform, r.platform())

    def test_extracts_name_correctly(self):
        self._check_name('LayoutTests/fast/dom/a-expected.txt', 'fast/dom/a')
        self._check_name('LayoutTests/fast/dom/a-actual.txt', 'fast/dom/a')
        self._check_name('LayoutTests/platform/win/fast/a-expected.txt', 'fast/a')
        self._check_name('LayoutTests/platform/win/fast/a-expected.checksum', 'fast/a')
        self._check_name('fast/dom/test-expected.txt', 'fast/dom/test')
        self._check_name('layout-test-results/fast/a-actual.checksum', 'fast/a')

    def test_extracts_platform_correctly(self):
        self._check_platform('LayoutTests/platform/win/fast/a-expected.txt', 'win')
        self._check_platform('platform/win/fast/a-expected.txt', 'win')
        self._check_platform('platform/mac/fast/a-expected.txt', 'mac')
        self._check_platform('fast/a-expected.txt', None)

    def test_outputs_from_an_actual_file_are_marked_as_such(self):
        r = testoutput.TextTestOutput(None, FakeFile('test-actual.txt'))
        self.assertTrue(r.is_actual())

    def test_outputs_from_an_expected_file_are_not_actual(self):
        r = testoutput.TextTestOutput(None, FakeFile('test-expected.txt'))
        self.assertFalse(r.is_actual())

    def test_is_new_baseline_for(self):
        expected = testoutput.TextTestOutput('mac', FakeFile('test-expected.txt'))
        actual = testoutput.TextTestOutput('mac', FakeFile('test-actual.txt'))
        self.assertTrue(actual.is_new_baseline_for(expected))
        self.assertFalse(expected.is_new_baseline_for(actual))

    def test__eq__(self):
        r1 = testoutput.TextTestOutput('mac', FakeFile('test-expected.txt', 'contents'))
        r2 = testoutput.TextTestOutput('mac', FakeFile('test-expected.txt', 'contents'))
        r3 = testoutput.TextTestOutput('win', FakeFile('test-expected.txt', 'contents'))

        self.assertEquals(r1, r2)
        self.assertEquals(r1, r2.retarget('mac'))
        self.assertNotEquals(r1, r2.retarget('win'))

    def test__hash__(self):
        r1 = testoutput.TextTestOutput('mac', FakeFile('test-expected.txt', 'contents'))
        r2 = testoutput.TextTestOutput('mac', FakeFile('test-expected.txt', 'contents'))
        r3 = testoutput.TextTestOutput(None, FakeFile('test-expected.txt', None))

        x = set([r1, r2])
        self.assertEquals(1, len(set([r1, r2])))
        self.assertEquals(2, len(set([r1, r2, r3])))

    def test_image_diff_is_invoked_for_image_outputs_without_checksum(self):
        r1 = testoutput.ImageTestOutput('mac', FakeFile('test-expected.png', 'asdf'), FakeFile('test-expected.checksum', 'check'))
        r2 = testoutput.ImageTestOutput('mac', FakeFile('test-expected.png', 'asdf'), None)

        # Default behaviour is to just compare on image contents.
        self.assertTrue(r1.same_content(r2))

        class AllImagesAreDifferent(object):
            def same_image(self, image1, image2):
                return False

        # But we can install other image differs.
        testoutput.ImageTestOutput.image_differ = AllImagesAreDifferent()

        self.assertFalse(r1.same_content(r2))

if __name__ == "__main__":
    unittest.main()
