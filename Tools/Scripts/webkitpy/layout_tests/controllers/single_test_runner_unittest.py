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

from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests import run_webkit_tests
from webkitpy.layout_tests.controllers.layout_test_finder import LayoutTestFinder
from webkitpy.layout_tests.controllers.single_test_runner import SingleTestRunner
from webkitpy.layout_tests.models.test import Reference
from webkitpy.layout_tests.models.test_input import ReferenceInput, Test, TestInput
from webkitpy.port.driver import DriverOutput
from webkitpy.port.test import TestPort, add_unit_tests_to_mock_filesystem


class TestDriver:
    def run_test(self, driver_input, stop_when_done):
        text = ''
        timeout = False
        crash = False
        return DriverOutput(text, '', '', '', crash=crash, timeout=timeout)

    def start(self):
        """do nothing"""

    def stop(self):
        """do nothing"""


class SingleTestRunnerTest(unittest.TestCase):

    def _make_test_runner(self, test_name, fuzzy=None, test=None, host=None):
        host = host or MockHost()
        port = TestPort(host)
        driver = TestDriver()
        results_directory = 'layout-test-results'
        worker_name = ''

        test_input = TestInput(test or Test(test_name, fuzzy=fuzzy))
        return SingleTestRunner(port, port._options, results_directory, worker_name, driver, test_input, True)

    def test_save_baseline_data_rebaselining_uses_test_model_path(self):
        # When rebaselining, the output directory for a new .txt baseline
        # comes from test.expected_text_path, which the finder may have
        # resolved to an existing .webarchive baseline rather than a .txt
        # one -- the directory should still be correct either way.
        host = MockHost()
        options, _ = run_webkit_tests.parse_args([])
        port = TestPort(host, options=options)
        fs = port.host.filesystem
        expected_dir = fs.join(port.layout_tests_dir(), 'platform/test-mac-leopard/passes')
        test = Test(
            'passes/text.html',
            expected_text_path=fs.join(expected_dir, 'text-expected.webarchive'),
        )
        runner = SingleTestRunner(
            port, port._options, 'layout-test-results', '', TestDriver(), TestInput(test), True
        )
        runner._driver.host = host

        runner._save_baseline_data(b'new text', '.txt', rebaselining=True)

        self.assertIn(fs.join(expected_dir, 'text-expected.txt'), fs.written_files)

    def test_save_baseline_data_rebaselining_maps_each_extension_to_its_own_path(self):
        host = MockHost()
        options, _ = run_webkit_tests.parse_args([])
        port = TestPort(host, options=options)
        fs = port.host.filesystem
        d = port.layout_tests_dir()
        dirs = {
            '.txt': fs.join(d, 'platform/test-mac-leopard/txt-dir'),
            '.png': fs.join(d, 'platform/test-mac-leopard/png-dir'),
            '.wav': fs.join(d, 'platform/test-mac-leopard/wav-dir'),
        }
        test = Test(
            'passes/text.html',
            expected_text_path=fs.join(dirs['.txt'], 'text-expected.txt'),
            expected_image_path=fs.join(dirs['.png'], 'text-expected.png'),
            expected_audio_path=fs.join(dirs['.wav'], 'text-expected.wav'),
        )
        for extension, expected_dir in dirs.items():
            with self.subTest(extension=extension):
                runner = SingleTestRunner(
                    port, port._options, 'layout-test-results', '', TestDriver(), TestInput(test), True
                )
                runner._driver.host = host
                runner._save_baseline_data(b'new data', extension, rebaselining=True)
                self.assertIn(fs.join(expected_dir, 'text-expected' + extension), fs.written_files)

    def test_save_baseline_data_rebaselining_without_existing_baseline(self):
        host = MockHost()
        options, _ = run_webkit_tests.parse_args([])
        port = TestPort(host, options=options)
        fs = port.host.filesystem
        runner = SingleTestRunner(
            port, port._options, 'layout-test-results', '', TestDriver(), TestInput(Test('passes/text.html')), True
        )
        runner._driver.host = host

        runner._save_baseline_data(b'new text', '.txt', rebaselining=True)

        self.assertIn(fs.join(port.layout_tests_dir(), 'passes/text-expected.txt'), fs.written_files)

    def test_fuzzy_matching_values(self):
        fuzzy = {None: [[15, 15], [300, 300]]}
        single_test_runner = self._make_test_runner('fuzzy-test.html', fuzzy=fuzzy)
        fuzzy_data = single_test_runner._fuzzy_tolerance_for_reference('/test.checkout/LayoutTests/fuzzy-test-expected.html')
        self.assertEqual(fuzzy_data, {'max_difference': [15, 15], 'total_pixels': [300, 300]})

    def test_fuzzy_matching_values_for_ref(self):
        test_name = 'fuzzy-test.html'
        fuzzy = {
            None: [[15, 15], [300, 300]],
            '/test.checkout/LayoutTests/reference.html': [[5, 8], [78, 84]],
        }
        single_test_runner = self._make_test_runner(test_name, fuzzy=fuzzy)
        fuzzy_data = single_test_runner._fuzzy_tolerance_for_reference('/test.checkout/LayoutTests/reference.html')
        self.assertEqual(fuzzy_data, {'max_difference': [5, 8], 'total_pixels': [78, 84]})

    def _assert_discovered_fuzzy_reaches_runner(self, test_path, files, per_reference):
        """Discover a reftest with the finder, then check the tolerance the
        runner looks up for the test's reference is the one from the test's
        metadata -- neither the default nor zero."""
        host = MockHost()
        add_unit_tests_to_mock_filesystem(host.filesystem)
        port = TestPort(host)
        fs = host.filesystem
        for rel_path, contents in files.items():
            path = fs.join(port.layout_tests_dir(), rel_path)
            fs.maybe_make_directory(fs.dirname(path))
            fs.write_text_file(path, contents)
        finder = LayoutTestFinder(fs, port.layout_tests_dir(), port.baseline_search_path(), test_routes=port.test_routes())

        (test,) = list(finder.get_tests([test_path]))
        runner = self._make_test_runner(test.test_path, test=test, host=host)

        default = {'max_difference': [15, 15], 'total_pixels': [300, 300]}
        self.assertNotEqual(per_reference, default)
        self.assertEqual(runner._fuzzy_tolerance_for_reference(test.reference_files[0].path), per_reference)
        self.assertEqual(runner._fuzzy_tolerance_for_reference(fs.join(port.layout_tests_dir(), 'unrelated-ref.html')), default)

    FUZZY_METAS = (
        '<meta name=fuzzy content="maxDifference=15;totalPixels=300">'
        '<meta name=fuzzy content="%s:maxDifference=5-8;totalPixels=78-84">'
    )

    def test_discovered_non_wpt_fuzzy_reaches_runner(self):
        self._assert_discovered_fuzzy_reaches_runner(
            'fast/borders/fuzzy-test.html',
            {
                'fast/borders/fuzzy-test.html': '<html><head><link rel=match href="fuzzy-test-expected.html">' + self.FUZZY_METAS % 'fuzzy-test-expected.html',
                'fast/borders/fuzzy-test-expected.html': '<html></html>',
            },
            {'max_difference': [5, 8], 'total_pixels': [78, 84]},
        )

    def test_discovered_wpt_fuzzy_reaches_runner(self):
        self._assert_discovered_fuzzy_reaches_runner(
            'imported/w3c/web-platform-tests/foo/fuzzy-test.html',
            {
                'imported/w3c/web-platform-tests/foo/fuzzy-test.html': '<html><head><link rel=match href="wpt-ref.html">' + self.FUZZY_METAS % 'wpt-ref.html',
                'imported/w3c/web-platform-tests/foo/wpt-ref.html': '<html></html>',
            },
            {'max_difference': [5, 8], 'total_pixels': [78, 84]},
        )

    def test_fuzzy_matching_values_no_common_data(self):
        test_name = 'fast/borders/fuzzy-test.html'
        fuzzy = {'/test.checkout/LayoutTests/fast/resources/common-ref.html': [[5, 8], [78, 84]]}
        single_test_runner = self._make_test_runner(test_name, fuzzy=fuzzy)
        fuzzy_data = single_test_runner._fuzzy_tolerance_for_reference('/test.checkout/LayoutTests/reference.html')
        self.assertEqual(fuzzy_data, {'max_difference': [0, 0], 'total_pixels': [0, 0]})

    def test_fuzzy_matching_comparisons(self):
        self.assertTrue(SingleTestRunner._test_passes_fuzzy_matching({'max_difference': [0, 0], 'total_pixels': [0, 0]}, {'max_difference': 0, 'total_pixels': 0}))
        self.assertFalse(SingleTestRunner._test_passes_fuzzy_matching({'max_difference': [0, 0], 'total_pixels': [0, 0]}, {'max_difference': 1, 'total_pixels': 1}))

        self.assertTrue(SingleTestRunner._test_passes_fuzzy_matching({'max_difference': [5, 7], 'total_pixels': [10, 12]}, {'max_difference': 5, 'total_pixels': 10}))
        self.assertTrue(SingleTestRunner._test_passes_fuzzy_matching({'max_difference': [5, 7], 'total_pixels': [10, 12]}, {'max_difference': 6, 'total_pixels': 11}))
        self.assertTrue(SingleTestRunner._test_passes_fuzzy_matching({'max_difference': [5, 7], 'total_pixels': [10, 12]}, {'max_difference': 7, 'total_pixels': 12}))

        self.assertFalse(SingleTestRunner._test_passes_fuzzy_matching({'max_difference': [5, 7], 'total_pixels': [10, 12]}, {'max_difference': 0, 'total_pixels': 0}))
        self.assertFalse(SingleTestRunner._test_passes_fuzzy_matching({'max_difference': [5, 7], 'total_pixels': [10, 12]}, {'max_difference': 5, 'total_pixels': 8}))
        self.assertFalse(SingleTestRunner._test_passes_fuzzy_matching({'max_difference': [5, 7], 'total_pixels': [10, 12]}, {'max_difference': 3, 'total_pixels': 11}))
        self.assertFalse(SingleTestRunner._test_passes_fuzzy_matching({'max_difference': [5, 7], 'total_pixels': [10, 12]}, {'max_difference': 9, 'total_pixels': 11}))
        self.assertFalse(SingleTestRunner._test_passes_fuzzy_matching({'max_difference': [5, 7], 'total_pixels': [10, 12]}, {'max_difference': 6, 'total_pixels': 13}))

    def test_run_reftest_uses_reference_urls_from_test_input(self):
        class RecordingDriver:
            def __init__(self):
                self.driver_inputs = []

            def run_test(self, driver_input, stop_when_done):
                self.driver_inputs.append(driver_input)
                return DriverOutput('', b'image', 'hash', '')

            def stop(self):
                """do nothing"""

        runner = self._make_test_runner('http/tests/foo/test.html')
        d = runner._port.layout_tests_dir()
        reference = Reference('==', runner._port.host.filesystem.join(d, 'http/tests/foo/ref.html'))
        runner._test_input = TestInput(
            Test('http/tests/foo/test.html', reference_files=(reference,)),
            url='http://127.0.0.1:8000/foo/test.html',
            reference_inputs=(ReferenceInput(reference, 'http://127.0.0.1:8000/foo/ref.html'),),
        )
        runner._options.additional_header = None
        runner._driver = RecordingDriver()

        runner._run_reftest()

        self.assertEqual(
            [(i.test_name, i.url) for i in runner._driver.driver_inputs],
            [
                ('http/tests/foo/test.html', 'http://127.0.0.1:8000/foo/test.html'),
                ('http/tests/foo/ref.html', 'http://127.0.0.1:8000/foo/ref.html'),
            ],
        )

    def test_driver_input_passes_test_input_url_through(self):
        # The URL (including any cross-origin rewrite) is computed by Manager;
        # SingleTestRunner must not alter it.
        runner = self._make_test_runner('fast/foo.html')
        url = 'file:///layout-tests/fast/foo.html'
        runner._test_input = TestInput(Test('fast/foo.html'), url=url)
        runner._options.additional_header = 'runInCrossOriginFrame=true'

        driver_input = runner._driver_input()
        self.assertEqual(driver_input.url, url)
        self.assertEqual(driver_input.additional_header, 'runInCrossOriginFrame=true')
