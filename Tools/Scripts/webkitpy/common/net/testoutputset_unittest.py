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

import sys

from webkitpy.common.system.zip_mock import MockZip
import testoutputset
import unittest


class TestOutputSetTest(unittest.TestCase):
    def _outputset_with_zip(self, zip, **kwargs):
        return testoutputset.TestOutputSet('<fake-outputset>', '<fake-platform>', zip, **kwargs)

    def test_text_files_get_interpreted_as_text_outputs(self):
        zip = MockZip()
        zip.insert('fast/dom/some-test-actual.txt', 'actual outputs')
        b = self._outputset_with_zip(zip)
        self.assertEquals(1, len(b.outputs_for('fast/dom/some-test')))
        self.assertEquals('fast/dom/some-test', b.outputs_for('fast/dom/some-test.html')[0].name())

    def test_image_and_checksum_files_get_interpreted_as_a_single_image_output(self):
        zip = MockZip()
        zip.insert('fast/dom/some-test-actual.checksum', 'abc123')
        zip.insert('fast/dom/some-test-actual.png', '<image data>')
        b = self._outputset_with_zip(zip)
        outputs = b.outputs_for('fast/dom/some-test')
        self.assertEquals(1, len(outputs))
        output = outputs[0]
        self.assertEquals('image', output.type())
        self.assertEquals('abc123', output.checksum())

    def test_multiple_image_outputs_are_detected(self):
        zip = MockZip()
        zip.insert('platform/win/fast/dom/some-test-actual.checksum', 'checksum1')
        zip.insert('platform/win/fast/dom/some-test-actual.png', '<image data 1>')
        zip.insert('platform/mac/fast/dom/some-test-actual.checksum', 'checksum2')
        zip.insert('platform/mac/fast/dom/some-test-actual.png', '<image data 2>')
        b = self._outputset_with_zip(zip)
        outputs = b.outputs_for('fast/dom/some-test')
        self.assertEquals(2, len(outputs))
        self.assertFalse(outputs[0].same_content(outputs[1]))

    def test_aggregate_output_set_correctly_retrieves_tests_from_multiple_output_sets(self):
        outputset1_zip = MockZip()
        outputset1_zip.insert('fast/dom/test-actual.txt', 'linux text output')
        outputset1 = testoutputset.TestOutputSet('linux-outputset', 'linux', outputset1_zip)
        outputset2_zip = MockZip()
        outputset2_zip.insert('fast/dom/test-actual.txt', 'windows text output')
        outputset2 = testoutputset.TestOutputSet('win-outputset', 'win', outputset2_zip)

        b = testoutputset.AggregateTestOutputSet([outputset1, outputset2])
        self.assertEquals(2, len(b.outputs_for('fast/dom/test')))

    def test_can_infer_platform_from_path_if_none_provided(self):
        # FIXME: unclear what the right behavior on win32 is.
        # https://bugs.webkit.org/show_bug.cgi?id=54525.
        if sys.platform == 'win32':
            return

        zip = MockZip()
        zip.insert('platform/win/some-test-expected.png', '<image data>')
        zip.insert('platform/win/some-test-expected.checksum', 'abc123')
        b = testoutputset.TestOutputSet('local LayoutTests outputset', None, zip)

        outputs = b.outputs_for('some-test')
        self.assertEquals(1, len(outputs))
        self.assertEquals('win', outputs[0].platform())

    def test_test_extension_is_ignored(self):
        zip = MockZip()
        zip.insert('test/test-a-actual.txt', 'actual outputs')
        b = self._outputset_with_zip(zip)
        outputs = b.outputs_for('test/test-a.html')
        self.assertEquals(1, len(outputs))
        self.assertEquals('test/test-a', outputs[0].name())

    def test_existing_outputs_are_marked_as_such(self):
        zip = MockZip()
        zip.insert('test/test-a-expected.txt', 'expected outputs')
        b = self._outputset_with_zip(zip)
        outputs = b.outputs_for('test/test-a.html')
        self.assertEquals(1, len(outputs))
        self.assertFalse(outputs[0].is_actual())

    def test_only_returns_outputs_of_specified_type(self):
        zip = MockZip()
        zip.insert('test/test-a-expected.txt', 'expected outputs')
        zip.insert('test/test-a-expected.checksum', 'expected outputs')
        zip.insert('test/test-a-expected.png', 'expected outputs')
        b = self._outputset_with_zip(zip)

        outputs = b.outputs_for('test/test-a.html')
        text_outputs = b.outputs_for('test/test-a.html', target_type='text')
        image_outputs = b.outputs_for('test/test-a.html', target_type='image')

        self.assertEquals(2, len(outputs))
        self.assertEquals(1, len(text_outputs))
        self.assertEquals(1, len(image_outputs))
        self.assertEquals('text', text_outputs[0].type())
        self.assertEquals('image', image_outputs[0].type())

    def test_exclude_expected_outputs_works(self):
        zip = MockZip()
        zip.insert('test-expected.txt',  'expected outputs stored on server for some reason')
        b = self._outputset_with_zip(zip, include_expected=False)
        outputs = b.outputs_for('test', target_type=None)
        self.assertEquals(0, len(outputs))

if __name__ == "__main__":
    unittest.main()
