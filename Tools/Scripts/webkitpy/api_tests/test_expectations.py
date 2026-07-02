# Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

from webkitexpectationspy import (
    ExpectationsManager, ResultStatus,
)
from webkitexpectationspy.suites.api_tests import APITestSuite

_log = logging.getLogger(__name__)

PASS = ResultStatus.PASS
FAIL = ResultStatus.FAIL
CRASH = ResultStatus.CRASH
TIMEOUT = ResultStatus.TIMEOUT

_RUNNER_STATUS_MAP = {
    0: PASS,
    1: FAIL,
    2: CRASH,
    3: TIMEOUT,
}


def runner_status_to_expectation(runner_status):
    return _RUNNER_STATUS_MAP.get(runner_status)


class APITestExpectations:
    def __init__(self, port, tests=None):
        self._port = port
        self._tests = tests
        self._suite = APITestSuite()
        self._manager = ExpectationsManager(suite=self._suite)

    def parse_all_expectations(self):
        for filepath in self._port.api_test_expectations_files():
            if self._port.host.filesystem.exists(filepath):
                _log.debug('Loading expectations from {}'.format(filepath))
                content = self._port.host.filesystem.read_text_file(filepath)
                warnings = self._manager.load_content(filepath, content)
                for w in warnings:
                    _log.warning(str(w))
            else:
                _log.debug('Expectations file not found (skipping): {}'.format(filepath))

    def parse_additional_file(self, filepath):
        if self._port.host.filesystem.exists(filepath):
            content = self._port.host.filesystem.read_text_file(filepath)
            return self._manager.load_content(filepath, content)
        _log.warning('Additional expectations file not found: {}'.format(filepath))
        return []

    def model(self):
        return self._manager

    def get_current_configuration(self):
        config = set()
        port_config = self._port.api_test_current_configuration()

        for key in ('platform', 'version', 'style', 'architecture', 'hardware', 'hardware_type'):
            value = port_config.get(key)
            if value:
                config.add(value.lower())

        return config

    def get_expectation(self, test_name, config=None):
        if config is None:
            config = self.get_current_configuration()
        version_order = self._port.api_test_version_order() if hasattr(self._port, 'api_test_version_order') else []
        current_version = None
        port_config = self._port.api_test_current_configuration()
        if port_config.get('version'):
            current_version = port_config['version'].lower()
        return self._manager.get_expectation(
            test_name,
            current_config=config,
            current_version=current_version,
            version_order=version_order,
        )

    def lint(self, all_tests=None):
        return self._manager.lint(all_tests=all_tests)
