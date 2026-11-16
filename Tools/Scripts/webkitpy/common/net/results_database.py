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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import urllib.parse

import requests

RESULTS_DB_URL = 'https://results.webkit.org'

_session = requests.Session()


def get_results_summary(test, suite='layout-tests', timeout=10):
    url = '{}/api/results-summary/{}/{}'.format(
        RESULTS_DB_URL,
        urllib.parse.quote(suite, safe=''),
        urllib.parse.quote(test, safe=''),
    )
    try:
        response = _session.get(url, timeout=timeout)
        response.raise_for_status()
        return response.json()
    except Exception:
        return {}


def is_pre_existing_failure(test, suite='layout-tests', threshold=80):
    # Threshold of 80 matches ResultsDatabase.PERCENT_SUCCESS_RATE_FOR_PRE_EXISTING_FAILURE used by EWS bots.
    data = get_results_summary(test, suite=suite)
    pass_rate = data.get('pass', 100) + data.get('warning', 0)
    return pass_rate < threshold


def check_pre_existing_failures(tests, suite, limit, writeln):
    tests = list(tests)
    checked = tests if not limit else tests[:limit]
    skipped_count = len(tests) - len(checked)

    writeln('')
    writeln(f'Checking results.webkit.org for pre-existing failures ({len(checked)} of {len(tests)}) ...')

    pre_existing = []
    possibly_new = []
    for test in checked:
        if is_pre_existing_failure(test, suite=suite):
            pre_existing.append(test)
        else:
            possibly_new.append(test)

    if pre_existing:
        writeln(f'Pre-existing on CI ({len(pre_existing)}):')
        for t in pre_existing:
            writeln(f'  [pre-existing] {t}')
    if possibly_new:
        writeln(f'Possibly new ({len(possibly_new)}):')
        for t in possibly_new:
            writeln(f'  [new?] {t}')
    if skipped_count:
        writeln(f'({skipped_count} further failures not checked; increase --max-pre-existing-checks to check all)')
