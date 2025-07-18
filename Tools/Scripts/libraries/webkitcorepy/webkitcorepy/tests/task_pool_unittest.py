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
