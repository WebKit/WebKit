#!/usr/bin/env python3

# Copyright (C) 2022 Apple Inc. All rights reserved.
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
import requests
import sys


class ResultsDatabase(object):
    HOSTNAME = 'https://results.webkit.org'
    # TODO: Support more suites (Note, the API we're talking to already does)
    SUITE = 'layout-tests'
    PERCENT_THRESHOLD = 10
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
    def get_results_summary(cls, test, commit=None, configuration=None):
        params = dict()
        for key, value in configuration.items():
            if key not in cls.CONFIGURATION_KEYS:
                sys.stderr.write(f"'{key}' is not a valid configuration key\n")
            params[key] = value
        if commit:
            params['ref'] = commit
        response = requests.get(f'{cls.HOSTNAME}/api/results-summary/{cls.SUITE}/{test}', params=params)
        if response.status_code != 200:
            sys.stderr.write(f"Failed to query results summary with status code '{response.status_code}'\n")
            return None
        try:
            return response.json()
        except json.decoder.JSONDecodeError:
            sys.stderr.write('Non-json response from results summary query\n')
            return None

    @classmethod
    def is_test_expected_to(cls, test, result_type=None, commit=None, configuration=None, log=False):
        data = cls.get_results_summary(test, commit=commit, configuration=configuration)
        if log:
            print(test)
            for key, value in (data or dict()).items():
                if not value:
                    continue
                print(f'    {key}: {value}%')
            if not data:
                print('    No historic data found for query')
        if not data:
            return -1
        if result_type:
            return data.get(result_type.lower(), 0) > cls.PERCENT_THRESHOLD
        return 100 - (data.get('pass', 0) + data.get('warning', 0)) > cls.PERCENT_THRESHOLD

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

        if cls.is_test_expected_to(parsed.test, result_type=parsed.result, commit=parsed.commit, log=True, configuration=configuration):
            print('EXPECTED')
        else:
            print('UNEXPECTED')
        return 0


if __name__ == '__main__':
    sys.exit(ResultsDatabase.main())
