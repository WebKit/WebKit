# Copyright (C) 2023 Apple Inc. All rights reserved.
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

from __future__ import annotations

import argparse
import logging
import math
import re
import sys
import time
import unittest
from typing import Any, Iterable, Sequence

from webkitcorepy import arguments, log, string_utils, Terminal


class TestRunner(object):
    INDENT = 4

    @classmethod
    def combine(cls, *results: unittest.TestResult) -> unittest.TestResult:
        combined = unittest.TestResult()
        for result in results:
            for attribute in [
                'failures', 'errors', 'testsRun',
                'skipped', 'expectedFailures', 'unexpectedSuccesses',
            ]:
                setattr(
                    combined, attribute,
                    getattr(combined, attribute) + getattr(result, attribute),
                )
        return combined

    def __init__(self, description: str, loggers: Sequence[logging.Logger] | None = None) -> None:
        self.parser = argparse.ArgumentParser(description=description)
        self.parser.add_argument(
            '-l', '--list',
            help='List all tests in suite',
            dest='list',
            action='store_true',
            default=False
        )
        self.parser.add_argument(
            'tests', nargs='*',
            type=lambda value: re.compile('{}.*'.format(re.escape(value).replace('\\*', '.*'))),
            default=[],
            help='Names of tests, suites or globs of tests to run',
        )

        arguments.LoggingGroup(
            self.parser,
            loggers=loggers or [logging.getLogger(), log],
        )

    def tests(self, args: argparse.Namespace | None = None) -> Iterable[str]:
        raise NotImplementedError('Subclass must implement')

    def run_test(self, test: str) -> unittest.TestResult:
        raise NotImplementedError('Subclass must implement')

    # Results identify tests by TestCase, or by name when a test couldn't be found.
    def id(self, test: str | unittest.TestCase) -> str:
        return test if isinstance(test, str) else str(test)

    def run(self, args: argparse.Namespace) -> int:
        tm = time.time
        start_time = tm()

        results = unittest.TestResult()
        tests_to_run = list(self.tests(args))

        try:
            for count in range(len(tests_to_run)):
                test = tests_to_run[count]

                line = '[{}/{}] {}'.format(count + 1, len(tests_to_run), test)
                chars = len(line)
                sys.stdout.write(line)
                sys.stdout.flush()

                incremental = self.run_test(test)
                results = self.combine(results, incremental)

                sys.stdout.write(' ')
                result = False
                for description, attribute in (('failed', 'failures'), ('erred', 'errors')):
                    value = getattr(incremental, attribute)
                    if not value:
                        continue
                    sys.stdout.write('{}\n'.format(description))
                    if args.log_level <= logging.ERROR:
                        if not result:
                            sys.stderr.write('\n')
                        for member in value:
                            sys.stderr.write(member[1] + '\n')
                    result = True
                if incremental.unexpectedSuccesses:
                    sys.stdout.write('unexpectedly passed\n')
                    if args.log_level <= logging.ERROR:
                        sys.stderr.write('\n')
                        for unexpected in incremental.unexpectedSuccesses:
                            sys.stderr.write(f'UNEXPECTED SUCCESS: {unexpected.id()}\n')
                    result = True
                if result:
                    continue

                if incremental.skipped or not incremental.testsRun:
                    chars += 8
                    sys.stdout.write('skipped\n')
                elif incremental.expectedFailures:
                    chars += 16
                    sys.stdout.write('expected failure\n')
                else:
                    chars += 7
                    sys.stdout.write('passed\n')

                if args.log_level >= logging.WARNING and sys.stdout.isatty():
                    _, columns = Terminal.size()
                    sys.stdout.write('\033[F\033[K' * int(math.ceil(chars / (columns or chars))))

        except KeyboardInterrupt:
            sys.stderr.write('\nUser interupted the program\n\n')

        print('\nRan {} of {} tests in {:0.3f} seconds\n'.format(results.testsRun, len(tests_to_run), tm() - start_time))

        if results.failures or results.errors or results.unexpectedSuccesses:
            print('FAILED')
            for attribute in ('failure', 'error'):
                value = getattr(results, attribute + 's')
                if not value:
                    continue
                print('{}{}'.format(' ' * self.INDENT, string_utils.pluralize(len(value), attribute)))
                if args.log_level < logging.WARNING:
                    for part in value:
                        print('{}{}'.format(' ' * self.INDENT * 2, self.id(part[0])))
                        if args.log_level >= logging.INFO or not part[1]:
                            continue
                        print()
                        for line in part[1].splitlines():
                            print('{}{}'.format(' ' * self.INDENT * 3, line))
                        print()
            if results.unexpectedSuccesses:
                print(f'    {len(results.unexpectedSuccesses)} unexpected success(es)')
                if args.log_level < logging.WARNING:
                    for unexpected in results.unexpectedSuccesses:
                        print(f'        {self.id(unexpected)}')
            return 1
        elif not results.testsRun:
            print('No tests run')
            return -1
        print('SUCCESS')
        if results.expectedFailures:
            print(f'    {len(results.expectedFailures)} expected failure(s)')
        return 0

    def main(self, *args: str, **kwargs: Any) -> int:
        parsed = self.parser.parse_args(args)
        parsed.log_level = getattr(parsed, 'log_level', log.level)

        if parsed.log_level < logging.INFO:
            log.debug('Found {} tests...'.format(len(list(self.tests()))))
            log.debug('{} tests match filters'.format(len(list(self.tests(parsed)))))

        if parsed.list:
            tests = self.tests(parsed)
            if not tests:
                sys.stderr.write('No tests found\n')
                return 1
            for test in tests:
                print(test)
            return 0

        return self.run(parsed)
