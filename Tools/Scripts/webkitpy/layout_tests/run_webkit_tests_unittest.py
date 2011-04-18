#!/usr/bin/python
# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
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

"""Unit tests for run_webkit_tests."""

from __future__ import with_statement

import codecs
import itertools
import logging
import os
import Queue
import sys
import thread
import time
import threading
import unittest

try:
    import multiprocessing
except ImportError:
    multiprocessing = None

from webkitpy.common import array_stream
from webkitpy.common.system import outputcapture
from webkitpy.common.system import filesystem_mock
from webkitpy.tool import mocktool
from webkitpy.layout_tests import port
from webkitpy.layout_tests import run_webkit_tests
from webkitpy.layout_tests.port.test import TestPort, TestDriver
from webkitpy.layout_tests.port.test_files import is_reference_html_file
from webkitpy.python24.versioning import compare_version
from webkitpy.test.skip import skip_if

from webkitpy.thirdparty.mock import Mock


def parse_args(extra_args=None, record_results=False, tests_included=False,
               print_nothing=True):
    extra_args = extra_args or []
    if print_nothing:
        args = ['--print', 'nothing']
    else:
        args = []
    if not '--platform' in extra_args:
        args.extend(['--platform', 'test'])
    if not record_results:
        args.append('--no-record-results')
    if not '--child-processes' in extra_args and not '--worker-model' in extra_args:
        args.extend(['--worker-model', 'inline'])
    args.extend(extra_args)
    if not tests_included:
        # We use the glob to test that globbing works.
        args.extend(['passes',
                     'http/tests',
                     'websocket/tests',
                     'failures/expected/*'])
    return run_webkit_tests.parse_args(args)


def passing_run(extra_args=None, port_obj=None, record_results=False,
                tests_included=False, filesystem=None):
    options, parsed_args = parse_args(extra_args, record_results,
                                      tests_included)
    if not port_obj:
        port_obj = port.get(port_name=options.platform, options=options,
                            user=mocktool.MockUser(), filesystem=filesystem)
    res = run_webkit_tests.run(port_obj, options, parsed_args)
    return res == 0


def logging_run(extra_args=None, port_obj=None, record_results=False, tests_included=False, filesystem=None):
    options, parsed_args = parse_args(extra_args=extra_args,
                                      record_results=record_results,
                                      tests_included=tests_included,
                                      print_nothing=False)
    user = mocktool.MockUser()
    if not port_obj:
        port_obj = port.get(port_name=options.platform, options=options,
                            user=user, filesystem=filesystem)

    res, buildbot_output, regular_output = run_and_capture(port_obj, options,
                                                           parsed_args)
    return (res, buildbot_output, regular_output, user)


def run_and_capture(port_obj, options, parsed_args):
    oc = outputcapture.OutputCapture()
    try:
        oc.capture_output()
        buildbot_output = array_stream.ArrayStream()
        regular_output = array_stream.ArrayStream()
        res = run_webkit_tests.run(port_obj, options, parsed_args,
                                   buildbot_output=buildbot_output,
                                   regular_output=regular_output)
    finally:
        oc.restore_output()
    return (res, buildbot_output, regular_output)


def get_tests_run(extra_args=None, tests_included=False, flatten_batches=False,
                  filesystem=None, include_reference_html=False):
    extra_args = extra_args or []
    if not tests_included:
        # Not including http tests since they get run out of order (that
        # behavior has its own test, see test_get_test_file_queue)
        extra_args = ['passes', 'failures'] + extra_args
    options, parsed_args = parse_args(extra_args, tests_included=True)

    user = mocktool.MockUser()

    test_batches = []


    class RecordingTestDriver(TestDriver):
        def __init__(self, port, worker_number):
            TestDriver.__init__(self, port, worker_number)
            self._current_test_batch = None

        def poll(self):
            # So that we don't create a new driver for every test
            return None

        def stop(self):
            self._current_test_batch = None

        def run_test(self, test_input):
            if self._current_test_batch is None:
                self._current_test_batch = []
                test_batches.append(self._current_test_batch)
            test_name = self._port.relative_test_filename(test_input.filename)
            # In case of reftest, one test calls the driver's run_test() twice.
            # We should not add a reference html used by reftests to tests unless include_reference_html parameter
            # is explicitly given.
            if include_reference_html or not is_reference_html_file(test_input.filename):
                self._current_test_batch.append(test_name)
            return TestDriver.run_test(self, test_input)

    class RecordingTestPort(TestPort):
        def create_driver(self, worker_number):
            return RecordingTestDriver(self, worker_number)

    recording_port = RecordingTestPort(options=options, user=user, filesystem=filesystem)
    run_and_capture(recording_port, options, parsed_args)

    if flatten_batches:
        return list(itertools.chain(*test_batches))

    return test_batches


class MainTest(unittest.TestCase):
    def test_accelerated_compositing(self):
        # This just tests that we recognize the command line args
        self.assertTrue(passing_run(['--accelerated-compositing']))
        self.assertTrue(passing_run(['--no-accelerated-compositing']))

    def test_accelerated_2d_canvas(self):
        # This just tests that we recognize the command line args
        self.assertTrue(passing_run(['--accelerated-2d-canvas']))
        self.assertTrue(passing_run(['--no-accelerated-2d-canvas']))

    def test_basic(self):
        self.assertTrue(passing_run())

    def test_batch_size(self):
        batch_tests_run = get_tests_run(['--batch-size', '2'])
        for batch in batch_tests_run:
            self.assertTrue(len(batch) <= 2, '%s had too many tests' % ', '.join(batch))

    def test_child_process_1(self):
        _, _, regular_output, _ = logging_run(
             ['--print', 'config', '--worker-model', 'threads', '--child-processes', '1'])
        self.assertTrue(any(['Running 1 ' in line for line in regular_output.get()]))

    def test_child_processes_2(self):
        _, _, regular_output, _ = logging_run(
             ['--print', 'config', '--worker-model', 'threads', '--child-processes', '2'])
        self.assertTrue(any(['Running 2 ' in line for line in regular_output.get()]))

    def test_child_processes_min(self):
        _, _, regular_output, _ = logging_run(
             ['--print', 'config', '--worker-model', 'threads', '--child-processes', '2', 'passes'],
             tests_included=True)
        self.assertTrue(any(['Running 1 ' in line for line in regular_output.get()]))

    def test_dryrun(self):
        batch_tests_run = get_tests_run(['--dry-run'])
        self.assertEqual(batch_tests_run, [])

        batch_tests_run = get_tests_run(['-n'])
        self.assertEqual(batch_tests_run, [])

    def test_exception_raised(self):
        self.assertRaises(ValueError, logging_run,
            ['failures/expected/exception.html'], tests_included=True)

    def test_full_results_html(self):
        # FIXME: verify html?
        res, out, err, user = logging_run(['--full-results-html'])
        self.assertEqual(res, 0)

    def test_help_printing(self):
        res, out, err, user = logging_run(['--help-printing'])
        self.assertEqual(res, 0)
        self.assertTrue(out.empty())
        self.assertFalse(err.empty())

    def test_hung_thread(self):
        res, out, err, user = logging_run(['--run-singly', '--time-out-ms=50',
                                          'failures/expected/hang.html'],
                                          tests_included=True)
        self.assertEqual(res, 0)
        self.assertFalse(out.empty())
        self.assertFalse(err.empty())

    def test_keyboard_interrupt(self):
        # Note that this also tests running a test marked as SKIP if
        # you specify it explicitly.
        self.assertRaises(KeyboardInterrupt, logging_run,
            ['failures/expected/keyboard.html'], tests_included=True)

    def test_keyboard_interrupt_inline_worker_model(self):
        self.assertRaises(KeyboardInterrupt, logging_run,
            ['failures/expected/keyboard.html', '--worker-model', 'inline'],
            tests_included=True)

    def test_last_results(self):
        fs = port.unit_test_filesystem()
        # We do a logging run here instead of a passing run in order to
        # suppress the output from the json generator.
        res, buildbot_output, regular_output, user = logging_run(['--clobber-old-results'], record_results=True, filesystem=fs)
        res, buildbot_output, regular_output, user = logging_run(
            ['--print-last-failures'], filesystem=fs)
        self.assertEqual(regular_output.get(), ['\n\n'])
        self.assertEqual(buildbot_output.get(), [])

    def test_lint_test_files(self):
        res, out, err, user = logging_run(['--lint-test-files'])
        self.assertEqual(res, 0)
        self.assertTrue(out.empty())
        self.assertTrue(any(['Lint succeeded' in msg for msg in err.get()]))

    def test_lint_test_files__errors(self):
        options, parsed_args = parse_args(['--lint-test-files'])
        user = mocktool.MockUser()
        port_obj = port.get(options.platform, options=options, user=user)
        port_obj.test_expectations = lambda: "# syntax error"
        res, out, err = run_and_capture(port_obj, options, parsed_args)

        self.assertEqual(res, -1)
        self.assertTrue(out.empty())
        self.assertTrue(any(['Lint failed' in msg for msg in err.get()]))

    def test_no_tests_found(self):
        res, out, err, user = logging_run(['resources'], tests_included=True)
        self.assertEqual(res, -1)
        self.assertTrue(out.empty())
        self.assertTrue('No tests to run.\n' in err.get())

    def test_no_tests_found_2(self):
        res, out, err, user = logging_run(['foo'], tests_included=True)
        self.assertEqual(res, -1)
        self.assertTrue(out.empty())
        self.assertTrue('No tests to run.\n' in err.get())

    def test_randomize_order(self):
        # FIXME: verify order was shuffled
        self.assertTrue(passing_run(['--randomize-order']))

    def test_run_chunk(self):
        # Test that we actually select the right chunk
        all_tests_run = get_tests_run(flatten_batches=True)
        chunk_tests_run = get_tests_run(['--run-chunk', '1:4'], flatten_batches=True)
        self.assertEquals(all_tests_run[4:8], chunk_tests_run)

        # Test that we wrap around if the number of tests is not evenly divisible by the chunk size
        tests_to_run = ['passes/error.html', 'passes/image.html', 'passes/platform_image.html', 'passes/text.html']
        chunk_tests_run = get_tests_run(['--run-chunk', '1:3'] + tests_to_run, tests_included=True, flatten_batches=True)
        self.assertEquals(['passes/text.html', 'passes/error.html', 'passes/image.html'], chunk_tests_run)

    def test_run_force(self):
        # This raises an exception because we run
        # failures/expected/exception.html, which is normally SKIPped.
        self.assertRaises(ValueError, logging_run, ['--force'])

    def test_run_part(self):
        # Test that we actually select the right part
        tests_to_run = ['passes/error.html', 'passes/image.html', 'passes/platform_image.html', 'passes/text.html']
        tests_run = get_tests_run(['--run-part', '1:2'] + tests_to_run, tests_included=True, flatten_batches=True)
        self.assertEquals(['passes/error.html', 'passes/image.html'], tests_run)

        # Test that we wrap around if the number of tests is not evenly divisible by the chunk size
        # (here we end up with 3 parts, each with 2 tests, and we only have 4 tests total, so the
        # last part repeats the first two tests).
        chunk_tests_run = get_tests_run(['--run-part', '3:3'] + tests_to_run, tests_included=True, flatten_batches=True)
        self.assertEquals(['passes/error.html', 'passes/image.html'], chunk_tests_run)

    def test_run_singly(self):
        batch_tests_run = get_tests_run(['--run-singly'])
        for batch in batch_tests_run:
            self.assertEquals(len(batch), 1, '%s had too many tests' % ', '.join(batch))

    def test_run_singly_actually_runs_tests(self):
        res, _, _, _ = logging_run(['--run-singly', 'failures/unexpected'])
        self.assertEquals(res, 5)

    def test_single_file(self):
        tests_run = get_tests_run(['passes/text.html'], tests_included=True, flatten_batches=True)
        self.assertEquals(['passes/text.html'], tests_run)

    def test_single_file_with_prefix(self):
        tests_run = get_tests_run(['LayoutTests/passes/text.html'], tests_included=True, flatten_batches=True)
        self.assertEquals(['passes/text.html'], tests_run)

    def test_single_skipped_file(self):
        tests_run = get_tests_run(['failures/expected/keybaord.html'], tests_included=True, flatten_batches=True)
        self.assertEquals([], tests_run)

    def test_stderr_is_saved(self):
        fs = port.unit_test_filesystem()
        self.assertTrue(passing_run(filesystem=fs))
        self.assertEquals(fs.read_text_file('/tmp/layout-test-results/passes/error-stderr.txt'),
                          'stuff going to stderr')

    def test_test_list(self):
        fs = port.unit_test_filesystem()
        filename = '/tmp/foo.txt'
        fs.write_text_file(filename, 'passes/text.html')
        tests_run = get_tests_run(['--test-list=%s' % filename], tests_included=True, flatten_batches=True, filesystem=fs)
        self.assertEquals(['passes/text.html'], tests_run)
        fs.remove(filename)
        res, out, err, user = logging_run(['--test-list=%s' % filename],
                                          tests_included=True, filesystem=fs)
        self.assertEqual(res, -1)
        self.assertFalse(err.empty())

    def test_test_list_with_prefix(self):
        fs = port.unit_test_filesystem()
        filename = '/tmp/foo.txt'
        fs.write_text_file(filename, 'LayoutTests/passes/text.html')
        tests_run = get_tests_run(['--test-list=%s' % filename], tests_included=True, flatten_batches=True, filesystem=fs)
        self.assertEquals(['passes/text.html'], tests_run)

    def test_unexpected_failures(self):
        # Run tests including the unexpected failures.
        self._url_opened = None
        res, out, err, user = logging_run(tests_included=True)

        # Update this magic number if you add an unexpected test to webkitpy.layout_tests.port.test
        # FIXME: It's nice to have a routine in port/test.py that returns this number.
        unexpected_tests_count = 5

        self.assertEqual(res, unexpected_tests_count)
        self.assertFalse(out.empty())
        self.assertFalse(err.empty())
        self.assertEqual(user.opened_urls, ['/tmp/layout-test-results/results.html'])

    def test_exit_after_n_failures_upload(self):
        fs = port.unit_test_filesystem()
        res, buildbot_output, regular_output, user = logging_run([
                'failures/unexpected/text-image-checksum.html',
                'passes/text.html',
                '--exit-after-n-failures', '1',
            ],
            tests_included=True,
            record_results=True,
            filesystem=fs)
        self.assertTrue('/tmp/layout-test-results/incremental_results.json' in fs.files)

    def test_exit_after_n_failures(self):
        # Unexpected failures should result in tests stopping.
        tests_run = get_tests_run([
                'failures/unexpected/text-image-checksum.html',
                'passes/text.html',
                '--exit-after-n-failures', '1',
            ],
            tests_included=True,
            flatten_batches=True)
        self.assertEquals(['failures/unexpected/text-image-checksum.html'], tests_run)

        # But we'll keep going for expected ones.
        tests_run = get_tests_run([
                'failures/expected/text.html',
                'passes/text.html',
                '--exit-after-n-failures', '1',
            ],
            tests_included=True,
            flatten_batches=True)
        self.assertEquals(['failures/expected/text.html', 'passes/text.html'], tests_run)

    def test_exit_after_n_crashes(self):
        # Unexpected crashes should result in tests stopping.
        tests_run = get_tests_run([
                'failures/unexpected/crash.html',
                'passes/text.html',
                '--exit-after-n-crashes-or-timeouts', '1',
            ],
            tests_included=True,
            flatten_batches=True)
        self.assertEquals(['failures/unexpected/crash.html'], tests_run)

        # Same with timeouts.
        tests_run = get_tests_run([
                'failures/unexpected/timeout.html',
                'passes/text.html',
                '--exit-after-n-crashes-or-timeouts', '1',
            ],
            tests_included=True,
            flatten_batches=True)
        self.assertEquals(['failures/unexpected/timeout.html'], tests_run)

        # But we'll keep going for expected ones.
        tests_run = get_tests_run([
                'failures/expected/crash.html',
                'passes/text.html',
                '--exit-after-n-crashes-or-timeouts', '1',
            ],
            tests_included=True,
            flatten_batches=True)
        self.assertEquals(['failures/expected/crash.html', 'passes/text.html'], tests_run)

    def test_exit_after_n_crashes_inline_worker_model(self):
        tests_run = get_tests_run([
                'failures/unexpected/timeout.html',
                'passes/text.html',
                '--exit-after-n-crashes-or-timeouts', '1',
                '--worker-model', 'inline',
            ],
            tests_included=True,
            flatten_batches=True)
        self.assertEquals(['failures/unexpected/timeout.html'], tests_run)

    def test_results_directory_absolute(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.

        fs = port.unit_test_filesystem()
        with fs.mkdtemp() as tmpdir:
            res, out, err, user = logging_run(['--results-directory=' + str(tmpdir)],
                                              tests_included=True, filesystem=fs)
            self.assertEqual(user.opened_urls, [fs.join(tmpdir, 'results.html')])

    def test_results_directory_default(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.

        # This is the default location.
        res, out, err, user = logging_run(tests_included=True)
        self.assertEqual(user.opened_urls, ['/tmp/layout-test-results/results.html'])

    def test_results_directory_relative(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.
        fs = port.unit_test_filesystem()
        fs.maybe_make_directory('/tmp/cwd')
        fs.chdir('/tmp/cwd')
        res, out, err, user = logging_run(['--results-directory=foo'],
                                          tests_included=True, filesystem=fs)
        self.assertEqual(user.opened_urls, ['/tmp/cwd/foo/results.html'])

    # These next tests test that we run the tests in ascending alphabetical
    # order per directory. HTTP tests are sharded separately from other tests,
    # so we have to test both.
    def assert_run_order(self, worker_model, child_processes='1'):
        tests_run = get_tests_run(['--worker-model', worker_model,
            '--child-processes', child_processes, 'passes'],
            tests_included=True, flatten_batches=True)
        self.assertEquals(tests_run, sorted(tests_run))

        tests_run = get_tests_run(['--worker-model', worker_model,
            '--child-processes', child_processes, 'http/tests/passes'],
            tests_included=True, flatten_batches=True)
        self.assertEquals(tests_run, sorted(tests_run))

    def test_run_order__inline(self):
        self.assert_run_order('inline')

    def test_tolerance(self):
        class ImageDiffTestPort(TestPort):
            def diff_image(self, expected_contents, actual_contents,
                   diff_filename=None):
                self.tolerance_used_for_diff_image = self._options.tolerance
                return True

        def get_port_for_run(args):
            options, parsed_args = run_webkit_tests.parse_args(args)
            test_port = ImageDiffTestPort(options=options, user=mocktool.MockUser())
            passing_run(args, port_obj=test_port, tests_included=True)
            return test_port

        base_args = ['--pixel-tests', 'failures/expected/*']

        # If we pass in an explicit tolerance argument, then that will be used.
        test_port = get_port_for_run(base_args + ['--tolerance', '.1'])
        self.assertEqual(0.1, test_port.tolerance_used_for_diff_image)
        test_port = get_port_for_run(base_args + ['--tolerance', '0'])
        self.assertEqual(0, test_port.tolerance_used_for_diff_image)

        # Otherwise the port's default tolerance behavior (including ignoring it)
        # should be used.
        test_port = get_port_for_run(base_args)
        self.assertEqual(None, test_port.tolerance_used_for_diff_image)

    def test_worker_model__inline(self):
        self.assertTrue(passing_run(['--worker-model', 'inline']))

    def test_worker_model__inline_with_child_processes(self):
        res, out, err, user = logging_run(['--worker-model', 'inline',
                                           '--child-processes', '2'])
        self.assertEqual(res, 0)
        self.assertTrue('--worker-model=inline overrides --child-processes\n' in err.get())

    def test_worker_model__processes(self):
        # FIXME: remove this when we fix test-webkitpy to work properly
        # with the multiprocessing module (bug 54520).
        if multiprocessing and sys.platform not in ('cygwin', 'win32'):
            self.assertTrue(passing_run(['--worker-model', 'processes']))

    def test_worker_model__processes_and_dry_run(self):
        if multiprocessing and sys.platform not in ('cygwin', 'win32'):
            self.assertTrue(passing_run(['--worker-model', 'processes', '--dry-run']))

    def test_worker_model__threads(self):
        self.assertTrue(passing_run(['--worker-model', 'threads']))

    def test_worker_model__unknown(self):
        self.assertRaises(ValueError, logging_run,
                          ['--worker-model', 'unknown'])

    def test_reftest_run(self):
        tests_run = get_tests_run(['passes/reftest.html'], tests_included=True, flatten_batches=True)
        self.assertEquals(['passes/reftest.html'], tests_run)

    def test_reftest_expected_html_should_be_ignored(self):
        tests_run = get_tests_run(['passes/reftest-expected.html'], tests_included=True, flatten_batches=True)
        self.assertEquals([], tests_run)

    def test_reftest_driver_should_run_expected_html(self):
        tests_run = get_tests_run(['passes/reftest.html'], tests_included=True, flatten_batches=True,
                                  include_reference_html=True)
        self.assertEquals(['passes/reftest.html', 'passes/reftest-expected.html'], tests_run)

    def test_reftest_driver_should_run_expected_mismatch_html(self):
        tests_run = get_tests_run(['passes/mismatch.html'], tests_included=True, flatten_batches=True,
                                  include_reference_html=True)
        self.assertEquals(['passes/mismatch.html', 'passes/mismatch-expected-mismatch.html'], tests_run)

    def test_additional_platform_directory(self):
        self.assertTrue(passing_run(['--additional-platform-directory', '/tmp/foo']))
        self.assertTrue(passing_run(['--additional-platform-directory', '/tmp/../foo']))
        self.assertTrue(passing_run(['--additional-platform-directory', '/tmp/foo',
            '--additional-platform-directory', '/tmp/bar']))

        res, buildbot_output, regular_output, user = logging_run(
             ['--additional-platform-directory', 'foo'])
        self.assertTrue('--additional-platform-directory=foo is ignored since it is not absolute\n'
                        in regular_output.get())


MainTest = skip_if(MainTest, sys.platform == 'cygwin' and compare_version(sys, '2.6')[0] < 0, 'new-run-webkit-tests tests hang on Cygwin Python 2.5.2')


class RebaselineTest(unittest.TestCase):
    def assertBaselines(self, file_list, file):
        "assert that the file_list contains the baselines."""
        for ext in [".txt", ".png", ".checksum"]:
            baseline = file + "-expected" + ext
            self.assertTrue(any(f.find(baseline) != -1 for f in file_list))

    # FIXME: Add tests to ensure that we're *not* writing baselines when we're not
    # supposed to be.

    def test_reset_results(self):
        # Test that we update expectations in place. If the expectation
        # is missing, update the expected generic location.
        fs = port.unit_test_filesystem()
        passing_run(['--pixel-tests',
                        '--reset-results',
                        'passes/image.html',
                        'failures/expected/missing_image.html'],
                        tests_included=True, filesystem=fs)
        file_list = fs.written_files.keys()
        file_list.remove('/tmp/layout-test-results/tests_run0.txt')
        self.assertEqual(len(file_list), 6)
        self.assertBaselines(file_list,
            "/passes/image")
        self.assertBaselines(file_list,
            "/failures/expected/missing_image")

    def test_new_baseline(self):
        # Test that we update the platform expectations. If the expectation
        # is mssing, then create a new expectation in the platform dir.
        fs = port.unit_test_filesystem()
        passing_run(['--pixel-tests',
                        '--new-baseline',
                        'passes/image.html',
                        'failures/expected/missing_image.html'],
                    tests_included=True, filesystem=fs)
        file_list = fs.written_files.keys()
        file_list.remove('/tmp/layout-test-results/tests_run0.txt')
        self.assertEqual(len(file_list), 6)
        self.assertBaselines(file_list,
            "/platform/test-mac-leopard/passes/image")
        self.assertBaselines(file_list,
            "/platform/test-mac-leopard/failures/expected/missing_image")


class DryrunTest(unittest.TestCase):
    # FIXME: it's hard to know which platforms are safe to test; the
    # chromium platforms require a chromium checkout, and the mac platform
    # requires fcntl, so it can't be tested on win32, etc. There is
    # probably a better way of handling this.
    def disabled_test_darwin(self):
        if sys.platform != "darwin":
            return

        self.assertTrue(passing_run(['--platform', 'dryrun', 'fast/html'],
                        tests_included=True))
        self.assertTrue(passing_run(['--platform', 'dryrun-mac', 'fast/html'],
                        tests_included=True))

    def test_test(self):
        self.assertTrue(passing_run(['--platform', 'dryrun-test',
                                           '--pixel-tests']))


if __name__ == '__main__':
    unittest.main()
