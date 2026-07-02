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

import os
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests.controllers.single_test_runner import SingleTestRunner
from webkitpy.layout_tests.models.test_input import Test, TestInput
from webkitpy.port.driver import DriverOutput
from webkitpy.port.test import TestPort


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

    def _add_file(self, port, file_path, contents):
        filesystem = port.host.filesystem
        file_dir, file_name = os.path.split(file_path)
        dirname = filesystem.join(port.layout_tests_dir(), file_dir)
        filesystem.maybe_make_directory(dirname)
        filesystem.write_binary_file(filesystem.join(dirname, file_name), contents)

    def _make_test_runner(self, test_name):
        host = MockHost()
        port = TestPort(host)
        driver = TestDriver()
        results_directory = 'layout-test-results'
        worker_name = ''

        test_input = TestInput(Test(test_name))
        return SingleTestRunner(port, port._options, results_directory, worker_name, driver, test_input, True)

    def test_fuzzy_matching_values(self):
        single_test_runner = self._make_test_runner('fuzzy-test.html')
        self._add_file(single_test_runner._port, 'fuzzy-test.html', b'<html><head><meta name=fuzzy content="maxDifference=15;totalPixels=300">')
        fuzzy_data = single_test_runner._fuzzy_tolerance_for_reference('/test.checkout/LayoutTests/fuzzy-test-expected.html')
        self.assertEqual(fuzzy_data, {'max_difference': [15, 15], 'total_pixels': [300, 300]})

    def test_fuzzy_matching_values_for_ref(self):
        test_name = 'fuzzy-test.html'
        single_test_runner = self._make_test_runner(test_name)
        self._add_file(single_test_runner._port, test_name, """<html><head>
            <meta name=fuzzy content="maxDifference=15;totalPixels=300">
            <meta name=fuzzy content="reference.html:maxDifference=5-8;totalPixels=78-84">
        """)
        fuzzy_data = single_test_runner._fuzzy_tolerance_for_reference('/test.checkout/LayoutTests/reference.html')
        self.assertEqual(fuzzy_data, {'max_difference': [5, 8], 'total_pixels': [78, 84]})

    def test_fuzzy_matching_values_for_relative_path_ref(self):
        test_name = 'fast/borders/fuzzy-test.html'
        single_test_runner = self._make_test_runner(test_name)
        self._add_file(single_test_runner._port, test_name, """<html><head>
            <meta name=fuzzy content="maxDifference=15;totalPixels=300">
            <meta name=fuzzy content="../resources/common-ref.html:maxDifference=5-8;totalPixels=78-84">
        """)
        self._add_file(single_test_runner._port, 'fast/resources/common-ref.html', b'')
        fuzzy_data = single_test_runner._fuzzy_tolerance_for_reference('/test.checkout/LayoutTests/fast/resources/common-ref.html')
        self.assertEqual(fuzzy_data, {'max_difference': [5, 8], 'total_pixels': [78, 84]})

    def test_fuzzy_matching_values_for_xml_document(self):
        test_name = 'fuzzy-test.svg'
        single_test_runner = self._make_test_runner(test_name)
        self._add_file(single_test_runner._port, test_name, """<svg width="340" height="140" xmlns="http://www.w3.org/2000/svg" xmlns:html="http://www.w3.org/1999/xhtml">
            <html:meta name="fuzzy" content="maxDifference=0-1; totalPixels=0-2"/>
        """)
        fuzzy_data = single_test_runner._fuzzy_tolerance_for_reference('/test.checkout/LayoutTests/fuzzy-test-expected.svg')
        self.assertEqual(fuzzy_data, {'max_difference': [0, 1], 'total_pixels': [0, 2]})

    def test_fuzzy_matching_values_no_common_data(self):
        test_name = 'fast/borders/fuzzy-test.html'
        single_test_runner = self._make_test_runner(test_name)
        self._add_file(single_test_runner._port, test_name, """<html><head>
            <meta name=fuzzy content="../resources/common-ref.html:maxDifference=5-8;totalPixels=78-84">
        """)

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

    def _fuzzy_metadata_from_test_with_contents(self, test_contents):
        test_path = '/test.checkout/LayoutTests/fuzzy-test.html'
        runner = self._make_test_runner(test_path)
        self._add_file(runner._port, test_path, test_contents)
        return runner._fuzzy_metadata_for_file(test_path)

    def test_simple_fuzzy_data(self):
        """ Tests basic form of fuzzy_metadata()"""

        test_html = b"""<html><head>
<link rel="match" href="green-box-ref.xht" />
<meta name=fuzzy content="maxDifference = 15 ; totalPixels = 300">
</head>
<body>CONTENT OF TEST</body></html>
"""
        actual_fuzzy = self._fuzzy_metadata_from_test_with_contents(test_html)

        expected_fuzzy = {None: [[15, 15], [300, 300]]}
        self.assertEqual(actual_fuzzy, expected_fuzzy, 'fuzzy data did not match expected')

    def test_nameless_fuzzy_data(self):
        """ Tests fuzzy_metadata() in short form"""

        test_html = b"""<html><head>
<link rel="match" href="green-box-ref.xht" />
<meta name=fuzzy content=" 15 ; 300 ">
</head>
<body>CONTENT OF TEST</body></html>
"""
        actual_fuzzy = self._fuzzy_metadata_from_test_with_contents(test_html)
        expected_fuzzy = {None: [[15, 15], [300, 300]]}
        self.assertEqual(actual_fuzzy, expected_fuzzy, 'fuzzy data did not match expected')

    def test_range_fuzzy_data(self):
        """ Tests fuzzy_metadata() in range form"""

        test_html = b"""<html><head>
<link rel="match" href="green-box-ref.xht" />
<meta name=fuzzy content="maxDifference=5-15;totalPixels =  200 - 300 ">
</head>
<body>CONTENT OF TEST</body></html>
"""
        actual_fuzzy = self._fuzzy_metadata_from_test_with_contents(test_html)

        expected_fuzzy = {None: [[5, 15], [200, 300]]}
        self.assertEqual(actual_fuzzy, expected_fuzzy, 'fuzzy data did not match expected')

    def test_nameless_range_fuzzy_data(self):
        """ Tests fuzzy_metadata() in short range form"""

        test_html = b"""<html><head>
<link rel="match" href="green-box-ref.xht" />
<meta name=fuzzy content="5-15;  200 - 300 ">
</head>
<body>CONTENT OF TEST</body></html>
"""
        actual_fuzzy = self._fuzzy_metadata_from_test_with_contents(test_html)

        expected_fuzzy = {None: [[5, 15], [200, 300]]}
        self.assertEqual(actual_fuzzy, expected_fuzzy, 'fuzzy data did not match expected')

    def test_per_ref_fuzzy_data(self):
        """ Tests fuzzy_metadata() with values for difference reference files"""

        test_html = b"""<html><head>
<link rel="match" href="green-box-ref.xht" />
<link rel="match" href="close-match-ref.html" />
<link rel="match" href="worse-match-ref.html" />
<meta name=fuzzy content="5-15;200-300 ">
<meta name=fuzzy content="close-match-ref.html:5;20">
<meta name=fuzzy content="worse-match-ref.html: 15;30">
</head>
<body>CONTENT OF TEST</body></html>
"""
        actual_fuzzy = self._fuzzy_metadata_from_test_with_contents(test_html)

        expected_fuzzy = {
            None: [[5, 15], [200, 300]],
            'close-match-ref.html': [[5, 5], [20, 20]],
            'worse-match-ref.html': [[15, 15], [30, 30]]
        }
        self.assertEqual(actual_fuzzy, expected_fuzzy, 'fuzzy data did not match expected')
