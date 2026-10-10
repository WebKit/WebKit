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

"""How old the numbers on a page are.

Not optional: this dashboard's worst failure mode is looking fine while serving stale numbers, which
the prototype did for twelve days with no visible symptom.
"""

from __future__ import annotations

import sqlite3
import time
from dataclasses import dataclass
from typing import Optional

REFRESH_STALE_AFTER_SECONDS = 6 * 3600
CLASSIFICATION_LAG_STALE_AFTER_SECONDS = 24 * 3600

NEVER_REFRESHED = 'never_refreshed'
REFRESH_FAILED = 'refresh_failed'
REFRESH_OVERDUE = 'refresh_overdue'
CLASSIFICATION_BEHIND = 'classification_behind'


@dataclass(frozen=True)
class Freshness:
    refreshed_at: Optional[int]
    newest_build_at: Optional[int]
    classified_at: Optional[int]
    # What killed the most recent attempt, whether or not an older run finished.
    failure: Optional[str] = None

    @property
    def refresh_age_seconds(self) -> Optional[int]:
        if self.refreshed_at is None:
            return None
        return int(time.time()) - self.refreshed_at

    @property
    def classification_lag_seconds(self) -> Optional[int]:
        if self.refreshed_at is None or self.classified_at is None:
            return None
        return max(0, self.refreshed_at - self.classified_at)

    @property
    def reason(self) -> Optional[str]:
        """Why the numbers should be read with suspicion, or None when they should not.

        The page says which of these it is: a refresh that died and a classification pass that fell
        behind need different things done about them, and blaming the interval for either sends
        whoever reads it to the wrong place.
        """
        if self.refreshed_at is None:
            return NEVER_REFRESHED
        if self.failure is not None:
            return REFRESH_FAILED
        age = self.refresh_age_seconds
        if age is not None and age > REFRESH_STALE_AFTER_SECONDS:
            return REFRESH_OVERDUE
        lag = self.classification_lag_seconds
        if lag is not None and lag > CLASSIFICATION_LAG_STALE_AFTER_SECONDS:
            return CLASSIFICATION_BEHIND
        return None

    @property
    def stale(self) -> bool:
        """Whether anything on the page should be read with suspicion.

        Never having refreshed counts as stale; an empty database is not a fresh one.
        """
        return self.reason is not None

    @property
    def signature(self) -> str:
        """What a dismissal of the banner is remembered against.

        A dismissal must not outlive what it dismissed: hiding "refreshed an hour ago" cannot go on
        to hide "the last refresh died", so those two carry different signatures.
        """
        return f'{self.refreshed_at or 0}:{self.reason or "current"}'


def _latest(connection: sqlite3.Connection, sql: str) -> Optional[int]:
    row = connection.execute(sql).fetchone()
    return None if row is None else row[0]


def _latest_failure(connection: sqlite3.Connection) -> Optional[str]:
    """The error from the newest run, but only while no later run has succeeded."""
    row = connection.execute(
        'SELECT error, finished_at FROM refresh_runs ORDER BY started_at DESC LIMIT 1',
    ).fetchone()
    if row is None or row['finished_at'] is not None:
        return None
    return row['error']


def current(connection: sqlite3.Connection) -> Freshness:
    return Freshness(
        refreshed_at=_latest(
            connection,
            'SELECT MAX(finished_at) FROM refresh_runs WHERE finished_at IS NOT NULL',
        ),
        newest_build_at=_latest(connection, 'SELECT MAX(started_at) FROM build_verdicts'),
        classified_at=_latest(connection, 'SELECT MAX(classified_at) FROM build_classifications'),
        failure=_latest_failure(connection),
    )
