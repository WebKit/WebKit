# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import json
import os
import subprocess
import sys
import tempfile
import textwrap
import unittest

# Runs a test file the way the W3C tests run, through a TestProcess, in a process of its own, so that a hang
# can be detected. Expectations are given by subtest name.
RUNNER = textwrap.dedent('''
    import json, os, sys
    sys.path.insert(0, sys.argv[1])
    from webkitpy.webdriver_tests import pytest_runner
    from webkitpy.webdriver_tests.test_process import TestProcess

    timeout = float(sys.argv[4])
    expectations_by_subtest = json.loads(sys.argv[5])

    class Expectations(object):
        def is_slow(self, test, subtest):
            return expectations_by_subtest.get(subtest, {}).get('slow', False)

        def get_expectation(self, test, subtest):
            return expectations_by_subtest.get(subtest, {}).get('status', ['PASS'])

    # Load only the plugins these tests need, not whatever happens to be installed.
    environment = dict(os.environ, PYTEST_DISABLE_PLUGIN_AUTOLOAD='1')

    def run_file(path, deselected, report):
        return pytest_runner.run(path, ['--strict-markers', '-p', 'pytest_asyncio.plugin'], timeout, environment, Expectations(), report=report, deselected=deselected)[0]

    restarts = []
    test_process = TestProcess(run_file, timeout, restart=lambda: restarts.append(True))
    test_process.start()
    harness_result, subtest_results = test_process.run(sys.argv[2])
    test_process.stop()
    with open(sys.argv[3], 'w') as f:
        json.dump({'harness': list(harness_result), 'restarts': len(restarts),
                   'subtests': [(name.split('::')[-1], status) for name, status, _, _ in subtest_results]}, f)
''')

# Shared by the test files. The socket pair never gets any data, so reading from it blocks like a
# request to a driver that doesn't answer.
PRELUDE = textwrap.dedent('''
    import asyncio
    import socket

    import pytest
    import pytest_asyncio

    _never_answers, _other_end = socket.socketpair()


    def block():
        _never_answers.recv(1)
''')

TEST_TIMEOUT_SECONDS = 1
# Generous, because test-webkitpy runs many tests in parallel.
HANG_SECONDS = 60


class PytestRunnerTimeoutTest(unittest.TestCase):

    @staticmethod
    def end_of(output, line_count=60):
        return '\n'.join((output or b'').decode('utf-8', 'replace').splitlines()[-line_count:])

    def run_tests(self, source, expectations=None, harness='OK'):
        scripts_directory = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        with tempfile.TemporaryDirectory() as directory:
            test_path = os.path.join(directory, 'test_timeouts.py')
            result_path = os.path.join(directory, 'result.json')
            with open(test_path, 'w') as f:
                f.write(PRELUDE + textwrap.dedent(source))
            try:
                process = subprocess.run([sys.executable, '-c', RUNNER, scripts_directory, test_path, result_path, str(TEST_TIMEOUT_SECONDS), json.dumps(expectations or {})],
                                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=HANG_SECONDS)
                output = process.stdout
            except subprocess.TimeoutExpired as error:
                self.fail('The test run hung for %d seconds. Its output ended with:\n%s' % (HANG_SECONDS, self.end_of(error.output)))
            if not os.path.exists(result_path):
                self.fail('The test run exited with status %d, without results. Its output ended with:\n%s' % (process.returncode, self.end_of(output)))
            with open(result_path) as f:
                result = json.load(f)
        self.assertEqual(result['harness'][0], harness, 'Harness result %s. The output of the test run ended with:\n%s' % (result['harness'], self.end_of(output)))
        self.restarts = result['restarts']
        # The last result of each subtest: a subtest that passes and then fails in teardown has two.
        return dict(result['subtests'])

    def test_sync_test_blocks(self):
        self.assertEqual(self.run_tests('''
            def test_blocks():
                block()
        '''), {'test_blocks': 'TIMEOUT'})

    def test_async_test_waits_forever(self):
        self.assertEqual(self.run_tests('''
            @pytest.mark.asyncio
            async def test_waits_forever():
                await asyncio.get_running_loop().create_future()
        '''), {'test_waits_forever': 'TIMEOUT'})

    def test_async_test_waits_forever_while_the_loop_is_busy(self):
        # When the timeout fires inside a callback that the loop runs, such as one handling WebSocket
        # data, asyncio catches the exception and only logs it, and the test never ends (bug 322171).
        self.assertEqual(self.run_tests('''
            def keep_busy():
                total = 0
                for value in range(20000):
                    total += value
                asyncio.get_running_loop().call_soon(keep_busy)


            @pytest.mark.asyncio
            @pytest.mark.parametrize('attempt', range(5))
            async def test_waits_forever(attempt):
                asyncio.get_running_loop().call_soon(keep_busy)
                await asyncio.get_running_loop().create_future()
        '''), {'test_waits_forever[%d]' % attempt: 'TIMEOUT' for attempt in range(5)})

    def test_async_fixture_setup_blocks(self):
        # Like the session fixture, when a driver doesn't answer New Session.
        self.assertEqual(self.run_tests('''
            @pytest_asyncio.fixture
            async def session():
                block()
                yield


            def test_uses_session(session):
                pass
        '''), {'test_uses_session': 'ERROR'})

    def test_async_fixture_teardown_blocks(self):
        # Like the session fixture's cleanup, when a driver doesn't answer.
        self.assertEqual(self.run_tests('''
            @pytest_asyncio.fixture
            async def session():
                yield
                block()


            def test_uses_session(session):
                pass


            def test_after():
                pass
        '''), {'test_uses_session': 'ERROR', 'test_after': 'PASS'})

    def test_teardown_blocks_after_the_test_timed_out(self):
        # A test leaves a dialog open and times out, and cleaning up the session would hang too.
        self.assertEqual(self.run_tests('''
            @pytest_asyncio.fixture
            async def session():
                yield
                block()


            def test_uses_session(session):
                block()


            def test_after():
                pass
        '''), {'test_uses_session': 'TIMEOUT', 'test_after': 'PASS'})

    def test_resumes_after_a_timeout(self):
        self.assertEqual(self.run_tests('''
            def test_before():
                pass


            def test_hangs():
                block()


            def test_hangs_too():
                block()


            def test_after():
                pass
        '''), {'test_before': 'PASS', 'test_hangs': 'TIMEOUT', 'test_hangs_too': 'TIMEOUT', 'test_after': 'PASS'})
        self.assertEqual(self.restarts, 2)

    def test_expected_timeout(self):
        self.assertEqual(self.run_tests('''
            def test_hangs():
                block()


            def test_expected_to_fail_hangs():
                block()
        ''', expectations={'test_hangs': {'status': ['TIMEOUT']}, 'test_expected_to_fail_hangs': {'status': ['FAIL']}}),
            {'test_hangs': 'XFAIL', 'test_expected_to_fail_hangs': 'TIMEOUT'})

    def test_slow_test(self):
        self.assertEqual(self.run_tests('''
            import time


            def test_slow():
                time.sleep(%s)
        ''' % (TEST_TIMEOUT_SECONDS * 2), expectations={'test_slow': {'slow': True}}), {'test_slow': 'PASS'})

    def test_hang_outside_of_any_subtest(self):
        self.assertEqual(self.run_tests('''
            block()


            def test_never_runs():
                pass
        ''', harness='ERROR'), {})

    def test_test_process_exits(self):
        self.assertEqual(self.run_tests('''
            import os


            def test_exits():
                os._exit(3)


            def test_after():
                pass
        '''), {'test_exits': 'ERROR', 'test_after': 'PASS'})

    def test_own_timeout_in_async_test(self):
        # Some tests expect a wait to time out. That isn't a test timeout.
        self.assertEqual(self.run_tests('''
            @pytest.mark.asyncio
            async def test_expects_timeout():
                with pytest.raises(asyncio.TimeoutError):
                    await asyncio.wait_for(asyncio.get_running_loop().create_future(), timeout=0.1)
        '''), {'test_expects_timeout': 'PASS'})

    def test_fast_tests(self):
        self.assertEqual(self.run_tests('''
            @pytest_asyncio.fixture
            async def session():
                yield


            def test_sync(session):
                pass


            @pytest.mark.asyncio
            async def test_async(session):
                await asyncio.sleep(0)


            def test_fails(session):
                assert False
        '''), {'test_sync': 'PASS', 'test_async': 'PASS', 'test_fails': 'FAIL'})
