#!/usr/bin/python
# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""Unit tests for rebaseline_chromium_webkit_tests.py."""

import os
import sys
import unittest

from webkitpy.tool import mocktool
from webkitpy.layout_tests import port
from webkitpy.layout_tests import rebaseline_chromium_webkit_tests
from webkitpy.common.system.executive import Executive, ScriptError


class MockPort(object):
    def __init__(self, image_diff_exists):
        self.image_diff_exists = image_diff_exists

    def check_image_diff(self, override_step, logging):
        return self.image_diff_exists


def get_mock_get(config_expectations):
    def mock_get(port_name, options):
        return MockPort(config_expectations[options.configuration])
    return mock_get


class TestGetHostPortObject(unittest.TestCase):
    def assert_result(self, release_present, debug_present, valid_port_obj):
        # Tests whether we get a valid port object returned when we claim
        # that Image diff is (or isn't) present in the two configs.
        port.get = get_mock_get({'Release': release_present,
                                 'Debug': debug_present})
        options = mocktool.MockOptions(configuration=None,
                                       html_directory=None)
        port_obj = rebaseline_chromium_webkit_tests.get_host_port_object(
            options)
        if valid_port_obj:
            self.assertNotEqual(port_obj, None)
        else:
            self.assertEqual(port_obj, None)

    def test_get_host_port_object(self):
        # Save the normal port.get() function for future testing.
        old_get = port.get

        # Test whether we get a valid port object back for the four
        # possible cases of having ImageDiffs built. It should work when
        # there is at least one binary present.
        self.assert_result(False, False, False)
        self.assert_result(True, False, True)
        self.assert_result(False, True, True)
        self.assert_result(True, True, True)

        # Restore the normal port.get() function.
        port.get = old_get


class TestRebaseliner(unittest.TestCase):
    def make_rebaseliner(self):
        options = mocktool.MockOptions(configuration=None,
                                       html_directory=None)
        host_port_obj = port.get('test', options)
        target_options = options
        target_port_obj = port.get('test', target_options)
        platform = 'test'
        return rebaseline_chromium_webkit_tests.Rebaseliner(
            host_port_obj, target_port_obj, platform, options)

    def test_parse_options(self):
        (options, target_options) = rebaseline_chromium_webkit_tests.parse_options([])
        self.assertTrue(target_options.chromium)
        self.assertEqual(options.tolerance, 0)

        (options, target_options) = rebaseline_chromium_webkit_tests.parse_options(['--target-platform', 'qt'])
        self.assertFalse(hasattr(target_options, 'chromium'))
        self.assertEqual(options.tolerance, 0)

    def test_noop(self):
        # this method tests that was can at least instantiate an object, even
        # if there is nothing to do.
        rebaseliner = self.make_rebaseliner()
        self.assertNotEqual(rebaseliner, None)

    def test_diff_baselines_txt(self):
        rebaseliner = self.make_rebaseliner()
        output = rebaseliner._port.expected_text(
            os.path.join(rebaseliner._port.layout_tests_dir(),
                         'passes/text.html'))
        self.assertFalse(rebaseliner._diff_baselines(output, output,
                                                     is_image=False))

    def test_diff_baselines_png(self):
        rebaseliner = self.make_rebaseliner()
        image = rebaseliner._port.expected_image(
            os.path.join(rebaseliner._port.layout_tests_dir(),
                         'passes/image.html'))
        self.assertFalse(rebaseliner._diff_baselines(image, image,
                                                     is_image=True))


class TestHtmlGenerator(unittest.TestCase):
    def make_generator(self, tests):
        return rebaseline_chromium_webkit_tests.HtmlGenerator(
            target_port=None,
            options=mocktool.MockOptions(configuration=None,
                                         html_directory='/tmp'),
            platforms=['mac'],
            rebaselining_tests=tests,
            executive=Executive())

    def test_generate_baseline_links(self):
        orig_platform = sys.platform
        orig_exists = os.path.exists

        try:
            sys.platform = 'darwin'
            os.path.exists = lambda x: True
            generator = self.make_generator(["foo.txt"])
            links = generator._generate_baseline_links("foo", ".txt", "mac")
            expected_links = '<td align=center><a href="file:///tmp/foo-expected-mac-old.txt">foo-expected.txt</a></td><td align=center><a href="file:///tmp/foo-expected-mac-new.txt">foo-expected.txt</a></td><td align=center><a href="file:///tmp/foo-expected-mac-diff.txt">Diff</a></td>'
            self.assertEqual(links, expected_links)
        finally:
            sys.platform = orig_platform
            os.path.exists = orig_exists


if __name__ == '__main__':
    unittest.main()
