# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
# Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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

import json
import sys
import unittest

from webkitcorepy import StringIO, OutputCapture

from webkitpy.common.system import path
from webkitpy.common.system.crashlogs_unittest import make_mock_crash_report_darwin
from webkitpy.common.system.systemhost import SystemHost
from webkitpy.common.host import Host
from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests import run_webkit_tests
from webkitpy.layout_tests.models.test_run_results import INTERRUPTED_EXIT_STATUS
from webkitpy.port import test
from webkitpy.port.image_diff import ImageDiffResult
from webkitpy.xcode.device_type import DeviceType


def parse_args(extra_args=None, tests_included=False, new_results=False, print_nothing=True):
    extra_args = extra_args or []
    args = []
    if not '--platform' in extra_args:
        args.extend(['--platform', 'test'])
    if not new_results:
        args.append('--no-new-test-results')

    if not '--child-processes' in extra_args:
        args.extend(['--child-processes', 1])

    if not '--world-leaks' in extra_args:
        args.append('--world-leaks')

    if not '--accessibility-isolated-tree' in extra_args:
        args.append('--accessibility-isolated-tree')

    args.extend(extra_args)
    if not tests_included:
        # We use the glob to test that globbing works.
        args.extend(['passes',
                     'http/tests',
                     'websocket/tests',
                     'failures/expected/*'])
    return run_webkit_tests.parse_args(args)


def passing_run(extra_args=None, port_obj=None, tests_included=False, host=None, shared_port=True):
    options, parsed_args = parse_args(extra_args, tests_included)
    if not port_obj:
        host = host or MockHost()
        port_obj = host.port_factory.get(port_name=options.platform, options=options)

    if shared_port:
        port_obj.host.port_factory.get = lambda *args, **kwargs: port_obj

    logging_stream = StringIO()
    run_details = run_webkit_tests.run(port_obj, options, parsed_args, logging_stream=logging_stream)
    return run_details.exit_code == 0


def logging_run(extra_args=None, port_obj=None, tests_included=False, host=None, new_results=False, shared_port=True):
    options, parsed_args = parse_args(extra_args=extra_args,
                                      tests_included=tests_included,
                                      print_nothing=False, new_results=new_results)
    host = host or MockHost()
    if not port_obj:
        port_obj = host.port_factory.get(port_name=options.platform, options=options)

    run_details, output = run_and_capture(port_obj, options, parsed_args, shared_port)
    return (run_details, output, host.user)


def run_and_capture(port_obj, options, parsed_args, shared_port=True):
    if shared_port:
        port_obj.host.port_factory.get = lambda *args, **kwargs: port_obj
    with OutputCapture():
        logging_stream = StringIO()
        run_details = run_webkit_tests.run(port_obj, options, parsed_args, logging_stream=logging_stream)

    return (run_details, logging_stream)


def get_tests_run(args, host=None):
    results = get_test_results(args, host)
    return [result.test_name for result in results]


def get_test_batches(args, host=None):
    results = get_test_results(args, host)
    batches = []
    batch = []
    current_pid = None
    for result in results:
        if batch and result.pid != current_pid:
            batches.append(batch)
            batch = []
        batch.append(result.test_name)
    if batch:
        batches.append(batch)
    return batches


def get_test_results(args, host=None):
    options, parsed_args = parse_args(args, tests_included=True)

    host = host or MockHost()
    port_obj = host.port_factory.get(port_name=options.platform, options=options)

    with OutputCapture():
        logging_stream = StringIO()
        run_details = run_webkit_tests.run(port_obj, options, parsed_args, logging_stream=logging_stream)

    all_results = []
    if run_details.initial_results:
        all_results.extend(run_details.initial_results.all_results)

    if run_details.retry_results:
        all_results.extend(run_details.retry_results.all_results)
    return all_results


def parse_full_results(full_results_text):
    json_to_eval = full_results_text.replace("ADD_RESULTS(", "").replace(");", "")
    compressed_results = json.loads(json_to_eval)
    return compressed_results


class StreamTestingMixin(object):
    def assertContains(self, stream, string):
        self.assertTrue(string in stream.getvalue())

    def assertEmpty(self, stream):
        self.assertFalse(stream.getvalue())

    def assertNotEmpty(self, stream):
        self.assertTrue(stream.getvalue())


class RunTest(unittest.TestCase, StreamTestingMixin):
    def setUp(self):
        # A real PlatformInfo object is used here instead of a
        # MockPlatformInfo because we need to actually check for
        # Windows and Mac to skip some tests.
        self._platform = SystemHost.get_default().platform

        # FIXME: Remove this when we fix test-webkitpy to work
        # properly on cygwin (bug 63846).
        self.should_test_processes = not self._platform.is_win()

    def serial_test_basic(self):
        options, args = parse_args(tests_included=True)
        logging_stream = StringIO()
        host = MockHost()
        port_obj = host.port_factory.get(options.platform, options)
        details = run_webkit_tests.run(port_obj, options, args, logging_stream)

        # These numbers will need to be updated whenever we add new tests.
        self.assertEqual(details.initial_results.total, test.TOTAL_TESTS)
        self.assertEqual(details.initial_results.expected_skips, test.TOTAL_SKIPS)
        self.assertEqual(len(details.initial_results.unexpected_results_by_name), test.UNEXPECTED_PASSES + test.UNEXPECTED_FAILURES)
        self.assertEqual(details.exit_code, test.UNEXPECTED_FAILURES - 3)  # FIXME: Figure out why - 3 is needed.
        self.assertEqual(details.retry_results.total, test.TOTAL_RETRIES)

        one_line_summary = "%d tests ran as expected, %d didn't:\n" % (
            details.initial_results.total - details.initial_results.expected_skips - len(details.initial_results.unexpected_results_by_name),
            len(details.initial_results.unexpected_results_by_name))
        self.assertTrue(one_line_summary in logging_stream.getvalue())

        # Ensure the results were summarized properly.
        self.assertEqual(details.summarized_results['num_regressions'], details.exit_code)

        # Ensure the image diff percentage is in the results.
        self.assertEqual(details.summarized_results['tests']['failures']['expected']['image.html']['image_diff_percent'], 1)

        # Ensure the results were written out and displayed.
        full_results_text = host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json')
        json_to_eval = full_results_text.replace("ADD_RESULTS(", "").replace(");", "")
        self.assertEqual(json.loads(json_to_eval), details.summarized_results)

        self.assertEqual(host.user.opened_urls, [path.abspath_to_uri(MockHost().platform, '/tmp/layout-test-results/results.html')])

    def serial_test_batch_size(self):
        batch_tests_run = get_test_batches(['--batch-size', '2'])
        for batch in batch_tests_run:
            self.assertTrue(len(batch) <= 2, '%s had too many tests' % ', '.join(batch))

    def test_child_processes_2(self):
        if self.should_test_processes:
            _, regular_output, _ = logging_run(
                ['--debug-rwt-logging', '--child-processes', '2'], shared_port=False)
            self.assertTrue(any(['Running 2 ' in line for line in regular_output.getvalue().splitlines()]))

    def test_child_processes_min(self):
        if self.should_test_processes:
            _, regular_output, _ = logging_run(
                ['--debug-rwt-logging', '--child-processes', '2', '-i', 'passes/passes', '-i', 'platform', 'passes'],
                tests_included=True, shared_port=False)
            self.assertTrue(any(['Running 1 ' in line for line in regular_output.getvalue().splitlines()]))

    def serial_test_dryrun(self):
        tests_run = get_tests_run(['--dry-run'])
        self.assertEqual(tests_run, [])

        tests_run = get_tests_run(['-n'])
        self.assertEqual(tests_run, [])

    def test_exception_raised(self):
        # Exceptions raised by a worker are treated differently depending on
        # whether they are in-process or out. inline exceptions work as normal,
        # which allows us to get the full stack trace and traceback from the
        # worker. The downside to this is that it could be any error, but this
        # is actually useful in testing.
        #
        # Exceptions raised in a separate process are re-packaged into
        # WorkerExceptions (a subclass of BaseException), which have a string capture of the stack which can
        # be printed, but don't display properly in the unit test exception handlers.
        self.assertRaises(BaseException, logging_run,
            ['failures/expected/exception.html', '--child-processes', '1'], tests_included=True)

        if self.should_test_processes:
            self.assertRaises(BaseException, logging_run,
                ['--child-processes', '2', '--force', 'failures/expected/exception.html', 'passes/text.html'], tests_included=True, shared_port=False)

    def serial_test_full_results_html(self):
        # FIXME: verify html?
        details, _, _ = logging_run(['--full-results-html'])
        self.assertEqual(details.exit_code, 0)

    def test_keyboard_interrupt(self):
        # Note that this also tests running a test marked as SKIP if
        # you specify it explicitly.
        details, _, _ = logging_run(['failures/expected/keyboard.html', '--child-processes', '1'], tests_included=True)
        self.assertEqual(details.exit_code, INTERRUPTED_EXIT_STATUS)

        if self.should_test_processes:
            _, regular_output, _ = logging_run(['failures/expected/keyboard.html', 'passes/text.html', '--child-processes', '2', '--force'], tests_included=True, shared_port=False)
            self.assertTrue(any(['Interrupted, exiting' in line for line in regular_output.getvalue().splitlines()]))

    def test_all_tests_skipped(self):
        details, err, _ = logging_run(['foo'], tests_included=True)
        self.assertEqual(details.exit_code, 0)
        self.assertContains(err, 'All tests skipped.\n')

    def test_natural_order(self):
        tests_to_run = ['passes/audio.html', 'failures/expected/text.html', 'failures/expected/missing_text.html', 'passes/args.html']
        tests_run = get_tests_run(['--order=natural'] + tests_to_run)
        self.assertEqual(['failures/expected/missing_text.html', 'failures/expected/text.html', 'passes/args.html', 'passes/audio.html'], tests_run)

    def test_natural_order_test_specified_multiple_times(self):
        tests_to_run = ['passes/args.html', 'passes/audio.html', 'passes/audio.html', 'passes/args.html']
        tests_run = get_tests_run(['--order=natural'] + tests_to_run)
        self.assertEqual(['passes/args.html', 'passes/args.html', 'passes/audio.html', 'passes/audio.html'], tests_run)

    def test_random_order(self):
        tests_to_run = ['passes/audio.html', 'failures/expected/text.html', 'failures/expected/missing_text.html', 'passes/args.html']
        tests_run = get_tests_run(['--order=random'] + tests_to_run)
        self.assertEqual(sorted(tests_to_run), sorted(tests_run))

    def test_random_order_test_specified_multiple_times(self):
        tests_to_run = ['passes/args.html', 'passes/audio.html', 'passes/audio.html', 'passes/args.html']
        tests_run = get_tests_run(['--order=random'] + tests_to_run)
        self.assertEqual(tests_run.count('passes/audio.html'), 2)
        self.assertEqual(tests_run.count('passes/args.html'), 2)

    def test_no_order(self):
        tests_to_run = ['passes/audio.html', 'failures/expected/text.html', 'failures/expected/missing_text.html', 'passes/args.html']
        tests_run = get_tests_run(['--order=none'] + tests_to_run)
        self.assertEqual(tests_to_run, tests_run)

    def test_no_order_test_specified_multiple_times(self):
        tests_to_run = ['passes/args.html', 'passes/audio.html', 'passes/audio.html', 'passes/args.html']
        tests_run = get_tests_run(['--order=none'] + tests_to_run)
        self.assertEqual(tests_to_run, tests_run)

    def serial_test_no_order_with_directory_entries_in_natural_order(self):
        tests_to_run = ['http/tests/ssl', 'http/tests/passes']
        tests_run = get_tests_run(['--order=none'] + tests_to_run)
        self.assertEqual(tests_run, ['http/tests/ssl/text.html', 'http/tests/passes/image.html', 'http/tests/passes/text.html'])

    def serial_test_gc_between_tests(self):
        self.assertTrue(passing_run(['--gc-between-tests']))

    def serial_test_check_for_world_leaks(self):
        self.assertTrue(passing_run(['--world-leaks']))

    def serial_test_complex_text(self):
        self.assertTrue(passing_run(['--complex-text']))

    def serial_test_threaded(self):
        self.assertTrue(passing_run(['--threaded']))

    def test_repeat_each(self):
        tests_to_run = ['passes/image.html', 'passes/text.html']
        tests_run = get_tests_run(['--repeat-each', '2'] + tests_to_run)
        self.assertEqual(tests_run, ['passes/image.html', 'passes/image.html', 'passes/text.html', 'passes/text.html'])

    def test_ignore_flag(self):
        # Note that passes/image.html is expected to be run since we specified it directly.
        tests_run = get_tests_run(['-i', 'passes', 'passes/image.html'])
        self.assertFalse('passes/text.html' in tests_run)
        self.assertTrue('passes/image.html' in tests_run)

    def test_skipped_flag(self):
        tests_run = get_tests_run(['passes'])
        self.assertFalse('passes/skipped/skip.html' in tests_run)
        num_tests_run_by_default = len(tests_run)

        # Check that nothing changes when we specify skipped=default.
        self.assertEqual(len(get_tests_run(['--skipped=default', 'passes'])),
                          num_tests_run_by_default)

        # Now check that we run one more test (the skipped one).
        tests_run = get_tests_run(['--skipped=ignore', 'passes'])
        self.assertTrue('passes/skipped/skip.html' in tests_run)
        self.assertEqual(len(tests_run), num_tests_run_by_default + 1)

        # Now check that we only run the skipped test.
        self.assertEqual(get_tests_run(['--skipped=only', 'passes']), ['passes/skipped/skip.html'])

        # Now check that we don't run anything.
        self.assertEqual(get_tests_run(['--skipped=always', 'passes/skipped/skip.html']), [])

    def test_ews_corner_case_failing_test(self):
        # We should skip running tests marked as failures (or flakies) when passing '--skip-failing-tests'
        self.assertEqual(get_tests_run(['--skip-failing-tests', 'failures/expected']), [])
        # But if we specify the name of the individual tests, then those tests should run
        list_of_tests_failing = ['failures/expected/timeout.html', 'failures/expected/text.html', 'failures/expected/crash.html', 'failures/expected/missing_image.html']
        self.assertEqual(get_tests_run(['--skip-failing-tests'] + list_of_tests_failing), list_of_tests_failing)
        # Unless we specify also '--skipped=always', then they should be skipped even when we list them individually on the command line
        self.assertEqual(get_tests_run(['--skip-failing-tests', '--skipped=always'] + list_of_tests_failing), [])

    def test_ews_corner_case_failing_directory(self):
        # When a whole directory is is marked as failing (or flaky), then the tests inside should not run if we specify the name of the directory and we pass '--skip-failing-tests'
        self.assertEqual(get_tests_run(['--skip-failing-tests', 'corner-cases/ews/directory-flaky']), [])
        # But if we specify on the command-line the name of individual tests inside that directory the tests should run (even with '--skip-failing-tests')
        list_of_tests_failing = ['corner-cases/ews/directory-skipped/failure.html', 'corner-cases/ews/directory-skipped/timeout.html']
        self.assertEqual(get_tests_run(['--no-retry-failures', '--skip-failing-tests'] + list_of_tests_failing), list_of_tests_failing)
        # Unless we specify also '--skipped=always' (in combination with '--skip-failing-tests'), then they should be skipped even when we list them individually on the command line
        self.assertEqual(get_tests_run(['--no-retry-failures',  '--skip-failing-tests', '--skipped=always'] + list_of_tests_failing), [])

    def test_ews_corner_case_skipped_test(self):
        # When we specify on the command line the name of a test skipped this test should run
        self.assertEqual(get_tests_run(['passes/skipped/skip.html']), ['passes/skipped/skip.html'])
        # Unless we specify also '--skipped=always', then it should be skipped even when we list it on the command line
        self.assertEqual(get_tests_run(['--skipped=always', 'passes/skipped/skip.html']), [])

    def test_ews_corner_case_skipped_directory(self):
        # When a whole directory is skipped, then the tests inside should not run if we specify the name of the directory
        self.assertEqual(get_tests_run(['corner-cases/ews/directory-skipped']), [])
        # But if we specify on the command-line the name of individual tests inside that directory the tests should run
        list_of_tests_failing = ['corner-cases/ews/directory-skipped/failure.html', 'corner-cases/ews/directory-skipped/timeout.html']
        self.assertEqual(get_tests_run(['--no-retry-failures'] + list_of_tests_failing), list_of_tests_failing)
        # Unless we specify also '--skipped=always', then they should be skipped even when we list them individually on the command line
        self.assertEqual(get_tests_run(['--no-retry-failures', '--skipped=always'] + list_of_tests_failing), [])

    def test_iterations(self):
        tests_to_run = ['passes/image.html', 'passes/text.html']
        tests_run = get_tests_run(['--iterations', '2'] + tests_to_run)
        self.assertEqual(tests_run, ['passes/image.html', 'passes/text.html', 'passes/image.html', 'passes/text.html'])

    def test_repeat_each_iterations_num_tests(self):
        # The total number of tests should be: number_of_tests *
        # repeat_each * iterations
        host = MockHost()
        _, err, _ = logging_run(
            ['--iterations', '2', '--repeat-each', '4', '--debug-rwt-logging', 'passes/text.html', 'failures/expected/text.html'],
            tests_included=True, host=host)
        self.assertContains(err, "All 16 tests ran as expected.\n")

    def test_run_chunk(self):
        # Test that we actually select the right chunk
        all_tests_run = get_tests_run(['passes', 'failures'])
        chunk_tests_run = get_tests_run(['--run-chunk', '1:4', 'passes', 'failures'])
        self.assertEqual(all_tests_run[4:8], chunk_tests_run)

        # Test that we wrap around if the number of tests is not evenly divisible by the chunk size
        tests_to_run = ['passes/error.html', 'passes/image.html', 'passes/platform_image.html', 'passes/text.html']
        chunk_tests_run = get_tests_run(['--run-chunk', '1:3', '--skipped', 'always'] + tests_to_run)
        self.assertEqual(['passes/text.html', 'passes/error.html', 'passes/image.html'], chunk_tests_run)

    def serial_test_run_force(self):
        # This raises an exception because we run
        # failures/expected/exception.html, which is normally SKIPped.

        self.assertRaises(ValueError, logging_run, ['--force'])

    def test_run_part(self):
        # Test that we actually select the right part
        tests_to_run = ['passes/error.html', 'passes/image.html', 'passes/platform_image.html', 'passes/text.html']
        tests_run = get_tests_run(['--run-part', '1:2', '--skipped', 'always'] + tests_to_run)
        self.assertEqual(['passes/error.html', 'passes/image.html'], tests_run)

        # Test that we wrap around if the number of tests is not evenly divisible by the chunk size
        # (here we end up with 3 parts, each with 2 tests, and we only have 4 tests total, so the
        # last part repeats the first two tests).
        chunk_tests_run = get_tests_run(['--run-part', '3:3', '--skipped', 'always'] + tests_to_run)
        self.assertEqual(['passes/error.html', 'passes/image.html'], chunk_tests_run)

    def serial_test_run_singly(self):
        batch_tests_run = get_test_batches(['--run-singly'])
        for batch in batch_tests_run:
            self.assertEqual(len(batch), 1, '%s had too many tests' % ', '.join(batch))

    def serial_test_skip_failing_tests(self):
        # This tests that we skip both known failing and known flaky tests. Because there are
        # no known flaky tests in the default test_expectations, we add additional expectations.
        host = MockHost()
        host.filesystem.write_text_file('/tmp/overrides.txt', 'Bug(x) passes/image.html [ ImageOnlyFailure Pass ]\n')

        batches = get_test_batches(['--skip-failing-tests', '--additional-expectations', '/tmp/overrides.txt'], host=host)
        has_passes_text = False
        for batch in batches:
            self.assertFalse('failures/expected/text.html' in batch)
            self.assertFalse('passes/image.html' in batch)
            has_passes_text = has_passes_text or ('passes/text.html' in batch)
        self.assertTrue(has_passes_text)

    def serial_test_run_singly_actually_runs_tests(self):
        details, _, _ = logging_run(['--run-singly'], tests_included=True)
        self.assertEqual(details.exit_code, test.UNEXPECTED_FAILURES - 2)  # FIXME: Figure out why - 2 is needed.

    def test_single_file(self):
        tests_run = get_tests_run(['passes/text.html'])
        self.assertEqual(tests_run, ['passes/text.html'])

    def test_single_file_with_prefix(self):
        tests_run = get_tests_run(['LayoutTests/passes/text.html'])
        self.assertEqual(['passes/text.html'], tests_run)

    def test_single_skipped_file(self):
        tests_run = get_tests_run(['failures/expected/keybaord.html'])
        self.assertEqual([], tests_run)

    def serial_test_stderr_is_saved(self):
        host = MockHost()
        self.assertTrue(passing_run(host=host))
        self.assertEqual(host.filesystem.read_text_file('/tmp/layout-test-results/passes/error-stderr.txt'),
                          'stuff going to stderr')

    def test_test_list(self):
        host = MockHost()
        filename = '/tmp/foo.txt'
        host.filesystem.write_text_file(filename, 'passes/text.html')
        tests_run = get_tests_run(['--test-list=%s' % filename], host=host)
        self.assertEqual(['passes/text.html'], tests_run)
        host.filesystem.remove(filename)
        details, err, user = logging_run(['--test-list=%s' % filename], tests_included=True, host=host)
        self.assertEqual(details.exit_code, -1)
        self.assertNotEmpty(err)

    def test_test_list_with_prefix(self):
        host = MockHost()
        filename = '/tmp/foo.txt'
        host.filesystem.write_text_file(filename, 'LayoutTests/passes/text.html')
        tests_run = get_tests_run(['--test-list=%s' % filename], host=host)
        self.assertEqual(['passes/text.html'], tests_run)

    def test_missing_and_unexpected_results(self):
        # Test that we update expectations in place. If the expectation
        # is missing, update the expected generic location.
        host = MockHost()
        details, err, _ = logging_run(['--no-show-results',
            'failures/expected/missing_image.html',
            'failures/unexpected/missing_text.html',
            'failures/unexpected/text-image-checksum.html'],
            tests_included=True, host=host)
        file_list = host.filesystem.written_files.keys()
        self.assertEqual(details.exit_code, 1)
        expected_dictionary = {
            'version': 4,
            'fixable': 3,
            'skipped': 0,
            'num_passes': 0,
            'num_flaky': 0,
            'num_missing': 1,
            'num_regressions': 1,
            'uses_expectations_file': True,
            'interrupted': False,
            'layout_tests_dir': '/test.checkout/LayoutTests',
            'has_pretty_patch': False,
            'pixel_tests_enabled': True,
            'other_crashes': {},
            'date': '10:10AM on December 13, 2019',
            'tests': {
                'failures': {
                    'expected': {
                        'missing_image.html': {
                            'expected': 'PASS MISSING',
                            'actual': 'MISSING',
                            'is_missing_image': True,
                        },
                    }, 'unexpected': {
                        'missing_text.html': {
                            'report': 'MISSING',
                            'expected': 'PASS',
                            'actual': 'MISSING',
                            'is_missing_text': True,
                        }, 'text-image-checksum.html': {
                            'report': 'REGRESSION',
                            'expected': 'PASS',
                            'actual': 'IMAGE+TEXT',
                            'image_diff_percent': 1,
                        },
                    },
                },
            },
        }
        actual_dictionary = json.loads(host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json')[len('ADD_RESULTS('):-2])
        self.assertEqual(
            sorted(list(expected_dictionary['tests']['failures']['expected'])),
            sorted(list(actual_dictionary['tests']['failures']['expected'])),
        )
        self.assertEqual(
            sorted(list(expected_dictionary['tests']['failures']['unexpected'])),
            sorted(list(actual_dictionary['tests']['failures']['unexpected'])),
        )
        self.assertEqual(expected_dictionary['num_regressions'], actual_dictionary['num_regressions'])
        self.assertEqual(expected_dictionary['num_flaky'], actual_dictionary['num_flaky'])
        self.assertEqual(expected_dictionary['num_missing'], actual_dictionary['num_missing'])

    def test_pixel_test_directories(self):
        host = MockHost()

        """Both tests have failing checksum. We include only the first in pixel tests so only that should fail."""
        args = ['--pixel-tests', '--pixel-test-directory', 'failures/unexpected/pixeldir',
                'failures/unexpected/pixeldir/image_in_pixeldir.html',
                'failures/unexpected/image_not_in_pixeldir.html']
        details, err, _ = logging_run(extra_args=args, host=host, tests_included=True)

        self.assertEqual(details.exit_code, 1)

        json_string = host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json')
        json = parse_full_results(json_string)
        test_data = json["tests"]["failures"]["unexpected"]["pixeldir"]["image_in_pixeldir.html"]
        self.assertEqual(test_data["expected"], "PASS")
        self.assertEqual(test_data["actual"], "IMAGE")
        self.assertEqual(test_data["report"], "REGRESSION")

    def test_missing_and_unexpected_results_with_custom_exit_code(self):
        # Test that we update expectations in place. If the expectation
        # is missing, update the expected generic location.
        class CustomExitCodePort(test.TestPort):
            def exit_code_from_summarized_results(self, unexpected_results):
                return unexpected_results['num_regressions'] + unexpected_results['num_missing']

        host = MockHost()
        options, parsed_args = run_webkit_tests.parse_args(['--pixel-tests', '--no-new-test-results'])
        test_port = CustomExitCodePort(host, options=options)
        details, err, _ = logging_run(['--no-show-results',
            'failures/expected/missing_image.html',
            'failures/unexpected/missing_text.html',
            'failures/unexpected/text-image-checksum.html'],
            tests_included=True, host=host, port_obj=test_port)
        self.assertEqual(details.exit_code, 2)

    def test_crash_with_stderr(self):
        host = MockHost()
        _, regular_output, _ = logging_run(['failures/unexpected/crash-with-stderr.html'], tests_included=True, host=host)
        actual_dictionary = json.loads(host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json')[len('ADD_RESULTS('):-2])
        expected_dictionary = {
            'version': 4,
            'fixable': 1,
            'skipped': 0,
            'num_passes': 0,
            'num_flaky': 0,
            'num_missing': 0,
            'num_regressions': 1,
            'uses_expectations_file': True,
            'interrupted': False,
            'layout_tests_dir': '/test.checkout/LayoutTests',
            'has_pretty_patch': False,
            'pixel_tests_enabled': True,
            'other_crashes': {},
            'date': '10:18AM on December 13, 2019',
            'tests': {
                'failures': {
                    'unexpected': {
                        'crash-with-stderr.html': {
                            'has_stderr': True,
                            'report': 'REGRESSION',
                            'expected': 'PASS',
                            'actual': 'CRASH',
                        },
                    },
                },
            },
        }
        self.assertEqual(
            sorted(list(expected_dictionary['tests']['failures']['unexpected'])),
            sorted(list(actual_dictionary['tests']['failures']['unexpected'])),
        )

    def test_wpt_tests(self):
        host = MockHost()
        _, regular_output, _ = logging_run(['imported/w3c/web-platform-tests/'], tests_included=True, host=host)
        actual_dictionary = json.loads(host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json')[len('ADD_RESULTS('):-2])
        expected_dictionary = {
            'tests': {
                'imported': {
                    'w3c': {
                        'web-platform-tests': {
                            'some': {
                                'new.html': {
                                    'is_missing_text': True,
                                    'report': 'REGRESSION',
                                    'expected': 'PASS',
                                    'actual': 'MISSING',
                                },
                                'test-crash-crash.html': {
                                    'has_stderr': True,
                                    'report': 'REGRESSION',
                                    'expected': 'PASS',
                                    'actual': 'CRASH',
                                },
                                'test-timeout-crash.html': {
                                    'report': 'REGRESSION',
                                    'expected': 'PASS',
                                    'actual': 'TIMEOUT',
                                },
                            },
                            'crashtests': {
                                'crash.html': {
                                    'has_stderr': True,
                                    'report': 'REGRESSION',
                                    'expected': 'PASS',
                                    'actual': 'CRASH',
                                },
                                'timeout.html': {
                                    'report': 'REGRESSION',
                                    'expected': 'PASS',
                                    'actual': 'TIMEOUT',
                                },
                            },
                        },
                    },
                },
            },
        }
        self.assertEqual(
            sorted(list(expected_dictionary['tests']['imported']['w3c']['web-platform-tests']['some'])),
            sorted(list(actual_dictionary['tests']['imported']['w3c']['web-platform-tests']['some'])))
        self.assertEqual(
            sorted(list(expected_dictionary['tests']['imported']['w3c']['web-platform-tests']['some']['new.html'])),
            sorted(list(actual_dictionary['tests']['imported']['w3c']['web-platform-tests']['some']['new.html'])))
        self.assertEqual(
            sorted(list(expected_dictionary['tests']['imported']['w3c']['web-platform-tests']['some']['test-crash-crash.html'])),
            sorted(list(actual_dictionary['tests']['imported']['w3c']['web-platform-tests']['some']['test-crash-crash.html'])))
        self.assertEqual(
            sorted(list(expected_dictionary['tests']['imported']['w3c']['web-platform-tests']['some']['test-timeout-crash.html'])),
            sorted(list(actual_dictionary['tests']['imported']['w3c']['web-platform-tests']['some']['test-timeout-crash.html'])))
        self.assertTrue(host.filesystem.exists('/tmp/layout-test-results/imported/w3c/web-platform-tests/some/new-actual.txt'))
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/imported/w3c/web-platform-tests/some/test-pass-crash-actual.txt'))
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/imported/w3c/web-platform-tests/some/test-crash-crash-actual.txt'))
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/imported/w3c/web-platform-tests/some/test-timeout-crash-actual.txt'))

    def test_wpt_tests_rebaseline(self):
        host = MockHost()
        mock_crash_report = None

        if self._platform.is_mac() or self._platform.is_win():
            # FIXME: Need to rewrite these tests to not be mac-specific, or move them elsewhere.
            # Currently CrashLog uploading only works on Darwin and Windows.
            mock_crash_report = make_mock_crash_report_darwin('WebKitTestRunner', 12345)
            host.filesystem.write_text_file('/tmp/layout-test-results/WebKitTestRunner_2011-06-13-150719_quadzen.crash', mock_crash_report)

        details, regular_output, _ = logging_run(['imported/w3c/web-platform-tests/', '--reset-results'], tests_included=True, host=host, new_results=True)
        actual_dictionary = json.loads(host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json')[len('ADD_RESULTS('):-2])
        file_list = host.filesystem.written_files.keys()
        self.assertEqual(details.exit_code, 4)
        self.assertTrue(host.filesystem.read_text_file('/test.checkout/LayoutTests/imported/w3c/web-platform-tests/some/new-expected.txt'), 'ok')
        self.assertFalse(host.filesystem.exists('/test.checkout/LayoutTests/imported/w3c/web-platform-tests/some/test-pass-expected.txt'))
        self.assertFalse(host.filesystem.exists('/test.checkout/LayoutTests/imported/w3c/web-platform-tests/some/test-crash-expected.txt'))
        self.assertFalse(host.filesystem.exists('/test.checkout/LayoutTests/imported/w3c/web-platform-tests/some/test-timeout-expected.txt'))

        if mock_crash_report:
            self.assertEqual(host.filesystem.read_text_file('/tmp/layout-test-results/imported/w3c/web-platform-tests/some/test-crash-crash-crash-log.txt'), mock_crash_report)

    def test_no_image_failure_with_image_diff(self):
        host = MockHost()
        _, regular_output, _ = logging_run(['failures/unexpected/checksum-with-matching-image.html'], tests_included=True, host=host)
        self.assertTrue(host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json').find('"num_regressions":0') != -1)

    def test_crash_log(self):
        # FIXME: Need to rewrite these tests to not be mac-specific, or move them elsewhere.
        # Currently CrashLog uploading only works on Darwin and Windows.
        if not self._platform.is_mac() or self._platform.is_win():
            return
        mock_crash_report = make_mock_crash_report_darwin('DumpRenderTree', 12345)
        host = MockHost()
        host.filesystem.write_text_file('/tmp/layout-test-results/DumpRenderTree_2011-06-13-150719_quadzen.crash', mock_crash_report)
        _, regular_output, _ = logging_run(['failures/unexpected/crash-with-stderr.html', '--dump-render-tree'], tests_included=True, host=host)
        expected_crash_log = mock_crash_report
        self.assertEqual(host.filesystem.read_text_file('/tmp/layout-test-results/failures/unexpected/crash-with-stderr-crash-log.txt'), expected_crash_log)

    def test_web_process_crash_log(self):
        # FIXME: Need to rewrite these tests to not be mac-specific, or move them elsewhere.
        # Currently CrashLog uploading only works on Darwin and Windows.
        if not self._platform.is_mac() or self._platform.is_win():
            return
        mock_crash_report = make_mock_crash_report_darwin('WebProcess', 12345)
        host = MockHost()
        host.filesystem.write_text_file('/tmp/layout-test-results/WebProcess_2011-06-13-150719_quadzen.crash', mock_crash_report)
        logging_run(['failures/unexpected/web-process-crash-with-stderr.html'], tests_included=True, host=host)
        self.assertEqual(host.filesystem.read_text_file('/tmp/layout-test-results/failures/unexpected/web-process-crash-with-stderr-crash-log.txt'), mock_crash_report)

    def test_exit_after_n_failures_upload(self):
        host = MockHost()
        details, regular_output, user = logging_run(
           ['failures/unexpected/text-image-checksum.html', 'passes/text.html', '--exit-after-n-failures', '1'],
           tests_included=True, host=host)

        # By returning False, we know that the incremental results were generated and then deleted.
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/incremental_results.json'))

        # This checks that we report only the number of tests that actually failed.
        self.assertEqual(details.exit_code, 1)

        # This checks that passes/text.html is considered SKIPped.
        self.assertTrue('"skipped":1' in host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))

        # This checks that we told the user we bailed out.
        self.assertTrue('Exiting early after 1 failures. 1 tests run.\n' in regular_output.getvalue())

        # This checks that neither test ran as expected.
        # FIXME: This log message is confusing; tests that were skipped should be called out separately.
        self.assertTrue('0 tests ran as expected, 2 didn\'t:\n' in regular_output.getvalue())

    def test_exit_after_n_failures(self):
        # Unexpected failures should result in tests stopping.
        tests_run = get_tests_run(['failures/unexpected/text-image-checksum.html', 'passes/text.html', '--exit-after-n-failures', '1'])
        self.assertEqual(['failures/unexpected/text-image-checksum.html'], tests_run)

        # But we'll keep going for expected ones.
        tests_run = get_tests_run(['failures/expected/text.html', 'passes/text.html', '--exit-after-n-failures', '1'])
        self.assertEqual(['failures/expected/text.html', 'passes/text.html'], tests_run)

    def test_exit_after_n_crashes(self):
        # Unexpected crashes should result in tests stopping.
        tests_run = get_tests_run(['failures/unexpected/crash.html', 'passes/text.html', '--exit-after-n-crashes-or-timeouts', '1'])
        self.assertEqual(['failures/unexpected/crash.html'], tests_run)

        # Same with timeouts.
        tests_run = get_tests_run(['failures/unexpected/timeout.html', 'passes/text.html', '--exit-after-n-crashes-or-timeouts', '1'])
        self.assertEqual(['failures/unexpected/timeout.html'], tests_run)

        # But we'll keep going for expected ones.
        tests_run = get_tests_run(['failures/expected/crash.html', 'passes/text.html', '--exit-after-n-crashes-or-timeouts', '1'])
        self.assertEqual(['failures/expected/crash.html', 'passes/text.html'], tests_run)

    def serial_test_results_directory_absolute(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.

        host = MockHost()
        with host.filesystem.mkdtemp() as tmpdir:
            _, _, user = logging_run(['--results-directory=' + str(tmpdir)], tests_included=True, host=host)
            self.assertEqual(user.opened_urls, [path.abspath_to_uri(host.platform, host.filesystem.join(tmpdir, 'results.html'))])

    def serial_test_results_directory_default(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.

        # This is the default location.
        _, _, user = logging_run(tests_included=True)
        self.assertEqual(user.opened_urls, [path.abspath_to_uri(MockHost().platform, '/tmp/layout-test-results/results.html')])

    def serial_test_results_directory_relative(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.
        host = MockHost()
        host.filesystem.maybe_make_directory('/tmp/cwd')
        host.filesystem.chdir('/tmp/cwd')
        _, _, user = logging_run(['--results-directory=foo'], tests_included=True, host=host)
        self.assertEqual(user.opened_urls, [path.abspath_to_uri(host.platform, '/tmp/cwd/foo/results.html')])

    def test_retrying_and_flaky_tests(self):
        host = MockHost()
        details, err, _ = logging_run(['--debug-rwt-logging', 'failures/flaky'], tests_included=True, host=host)
        self.assertEqual(details.exit_code, 0)
        self.assertTrue('Retrying' in err.getvalue())
        self.assertTrue(host.filesystem.exists('/tmp/layout-test-results/failures/flaky/text-actual.txt'))
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/retries/failures/flaky/text-actual.txt'))

        # Now we test that --clobber-old-results does remove the old entries and the old retries,
        # and that we don't retry again.
        host = MockHost()
        details, err, _ = logging_run(['--no-retry-failures', '--clobber-old-results', 'failures/flaky'], tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        self.assertTrue('Deleting results directory' in err.getvalue())
        self.assertTrue('flaky/text.html' in err.getvalue())
        self.assertTrue(host.filesystem.exists('/tmp/layout-test-results/failures/flaky/text-actual.txt'))
        self.assertFalse(host.filesystem.exists('retries'))

    def test_retrying_force_pixel_tests(self):
        host = MockHost()
        details, err, _ = logging_run(['--no-pixel-tests', 'failures/unexpected/text-image-checksum.html'], tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        self.assertTrue('Retrying' in err.getvalue())
        self.assertTrue(host.filesystem.exists('/tmp/layout-test-results/failures/unexpected/text-image-checksum-actual.txt'))
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/failures/unexpected/text-image-checksum-actual.png'))
        self.assertTrue(host.filesystem.exists('/tmp/layout-test-results/retries/failures/unexpected/text-image-checksum-actual.txt'))
        self.assertTrue(host.filesystem.exists('/tmp/layout-test-results/retries/failures/unexpected/text-image-checksum-actual.png'))
        json_string = host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json')
        json = parse_full_results(json_string)
        self.assertEqual(json["tests"]["failures"]["unexpected"]["text-image-checksum.html"],
                         {"expected": "PASS", "actual": "TEXT IMAGE+TEXT", "image_diff_percent": 1, 'image_difference': {'max_difference': 10, 'total_pixels': 20}, "report": "REGRESSION"})
        self.assertFalse(json["pixel_tests_enabled"])
        self.assertEqual(details.enabled_pixel_tests_in_retry, True)

    def test_failed_text_with_missing_pixel_results_on_retry(self):
        # Test what happens when pixel results are missing on retry.
        host = MockHost()
        details, err, _ = logging_run(['--no-show-results',
            '--no-new-test-results', '--no-pixel-tests',
            'failures/unexpected/text-image-missing.html'],
            tests_included=True, host=host)
        file_list = host.filesystem.written_files.keys()
        self.assertEqual(details.exit_code, 1)
        json_string = host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json')

        json = parse_full_results(json_string)
        test_data = json["tests"]["failures"]["unexpected"]["text-image-missing.html"]
        self.assertEqual(test_data["expected"], "PASS")
        self.assertEqual(test_data["actual"], "TEXT MISSING")
        self.assertEqual(test_data["report"], "REGRESSION")
        self.assertEqual(test_data["is_missing_image"], True)

        self.assertTrue(json_string.find('"num_regressions":1') != -1)
        self.assertTrue(json_string.find('"num_flaky":0') != -1)
        self.assertTrue(json_string.find('"num_missing":1') != -1)

    def test_retrying_uses_retries_directory(self):
        host = MockHost()
        details, err, _ = logging_run(['--debug-rwt-logging', 'failures/unexpected/text-image-checksum.html'], tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        self.assertTrue(host.filesystem.exists('/tmp/layout-test-results/failures/unexpected/text-image-checksum-actual.txt'))
        self.assertTrue(host.filesystem.exists('/tmp/layout-test-results/retries/failures/unexpected/text-image-checksum-actual.txt'))

    def serial_test_run_order__inline(self):
        # These next tests test that we run the tests in ascending alphabetical
        # order per directory. HTTP tests are sharded separately from other tests,
        # so we have to test both.
        tests_run = get_tests_run(['-i', 'passes/passes', 'passes'])
        self.assertEqual(tests_run, sorted(tests_run))

        tests_run = get_tests_run(['http/tests/passes'])
        self.assertEqual(tests_run, sorted(tests_run))

    def test_tolerance(self):
        class ImageDiffTestPort(test.TestPort):
            def diff_image(self, expected_contents, actual_contents, tolerance=None):
                self.tolerance_used_for_diff_image = self._options.tolerance
                return ImageDiffResult(passed=False, diff_image=b'', difference=1, fuzzy_data=None, tolerance=self._options.tolerance or 0)

        def get_port_for_run(args):
            options, parsed_args = run_webkit_tests.parse_args(args)
            host = MockHost()
            test_port = ImageDiffTestPort(host, options=options)
            res = passing_run(args, port_obj=test_port, tests_included=True)
            self.assertTrue(res)
            return test_port

        base_args = ['--pixel-tests', '--no-new-test-results', 'failures/expected/*']

        # If we pass in an explicit tolerance argument, then that will be used.
        test_port = get_port_for_run(base_args + ['--tolerance', '.1'])
        self.assertEqual(0.1, test_port.tolerance_used_for_diff_image)
        test_port = get_port_for_run(base_args + ['--tolerance', '0'])
        self.assertEqual(0, test_port.tolerance_used_for_diff_image)

        # Otherwise the port's default tolerance behavior (including ignoring it)
        # should be used.
        test_port = get_port_for_run(base_args)
        self.assertEqual(None, test_port.tolerance_used_for_diff_image)

    def test_reftest_run(self):
        tests_run = get_tests_run(['passes/reftest.html'])
        self.assertEqual(['passes/reftest.html'], tests_run)

    def test_reftest_run_reftests_if_pixel_tests_are_disabled(self):
        tests_run = get_tests_run(['--no-pixel-tests', 'passes/reftest.html'])
        self.assertEqual(['passes/reftest.html'], tests_run)

    def test_reftest_skip_reftests_if_no_ref_tests(self):
        tests_run = get_tests_run(['--no-ref-tests', 'passes/reftest.html'])
        self.assertEqual([], tests_run)
        tests_run = get_tests_run(['--no-ref-tests', '--no-pixel-tests', 'passes/reftest.html'])
        self.assertEqual([], tests_run)

    def test_reftest_expected_html_should_be_ignored(self):
        tests_run = get_tests_run(['passes/reftest-expected.html'])
        self.assertEqual([], tests_run)

    def test_reftest_driver_should_run_expected_html(self):
        tests_run = get_test_results(['passes/reftest.html'])
        self.assertEqual(tests_run[0].references, ['passes/reftest-expected.html'])

    def test_reftest_driver_should_run_expected_mismatch_html(self):
        tests_run = get_test_results(['passes/mismatch.html'])
        self.assertEqual(tests_run[0].references, ['passes/mismatch-expected-mismatch.html'])

    def serial_test_additional_platform_directory(self):
        self.assertTrue(passing_run(['--additional-platform-directory', '/tmp/foo']))
        self.assertTrue(passing_run(['--additional-platform-directory', '/tmp/../foo']))
        self.assertTrue(passing_run(['--additional-platform-directory', '/tmp/foo', '--additional-platform-directory', '/tmp/bar']))
        self.assertTrue(passing_run(['--additional-platform-directory', 'foo']))

    def test_additional_expectations(self):
        host = MockHost()
        host.filesystem.write_text_file('/tmp/overrides.txt', 'Bug(x) failures/unexpected/mismatch.html [ ImageOnlyFailure ]\n')
        self.assertTrue(passing_run(['--additional-expectations', '/tmp/overrides.txt', 'failures/unexpected/mismatch.html'],
                                    tests_included=True, host=host))

    def test_tests_options(self):
        host = MockHost()
        host.filesystem.write_text_file(
            '/test.checkout/LayoutTests/tests-options.json',
            '{"failures/unexpected/timeout.html":["slow"]}'
        )

        details, _, _ = logging_run(['failures/expected/timeout.html',
                                     'failures/unexpected/timeout.html'],
                                    host=host)
        self.assertEqual(details.initial_results.slow_tests,
                          {'failures/unexpected/timeout.html'})
        self.assertEqual(details.retry_results.slow_tests,
                          {'failures/unexpected/timeout.html'})

    def serial_test_no_http_and_force(self):
        # See test_run_force, using --force raises an exception.
        # FIXME: We would like to check the warnings generated.
        self.assertRaises(ValueError, logging_run, ['--force', '--no-http'])

    @staticmethod
    def has_test_of_type(tests, type):
        return [test for test in tests if type in test]

    def serial_test_no_http_tests(self):
        batch_tests_dryrun = get_tests_run(['LayoutTests/http', 'websocket/'])
        self.assertTrue(RunTest.has_test_of_type(batch_tests_dryrun, 'http'))
        self.assertTrue(RunTest.has_test_of_type(batch_tests_dryrun, 'websocket'))

        batch_tests_run_no_http = get_tests_run(['--no-http', 'LayoutTests/http', 'websocket/'])
        self.assertFalse(RunTest.has_test_of_type(batch_tests_run_no_http, 'http'))
        self.assertFalse(RunTest.has_test_of_type(batch_tests_run_no_http, 'websocket'))

        batch_tests_run_http = get_tests_run(['--http', 'LayoutTests/http', 'websocket/'])
        self.assertTrue(RunTest.has_test_of_type(batch_tests_run_http, 'http'))
        self.assertTrue(RunTest.has_test_of_type(batch_tests_run_http, 'websocket'))

    def serial_test_platform_tests_are_found(self):
        tests_run = get_tests_run(['--platform', 'test-mac-leopard', 'http'])
        self.assertTrue('platform/test-mac-leopard/http/test.html' in tests_run)
        self.assertFalse('platform/test-win-win7/http/test.html' in tests_run)

    def test_output_diffs(self):
        # Test to ensure that we don't generate -pretty.html if PrettyPatch isn't available.
        host = MockHost()
        _, err, _ = logging_run(['--pixel-tests', 'failures/unexpected/text-image-checksum.html'], tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertTrue(any(path.endswith('-diff.txt') for path in written_files.keys()))
        self.assertFalse(any(path.endswith('-pretty-diff.html') for path in written_files.keys()))

        full_results_text = host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json')
        full_results = json.loads(full_results_text.replace("ADD_RESULTS(", "").replace(");", ""))
        self.assertEqual(full_results['has_pretty_patch'], False)

    def test_unsupported_platform(self):
        stdout = StringIO()
        stderr = StringIO()
        res = run_webkit_tests.main(['--platform', 'foo'], stdout, stderr)

        self.assertEqual(res, run_webkit_tests.EXCEPTIONAL_EXIT_STATUS)
        self.assertEqual(stdout.getvalue(), '')
        self.assertTrue('unsupported platform' in stderr.getvalue())

    def test_verbose_in_child_processes(self):
        # When we actually run multiple processes, we may have to reconfigure logging in the
        # child process (e.g., on win32) and we need to make sure that works and we still
        # see the verbose log output. However, we can't use logging_run() because using
        # outputcapture to capture stdout and stderr latter results in a nonpicklable host.

        # Test is flaky on Windows: https://bugs.webkit.org/show_bug.cgi?id=98559
        if not self.should_test_processes:
            return

        options, parsed_args = parse_args(['--verbose', '--fully-parallel', '--child-processes', '2', 'passes/text.html', 'passes/image.html'], tests_included=True, print_nothing=False)
        host = MockHost()
        port_obj = host.port_factory.get(port_name=options.platform, options=options)
        logging_stream = StringIO()
        run_webkit_tests.run(port_obj, options, parsed_args, logging_stream=logging_stream)
        self.assertTrue('text.html passed' in logging_stream.getvalue())
        self.assertTrue('image.html passed' in logging_stream.getvalue())

    def test_device_type_test_division(self):
        host = MockHost()
        port = host.port_factory.get('ios-simulator')

        host.filesystem.write_text_file('/mock-checkout/LayoutTests/test1.html', '')
        host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/ios/test2.html', '')
        host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/ipad/test3.html', '')
        host.filesystem.write_text_file('/MOCK output of child process/ImageDiff', '')

        with OutputCapture() as captured:
            logging = StringIO()
            run_webkit_tests.run(port, run_webkit_tests.parse_args(['--debug-rwt-logging', '-n', '--no-build', '--root', '/build'])[0], [], logging_stream=logging)

        for line in logging.getvalue():
            if str(DeviceType.from_string('iPhone SE')) in line:
                self.assertTrue('Skipping 2 tests' in line)
            elif str(DeviceType.from_string('iPad (10th generation)')) in line:
                self.assertTrue('Skipping 1 test' in line)
            elif str(DeviceType.from_string('iPhone 7')) in line:
                self.assertTrue('Skipping 0 tests' in line)

    def test_device_type_specific_listing(self):
        host = MockHost()
        port = host.port_factory.get('ios-simulator')

        host.filesystem.write_text_file('/mock-checkout/LayoutTests/test1.html', '')
        host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/ios/test2.html', '')
        host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/ipad/test3.html', '')

        with OutputCapture() as captured:
            logging = StringIO()
            run_webkit_tests._print_expectations(port, run_webkit_tests.parse_args(['--print-expectations'])[0], [], logging_stream=logging)

        current_type = None
        by_type = {}
        for line in captured.stdout.getvalue().splitlines():
            if not line or 'skip' in line:
                continue
            if 'Tests to run' in line:
                current_type = DeviceType.from_string(line.split('for ')[-1].split(' running')[0]) if 'for ' in line else None
                by_type[current_type] = []
                continue
            by_type[current_type].append(line)

        self.assertEqual(3, len(by_type.keys()))
        self.assertEqual(2, len(by_type[DeviceType.from_string('iPhone 12')]))
        self.assertEqual(1, len(by_type[DeviceType.from_string('iPad (10th generation)')]))
        self.assertEqual(0, len(by_type[DeviceType.from_string('iPhone 7')]))

    def test_ipad_test_division(self):
        host = MockHost()
        port = host.port_factory.get('ipad-simulator')

        host.filesystem.write_text_file('/mock-checkout/LayoutTests/test1.html', '')
        host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/ios/test2.html', '')
        host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/ipad/test3.html', '')
        host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/iphone/test4.html', '')
        host.filesystem.write_text_file('/MOCK output of child process/ImageDiff', '')

        with OutputCapture():
            logging = StringIO()
            run_webkit_tests.run(port, run_webkit_tests.parse_args(['--debug-rwt-logging', '-n', '--no-build', '--root', '/build'])[0], [], logging_stream=logging)

        for line in logging.getvalue():
            if str(DeviceType.from_string('iPad (10th generation)')) in line:
                self.assertTrue('Skipping 3 test' in line)

    def test_ipad_listing(self):
        host = MockHost()
        port = host.port_factory.get('ipad-simulator')

        host.filesystem.write_text_file('/mock-checkout/LayoutTests/test1.html', '')
        host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/ios/test2.html', '')
        host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/ipad/test3.html', '')
        host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/iphone/test4.html', '')

        with OutputCapture() as captured:
            logging = StringIO()
            run_webkit_tests._print_expectations(port, run_webkit_tests.parse_args(['--print-expectations'])[0], [], logging_stream=logging)

        current_type = None
        by_type = {}
        for line in captured.stdout.getvalue().splitlines():
            if not line or 'skip' in line:
                continue
            if 'Tests to run' in line:
                current_type = DeviceType.from_string(line.split('for ')[-1].split(' running')[0]) if 'for ' in line else None
                by_type[current_type] = []
                continue
            by_type[current_type].append(line)

        self.assertEqual(1, len(by_type.keys()))
        self.assertEqual(3, len(by_type[DeviceType.from_string('iPad (10th generation)')]))


class RebaselineTest(unittest.TestCase, StreamTestingMixin):
    def assertBaselines(self, file_list, file, extensions, err):
        """assert that the file_list contains the baselines."""
        for ext in extensions:
            baseline = file + "-expected" + ext
            baseline_msg = 'Writing new expected result "%s"\n' % baseline
            self.assertTrue(any(f.find(baseline) != -1 for f in file_list))
            self.assertContains(err, baseline_msg)

    # FIXME: Add tests to ensure that we're *not* writing baselines when we're not
    # supposed to be.

    def test_reset_results(self):
        # Test that we update expectations in place. If the expectation
        # is missing, update the expected generic location.
        host = MockHost()
        host.filesystem.clear_written_files()
        details, err, _ = logging_run(
            ['--pixel-tests', '--reset-results', 'passes/image.html', 'failures/expected/missing_image.html'],
            tests_included=True, host=host, new_results=True)
        file_list = host.filesystem.written_files.keys()
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(file_list), 9)
        self.assertBaselines(file_list, "passes/image", [".txt", ".png"], err)
        self.assertBaselines(file_list, "failures/expected/missing_image", [".txt", ".png"], err)

    def test_missing_results(self):
        # Test that we update expectations in place. If the expectation
        # is missing, update the expected generic location.
        host = MockHost()
        host.filesystem.clear_written_files()
        details, err, _ = logging_run(['--no-show-results',
            'failures/unexpected/missing_text.html',
            'failures/unexpected/missing_image.html',
            'failures/unexpected/missing_audio.html',
            'failures/unexpected/missing_render_tree_dump.html'],
            tests_included=True, host=host, new_results=True)
        file_list = host.filesystem.written_files.keys()
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(file_list), 11)
        self.assertBaselines(file_list, "failures/unexpected/missing_text", [".txt"], err)
        self.assertBaselines(file_list, "platform/test/failures/unexpected/missing_image", [".png"], err)
        self.assertBaselines(file_list, "platform/test/failures/unexpected/missing_render_tree_dump", [".txt"], err)

    def test_new_baseline(self):
        # Test that we update the platform expectations in the version-specific directories
        # for both existing and new baselines.
        host = MockHost()
        host.filesystem.clear_written_files()
        details, err, _ = logging_run(
            ['--pixel-tests', '--new-baseline', 'passes/image.html', 'failures/expected/missing_image.html'],
            tests_included=True, host=host, new_results=True)
        file_list = host.filesystem.written_files.keys()
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(file_list), 9)
        self.assertBaselines(file_list,
            "platform/test-mac-leopard/passes/image", [".txt", ".png"], err)
        self.assertBaselines(file_list,
            "platform/test-mac-leopard/failures/expected/missing_image", [".txt", ".png"], err)


class PortTest(unittest.TestCase):
    def assert_mock_port_works(self, port_name, args=[]):
        self.assertTrue(passing_run(args + ['--platform', 'mock-' + port_name, 'fast/harness/results.html'], tests_included=True, host=Host()))

    def disabled_test_mac_lion(self):
        self.assert_mock_port_works('mac-lion')


class MainTest(unittest.TestCase):
    def test_exception_handling(self):
        orig_run_fn = run_webkit_tests.run

        # unused args pylint: disable=W0613
        def interrupting_run(port, options, args, stderr):
            raise KeyboardInterrupt

        def successful_run(port, options, args, stderr):

            class FakeRunDetails(object):
                exit_code = -1

            return FakeRunDetails()

        def exception_raising_run(port, options, args, stderr):
            assert False

        stdout = StringIO()
        stderr = StringIO()
        try:
            run_webkit_tests.run = interrupting_run
            res = run_webkit_tests.main([], stdout, stderr)
            self.assertEqual(res, INTERRUPTED_EXIT_STATUS)

            run_webkit_tests.run = successful_run
            res = run_webkit_tests.main(['--platform', 'test'], stdout, stderr)
            self.assertEqual(res, -1)

            run_webkit_tests.run = exception_raising_run
            res = run_webkit_tests.main([], stdout, stderr)
            self.assertEqual(res, run_webkit_tests.EXCEPTIONAL_EXIT_STATUS)
        finally:
            run_webkit_tests.run = orig_run_fn
