#!/usr/bin/env python
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

"""A Thread object for running the test shell and processing URLs from a
shared queue.

Each thread runs a separate instance of the test_shell binary and validates
the output.  When there are no more URLs to process in the shared queue, the
thread exits.
"""

import copy
import logging
import os
import Queue
import signal
import subprocess
import sys
import thread
import threading
import time

from port import path_utils
import test_failures


def process_output(proc, test_info, test_types, test_args, target, output_dir):
    """Receives the output from a test_shell process, subjects it to a number
    of tests, and returns a list of failure types the test produced.

    Args:
      proc: an active test_shell process
      test_info: Object containing the test filename, uri and timeout
      test_types: list of test types to subject the output to
      test_args: arguments to be passed to each test
      target: Debug or Release
      output_dir: directory to put crash stack traces into

    Returns: a list of failure objects and times for the test being processed
    """
    outlines = []
    extra_lines = []
    failures = []
    crash = False

    # Some test args, such as the image hash, may be added or changed on a
    # test-by-test basis.
    local_test_args = copy.copy(test_args)

    start_time = time.time()

    line = proc.stdout.readline()

    # Only start saving output lines once we've loaded the URL for the test.
    url = None
    test_string = test_info.uri.strip()

    while line.rstrip() != "#EOF":
        # Make sure we haven't crashed.
        if line == '' and proc.poll() is not None:
            failures.append(test_failures.FailureCrash())

            # This is hex code 0xc000001d, which is used for abrupt
            # termination. This happens if we hit ctrl+c from the prompt and
            # we happen to be waiting on the test_shell.
            # sdoyon: Not sure for which OS and in what circumstances the
            # above code is valid. What works for me under Linux to detect
            # ctrl+c is for the subprocess returncode to be negative SIGINT.
            # And that agrees with the subprocess documentation.
            if (-1073741510 == proc.returncode or
                - signal.SIGINT == proc.returncode):
                raise KeyboardInterrupt
            crash = True
            break

        # Don't include #URL lines in our output
        if line.startswith("#URL:"):
            url = line.rstrip()[5:]
            if url != test_string:
                logging.fatal("Test got out of sync:\n|%s|\n|%s|" %
                              (url, test_string))
                raise AssertionError("test out of sync")
        elif line.startswith("#MD5:"):
            local_test_args.hash = line.rstrip()[5:]
        elif line.startswith("#TEST_TIMED_OUT"):
            # Test timed out, but we still need to read until #EOF.
            failures.append(test_failures.FailureTimeout())
        elif url:
            outlines.append(line)
        else:
            extra_lines.append(line)

        line = proc.stdout.readline()

    end_test_time = time.time()

    if len(extra_lines):
        extra = "".join(extra_lines)
        if crash:
            logging.debug("Stacktrace for %s:\n%s" % (test_string, extra))
            # Strip off "file://" since RelativeTestFilename expects
            # filesystem paths.
            filename = os.path.join(output_dir,
                path_utils.relative_test_filename(test_string[7:]))
            filename = os.path.splitext(filename)[0] + "-stack.txt"
            path_utils.maybe_make_directory(os.path.split(filename)[0])
            open(filename, "wb").write(extra)
        else:
            logging.debug("Previous test output extra lines after dump:\n%s" %
                extra)

    # Check the output and save the results.
    time_for_diffs = {}
    for test_type in test_types:
        start_diff_time = time.time()
        new_failures = test_type.compare_output(test_info.filename,
                                                proc, ''.join(outlines),
                                                local_test_args, target)
        # Don't add any more failures if we already have a crash, so we don't
        # double-report those tests. We do double-report for timeouts since
        # we still want to see the text and image output.
        if not crash:
            failures.extend(new_failures)
        time_for_diffs[test_type.__class__.__name__] = (
            time.time() - start_diff_time)

    total_time_for_all_diffs = time.time() - end_test_time
    test_run_time = end_test_time - start_time
    return TestStats(test_info.filename, failures, test_run_time,
        total_time_for_all_diffs, time_for_diffs)


def start_test_shell(command, args):
    """Returns the process for a new test_shell started in layout-tests mode.
    """
    cmd = []
    # Hook for injecting valgrind or other runtime instrumentation,
    # used by e.g. tools/valgrind/valgrind_tests.py.
    wrapper = os.environ.get("BROWSER_WRAPPER", None)
    if wrapper != None:
        cmd += [wrapper]
    cmd += command + ['--layout-tests'] + args
    return subprocess.Popen(cmd,
                            stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)


class TestStats:

    def __init__(self, filename, failures, test_run_time,
                 total_time_for_all_diffs, time_for_diffs):
        self.filename = filename
        self.failures = failures
        self.test_run_time = test_run_time
        self.total_time_for_all_diffs = total_time_for_all_diffs
        self.time_for_diffs = time_for_diffs


class SingleTestThread(threading.Thread):
    """Thread wrapper for running a single test file."""

    def __init__(self, test_shell_command, shell_args, test_info, test_types,
        test_args, target, output_dir):
        """
        Args:
          test_info: Object containing the test filename, uri and timeout
          output_dir: Directory to put crash stacks into.
          See TestShellThread for documentation of the remaining arguments.
        """

        threading.Thread.__init__(self)
        self._command = test_shell_command
        self._shell_args = shell_args
        self._test_info = test_info
        self._test_types = test_types
        self._test_args = test_args
        self._target = target
        self._output_dir = output_dir

    def run(self):
        proc = start_test_shell(self._command, self._shell_args +
            ["--time-out-ms=" + self._test_info.timeout, self._test_info.uri])
        self._test_stats = process_output(proc, self._test_info,
            self._test_types, self._test_args, self._target, self._output_dir)

    def get_test_stats(self):
        return self._test_stats


class TestShellThread(threading.Thread):

    def __init__(self, filename_list_queue, result_queue, test_shell_command,
                 test_types, test_args, shell_args, options):
        """Initialize all the local state for this test shell thread.

        Args:
          filename_list_queue: A thread safe Queue class that contains lists
              of tuples of (filename, uri) pairs.
          result_queue: A thread safe Queue class that will contain tuples of
              (test, failure lists) for the test results.
          test_shell_command: A list specifying the command+args for
              test_shell
          test_types: A list of TestType objects to run the test output
              against.
          test_args: A TestArguments object to pass to each TestType.
          shell_args: Any extra arguments to be passed to test_shell.exe.
          options: A property dictionary as produced by optparse. The
              command-line options should match those expected by
              run_webkit_tests; they are typically passed via the
              run_webkit_tests.TestRunner class."""
        threading.Thread.__init__(self)
        self._filename_list_queue = filename_list_queue
        self._result_queue = result_queue
        self._filename_list = []
        self._test_shell_command = test_shell_command
        self._test_types = test_types
        self._test_args = test_args
        self._test_shell_proc = None
        self._shell_args = shell_args
        self._options = options
        self._canceled = False
        self._exception_info = None
        self._directory_timing_stats = {}
        self._test_stats = []
        self._num_tests = 0
        self._start_time = 0
        self._stop_time = 0

        # Current directory of tests we're running.
        self._current_dir = None
        # Number of tests in self._current_dir.
        self._num_tests_in_current_dir = None
        # Time at which we started running tests from self._current_dir.
        self._current_dir_start_time = None

    def get_directory_timing_stats(self):
        """Returns a dictionary mapping test directory to a tuple of
        (number of tests in that directory, time to run the tests)"""
        return self._directory_timing_stats

    def get_individual_test_stats(self):
        """Returns a list of (test_filename, time_to_run_test,
        total_time_for_all_diffs, time_for_diffs) tuples."""
        return self._test_stats

    def cancel(self):
        """Set a flag telling this thread to quit."""
        self._canceled = True

    def get_exception_info(self):
        """If run() terminated on an uncaught exception, return it here
        ((type, value, traceback) tuple).
        Returns None if run() terminated normally. Meant to be called after
        joining this thread."""
        return self._exception_info

    def get_total_time(self):
        return max(self._stop_time - self._start_time, 0.0)

    def get_num_tests(self):
        return self._num_tests

    def run(self):
        """Delegate main work to a helper method and watch for uncaught
        exceptions."""
        self._start_time = time.time()
        self._num_tests = 0
        try:
            logging.debug('%s starting' % (self.getName()))
            self._run(test_runner=None, result_summary=None)
            logging.debug('%s done (%d tests)' % (self.getName(),
                          self.get_num_tests()))
        except:
            # Save the exception for our caller to see.
            self._exception_info = sys.exc_info()
            self._stop_time = time.time()
            # Re-raise it and die.
            logging.error('%s dying: %s' % (self.getName(),
                          self._exception_info))
            raise
        self._stop_time = time.time()

    def run_in_main_thread(self, test_runner, result_summary):
        """This hook allows us to run the tests from the main thread if
        --num-test-shells==1, instead of having to always run two or more
        threads. This allows us to debug the test harness without having to
        do multi-threaded debugging."""
        self._run(test_runner, result_summary)

    def _run(self, test_runner, result_summary):
        """Main work entry point of the thread. Basically we pull urls from the
        filename queue and run the tests until we run out of urls.

        If test_runner is not None, then we call test_runner.UpdateSummary()
        with the results of each test."""
        batch_size = 0
        batch_count = 0
        if self._options.batch_size:
            try:
                batch_size = int(self._options.batch_size)
            except:
                logging.info("Ignoring invalid batch size '%s'" %
                             self._options.batch_size)

        # Append tests we're running to the existing tests_run.txt file.
        # This is created in run_webkit_tests.py:_PrepareListsAndPrintOutput.
        tests_run_filename = os.path.join(self._options.results_directory,
                                          "tests_run.txt")
        tests_run_file = open(tests_run_filename, "a")

        while True:
            if self._canceled:
                logging.info('Testing canceled')
                tests_run_file.close()
                return

            if len(self._filename_list) is 0:
                if self._current_dir is not None:
                    self._directory_timing_stats[self._current_dir] = \
                        (self._num_tests_in_current_dir,
                         time.time() - self._current_dir_start_time)

                try:
                    self._current_dir, self._filename_list = \
                        self._filename_list_queue.get_nowait()
                except Queue.Empty:
                    self._kill_test_shell()
                    tests_run_file.close()
                    return

                self._num_tests_in_current_dir = len(self._filename_list)
                self._current_dir_start_time = time.time()

            test_info = self._filename_list.pop()

            # We have a url, run tests.
            batch_count += 1
            self._num_tests += 1
            if self._options.run_singly:
                failures = self._run_test_singly(test_info)
            else:
                failures = self._run_test(test_info)

            filename = test_info.filename
            tests_run_file.write(filename + "\n")
            if failures:
                # Check and kill test shell if we need too.
                if len([1 for f in failures if f.should_kill_test_shell()]):
                    self._kill_test_shell()
                    # Reset the batch count since the shell just bounced.
                    batch_count = 0
                # Print the error message(s).
                error_str = '\n'.join(['  ' + f.message() for f in failures])
                logging.debug("%s %s failed:\n%s" % (self.getName(),
                              path_utils.relative_test_filename(filename),
                              error_str))
            else:
                logging.debug("%s %s passed" % (self.getName(),
                              path_utils.relative_test_filename(filename)))
            self._result_queue.put((filename, failures))

            if batch_size > 0 and batch_count > batch_size:
                # Bounce the shell and reset count.
                self._kill_test_shell()
                batch_count = 0

            if test_runner:
                test_runner.update_summary(result_summary)

    def _run_test_singly(self, test_info):
        """Run a test in a separate thread, enforcing a hard time limit.

        Since we can only detect the termination of a thread, not any internal
        state or progress, we can only run per-test timeouts when running test
        files singly.

        Args:
          test_info: Object containing the test filename, uri and timeout

        Return:
          A list of TestFailure objects describing the error.
        """
        worker = SingleTestThread(self._test_shell_command,
                                  self._shell_args,
                                  test_info,
                                  self._test_types,
                                  self._test_args,
                                  self._options.target,
                                  self._options.results_directory)

        worker.start()

        # When we're running one test per test_shell process, we can enforce
        # a hard timeout. the test_shell watchdog uses 2.5x the timeout
        # We want to be larger than that.
        worker.join(int(test_info.timeout) * 3.0 / 1000.0)
        if worker.isAlive():
            # If join() returned with the thread still running, the
            # test_shell.exe is completely hung and there's nothing
            # more we can do with it.  We have to kill all the
            # test_shells to free it up. If we're running more than
            # one test_shell thread, we'll end up killing the other
            # test_shells too, introducing spurious crashes. We accept that
            # tradeoff in order to avoid losing the rest of this thread's
            # results.
            logging.error('Test thread hung: killing all test_shells')
            path_utils.kill_all_test_shells()

        try:
            stats = worker.get_test_stats()
            self._test_stats.append(stats)
            failures = stats.failures
        except AttributeError, e:
            failures = []
            logging.error('Cannot get results of test: %s' %
                          test_info.filename)

        return failures

    def _run_test(self, test_info):
        """Run a single test file using a shared test_shell process.

        Args:
          test_info: Object containing the test filename, uri and timeout

        Return:
          A list of TestFailure objects describing the error.
        """
        self._ensure_test_shell_is_running()
        # Args to test_shell is a space-separated list of
        # "uri timeout pixel_hash"
        # The timeout and pixel_hash are optional.  The timeout is used if this
        # test has a custom timeout. The pixel_hash is used to avoid doing an
        # image dump if the checksums match, so it should be set to a blank
        # value if we are generating a new baseline.
        # (Otherwise, an image from a previous run will be copied into
        # the baseline.)
        image_hash = test_info.image_hash
        if image_hash and self._test_args.new_baseline:
            image_hash = ""
        self._test_shell_proc.stdin.write(("%s %s %s\n" %
            (test_info.uri, test_info.timeout, image_hash)))

        # If the test shell is dead, the above may cause an IOError as we
        # try to write onto the broken pipe. If this is the first test for
        # this test shell process, than the test shell did not
        # successfully start. If this is not the first test, then the
        # previous tests have caused some kind of delayed crash. We don't
        # try to recover here.
        self._test_shell_proc.stdin.flush()

        stats = process_output(self._test_shell_proc, test_info,
                               self._test_types, self._test_args,
                               self._options.target,
                               self._options.results_directory)

        self._test_stats.append(stats)
        return stats.failures

    def _ensure_test_shell_is_running(self):
        """Start the shared test shell, if it's not running.  Not for use when
        running tests singly, since those each start a separate test shell in
        their own thread.
        """
        if (not self._test_shell_proc or
            self._test_shell_proc.poll() is not None):
            self._test_shell_proc = start_test_shell(self._test_shell_command,
                                                     self._shell_args)

    def _kill_test_shell(self):
        """Kill the test shell process if it's running."""
        if self._test_shell_proc:
            self._test_shell_proc.stdin.close()
            self._test_shell_proc.stdout.close()
            if self._test_shell_proc.stderr:
                self._test_shell_proc.stderr.close()
            if (sys.platform not in ('win32', 'cygwin') and
                not self._test_shell_proc.poll()):
                # Closing stdin/stdout/stderr hangs sometimes on OS X.
                null = open(os.devnull, "w")
                subprocess.Popen(["kill", "-9",
                                 str(self._test_shell_proc.pid)], stderr=null)
                null.close()
            self._test_shell_proc = None
