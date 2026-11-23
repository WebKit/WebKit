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

"""Read-only client for EWS's Buildbot v2 API."""

from __future__ import annotations

import http.client
import json
import time
import urllib.error
import urllib.parse
import urllib.request
from typing import Iterator, Optional

from ews_dashboard import config

# Measured against ews-build.webkit.org with property=*: 30 builds in 0.3s, 200 in 1.3s, so a larger
# page is strictly cheaper per build. 100 leaves headroom under the truncation this server has shown
# on large responses, which `builds` treats as an error rather than as the end of the walk.
PAGE_LIMIT = 100

HTTP_TIMEOUT_SECONDS = 30
RETRY_ATTEMPTS = 4
RETRY_BACKOFF_SECONDS = 1.5

# A dropped read is transient here, not a failure: the v2 API truncates large responses and resets
# connections under load. Named by base class rather than one entry per failure, because every such
# enumeration has come up an exception short — `socket.timeout` is an OSError that is not a
# TimeoutError on Python 3.9, and `ssl.SSLError` is neither. Everything urlopen raises is an OSError
# or an HTTPException; a truncated body that parses as nothing is the JSONDecodeError.
TRANSIENT_ERRORS = (
    OSError,
    http.client.HTTPException,
    json.JSONDecodeError,
)

# Sleeping through the backoff would not change a rejected request. 429 is excluded because waiting
# is exactly what it asks for, and HTTPError subclasses URLError, so without this every mistyped
# path costs the full retry budget before raising.
PERMANENT_STATUS = frozenset(range(400, 500)) - {429}


class BuildbotClient:
    def __init__(self, base_url: str = config.BUILDBOT_URL,
                 timeout: int = HTTP_TIMEOUT_SECONDS) -> None:
        self.base_url = base_url.rstrip('/')
        self.timeout = timeout

    def _get(self, path: str) -> dict:
        url = f'{self.base_url}{path}'
        last_error: Optional[Exception] = None
        for attempt in range(RETRY_ATTEMPTS):
            try:
                request = urllib.request.Request(url, headers={'Accept': 'application/json'})
                with urllib.request.urlopen(request, timeout=self.timeout) as response:
                    return json.loads(response.read())
            except urllib.error.HTTPError as error:
                if error.code in PERMANENT_STATUS:
                    raise
                last_error = error
                if attempt + 1 < RETRY_ATTEMPTS:
                    time.sleep(RETRY_BACKOFF_SECONDS * (attempt + 1))
            except TRANSIENT_ERRORS as error:
                last_error = error
                if attempt + 1 < RETRY_ATTEMPTS:
                    time.sleep(RETRY_BACKOFF_SECONDS * (attempt + 1))
        raise last_error

    def builders(self) -> list:
        return [
            {'name': builder['name'], 'id': builder['builderid']}
            for builder in self._get('/api/v2/builders').get('builders', [])
        ]

    def builder_id(self, name: str) -> int:
        for builder in self.builders():
            if builder['name'] == name:
                return builder['id']
        raise ValueError(f'no such builder: {name}')

    def builds(
        self,
        builder_id: int,
        since: Optional[int] = None,
        limit: Optional[int] = None,
    ) -> Iterator[dict]:
        """Walk one builder's completed builds, newest first.

        Builds are reachable per builder only through this path; builderid= on /api/v2/builds is a
        400. Pages are anchored to the previous page's lowest build number rather than a row
        offset, because a builder completing new builds during the walk shifts an offset-based
        listing down and silently skips builds.

        Only an empty page ends the walk. A page shorter than the limit used to end it too, which
        would read a truncated response as the end of a builder's history.
        """
        number_cursor: Optional[int] = None
        yielded = 0
        while True:
            parameters = {
                'complete': 'true',
                'order': '-number',
                'limit': PAGE_LIMIT,
                'property': '*',
            }
            if number_cursor is not None:
                parameters['number__lt'] = number_cursor
            query = urllib.parse.urlencode(parameters)
            page = self._get(f'/api/v2/builders/{builder_id}/builds?{query}').get('builds', [])
            if not page:
                return
            for build in page:
                if since is not None and (build.get('started_at') or build.get('complete_at') or 0) < since:
                    return
                yield build
                yielded += 1
                if limit is not None and yielded >= limit:
                    return
            number_cursor = page[-1]['number']

    def steps(self, build_id: int) -> list:
        return self._get(f'/api/v2/builds/{build_id}/steps?limit=200').get('steps', [])

    def log_text(self, step_id: int, log_name: str) -> Optional[str]:
        """The full text of one named log, or None when the step has no such log.

        Buildbot prefixes an o/e/h stream marker onto every line of a type 's' log.
        AnalyzeCompileWebKitResults.getResults in steps.py strips it the same way.
        """
        logs = self._get(f'/api/v2/steps/{step_id}/logs').get('logs', [])
        log = next((candidate for candidate in logs if candidate['name'] == log_name), None)
        if log is None:
            return None
        chunks = self._get(f"/api/v2/logs/{log['logid']}/contents").get('logchunks', [])
        text = ''.join(chunk['content'] for chunk in chunks)
        if log.get('type') == 's':
            return ''.join(line[1:] for line in text.splitlines())
        return text


def property_value(properties: dict, key: str, default: object = None) -> object:
    """One build property's value.

    Buildbot reports each property as a [value, source] pair, so the value has to be unwrapped.
    """
    raw = properties.get(key)
    if raw is None:
        return default
    if isinstance(raw, list):
        return raw[0]
    return raw
