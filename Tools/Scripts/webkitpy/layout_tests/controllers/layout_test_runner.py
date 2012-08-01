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

import logging
import math
import re
import threading
import time

from webkitpy.layout_tests.controllers import single_test_runner
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_results


_log = logging.getLogger(__name__)


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
        self._tests_run_file = None
        self._tests_run_filename = None

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
        tests_run_filename = self._filesystem.join(self._results_directory, "tests_run%d.txt" % self._worker_number)
        self._tests_run_file = self._filesystem.open_text_file_for_writing(tests_run_filename)

    def handle(self, name, source, test_list_name, test_inputs):
        assert name == 'test_list'
        start_time = time.time()
        for test_input in test_inputs:
            self._run_test(test_input)
        elapsed_time = time.time() - start_time
        self._caller.post('finished_test_list', test_list_name, len(test_inputs), elapsed_time)

    def _update_test_input(self, test_input):
        if test_input.reference_files is None:
            # Lazy initialization.
            test_input.reference_files = self._port.reference_files(test_input.test_name)
        if test_input.reference_files:
            test_input.should_run_pixel_test = True
        else:
            test_input.should_run_pixel_test = self._port.should_run_as_pixel_test(test_input)

    def _run_test(self, test_input):
        self._update_test_input(test_input)
        test_timeout_sec = self._timeout(test_input)
        start = time.time()
        self._caller.post('started_test', test_input, test_timeout_sec)

        result = self._run_test_with_timeout(test_input, test_timeout_sec)

        elapsed_time = time.time() - start
        self._caller.post('finished_test', result, elapsed_time)

        self._clean_up_after_test(test_input, result)

    def stop(self):
        _log.debug("%s cleaning up" % self._name)
        self._kill_driver()
        if self._tests_run_file:
            self._tests_run_file.close()
            self._tests_run_file = None

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

    def _run_test_with_timeout(self, test_input, timeout):
        if self._options.run_singly:
            return self._run_test_in_another_thread(test_input, timeout)
        return self._run_test_in_this_thread(test_input)

    def _clean_up_after_test(self, test_input, result):
        self._batch_count += 1
        test_name = test_input.test_name
        self._tests_run_file.write(test_name + "\n")

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

        if self._batch_size > 0 and self._batch_count >= self._batch_size:
            self._kill_driver()
            self._batch_count = 0

    def _run_test_in_another_thread(self, test_input, thread_timeout_sec):
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

        driver = self._port.create_driver(self._worker_number)

        class SingleTestThread(threading.Thread):
            def __init__(self):
                threading.Thread.__init__(self)
                self.result = None

            def run(self):
                self.result = worker._run_single_test(driver, test_input)

        thread = SingleTestThread()
        thread.start()
        thread.join(thread_timeout_sec)
        result = thread.result
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

        driver.stop()

        if not result:
            result = test_results.TestResult(test_input.test_name, failures=[], test_run_time=0)
        return result

    def _run_test_in_this_thread(self, test_input):
        """Run a single test file using a shared DumpRenderTree process.

        Args:
          test_input: Object containing the test filename, uri and timeout

        Returns: a TestResult object.
        """
        if self._driver and self._driver.has_crashed():
            self._kill_driver()
        if not self._driver:
            self._driver = self._port.create_driver(self._worker_number)
        return self._run_single_test(self._driver, test_input)

    def _run_single_test(self, driver, test_input):
        return single_test_runner.run_single_test(self._port, self._options,
            test_input, driver, self._name)


class TestShard(object):
    """A test shard is a named list of TestInputs."""

    def __init__(self, name, test_inputs):
        self.name = name
        self.test_inputs = test_inputs
        self.requires_lock = test_inputs[0].requires_lock

    def __repr__(self):
        return "TestShard(name='%s', test_inputs=%s, requires_lock=%s'" % (self.name, self.test_inputs, self.requires_lock)

    def __eq__(self, other):
        return self.name == other.name and self.test_inputs == other.test_inputs


class Sharder(object):
    def __init__(self, test_split_fn, test_path_separator, max_locked_shards):
        self._split = test_split_fn
        self._sep = test_path_separator
        self._max_locked_shards = max_locked_shards

    def shard_tests(self, test_inputs, num_workers, fully_parallel, shard_ref_tests):
        """Groups tests into batches.
        This helps ensure that tests that depend on each other (aka bad tests!)
        continue to run together as most cross-tests dependencies tend to
        occur within the same directory.
        Return:
            Two list of TestShards. The first contains tests that must only be
            run under the server lock, the second can be run whenever.
        """

        # FIXME: Move all of the sharding logic out of manager into its
        # own class or module. Consider grouping it with the chunking logic
        # in prepare_lists as well.
        if num_workers == 1:
            return self._shard_in_two(test_inputs, shard_ref_tests)
        elif fully_parallel:
            return self._shard_every_file(test_inputs)
        return self._shard_by_directory(test_inputs, num_workers, shard_ref_tests)

    def _shard_in_two(self, test_inputs, shard_ref_tests):
        """Returns two lists of shards, one with all the tests requiring a lock and one with the rest.

        This is used when there's only one worker, to minimize the per-shard overhead."""
        locked_inputs = []
        locked_ref_test_inputs = []
        unlocked_inputs = []
        unlocked_ref_test_inputs = []
        for test_input in test_inputs:
            if test_input.requires_lock:
                if shard_ref_tests and test_input.reference_files:
                    locked_ref_test_inputs.append(test_input)
                else:
                    locked_inputs.append(test_input)
            else:
                if shard_ref_tests and test_input.reference_files:
                    unlocked_ref_test_inputs.append(test_input)
                else:
                    unlocked_inputs.append(test_input)
        locked_inputs.extend(locked_ref_test_inputs)
        unlocked_inputs.extend(unlocked_ref_test_inputs)

        locked_shards = []
        unlocked_shards = []
        if locked_inputs:
            locked_shards = [TestShard('locked_tests', locked_inputs)]
        if unlocked_inputs:
            unlocked_shards.append(TestShard('unlocked_tests', unlocked_inputs))

        return locked_shards, unlocked_shards

    def _shard_every_file(self, test_inputs):
        """Returns two lists of shards, each shard containing a single test file.

        This mode gets maximal parallelism at the cost of much higher flakiness."""
        locked_shards = []
        unlocked_shards = []
        for test_input in test_inputs:
            # Note that we use a '.' for the shard name; the name doesn't really
            # matter, and the only other meaningful value would be the filename,
            # which would be really redundant.
            if test_input.requires_lock:
                locked_shards.append(TestShard('.', [test_input]))
            else:
                unlocked_shards.append(TestShard('.', [test_input]))

        return locked_shards, unlocked_shards

    def _shard_by_directory(self, test_inputs, num_workers, shard_ref_tests):
        """Returns two lists of shards, each shard containing all the files in a directory.

        This is the default mode, and gets as much parallelism as we can while
        minimizing flakiness caused by inter-test dependencies."""
        locked_shards = []
        unlocked_shards = []
        tests_by_dir = {}
        ref_tests_by_dir = {}
        # FIXME: Given that the tests are already sorted by directory,
        # we can probably rewrite this to be clearer and faster.
        for test_input in test_inputs:
            directory = self._split(test_input.test_name)[0]
            if shard_ref_tests and test_input.reference_files:
                ref_tests_by_dir.setdefault(directory, [])
                ref_tests_by_dir[directory].append(test_input)
            else:
                tests_by_dir.setdefault(directory, [])
                tests_by_dir[directory].append(test_input)

        for directory, test_inputs in tests_by_dir.iteritems():
            shard = TestShard(directory, test_inputs)
            if test_inputs[0].requires_lock:
                locked_shards.append(shard)
            else:
                unlocked_shards.append(shard)

        for directory, test_inputs in ref_tests_by_dir.iteritems():
            # '~' to place the ref tests after other tests after sorted.
            shard = TestShard('~ref:' + directory, test_inputs)
            if test_inputs[0].requires_lock:
                locked_shards.append(shard)
            else:
                unlocked_shards.append(shard)

        # Sort the shards by directory name.
        locked_shards.sort(key=lambda shard: shard.name)
        unlocked_shards.sort(key=lambda shard: shard.name)

        # Put a ceiling on the number of locked shards, so that we
        # don't hammer the servers too badly.

        # FIXME: For now, limit to one shard or set it
        # with the --max-locked-shards. After testing to make sure we
        # can handle multiple shards, we should probably do something like
        # limit this to no more than a quarter of all workers, e.g.:
        # return max(math.ceil(num_workers / 4.0), 1)
        return (self._resize_shards(locked_shards, self._max_locked_shards, 'locked_shard'),
                unlocked_shards)

    def _resize_shards(self, old_shards, max_new_shards, shard_name_prefix):
        """Takes a list of shards and redistributes the tests into no more
        than |max_new_shards| new shards."""

        # This implementation assumes that each input shard only contains tests from a
        # single directory, and that tests in each shard must remain together; as a
        # result, a given input shard is never split between output shards.
        #
        # Each output shard contains the tests from one or more input shards and
        # hence may contain tests from multiple directories.

        def divide_and_round_up(numerator, divisor):
            return int(math.ceil(float(numerator) / divisor))

        def extract_and_flatten(shards):
            test_inputs = []
            for shard in shards:
                test_inputs.extend(shard.test_inputs)
            return test_inputs

        def split_at(seq, index):
            return (seq[:index], seq[index:])

        num_old_per_new = divide_and_round_up(len(old_shards), max_new_shards)
        new_shards = []
        remaining_shards = old_shards
        while remaining_shards:
            some_shards, remaining_shards = split_at(remaining_shards, num_old_per_new)
            new_shards.append(TestShard('%s_%d' % (shard_name_prefix, len(new_shards) + 1), extract_and_flatten(some_shards)))
        return new_shards

    def test_key(self, test_name):
        """Turns a test name into a list with two sublists, the natural key of the
        dirname, and the natural key of the basename.

        This can be used when sorting paths so that files in a directory.
        directory are kept together rather than being mixed in with files in
        subdirectories."""
        dirname, basename = self._split(test_name)
        return (self.natural_sort_key(dirname + self._sep), self.natural_sort_key(basename))

    @staticmethod
    def natural_sort_key(string_to_split):
        """ Turns a string into a list of string and number chunks, i.e. "z23a" -> ["z", 23, "a"]

        This can be used to implement "natural sort" order. See:
        http://www.codinghorror.com/blog/2007/12/sorting-for-humans-natural-sort-order.html
        http://nedbatchelder.com/blog/200712.html#e20071211T054956
        """
        def tryint(val):
            try:
                return int(val)
            except ValueError:
                return val

        return [tryint(chunk) for chunk in re.split('(\d+)', string_to_split)]
