# Copyright (C) 2011 Google Inc. All rights reserved.
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

from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests.controllers import test_result_writer
from webkitpy.layout_tests.models import test_failures
from webkitpy.port.driver import DriverOutput
from webkitpy.port.test import TestPort
from webkitpy.port.image_diff import ImageDiffResult


class TestResultWriterTest(unittest.TestCase):

    def test_reftest_diff_image(self):
        """A write_test_result should call port.diff_image with tolerance=0 in case of FailureReftestMismatch."""
        used_tolerance_values = []

        class ImageDiffTestPort(TestPort):
            def diff_image(self, expected_contents, actual_contents, tolerance):
                used_tolerance_values.append(tolerance)
                return ImageDiffResult(passed=False, diff_image=b'', difference=1, tolerance=tolerance)

        host = MockHost()
        port = ImageDiffTestPort(host)
        test_name = 'failures/unexpected/reftest.html'
        test_reference_file = host.filesystem.join(port.layout_tests_dir(), 'failures/unexpected/reftest-expected.html')
        driver_output1 = DriverOutput('text1', 'image1', 'imagehash1', 'audio1')
        driver_output2 = DriverOutput('text2', 'image2', 'imagehash2', 'audio2')
        failures = [test_failures.FailureReftestMismatch(test_reference_file, ImageDiffResult(passed=False, diff_image=b'', difference=1, tolerance=1))]
        test_result_writer.write_test_result(host.filesystem, ImageDiffTestPort(host), port.results_directory(), test_name,
                                             driver_output1, driver_output2, failures)
        self.assertEqual([0], used_tolerance_values)

    def test_output_filename(self):
        host = MockHost()
        port = TestPort(host)
        fs = host.filesystem
        writer = test_result_writer.TestResultWriter(fs, port, port.results_directory(), 'require-corp-revalidated-images.https.html')
        self.assertEqual(writer.output_filename('-diff.txt'), fs.join(port.results_directory(), 'require-corp-revalidated-images.https-diff.txt'))

    def test_output_filename_worker_variant(self):
        host = MockHost()
        port = TestPort(host)
        fs = host.filesystem
        writer = test_result_writer.TestResultWriter(fs, port, port.results_directory(), 'template_test/pbkdf2.https.any.worker.html?1-1000')
        self.assertEqual(fs.join(port.results_directory(), 'template_test/pbkdf2.https.any.worker_1-1000-diff.txt'), writer.output_filename('-diff.txt'))

    def test_output_filename_variant(self):
        host = MockHost()
        port = TestPort(host)
        fs = host.filesystem
        writer = test_result_writer.TestResultWriter(fs, port, port.results_directory(), 'template_test2/pbkdf2.https.any.html?1-1000')
        self.assertEqual(fs.join(port.results_directory(), 'template_test2/pbkdf2.https.any_1-1000-diff.txt'), writer.output_filename('-diff.txt'))

    def test_output_svg_filename(self):
        host = MockHost()
        port = TestPort(host)
        fs = host.filesystem
        writer = test_result_writer.TestResultWriter(fs, port, port.results_directory(), 'svg/W3C-SVG-1.1/animate-elem-02-t.svg')
        self.assertEqual(fs.join(port.results_directory(), 'svg/W3C-SVG-1.1/animate-elem-02-t-diff.txt'), writer.output_filename('-diff.txt'))
