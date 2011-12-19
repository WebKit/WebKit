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

from webkitpy.tool.mocktool import MockOptions
from webkitpy.common.system import urlfetcher_mock
from webkitpy.common.host_mock import MockHost

from webkitpy.common.system import zipfileset_mock
from webkitpy.common.system import outputcapture
from webkitpy.common.system.executive import Executive, ScriptError

from webkitpy.layout_tests import port
from webkitpy.to_be_moved import rebaseline_chromium_webkit_tests


class MockPort(object):
    def __init__(self, image_diff_exists):
        self.image_diff_exists = image_diff_exists

    def check_image_diff(self, override_step, logging):
        return self.image_diff_exists


class MockPortFactory(object):
    def __init__(self, config_expectations):
        self.config_expectations = config_expectations

    def get(self, port_name=None, options=None):
        return MockPort(self.config_expectations[options.configuration])


ARCHIVE_URL = 'http://localhost/layout_test_results'


def test_options():
    return MockOptions(configuration=None,
                                backup=False,
                                html_directory='/tmp',
                                archive_url=ARCHIVE_URL,
                                force_archive_url=None,
                                verbose=False,
                                quiet=False,
                                platforms=None)


def test_host_port_and_filesystem(options, expectations):
    host = MockHost()
    host_port_obj = host.port_factory.get('test', options)
    expectations_path = host_port_obj.path_to_test_expectations_file()
    host.filesystem.write_text_file(expectations_path, expectations)
    return (host, host_port_obj, host.filesystem)


def test_url_fetcher(filesystem):
    urls = {
        ARCHIVE_URL + '/Webkit_Mac10_6/': '<a href="4/">',
        ARCHIVE_URL + '/Webkit_Mac10_5/': '<a href="1/"><a href="2/">',
        ARCHIVE_URL + '/Webkit_Mac10_6__CG_/': '<a href="4/">',
        ARCHIVE_URL + '/Webkit_Mac10_5__CG_/': '<a href="1/"><a href="2/">',
        ARCHIVE_URL + '/Webkit_Win7/': '<a href="1/">',
        ARCHIVE_URL + '/Webkit_Vista/': '<a href="1/">',
        ARCHIVE_URL + '/Webkit_Win/': '<a href="1/">',
        ARCHIVE_URL + '/Webkit_Linux/': '<a href="1/">',
    }
    return urlfetcher_mock.make_fetcher_cls(urls)(filesystem)


def test_zip_factory():
    ziphashes = {
        ARCHIVE_URL + '/Webkit_Mac10_5/2/layout-test-results.zip': {
            'layout-test-results/failures/expected/image-actual.txt': 'new-image-txt',
            'layout-test-results/failures/expected/image-actual.checksum': 'new-image-checksum',
            'layout-test-results/failures/expected/image-actual.png': 'new-image-png',
            'layout-test-results/failures/expected/image_checksum-actual.txt': 'png-comment-txt',
            'layout-test-results/failures/expected/image_checksum-actual.checksum': '0123456789',
            'layout-test-results/failures/expected/image_checksum-actual.png': 'tEXtchecksum\x000123456789',
        },
        ARCHIVE_URL + '/Webkit_Mac10_6/4/layout-test-results.zip': {
            'layout-test-results/failures/expected/image-actual.txt': 'new-image-txt',
            'layout-test-results/failures/expected/image-actual.checksum': 'new-image-checksum',
            'layout-test-results/failures/expected/image-actual.png': 'new-image-png',
        },
        ARCHIVE_URL + '/Webkit_Mac10_5__CG_/2/layout-test-results.zip': {
            'layout-test-results/failures/expected/image-actual.txt': 'new-image-txt',
            'layout-test-results/failures/expected/image-actual.checksum': 'new-image-checksum',
            'layout-test-results/failures/expected/image-actual.png': 'new-image-png',
            'layout-test-results/failures/expected/image_checksum-actual.txt': 'png-comment-txt',
            'layout-test-results/failures/expected/image_checksum-actual.checksum': '0123456789',
            'layout-test-results/failures/expected/image_checksum-actual.png': 'tEXtchecksum\x000123456789',
        },
        ARCHIVE_URL + '/Webkit_Mac10_6__CG_/4/layout-test-results.zip': {
            'layout-test-results/failures/expected/image-actual.txt': 'new-image-txt',
            'layout-test-results/failures/expected/image-actual.checksum': 'new-image-checksum',
            'layout-test-results/failures/expected/image-actual.png': 'new-image-png',
        },
        ARCHIVE_URL + '/Webkit_Vista/1/layout-test-results.zip': {
            'layout-test-results/failures/expected/image-actual.txt': 'win-image-txt',
            'layout-test-results/failures/expected/image-actual.checksum': 'win-image-checksum',
            'layout-test-results/failures/expected/image-actual.png': 'win-image-png',
        },
        ARCHIVE_URL + '/Webkit_Win7/1/layout-test-results.zip': {
            'layout-test-results/failures/expected/image-actual.txt': 'win-image-txt',
            'layout-test-results/failures/expected/image-actual.checksum': 'win-image-checksum',
            'layout-test-results/failures/expected/image-actual.png': 'win-image-png',
        },
        ARCHIVE_URL + '/Webkit_Win/1/layout-test-results.zip': {
            'layout-test-results/failures/expected/image-actual.txt': 'win-image-txt',
            'layout-test-results/failures/expected/image-actual.checksum': 'win-image-checksum',
            'layout-test-results/failures/expected/image-actual.png': 'win-image-png',
        },
        ARCHIVE_URL + '/Webkit_Linux/1/layout-test-results.zip': {
            'layout-test-results/failures/expected/image-actual.txt': 'win-image-txt',
            'layout-test-results/failures/expected/image-actual.checksum': 'win-image-checksum',
            'layout-test-results/failures/expected/image-actual.png': 'win-image-png',
        },
    }
    return zipfileset_mock.make_factory(ziphashes)


def test_archive(orig_archive_dict):
    new_archive_dict = {}
    for platform, dirname in orig_archive_dict.iteritems():
        # This is a giant hack. :(
        platform = platform.replace('chromium', 'test')
        platform = platform.replace('test-cg', 'test')
        new_archive_dict[platform] = dirname
    return new_archive_dict


class TestGetHostPortObject(unittest.TestCase):
    def assert_result(self, release_present, debug_present, valid_port_obj):
        # Tests whether we get a valid port object returned when we claim
        # that Image diff is (or isn't) present in the two configs.
        port_factory = MockPortFactory({'Release': release_present, 'Debug': debug_present})
        options = MockOptions(configuration=None, html_directory='/tmp')
        port_obj = rebaseline_chromium_webkit_tests.get_host_port_object(port_factory, options)
        if valid_port_obj:
            self.assertNotEqual(port_obj, None)
        else:
            self.assertEqual(port_obj, None)

    def test_get_host_port_object(self):
        # Test whether we get a valid port object back for the four
        # possible cases of having ImageDiffs built. It should work when
        # there is at least one binary present.
        self.assert_result(False, False, False)
        self.assert_result(True, False, True)
        self.assert_result(False, True, True)
        self.assert_result(True, True, True)


class TestOptions(unittest.TestCase):
    def test_parse_options(self):
        (options, target_options) = rebaseline_chromium_webkit_tests.parse_options([])
        self.assertTrue(target_options.chromium)
        self.assertEqual(options.tolerance, 0)

        (options, target_options) = rebaseline_chromium_webkit_tests.parse_options(['--target-platform', 'qt'])
        self.assertFalse(hasattr(target_options, 'chromium'))
        self.assertEqual(options.tolerance, 0)


class TestRebaseliner(unittest.TestCase):
    def setUp(self):
        if not hasattr(self, '_orig_archive'):
            self._orig_archive = rebaseline_chromium_webkit_tests.ARCHIVE_DIR_NAME_DICT
            rebaseline_chromium_webkit_tests.ARCHIVE_DIR_NAME_DICT = test_archive(self._orig_archive)

    def tearDown(self):
        rebaseline_chromium_webkit_tests.ARCHIVE_DIR_NAME_DICT = self._orig_archive

    def make_rebaseliner(self, expectations):
        options = test_options()
        host, host_port_obj, filesystem = test_host_port_and_filesystem(options, expectations)

        target_options = options
        target_port_obj = host.port_factory.get('test', target_options)
        platform = target_port_obj.name()

        url_fetcher = test_url_fetcher(filesystem)
        zip_factory = test_zip_factory()

        # FIXME: SCM module doesn't handle paths that aren't relative to the checkout_root consistently.
        filesystem.chdir("/test.checkout")

        rebaseliner = rebaseline_chromium_webkit_tests.Rebaseliner(host, host_port_obj,
            target_port_obj, platform, options, url_fetcher, zip_factory)
        return rebaseliner, filesystem

    def test_noop(self):
        # this method tests that was can at least instantiate an object, even
        # if there is nothing to do.
        rebaseliner, filesystem = self.make_rebaseliner("")
        rebaseliner.run()
        self.assertEqual(len(filesystem.written_files), 1)

    def test_rebaselining_tests(self):
        rebaseliner, filesystem = self.make_rebaseliner(
            "BUGX REBASELINE MAC : failures/expected/image.html = IMAGE")
        compile_success = rebaseliner._compile_rebaselining_tests()
        self.assertTrue(compile_success)
        self.assertEqual(set(['failures/expected/image.html']), rebaseliner._rebaselining_tests)

    def test_rebaselining_tests_should_ignore_reftests(self):
        rebaseliner, filesystem = self.make_rebaseliner(
            "BUGX REBASELINE : failures/expected/reftest.html = IMAGE")
        compile_success = rebaseliner._compile_rebaselining_tests()
        self.assertFalse(compile_success)
        self.assertFalse(rebaseliner._rebaselining_tests)

    def test_one_platform(self):
        rebaseliner, filesystem = self.make_rebaseliner(
            "BUGX REBASELINE MAC : failures/expected/image.html = IMAGE")

        rebaseliner.run()
        # We expect to have written 13 files over the course of this rebaseline:
        # *) 3 files in /__im_tmp for the extracted archive members
        # *) 3 new baselines under '/test.checkout/LayoutTests'
        # *) 4 files in /tmp for the new and old baselines in the result file
        #    (-{old,new}.{txt,png}
        # *) 1 text diff in /tmp for the result file (-diff.txt).
        # *) 1 image diff in /tmp for the result file (-diff.png).
        # *) 1 updated test_expectations file
        self.assertEqual(len(filesystem.written_files), 13)
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image-expected.checksum'], 'new-image-checksum')
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image-expected.png'], 'new-image-png')
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image-expected.txt'], 'new-image-txt')

    def test_all_platforms(self):
        rebaseliner, filesystem = self.make_rebaseliner(
            "BUGX REBASELINE : failures/expected/image.html = IMAGE")
        rebaseliner.run()
        # See comment in test_one_platform for an explanation of the 13 written tests.
        # Note that even though the rebaseline is marked for all platforms, each
        # rebaseliner only ever does one.
        self.assertEqual(len(filesystem.written_files), 13)
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image-expected.checksum'], 'new-image-checksum')
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image-expected.png'], 'new-image-png')
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image-expected.txt'], 'new-image-txt')

    def test_png_file_with_comment(self):
        rebaseliner, filesystem = self.make_rebaseliner(
            "BUGX REBASELINE MAC : failures/expected/image_checksum.html = IMAGE")
        compile_success = rebaseliner._compile_rebaselining_tests()
        self.assertTrue(compile_success)
        self.assertEqual(set(['failures/expected/image_checksum.html']), rebaseliner._rebaselining_tests)
        rebaseliner.run()
        # There is one less file written than |test_one_platform| because we only
        # write 2 expectations (the png and the txt file).
        self.assertEqual(len(filesystem.written_files), 12)
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image_checksum-expected.png'], 'tEXtchecksum\x000123456789')
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image_checksum-expected.txt'], 'png-comment-txt')
        self.assertFalse(filesystem.files.get('/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image_checksum-expected.checksum', None))

    def test_png_file_with_comment_remove_old_checksum(self):
        rebaseliner, filesystem = self.make_rebaseliner(
            "BUGX REBASELINE MAC : failures/expected/image_checksum.html = IMAGE")
        filesystem.files['/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image_checksum-expected.png'] = 'old'
        filesystem.files['/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image_checksum-expected.checksum'] = 'old'
        filesystem.files['/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image_checksum-expected.txt'] = 'old'

        compile_success = rebaseliner._compile_rebaselining_tests()
        self.assertTrue(compile_success)
        self.assertEqual(set(['failures/expected/image_checksum.html']), rebaseliner._rebaselining_tests)
        rebaseliner.run()
        # There is one more file written than |test_png_file_with_comment_remove_old_checksum|
        # because we also delete the old checksum.
        self.assertEqual(len(filesystem.written_files), 13)
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image_checksum-expected.png'], 'tEXtchecksum\x000123456789')
        self.assertEqual(filesystem.files['/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image_checksum-expected.txt'], 'png-comment-txt')
        self.assertEqual(filesystem.files.get('/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image_checksum-expected.checksum', None), None)

    def test_png_file_with_comment_as_duplicate(self):
        rebaseliner, filesystem = self.make_rebaseliner(
            "BUGX REBASELINE MAC : failures/expected/image_checksum.html = IMAGE")
        filesystem.files['/test.checkout/LayoutTests/platform/test-mac-snowleopard/failures/expected/image_checksum-expected.png'] = 'tEXtchecksum\x000123456789'
        filesystem.files['/test.checkout/LayoutTests/platform/test-mac-snowleopard/failures/expected/image_checksum-expected.txt'] = 'png-comment-txt'

        compile_success = rebaseliner._compile_rebaselining_tests()
        self.assertTrue(compile_success)
        self.assertEqual(set(['failures/expected/image_checksum.html']), rebaseliner._rebaselining_tests)
        rebaseliner.run()
        self.assertEqual(filesystem.files.get('/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image_checksum-expected.png', None), None)
        self.assertEqual(filesystem.files.get('/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image_checksum-expected.txt', None), None)
        self.assertEqual(filesystem.files.get('/test.checkout/LayoutTests/platform/test-mac-leopard/failures/expected/image_checksum-expected.checksum', None), None)

    def test_diff_baselines_txt(self):
        rebaseliner, filesystem = self.make_rebaseliner("")
        port = rebaseliner._port
        output = port.expected_text('passes/text.html')
        self.assertFalse(rebaseliner._diff_baselines(output, output,
                                                     is_image=False))

    def test_diff_baselines_png(self):
        rebaseliner, filesystem = self.make_rebaseliner('')
        port = rebaseliner._port
        image = port.expected_image('passes/image.html')
        self.assertFalse(rebaseliner._diff_baselines(image, image,
                                                     is_image=True))


class TestRealMain(unittest.TestCase):
    def setUp(self):
        if not hasattr(self, '_orig_archive'):
            self._orig_archive = rebaseline_chromium_webkit_tests.ARCHIVE_DIR_NAME_DICT
            rebaseline_chromium_webkit_tests.ARCHIVE_DIR_NAME_DICT = test_archive(self._orig_archive)

    def tearDown(self):
        rebaseline_chromium_webkit_tests.ARCHIVE_DIR_NAME_DICT = self._orig_archive

    def test_all_platforms(self):
        expectations = "BUGX REBASELINE : failures/expected/image.html = IMAGE"

        options = test_options()
        host, host_port_obj, filesystem = test_host_port_and_filesystem(options, expectations)
        url_fetcher = test_url_fetcher(filesystem)
        zip_factory = test_zip_factory()

        # FIXME: SCM module doesn't handle paths that aren't relative to the checkout_root consistently.
        filesystem.chdir("/test.checkout")

        oc = outputcapture.OutputCapture()
        oc.capture_output()
        res = rebaseline_chromium_webkit_tests.real_main(host, options, options,
            host_port_obj, host_port_obj, url_fetcher, zip_factory)
        oc.restore_output()

        # We expect to have written 38 files over the course of this rebaseline:
        # *) 6*3 files in /__im_tmp/ for the archived members of the 6 ports
        # *) 2*3 files in /test.checkout for actually differing baselines
        # *) 1 file in /test.checkout for the updated test_expectations file
        # *) 2*4 files in /tmp for the old/new baselines for the two actual ports
        # *) 2 files in /tmp for the text diffs for the two ports
        # *) 2 files in /tmp for the image diffs for the two ports
        # *) 1 file in /tmp for the rebaseline results html file
        # FIXME: These numbers depend on MockSCM.exists() returning True for all files.
        self.assertEqual(res, 0)
        self.assertEqual(len(filesystem.written_files), 38)


class TestHtmlGenerator(unittest.TestCase):
    def make_generator(self, files, tests):
        options = MockOptions(configuration=None, html_directory='/tmp')
        host = MockHost()
        fs = host.filesystem
        for filename, contents in files.iteritems():
            fs.maybe_make_directory(fs.dirname(filename))
            fs.write_binary_file(filename, contents)
        host_port = host.port_factory.get('test', options)
        generator = rebaseline_chromium_webkit_tests.HtmlGenerator(host_port,
            target_port=None, options=options, platforms=['test-mac-leopard'], rebaselining_tests=tests)
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
