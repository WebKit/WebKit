#!/usr/bin/env python3

# Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

import argparse
import json
import os
import sys
import twisted
import re
import urllib.parse

from twisted_additions import TwistedAdditions
from twisted.internet import defer, reactor


class ResultsDatabase(object):
    HOSTNAME = 'https://results.webkit.org'
    # TODO: Support more suites (Note, the API we're talking to already does)
    DEFAULT_SUITE = 'layout-tests'
    SUITES = ('layout-tests', 'api-tests')
    PERCENT_THRESHOLD = 10
    PERECENT_SUCCESS_RATE_FOR_PRE_EXISTING_FAILURE = 80
    CONFIGURATION_KEYS = [
        'architecture',
        'platform',
        'is_simulator',
        'version',
        'flavor',
        'style',
        'model',
        'version_name',
        'sdk',
    ]

    @classmethod
    @defer.inlineCallbacks
    def get_results_summary(cls, test, commit=None, configuration=None, logger=None, suite=None):
        logger = logger or (lambda log: None)
        params = dict()
        suite = suite or cls.DEFAULT_SUITE
        if not test:
            logger('Test name not provided\n')
            defer.returnValue({})
            return
        if suite not in cls.SUITES:
            logger(f"'{suite}' is not a valid suite name\n")
            defer.returnValue({})
            return
        if not configuration:
            configuration = {}
        for key, value in configuration.items():
            if key not in cls.CONFIGURATION_KEYS:
                logger(f"'{key}' is not a valid configuration key\n")
            params[key] = value
        if commit:
            params['ref'] = commit

        response = yield TwistedAdditions.request(f'{cls.HOSTNAME}/api/results-summary/{urllib.parse.quote(suite)}/{urllib.parse.quote(test)}', params=params, logger=logger)

        if not response:
            logger(f'No response from {cls.HOSTNAME}\n')
        elif response.status_code == 200:
            defer.returnValue(response.json())
            return
        else:
            logger(f'Failed to query results summary with status code {response.status_code}\n')

        defer.returnValue({})

    @classmethod
    @defer.inlineCallbacks
    def is_test_pre_existing_failure(cls, test, commit=None, configuration=None, suite=None):
        logs = []
        data = yield cls.get_results_summary(test, commit, configuration, logger=lambda log: logs.append(log), suite=suite)
        pass_rate = data.get('pass', 100) + data.get('warning', 0)
        is_existing_failure = (pass_rate <= cls.PERECENT_SUCCESS_RATE_FOR_PRE_EXISTING_FAILURE)
        output = {
            'is_existing_failure': is_existing_failure,
            'pass_rate': data.get('pass', 'Unknown'),
            'raw_data': data,
            'logs': ''.join(logs),
        }
        defer.returnValue(output)

    @classmethod
    @defer.inlineCallbacks
    def is_test_expected_to(cls, test, result_type=None, commit=None, configuration=None, logger=None, suite=None):
        logger = logger or (lambda log: None)
        has_commit = False
        if commit:
            has_commit = yield cls.has_commit(commit=commit)
            if not has_commit:
                logger(f"'{commit}' is not registered on '{cls.HOSTNAME}'\n")

        data = yield cls.get_results_summary(
            test, configuration=configuration, logger=logger,
            commit=commit if has_commit else None,
            suite=suite,
        )
        logger(f'{test}\n')
        for key, value in (data or dict()).items():
            if not value:
                continue
            logger(f'    {key}: {value}%\n')
        if not data:
            logger('    No historic data found for query\n')
        if not data:
            return defer.returnValue(-1)
        if result_type:
            return defer.returnValue(data.get(result_type.lower(), 0) > cls.PERCENT_THRESHOLD)
        return defer.returnValue(100 - (data.get('pass', 0) + data.get('warning', 0)) > cls.PERCENT_THRESHOLD)

    @classmethod
    @defer.inlineCallbacks
    def has_commit(cls, commit, logger=None):
        response = yield TwistedAdditions.request(f'{cls.HOSTNAME}/api/commits', params=dict(ref=commit), logger=logger)
        if not response or response.status_code != 200:
            defer.returnValue(False)
        else:
            defer.returnValue(True)

    @classmethod
    def main(cls, args=None):
        parser = argparse.ArgumentParser(
            description='A script which uses the same logic ews-build.webkit.org does to determine if a given ' +
                        'test in a given configuration on a specific commit is currently expected to fail.',
        )
        parser.add_argument(
            'test', type=str,
            help='The test to return results for.',
        )
        parser.add_argument(
            '-c', '--commit',
            type=str, default=None,
            help='Commit ref to focus on',
        )
        parser.add_argument(
            '--suite',
            type=str, default=None,
            help=f'Suite test is found in ({cls.DEFAULT_SUITE} by default)',
        )
        parser.add_argument(
            '-r', '--result',
            type=str, default=None,
            help='Result to filter for (failure, crash, timeout, ect.)',
        )
        parser.add_argument(
            '-s', '--style',
            type=str, default=None,
            help='Build style configuration to focus on (debug, release, ect.)',
        )
        parser.add_argument(
            '-p', '--platform',
            type=str, default=None,
            help='Platform to focus on (mac, ios, gtk, ect.)',
        )
        parser.add_argument(
            '-f', '--flavor',
            type=str, default=None,
            help='Flavor to focus on (wk1, wk2, ect.)',
        )
        parser.add_argument(
            '-v', '--version',
            type=str, default=None,
            help='OS version focus on (Ventura, Monterey, ect.)',
        )
        parser.add_argument(
            '-a', '--architecture',
            type=str, default=None,
            help='Architecture focus on (arm64, x86_64, ect.)',
        )
        parsed = parser.parse_args(args)

        configuration = dict()
        for key in cls.CONFIGURATION_KEYS:
            attr = getattr(parsed, key, None)
            if attr:
                configuration[key] = attr

        d = cls.is_test_expected_to(
            parsed.test,
            result_type=parsed.result, commit=parsed.commit,
            logger=sys.stdout.write, configuration=configuration,
            suite=getattr(parsed, 'suite', None)
        )

        def callback(result):
            if result:
                print('EXPECTED')
            else:
                print('UNEXPECTED')

        d.addCallback(callback)
        d.addBoth(lambda _: reactor.stop())

        return reactor.run()


if __name__ == '__main__':
    sys.exit(ResultsDatabase.main())
