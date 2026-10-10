# Copyright (C) 2026 Apple Inc. All rights reserved.
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

"""Links out to the sites that already own this data.

The organizing rule of this dashboard is that it renders only the join between a conviction and the
rule behind it. Test history belongs to results.webkit.org and build detail belongs to EWS, so both
are linked, never re-rendered.
"""

from __future__ import annotations

import urllib.parse
from typing import Optional

from ews_dashboard import config, results

WEBKIT_PULL_REQUEST_URL = 'https://github.com/WebKit/WebKit/pull'


def _test_parameters(configuration: results.Configuration, test_name: str) -> dict:
    return {'suite': configuration.suite, 'test': test_name}


def test_history(configuration: results.Configuration, test_name: str) -> str:
    """Where a test started flaking, on results.webkit.org's own timeline.

    `test_investigation` builds the same `RESULTS_URL` query, just unfiltered by configuration.
    """
    parameters = _test_parameters(configuration, test_name)
    parameters.update(configuration.query_parameters())
    return f'{config.RESULTS_URL}/?{urllib.parse.urlencode(parameters)}'


def test_investigation(configuration: results.Configuration, test_name: str) -> str:
    """The same timeline unfiltered by configuration, so every configuration of the test shows."""
    query = urllib.parse.urlencode(_test_parameters(configuration, test_name))
    return f'{config.RESULTS_URL}/?{query}'


def build(builder_id: int, build_number: int) -> str:
    return f'{config.BUILDBOT_URL}/#/builders/{builder_id}/builds/{build_number}'


def pull_request(pr_id: Optional[int]) -> Optional[str]:
    if pr_id is None:
        return None
    return f'{WEBKIT_PULL_REQUEST_URL}/{pr_id}'
