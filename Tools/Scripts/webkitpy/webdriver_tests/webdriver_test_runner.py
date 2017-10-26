# Copyright (C) 2017 Igalia S.L.
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

import logging

from webkitpy.webdriver_tests.webdriver_test_runner_w3c import WebDriverTestRunnerW3C

_log = logging.getLogger(__name__)


class WebDriverTestRunner(object):

    def __init__(self, port):
        self._port = port
        _log.info('Using port %s' % self._port.name())
        _log.info('Test configuration: %s' % self._port.test_configuration())
        _log.info('Using display server %s' % (self._port._display_server))

        self._display_driver = self._port.create_driver(worker_number=0, no_timeout=True)._make_driver(pixel_tests=False)
        if not self._display_driver.check_driver(self._port):
            raise RuntimeError("Failed to check driver %s" % self._display_driver.__class__.__name__)
        self._runner = WebDriverTestRunnerW3C(self._port, self._display_driver)

    def run(self, tests=[]):
        runner_tests = self._runner.collect_tests(tests)
        if runner_tests:
            _log.info('Collected %d tests' % len(runner_tests))
            return self._runner.run(runner_tests)

        _log.info('No tests found')
        return 0

    def print_results(self):
        results = {}
        passed_count = 0
        failures_count = 0
        for result in self._runner.results():
            if result.status == 'PASS':
                passed_count += 1
            elif result.status == 'FAIL':
                results.setdefault(result.status, []).append(result.test)
                failures_count += 1

        _log.info('')

        if not results:
            _log.info('All tests run as expected')
            return

        _log.info('%d tests ran as expected, %d didn\'t\n' % (passed_count, failures_count))

        if 'FAIL' in results:
            failed = results['FAIL']
            _log.info('Unexpected failures (%d)' % len(failed))
            for test in failed:
                _log.info('  %s' % test)
