# Copyright (C) 2021 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import time
import sys
import unittest

from webkitcorepy import OutputCapture, TaskPool, Timeout, log as logger


def setup(arg='Setting up'):
    logger.warning(arg)


def teardown(arg='Tearing down'):
    logger.warning(arg)


def action(argument, include_worker=False):
    print('{}action({})'.format(
        '{} '.format(TaskPool.Process.name) if include_worker else '',
        argument,
    ))
    return argument


def log(level, value):
    logger.log(level, value)
    return value


def wait(seconds):
    logger.info('waiting {} seconds...'.format(seconds))
    time.sleep(seconds)
    logger.info('waited {} seconds!'.format(seconds))
    return seconds


def exception(value):
    raise RuntimeError(value)


def simple_repeat_task():
    """A simple repeat task that just returns a constant value."""
    return 'repeat_task_executed'


def printing_repeat_task(marker):
    """A repeat task that prints a marker for counting."""
    print('REPEAT_MARKER_{}'.format(marker))
    time.sleep(0.01)
    return marker


class TaskPoolUnittest(unittest.TestCase):
    alphabet = 'abcdefghijklmnopqrstuvwxyz'

    def test_single_no_fork(self):
        with OutputCapture(level=logging.WARNING) as captured:
            with TaskPool(workers=1, force_fork=False) as pool:
                pool.do(action, 'a')
                pool.do(log, logging.WARNING, '1')
                pool.wait()

        self.assertEqual(captured.stdout.getvalue(), 'action(a)\n')
        self.assertEqual(captured.webkitcorepy.log.getvalue(), '1\n')

    def test_callback(self):
        sequence = []

        with OutputCapture():
            with TaskPool(workers=4) as pool:
                for character in self.alphabet:
                    pool.do(action, character, callback=lambda value: sequence.append(value))
                pool.wait()
        self.assertEqual(
            self.alphabet,
            ''.join(sorted(sequence)),
        )

    def test_exception_no_fork(self):
        with OutputCapture(level=logging.INFO) as captured:
            with self.assertRaises(RuntimeError):
                with TaskPool(workers=1, force_fork=False) as pool:
                    pool.do(exception, 'Testing exception')
                    pool.wait()
        self.assertEqual(captured.webkitcorepy.log.getvalue(), '')

    if sys.platform != 'cygwin':
        def test_single(self):
            with OutputCapture(level=logging.WARNING) as captured:
                with TaskPool(workers=1, force_fork=True) as pool:
                    pool.do(action, 'a')
                    pool.do(log, logging.WARNING, '1')
                    pool.wait()

            self.assertEqual(captured.stdout.getvalue(), 'action(a)\n')
            self.assertEqual(captured.webkitcorepy.log.getvalue(), 'worker/0 1\n')

        def test_multiple(self):
            with OutputCapture(level=logging.INFO) as captured:
                with TaskPool(workers=4) as pool:
                    for character in self.alphabet:
                        pool.do(action, character)
                    pool.wait()

            lines = captured.stdout.getvalue().splitlines()
            self.assertEqual(sorted(lines), ['action({})'.format(character) for character in self.alphabet])
            self.assertEqual(
                sorted(captured.webkitcorepy.log.getvalue().splitlines()),
                sorted(['worker/{} starting'.format(number) for number in range(4)] + ['worker/{} stopping'.format(number) for number in range(4)]),
            )

        def test_exception(self):
            with OutputCapture(level=logging.INFO) as captured:
                with self.assertRaises(RuntimeError):
                    with TaskPool(workers=1, force_fork=True) as pool:
                        pool.do(exception, 'Testing exception')
                        pool.wait()
            self.assertEqual(
                captured.webkitcorepy.log.getvalue().splitlines(),
                ['worker/0 starting', 'worker/0 stopping'],
            )

        def test_setup(self):
            with OutputCapture() as captured:
                with TaskPool(workers=4, setup=setup) as pool:
                    for character in self.alphabet:
                        pool.do(action, character)
                    pool.wait()
            self.assertEqual(
                sorted(captured.webkitcorepy.log.getvalue().splitlines()),
                ['worker/{} Setting up'.format(x) for x in range(4)],
            )

        def test_setup_arguments(self):
            with OutputCapture() as captured:
                with TaskPool(workers=4, setup=setup, setupargs=['Setup argument']) as pool:
                    for character in self.alphabet:
                        pool.do(action, character)
                    pool.wait()
            self.assertEqual(
                sorted(captured.webkitcorepy.log.getvalue().splitlines()),
                ['worker/{} Setup argument'.format(x) for x in range(4)],
            )

        def test_teardown(self):
            with OutputCapture() as captured:
                with TaskPool(workers=4, teardown=teardown) as pool:
                    for character in self.alphabet:
                        pool.do(action, character)
                    pool.wait()
            self.assertEqual(
                sorted(captured.webkitcorepy.log.getvalue().splitlines()),
                ['worker/{} Tearing down'.format(x) for x in range(4)],
            )

        def test_teardown_arguments(self):
            with OutputCapture() as captured:
                with TaskPool(workers=4, teardown=teardown, teardownargs=['Teardown argument']) as pool:
                    for character in self.alphabet:
                        pool.do(action, character)
                    pool.wait()
            self.assertEqual(
                sorted(captured.webkitcorepy.log.getvalue().splitlines()),
                ['worker/{} Teardown argument'.format(x) for x in range(4)],
            )

        def test_mutually_exclusive_group(self):
            with OutputCapture(level=logging.INFO) as captured:
                with TaskPool(workers=4, mutually_exclusive_groups=['group']) as pool:
                    for character in self.alphabet:
                        pool.do(
                            action, character,
                            include_worker=True,
                            group='group' if character in ('a', 'b', 'c', 'd') else None,
                        )
                    pool.wait()

            self.assertEqual(
                sorted(captured.webkitcorepy.log.getvalue().splitlines()), [
                    'worker/0 starting (group)', 'worker/0 stopping (group)',
                    'worker/1 starting', 'worker/1 stopping',
                    'worker/2 starting', 'worker/2 stopping',
                    'worker/3 starting', 'worker/3 stopping',
                ],
            )
            self.assertEqual(
                sorted(captured.stdout.getvalue().splitlines())[:4], [
                    'worker/0 action(a)',
                    'worker/0 action(b)',
                    'worker/0 action(c)',
                    'worker/0 action(d)',
                ]
            )

        def test_mutually_exclusive_group_single_worker(self):
            with OutputCapture(level=logging.INFO) as captured:
                with TaskPool(workers=1, mutually_exclusive_groups=['group']) as pool:
                    for character in self.alphabet:
                        pool.do(
                            action, character,
                            include_worker=True,
                            group='group' if character in ('a', 'b', 'c', 'd') else None,
                        )
                    pool.wait()

            self.assertEqual(
                sorted(captured.stdout.getvalue().splitlines())[:4], [
                    'worker/0 action(a)',
                    'worker/0 action(b)',
                    'worker/0 action(c)',
                    'worker/0 action(d)',
                ]
            )

        def test_mutually_exclusive_groups(self):
            with OutputCapture(level=logging.INFO) as captured:
                with TaskPool(workers=4, mutually_exclusive_groups=[
                    'alpha', 'bravo', 'charlie', 'delta', 'echo', 'foxtrot',
                ]) as pool:
                    for character in self.alphabet:
                        pool.do(
                            action,
                            character,
                            include_worker=True,
                            group=dict(
                                a='alpha',
                                b='bravo',
                                c='charlie',
                                d='delta',
                                e='echo',
                                f='foxtrot',
                            ).get(character, None)
                        )
                    pool.wait()

            self.assertEqual(
                sorted(captured.webkitcorepy.log.getvalue().splitlines()), [
                    'worker/0 starting (alpha, bravo)', 'worker/0 stopping (alpha, bravo)',
                    'worker/1 starting (charlie, delta)', 'worker/1 stopping (charlie, delta)',
                    'worker/2 starting (echo)', 'worker/2 stopping (echo)',
                    'worker/3 starting (foxtrot)', 'worker/3 stopping (foxtrot)',
                ],
            )

            logging_by_worker = {}
            for line in captured.stdout.getvalue().splitlines():
                worker = line.split(' ')[0]
                if worker not in logging_by_worker:
                    logging_by_worker[worker] = []
                logging_by_worker[worker].append(line)

            self.assertEqual(
                sorted(logging_by_worker['worker/0'])[:2],
                ['worker/0 action(a)', 'worker/0 action(b)'],
            )
            self.assertEqual(
                sorted(logging_by_worker['worker/1'])[:2],
                ['worker/1 action(c)', 'worker/1 action(d)'],
            )
            self.assertEqual(
                sorted(logging_by_worker['worker/2'])[0],
                'worker/2 action(e)',
            )
            self.assertEqual(
                sorted(logging_by_worker['worker/3'])[0],
                'worker/3 action(f)',
            )

        def test_invalid_group(self):
            with OutputCapture(level=logging.INFO) as captured:
                with TaskPool(workers=2) as pool:
                    with self.assertRaises(ValueError):
                        pool.do(action, 'a', group='invalid')
                    pool.wait()

        def test_mutually_exclusive_group_hang(self):
            with OutputCapture(level=logging.INFO) as captured:
                with TaskPool(workers=2, mutually_exclusive_groups=['group']) as pool:
                    for character in self.alphabet:
                        pool.do(
                            action, character,
                            include_worker=True,
                            group='group'
                        )

                    with Timeout(1):
                        pool.wait()

            self.assertEqual(len(captured.stdout.getvalue().splitlines()), 26)

        def test_repeat_task_with_groups(self):
            """Test that repeat tasks run continuously in groups while regular tasks execute."""

            with OutputCapture(level=logging.INFO) as captured:
                with TaskPool(workers=4, force_fork=True, mutually_exclusive_groups=['repeat_group']) as pool:
                    pool.do(printing_repeat_task, 'TEST', repeat=True, group='repeat_group')

                    # Add slow tasks to keep the pool busy and allow repeat task to run multiple times
                    for i in range(6):
                        pool.do(wait, 0.1)

                    pool.wait()

            # Verify repeat task executed multiple times by checking stdout
            stdout_output = captured.stdout.getvalue()
            repeat_count = stdout_output.count('REPEAT_MARKER_TEST')
            self.assertGreater(repeat_count, 1, "Repeat task should execute multiple times (got {})".format(repeat_count))

        def test_repeat_task_multiple_groups(self):
            """Test that multiple repeat tasks in different groups run simultaneously."""

            with OutputCapture(level=logging.INFO) as captured:
                with TaskPool(workers=4, force_fork=True, mutually_exclusive_groups=['group1', 'group2']) as pool:
                    # Add repeat tasks with markers to different groups
                    pool.do(printing_repeat_task, 'GROUP1', repeat=True, group='group1')
                    pool.do(printing_repeat_task, 'GROUP2', repeat=True, group='group2')

                    # Add slow tasks to give repeat tasks time to run
                    for i in range(20):
                        pool.do(wait, 0.1)
                    pool.wait()

                    # Verify both repeat tasks executed multiple times
                    stdout_output = captured.stdout.getvalue()
                    group1_count = stdout_output.count('REPEAT_MARKER_GROUP1')
                    group2_count = stdout_output.count('REPEAT_MARKER_GROUP2')

                    self.assertGreater(group1_count, 1, "Repeat task in group1 should execute multiple times (got {})".format(group1_count))
                    self.assertGreater(group2_count, 1, "Repeat task in group2 should execute multiple times (got {})".format(group2_count))

        def test_repeat_task_with_regular_grouped_tasks(self):
            """Test repeat tasks alongside regular grouped tasks."""

            with OutputCapture(level=logging.INFO) as captured:
                with TaskPool(workers=4, force_fork=True, mutually_exclusive_groups=['repeat_group', 'regular_group']) as pool:
                    # Add repeat task to repeat_group
                    pool.do(printing_repeat_task, 'REGULAR', repeat=True, group='repeat_group')

                    # Add slow tasks to keep the pool busy and allow repeat task to run multiple times
                    for i in range(6):
                        pool.do(wait, 0.1)

                    # Add regular tasks to regular_group
                    for i in range(10):
                        pool.do(action, 'grouped{}'.format(i), include_worker=True, group='regular_group')

                    # Add regular tasks to main queue
                    for i in range(10):
                        pool.do(action, 'regular{}'.format(i))
                    pool.wait()

            stdout_lines = captured.stdout.getvalue().splitlines()
            for i in range(10):
                # Grouped tasks include worker name
                self.assertTrue(any('action(grouped{})'.format(i) in line for line in stdout_lines), 'action(grouped{}) not found in output'.format(i))
                self.assertIn('action(regular{})'.format(i), stdout_lines)

            # Verify repeat task executed multiple times
            stdout_output = captured.stdout.getvalue()
            repeat_count = stdout_output.count('REPEAT_MARKER_REGULAR')
            self.assertGreater(repeat_count, 1, "Repeat task should execute multiple times (got {})".format(repeat_count))

        def test_repeat_task_pool_state_tracking_forked(self):
            """Test that TaskPool correctly tracks repeat task state in forked mode."""
            with OutputCapture(level=logging.INFO) as captured:
                with TaskPool(workers=4, force_fork=True, mutually_exclusive_groups=['test_group']) as pool:
                    # Check initial state
                    self.assertEqual(pool.repeat_pending_count, 0)
                    self.assertFalse(pool._has_repeat_tasks)

                    # Add regular task
                    pool.do(simple_repeat_task)
                    self.assertEqual(pool.repeat_pending_count, 0)
                    self.assertFalse(pool._has_repeat_tasks)

                    # Add repeat task to a group - this should update the tracking
                    pool.do(simple_repeat_task, repeat=True, group='test_group')
                    self.assertEqual(pool.repeat_pending_count, 1)
                    self.assertTrue(pool._has_repeat_tasks)

                    # Call wait() to properly clean up
                    pool.wait()
