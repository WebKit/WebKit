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

"""The landed commit behind each convicted pull request.

Escape detection asks what a test did on main after a change landed, so it needs a point on main to
ask about. Nothing a build reports says where its change ended up; `webkit_checkout` works that out
from the pull request's title, and this module decides which pull requests are worth asking about,
remembers the answers, and re-asks the ones that had not landed yet.
"""

from __future__ import annotations

import sqlite3
import time
from collections import Counter
from dataclasses import dataclass
from typing import Optional

from ews_dashboard import webkit_checkout


@dataclass(frozen=True)
class Unresolved:
    """A pull request to look for, and the earliest build of it, which bounds the search."""

    pr_id: int
    title: str
    first_built_at: int


def convicted_pull_requests(connection: sqlite3.Connection, since: int,
                            until: int) -> 'list[Unresolved]':
    """Pull requests a flakiness rule convicted a test on, oldest build first.

    A conviction is what makes a landing worth looking for: it is the claim that the failure was
    nothing to do with the change, and the landed commit is the only place that claim can be wrong.
    """
    return [
        Unresolved(pr_id=row['pr_id'], title=row['pr_title'], first_built_at=row['first_built_at'])
        for row in connection.execute(
            '''SELECT build.pr_id, build.pr_title, MIN(build.started_at) AS first_built_at
               FROM latest_flakiness_verdicts AS verdict
               JOIN build_verdicts AS build USING (build_id)
               WHERE verdict.rule IS NOT NULL
                 AND build.pr_id IS NOT NULL AND build.pr_title IS NOT NULL
                 AND build.started_at >= :since AND build.started_at < :until
               GROUP BY build.pr_id
               ORDER BY first_built_at''',
            {'since': since, 'until': until},
        )
    ]


def landing_of(connection: sqlite3.Connection, pr_id: int) -> Optional[sqlite3.Row]:
    return connection.execute('SELECT * FROM landings WHERE pr_id = ?', (pr_id,)).fetchone()


def _needs_asking(cached: Optional[sqlite3.Row], branch_head: str) -> bool:
    if cached is None:
        return True
    # A landed or ambiguous answer cannot change: the commits it found stay in main's history.
    return cached['status'] == webkit_checkout.NOT_LANDED and cached['branch_head'] != branch_head


def _record(connection: sqlite3.Connection, pr_id: int,
            resolution: webkit_checkout.Resolution, branch_head: str) -> None:
    commit = resolution.commit
    with connection:
        connection.execute(
            '''INSERT OR REPLACE INTO landings (
                pr_id, status, matches, commit_hash, identifier, landed_at, subject,
                branch_head, resolved_at
            ) VALUES (?,?,?,?,?,?,?,?,?)''',
            (
                pr_id, resolution.status, resolution.matches,
                commit.commit_hash if commit else None,
                commit.identifier if commit else None,
                commit.landed_at if commit else None,
                commit.subject if commit else None,
                branch_head, int(time.time()),
            ),
        )


def resolve(connection: sqlite3.Connection, checkout: webkit_checkout.Checkout,
            since: int, until: int) -> dict:
    """Look for a landing for every convicted pull request in the window, and store what was found.

    Returns a count per outcome, `skipped` being the pull requests whose stored answer still holds.
    Raises CheckoutUnavailable before searching anything if the checkout cannot be read, because a
    window resolved against a checkout that is not there would record thousands of false negatives.
    """
    branch_head = checkout.head()
    outcomes: Counter = Counter()
    for pull_request in convicted_pull_requests(connection, since, until):
        cached = landing_of(connection, pull_request.pr_id)
        if not _needs_asking(cached, branch_head):
            outcomes['skipped'] += 1
            continue
        resolution = checkout.landing_of(pull_request.title,
                                         not_before=pull_request.first_built_at)
        _record(connection, pull_request.pr_id, resolution, branch_head)
        outcomes[resolution.status] += 1
    return dict(outcomes)
