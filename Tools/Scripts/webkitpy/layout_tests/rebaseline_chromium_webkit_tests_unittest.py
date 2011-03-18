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

import unittest

from webkitpy.tool import mocktool
from webkitpy.common.system import urlfetcher_mock
from webkitpy.common.system import filesystem_mock
from webkitpy.common.system import zipfileset_mock
from webkitpy.common.system import outputcapture
from webkitpy.common.system.executive import Executive, ScriptError

from webkitpy.layout_tests import port
from webkitpy.layout_tests import rebaseline_chromium_webkit_tests


class MockPort(object):
    def __init__(self, image_diff_exists):
        self.image_diff_exists = image_diff_exists

    def check_image_diff(self, override_step, logging):
        return self.image_diff_exists


def get_mock_get(config_expectations):
    def mock_get(port_name, options):
        return MockPort(config_expectations[options.configuration])
    return mock_get


ARCHIVE_URL = 'http://localhost/layout_test_results'


def test_options():
    return mocktool.MockOptions(configuration=None,
                                backup=False,
                                html_directory='/tmp',
                                archive_url=ARCHIVE_URL,
                                force_archive_url=None,
                                webkit_canary=True,
                                use_drt=False,
                                target_platform='chromium',
                                verbose=False,
                                quiet=False,
                                platforms='mac,win,win-xp',
                                gpu=False)


def test_host_port_and_filesystem(options, expectations):
    filesystem = port.unit_test_filesystem()
    host_port_obj = port.get('test', options, filesystem=filesystem,
                             user=mocktool.MockUser())

    expectations_path = host_port_obj.path_to_test_expectations_file()
    filesystem.write_text_file(expectations_path, expectations)
    return (host_port_obj, filesystem)


def test_url_fetcher(filesystem):
    urls = {
        ARCHIVE_URL + '/Webkit_Mac10_5/': '<a href="1/"><a href="2/">',
        ARCHIVE_URL + '/Webkit_Win/': '<a href="1/">',
    }
    return urlfetcher_mock.make_fetcher_cls(urls)(filesystem)


def test_zip_factory():
    ziphashes = {
        ARCHIVE_URL + '/Webkit_Mac10_5/2/layout-test-results.zip': {
            'layout-test-results/failures/expected/image-actual.txt': 'new-image-txt',
            'layout-test-results/failures/expected/image-actual.checksum': 'new-image-checksum',
            'layout-test-results/failures/expected/image-actual.png': 'new-image-png',
        },
        ARCHIVE_URL + '/Webkit_Win/1/layout-test-results.zip': {
            'layout-test-results/failures/expected/image-actual.txt': 'win-image-txt',
            'layout-test-results/failures/expected/image-actual.checksum': 'win-image-checksum',
            'layout-test-results/failures/expected/image-actual.png': 'win-image-png',
        },
    }
    return zipfileset_mock.make_factory(ziphashes)


class TestGetHostPortObject(unittest.TestCase):
    def assert_result(self, release_present, debug_present, valid_port_obj):
        # Tests whether we get a valid port object returned when we claim
        # that Image diff is (or isn't) present in the two configs.
        port.get = get_mock_get({'Release': release_present,
                                 'Debug': debug_present})
        options = mocktool.MockOptions(configuration=None,
                                       html_directory='/tmp')
        port_obj = rebaseline_chromium_webkit_tests.get_host_port_object(options)
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


class TestOptions(unittest.TestCase):
    def test_parse_options(self):
        (options, target_options) = rebaseline_chromium_webkit_tests.parse_options([])
        self.assertTrue(target_options.chromium)
        self.assertEqual(options.tolerance, 0)

        (options, target_options) = rebaseline_chromium_webkit_tests.parse_options(['--target-platform', 'qt'])
        self.assertFalse(hasattr(target_options, 'chromium'))
        self.assertEqual(options.tolerance, 0)


class TestRebaseliner(unittest.TestCase):
    def make_rebaseliner(self, expectations):
        options = test_options()
        host_port_obj, filesystem = test_host_port_and_filesystem(options, expectations)

        target_options = options
        target_port_obj = port.get('test', target_options,
                                   filesystem=filesystem)
        target_port_obj._expectations = expectations
        platform = target_port_obj.test_platform_name()

        url_fetcher = test_url_fetcher(filesystem)
        zip_factory = test_zip_factory()
        mock_scm = mocktool.MockSCM()
        rebaseliner = rebaseline_chromium_webkit_tests.Rebaseliner(host_port_obj,
            target_port_obj, platform, options, url_fetcher, zip_factory, mock_scm)
        return rebaseliner, filesystem

    def test_noop(self):
        # this method tests that was can at least instantiate an object, even
        # if there is nothing to do.
        rebaseliner, filesystem = self.make_rebaseliner("")
        rebaseliner.run(False)
        self.assertEqual(len(filesystem.written_files), 1)

    def test_rebaselining_tests(self):
        rebaseliner, filesystem = self.make_rebaseliner(
            "BUGX REBASELINE MAC : failures/expected/image.html = IMAGE")
        compile_success = rebaseliner._compile_rebaselining_tests()
        self.assertTrue(compile_success)
        self.assertEqual(set(['failures/expected/image.html']), rebaseliner.get_rebaselining_tests())

    def test_rebaselining_tests_should_ignore_reftests(self):
        rebaseliner, filesystem = self.make_rebaseliner(
            "BUGX REBASELINE : failures/expected/reftest.html = IMAGE")
        compile_success = rebaseliner._compile_rebaselining_tests()
        self.assertFalse(compile_success)
        self.assertFalse(rebaseliner.get_rebaselining_tests())

    def test_one_platform(self):
        rebaseliner, filesystem = self.make_rebaseliner(
            "BUGX REBASELINE MAC : failures/expected/image.html = IMAGE")
        rebaseliner.run(False)
        # We expect to have written 12 files over the course of this rebaseline:
        # *) 3 files in /__im_tmp for the extracted archive members
        # *) 3 new baselines under '/test.checkout/LayoutTests'
        # *) 4 files in /tmp for the new and old baselines in the result file
        #    (-{old,new}.{txt,png}
        # *) 1 text diff in /tmp for the result file (-diff.txt). We don't
        #    create image diffs (FIXME?) and don't display the checksums.
        # *) 1 updated test_expectations file
        self.assertEqual(len(filesystem.written_files), 12)
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test/test_expectations.txt'], '')
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac/failures/expected/image-expected.checksum'], 'new-image-checksum')
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac/failures/expected/image-expected.png'], 'new-image-png')
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac/failures/expected/image-expected.txt'], 'new-image-txt')

    def test_all_platforms(self):
        rebaseliner, filesystem = self.make_rebaseliner(
            "BUGX REBASELINE : failures/expected/image.html = IMAGE")
        rebaseliner.run(False)
        # See comment in test_one_platform for an explanation of the 12 written tests.
        # Note that even though the rebaseline is marked for all platforms, each
        # rebaseliner only ever does one.
        self.assertEqual(len(filesystem.written_files), 12)
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test/test_expectations.txt'],
            'BUGX REBASELINE WIN : failures/expected/image.html = IMAGE\n'
            'BUGX REBASELINE WIN-XP : failures/expected/image.html = IMAGE\n')
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac/failures/expected/image-expected.checksum'], 'new-image-checksum')
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac/failures/expected/image-expected.png'], 'new-image-png')
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac/failures/expected/image-expected.txt'], 'new-image-txt')

    def test_diff_baselines_txt(self):
        rebaseliner, filesystem = self.make_rebaseliner("")
        port = rebaseliner._port
        output = port.expected_text(
            port._filesystem.join(port.layout_tests_dir(), 'passes/text.html'))
        self.assertFalse(rebaseliner._diff_baselines(output, output,
                                                     is_image=False))

    def test_diff_baselines_png(self):
        rebaseliner, filesystem = self.make_rebaseliner('')
        port = rebaseliner._port
        image = port.expected_image(
            port._filesystem.join(port.layout_tests_dir(), 'passes/image.html'))
        self.assertFalse(rebaseliner._diff_baselines(image, image,
                                                     is_image=True))


class TestRealMain(unittest.TestCase):
    def test_all_platforms(self):
        expectations = "BUGX REBASELINE : failures/expected/image.html = IMAGE"

        options = test_options()

        host_port_obj, filesystem = test_host_port_and_filesystem(options, expectations)
        url_fetcher = test_url_fetcher(filesystem)
        zip_factory = test_zip_factory()
        mock_scm = mocktool.MockSCM()
        oc = outputcapture.OutputCapture()
        oc.capture_output()
        rebaseline_chromium_webkit_tests.real_main(options, options, host_port_obj,
            host_port_obj, url_fetcher, zip_factory, mock_scm)
        oc.restore_output()

        # We expect to have written 35 files over the course of this rebaseline:
        # *) 11 files * 3 ports for the new baselines and the diffs (see breakdown
        #    under test_one_platform, above)
        # *) the updated test_expectations file
        # *) the rebaseline results html file
        self.assertEqual(len(filesystem.written_files), 35)
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test/test_expectations.txt'], '')


class TestHtmlGenerator(unittest.TestCase):
    def make_generator(self, files, tests):
        options = mocktool.MockOptions(configuration=None, html_directory='/tmp')
        host_port = port.get('test', options, filesystem=port.unit_test_filesystem(files))
        generator = rebaseline_chromium_webkit_tests.HtmlGenerator(host_port,
            target_port=None, options=options, platforms=['mac'], rebaselining_tests=tests)
        return generator, host_port

    def test_generate_baseline_links(self):
        files = {
            "/tmp/foo-expected-mac-old.txt": "",
            "/tmp/foo-expected-mac-new.txt": "",
            "/tmp/foo-expected-mac-diff.txt": "",
        }
        tests = ["foo.txt"]
        generator, host_port = self.make_generator(files, tests)
        links = generator._generate_baseline_links("foo", ".txt", "mac")
        expected_links = '<td align=center><a href="file:///tmp/foo-expected-mac-old.txt">foo-expected.txt</a></td><td align=center><a href="file:///tmp/foo-expected-mac-new.txt">foo-expected.txt</a></td><td align=center><a href="file:///tmp/foo-expected-mac-diff.txt">Diff</a></td>'
        self.assertEqual(links, expected_links)


if __name__ == '__main__':
    unittest.main()
