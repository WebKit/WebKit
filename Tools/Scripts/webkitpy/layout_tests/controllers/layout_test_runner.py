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

import atexit
import logging
import math
import threading
import time

from webkitpy.common import message_pool
from webkitpy.layout_tests.controllers import single_test_runner
from webkitpy.layout_tests.models.test_run_results import TestRunResults
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.models import test_results
from webkitpy.tool import grammar


_log = logging.getLogger(__name__)


TestExpectations = test_expectations.TestExpectations

# Export this so callers don't need to know about message pools.
WorkerException = message_pool.WorkerException


class TestRunInterruptedException(Exception):
    """Raised when a test run should be stopped immediately."""
    def __init__(self, reason):
        Exception.__init__(self)
        self.reason = reason
        self.msg = reason

    def __reduce__(self):
        return self.__class__, (self.reason,)


class LayoutTestRunner(object):
    def __init__(self, options, port, printer, results_directory, test_is_slow_fn, needs_http=False, needs_websockets=False, needs_web_platform_test_server=False):
        self._options = options
        self._port = port
        self._printer = printer
        self._results_directory = results_directory
        self._test_is_slow = test_is_slow_fn
        self._needs_http = needs_http
        self._needs_websockets = needs_websockets
        self._needs_web_platform_test_server = needs_web_platform_test_server

        self._sharder = Sharder(self._port.split_test)
        self._filesystem = self._port.host.filesystem

        self._expectations = None
        self._test_inputs = []
        self._retrying = False
        self._current_run_results = None
        self._did_start_http_server = False
        self._did_start_websocket_server = False
        self._did_start_wpt_server = False

        if ((self._needs_http and self._options.http) or self._needs_web_platform_test_server) and self._port.get_option("start_http_servers_if_needed"):
            self.start_servers()
            atexit.register(lambda: self.stop_servers())

    def get_worker_count(self, test_inputs, child_process_count):
        all_shards = self._sharder.shard_tests(test_inputs, child_process_count, self._options.fully_parallel)
        return min(child_process_count, len(all_shards))

    def run_tests(self, expectations, test_inputs, tests_to_skip, num_workers, retrying):
        self._expectations = expectations
        self._test_inputs = test_inputs

        self._retrying = retrying

        # FIXME: rename all variables to test_run_results or some such ...
        run_results = TestRunResults(self._expectations, len(test_inputs) + len(tests_to_skip))
        self._current_run_results = run_results
        self._printer.num_tests = len(test_inputs)
        self._printer.num_started = 0

        if not retrying:
            self._printer.print_expected(run_results, self._expectations.model().get_tests_with_result_type)

        for test_name in set(tests_to_skip):
            result = test_results.TestResult(test_name)
            result.type = test_expectations.SKIP
            run_results.add(result, expected=True, test_is_slow=self._test_is_slow(test_name))

        self._printer.write_update('Sharding tests ...')
        all_shards = self._sharder.shard_tests(test_inputs, int(self._options.child_processes), self._options.fully_parallel)

        self._printer.print_workers_and_shards(num_workers, len(all_shards))

        if self._options.dry_run:
            return run_results

        self._printer.write_update('Starting %s ...' % grammar.pluralize(num_workers, "worker"))

        try:
            with message_pool.get(self, self._worker_factory, num_workers, self._port.worker_startup_delay_secs(), self._port.host) as pool:
                pool.run(('test_list', shard.name, shard.test_inputs) for shard in all_shards)
        except TestRunInterruptedException as e:
            _log.warning(e.reason)
            run_results.interrupted = True
        except KeyboardInterrupt:
            self._printer.flush()
            self._printer.writeln('Interrupted, exiting ...')
            run_results.keyboard_interrupted = True
        except Exception as e:
            _log.debug('%s("%s") raised, exiting' % (e.__class__.__name__, str(e)))
            raise

        return run_results

    def _worker_factory(self, worker_connection):
        results_directory = self._results_directory
        if self._retrying:
            self._filesystem.maybe_make_directory(self._filesystem.join(self._results_directory, 'retries'))
            results_directory = self._filesystem.join(self._results_directory, 'retries')
        return Worker(worker_connection, results_directory, self._options)

    def _handle_did_spawn_worker(self, worker_number):
        self._port.did_spawn_worker(worker_number)

    def _mark_interrupted_tests_as_skipped(self, run_results):
        for test_input in self._test_inputs:
            if test_input.test_name not in run_results.results_by_name:
                result = test_results.TestResult(test_input.test_name, [test_failures.FailureEarlyExit()])
                # FIXME: We probably need to loop here if there are multiple iterations.
                # FIXME: Also, these results are really neither expected nor unexpected. We probably
                # need a third type of result.
                run_results.add(result, expected=False, test_is_slow=self._test_is_slow(test_input.test_name))

    def _interrupt_if_at_failure_limits(self, run_results):
        # Note: The messages in this method are constructed to match old-run-webkit-tests
        # so that existing buildbot grep rules work.
        def interrupt_if_at_failure_limit(limit, failure_count, run_results, message):
            if limit and failure_count >= limit:
                message += " %d tests run." % (run_results.expected + run_results.unexpected)
                self._mark_interrupted_tests_as_skipped(run_results)
                raise TestRunInterruptedException(message)

        interrupt_if_at_failure_limit(
            self._options.exit_after_n_failures,
            run_results.unexpected_failures,
            run_results,
            "Exiting early after %d failures." % run_results.unexpected_failures)
        interrupt_if_at_failure_limit(
            self._options.exit_after_n_crashes_or_timeouts,
            run_results.unexpected_crashes + run_results.unexpected_timeouts,
            run_results,
            # This differs from ORWT because it does not include WebProcess crashes.
            "Exiting early after %d crashes and %d timeouts." % (run_results.unexpected_crashes, run_results.unexpected_timeouts))

    def _update_summary_with_result(self, run_results, result):
        if result.type == test_expectations.SKIP:
            exp_str = got_str = 'SKIP'
            expected = True
        else:
            expectations = self._expectations.filtered_expectations_for_test(result.test_name, self._options.pixel_tests or bool(result.reftest_type), self._options.world_leaks)
            expected = self._expectations.matches_an_expected_result(result.test_name, result.type, expectations)
            exp_str = self._expectations.model().expectations_to_string(expectations)
            got_str = self._expectations.model().expectation_to_string(result.type)

        run_results.add(result, expected, self._test_is_slow(result.test_name))

        self._printer.print_finished_test(result, expected, exp_str, got_str)

        self._interrupt_if_at_failure_limits(run_results)

    def _annotate_results_with_additional_failures(self, run_results, results):
        for new_result in results:
            existing_result = run_results.results_by_name.get(new_result.test_name)
            # When running a chunk (--run-chunk), results_by_name contains all the tests, but (confusingly) all_tests only contains those in the chunk that was run,
            # and we don't want to modify the results of a test that didn't run. existing_result.test_number is only non-None for tests that ran.
            if existing_result and existing_result.test_number is not None:
                expectations = self._expectations.filtered_expectations_for_test(new_result.test_name, self._options.pixel_tests or bool(new_result.reftest_type), self._options.world_leaks)
                was_expected = self._expectations.matches_an_expected_result(new_result.test_name, existing_result.type, expectations)
                now_expected = self._expectations.matches_an_expected_result(new_result.test_name, new_result.type, expectations)
                if was_expected != now_expected:
                    # When annotation is not just about leaks, this logging should be changed.
                    _log.warning('  %s -> changed by leak detection from a %s (%s) to a %s (%s)' % (new_result.test_name,
                        TestExpectations.EXPECTATION_DESCRIPTION[existing_result.type], 'expected' if was_expected else 'unexpected',
                        TestExpectations.EXPECTATION_DESCRIPTION[new_result.type], 'expected' if now_expected else 'unexpected'))
                run_results.change_result_to_failure(existing_result, new_result, was_expected, now_expected)

    def start_servers(self):
        if self._needs_http and not self._did_start_http_server and not self._port.is_http_server_running():
            self._printer.write_update('Starting HTTP server ...')
            self._port.start_http_server()
            self._did_start_http_server = True
        if self._needs_websockets and not self._did_start_websocket_server and not self._port.is_websocket_server_running():
            self._printer.write_update('Starting WebSocket server ...')
            self._port.start_websocket_server()
            self._did_start_websocket_server = True
        if self._needs_web_platform_test_server and not self._did_start_wpt_server and not self._port.is_wpt_server_running():
            self._printer.write_update('Starting Web Platform Test server ...')
            self._port.start_web_platform_test_server()
            self._did_start_wpt_server = True

    def stop_servers(self):
        if self._did_start_http_server:
            self._printer.write_update('Stopping HTTP server ...')
            self._port.stop_http_server()
            self._did_start_http_server = False
        if self._did_start_websocket_server:
            self._printer.write_update('Stopping WebSocket server ...')
            self._port.stop_websocket_server()
            self._did_start_websocket_server = False
        if self._did_start_wpt_server:
            self._printer.write_update('Stopping Web Platform Test server ...')
            self._port.stop_web_platform_test_server()
            self._did_start_wpt_server = False

    def handle(self, name, source, *args):
        method = getattr(self, '_handle_' + name)
        if method:
            return method(source, *args)
        raise AssertionError('unknown message %s received from %s, args=%s' % (name, source, repr(args)))

    def _handle_started_test(self, worker_name, test_input, test_timeout_sec):
        self._printer.print_started_test(test_input.test_name)

    def _handle_finished_test(self, worker_name, result, log_messages=[]):
        self._update_summary_with_result(self._current_run_results, result)

    def _handle_finished_test_group(self, worker_name, overlay_results, log_messages=[]):
        self._annotate_results_with_additional_failures(self._current_run_results, overlay_results)


class Worker(object):
    def __init__(self, caller, results_directory, options):
        self._caller = caller
        self._worker_number = caller.worker_number
        self._name = caller.name
        self._results_directory = results_directory
        self._options = options

        # The remaining fields are initialized in start()
        self._host = None
        self._port = None
        self._batch_size = None
        self._batch_count = None
        self._filesystem = None
        self._driver = None
        self._num_tests = 0

    def __del__(self):
        self.stop()

    def start(self):
        """This method is called when the object is starting to be used and it is safe
        for the object to create state that does not need to be pickled (usually this means
        it is called in a child process)."""
        self._host = self._caller.host
        self._filesystem = self._host.filesystem
        self._port = self._host.port_factory.get(self._options.platform, self._options)

        self._batch_count = 0
        self._batch_size = self._options.batch_size or 0

    def handle(self, name, source, test_list_name, test_inputs):
        assert name == 'test_list'
        for test_input in test_inputs:
            self._run_test(test_input, test_list_name)

        self._finished_test_group(test_inputs)

    def _update_test_input(self, test_input):
        if test_input.reference_files is None:
            # Lazy initialization.
            test_input.reference_files = self._port.reference_files(test_input.test_name, device_type=self._port.target_host(self._worker_number).device_type)
        if test_input.reference_files:
            test_input.should_run_pixel_test = True
        else:
            test_input.should_run_pixel_test = self._port.should_run_as_pixel_test(test_input)

    def _run_test(self, test_input, shard_name):
        self._batch_count += 1

        stop_when_done = False
        if self._batch_size > 0 and self._batch_count >= self._batch_size:
            self._batch_count = 0
            stop_when_done = True

        self._update_test_input(test_input)
        test_timeout_sec = self._timeout(test_input)
        start = time.time()
        self._caller.post('started_test', test_input, test_timeout_sec)

        result = self._run_test_with_or_without_timeout(test_input, test_timeout_sec, stop_when_done)
        result.shard_name = shard_name
        result.worker_name = self._name
        result.total_run_time = time.time() - start
        result.test_number = self._num_tests
        self._num_tests += 1

        self._caller.post('finished_test', result)

        self._clean_up_after_test(test_input, result)

    def _do_post_tests_work(self, driver):
        additional_results = []
        if not driver:
            return additional_results

        post_test_output = driver.do_post_tests_work()
        if post_test_output:
            for test_name, doc_list in post_test_output.world_leaks_dict.iteritems():
                additional_results.append(test_results.TestResult(test_name, [test_failures.FailureDocumentLeak(doc_list)]))
        return additional_results

    def _finished_test_group(self, test_inputs):
        _log.debug("%s finished test group" % self._name)

        if self._driver and self._driver.has_crashed():
            self._kill_driver()

        additional_results = []
        if not self._options.run_singly:
            additional_results = self._do_post_tests_work(self._driver)

        self._caller.post('finished_test_group', additional_results)

    def stop(self):
        _log.debug("%s cleaning up" % self._name)
        self._kill_driver()

    def _timeout(self, test_input):
        """Compute the appropriate timeout value for a test."""
        # The DumpRenderTree watchdog uses 2.5x the timeout; we want to be
        # larger than that. We also add a little more padding if we're
        # running tests in a separate thread.
        #
        # Note that we need to convert the test timeout from a
        # string value in milliseconds to a float for Python.
        driver_timeout_sec = 3.0 * float(test_input.timeout) / 1000.0
        if not self._options.run_singly:
            return driver_timeout_sec

        thread_padding_sec = 1.0
        thread_timeout_sec = driver_timeout_sec + thread_padding_sec
        return thread_timeout_sec

    def _kill_driver(self):
        # Be careful about how and when we kill the driver; if driver.stop()
        # raises an exception, this routine may get re-entered via __del__.
        driver = self._driver
        self._driver = None
        if driver:
            _log.debug("%s killing driver" % self._name)
            driver.stop()

    def _run_test_with_or_without_timeout(self, test_input, timeout, stop_when_done):
        if self._options.run_singly:
            return self._run_test_in_another_thread(test_input, timeout, stop_when_done)
        return self._run_test_in_this_thread(test_input, stop_when_done)

    def _clean_up_after_test(self, test_input, result):
        test_name = test_input.test_name

        if result.failures:
            # Check and kill DumpRenderTree if we need to.
            if any([f.driver_needs_restart() for f in result.failures]):
                self._kill_driver()
                # Reset the batch count since the shell just bounced.
                self._batch_count = 0

            # Print the error message(s).
            _log.debug("%s %s failed:" % (self._name, test_name))
            for f in result.failures:
                _log.debug("%s  %s" % (self._name, f.message()))
        elif result.type == test_expectations.SKIP:
            _log.debug("%s %s skipped" % (self._name, test_name))
        else:
            _log.debug("%s %s passed" % (self._name, test_name))

    def _run_test_in_another_thread(self, test_input, thread_timeout_sec, stop_when_done):
        """Run a test in a separate thread, enforcing a hard time limit.

        Since we can only detect the termination of a thread, not any internal
        state or progress, we can only run per-test timeouts when running test
        files singly.

        Args:
          test_input: Object containing the test filename and timeout
          thread_timeout_sec: time to wait before killing the driver process.
        Returns:
          A TestResult
        """
        worker = self

        driver = self._port.create_driver(self._worker_number, self._options.no_timeout)

        class SingleTestThread(threading.Thread):
            def __init__(self):
                threading.Thread.__init__(self)
                self.result = None

            def run(self):
                self.result = worker._run_single_test(driver, test_input, stop_when_done)

        thread = SingleTestThread()
        thread.start()
        thread.join(thread_timeout_sec)
        result = thread.result
        failures = []
        if thread.isAlive():
            # If join() returned with the thread still running, the
            # DumpRenderTree is completely hung and there's nothing
            # more we can do with it.  We have to kill all the
            # DumpRenderTrees to free it up. If we're running more than
            # one DumpRenderTree thread, we'll end up killing the other
            # DumpRenderTrees too, introducing spurious crashes. We accept
            # that tradeoff in order to avoid losing the rest of this
            # thread's results.
            _log.error('Test thread hung: killing all DumpRenderTrees')
            failures = [test_failures.FailureTimeout()]
        else:
            failure_results = self._do_post_tests_work(driver)
            for failure_result in failure_results:
                if failure_result.test_name == result.test_name:
                    result.convert_to_failure(failure_result)

        driver.stop()

        if not result:
            result = test_results.TestResult(test_input.test_name, failures=failures, test_run_time=0)
        return result

    def _run_test_in_this_thread(self, test_input, stop_when_done):
        """Run a single test file using a shared DumpRenderTree process.

        Args:
          test_input: Object containing the test filename, uri and timeout

        Returns: a TestResult object.
        """
        if self._driver and self._driver.has_crashed():
            self._kill_driver()
        if not self._driver:
            self._driver = self._port.create_driver(self._worker_number, self._options.no_timeout)
        return self._run_single_test(self._driver, test_input, stop_when_done)

    def _run_single_test(self, driver, test_input, stop_when_done):
        return single_test_runner.run_single_test(self._port, self._options, self._results_directory,
            self._name, driver, test_input, stop_when_done)


class TestShard(object):
    """A test shard is a named list of TestInputs."""

    def __init__(self, name, test_inputs):
        self.name = name
        self.test_inputs = test_inputs
        self.needs_servers = test_inputs[0].needs_servers

    def __repr__(self):
        return "TestShard(name='%s', test_inputs=%s, needs_servers=%s'" % (self.name, self.test_inputs, self.needs_servers)

    def __eq__(self, other):
        return self.name == other.name and self.test_inputs == other.test_inputs


class Sharder(object):
    def __init__(self, test_split_fn):
        self._split = test_split_fn

    def shard_tests(self, test_inputs, num_workers, fully_parallel):
        """Groups tests into batches.
        This helps ensure that tests that depend on each other (aka bad tests!)
        continue to run together as most cross-tests dependencies tend to
        occur within the same directory.
        Return:
            A list of TestShards.
        """

        # FIXME: Move all of the sharding logic out of manager into its
        # own class or module. Consider grouping it with the chunking logic
        # in prepare_lists as well.
        if num_workers == 1:
            return [TestShard('all_tests', test_inputs)]
        elif fully_parallel:
            return self._shard_every_file(test_inputs)
        return self._shard_by_directory(test_inputs, num_workers)

    def _shard_every_file(self, test_inputs):
        """Returns a list of shards, each shard containing a single test file.

        This mode gets maximal parallelism at the cost of much higher flakiness."""
        shards = []
        for test_input in test_inputs:
            # Note that we use a '.' for the shard name; the name doesn't really
            # matter, and the only other meaningful value would be the filename,
            # which would be really redundant.
            shards.append(TestShard('.', [test_input]))

        return shards

    def _shard_by_directory(self, test_inputs, num_workers):
        """Returns a lists of shards, each shard containing all the files in a directory.

        This is the default mode, and gets as much parallelism as we can while
        minimizing flakiness caused by inter-test dependencies."""
        shards = []
        tests_by_dir = {}
        # FIXME: Given that the tests are already sorted by directory,
        # we can probably rewrite this to be clearer and faster.
        for test_input in test_inputs:
            directory = self._split(test_input.test_name)[0]
            tests_by_dir.setdefault(directory, [])
            tests_by_dir[directory].append(test_input)

        for directory, test_inputs in tests_by_dir.iteritems():
            shard = TestShard(directory, test_inputs)
            shards.append(shard)

        # Sort the shards by directory name.
        shards.sort(key=lambda shard: shard.name)

        return shards
