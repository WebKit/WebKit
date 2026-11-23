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

"""Values shared by ingest, analysis and the web layer.

Nothing here reaches the network or the filesystem at import time.
"""

from __future__ import annotations

import datetime
import os
from dataclasses import dataclass
from typing import Optional

BUILDBOT_URL = 'https://ews-build.webkit.org'
RESULTS_URL = 'https://results.webkit.org'

DATABASE_PATH_VARIABLE = 'EWS_DASHBOARD_DATABASE'
DEFAULT_DATABASE_NAME = 'ews-dashboard.db'

CHECKOUT_PATH_VARIABLE = 'EWS_DASHBOARD_CHECKOUT'

# A test passing this often or less on main already fails without the change, so a failure here is
# not the author's doing.
PRE_EXISTING_THRESHOLD_PCT = 80

# How long after a change lands main is watched for the test a build called flaky, and how long
# before it lands is read as the baseline. Three days matches the window EWS's own rules use, so a
# test convicted for flaking over three days is checked over the same span.
ESCAPE_WINDOW_DAYS = 3

# What share of a test's post-landing runs on main must fail unexpectedly for an escape to count as a
# strong one. Whether anything escaped is decided by the baseline instead — main never failed the
# test before the landing — and below this share the escape stands on fewer failures.
ESCAPE_FAILURE_PCT = 50

# How far back from the moment of the check main is asked whether it is still failing an escaped
# test. A fresh window ending now, not the window either side of the landing: whether the regression
# is still there is a different question from how the conviction was graded, and no window fixed to
# the landing can answer it however wide it is.
CURRENCY_DAYS = 7

# How long that answer stands before the escape is asked about again. Only the escapes are ever
# asked, so this costs a query per escape per day rather than one per conviction.
CURRENCY_TTL_SECONDS = 24 * 3600

# Above this many author-visible failures the build is a crash storm, not a set of test results;
# classifying it test-by-test would cost hundreds of lookups to describe a broken checkout.
MAX_CLASSIFIABLE_SURFACED_TESTS = 60

CLEAN_TREE = 'CleanTree'
DIRTY_TREE = 'DirtyTree'
BETWEEN_BUILDS = 'BetweenBuilds'

# Display order, not alphabetical: CleanTree is the only rule that convicts on a single row.
FLAKINESS_RULES = (CLEAN_TREE, DIRTY_TREE, BETWEEN_BUILDS)

# Thresholds are ResultsDatabase's in results_db.py: DirtyTree wants 2 pull requests and 2 authors.
# BetweenBuilds wants 3 pull requests and 2 authors. Both windows are FLAKY_WINDOW_SECONDS, 3 days.
RULE_DESCRIPTIONS = {
    CLEAN_TREE: 'Recorded flaky with the change reverted, so the change cannot be the cause. One '
                'such row convicts: this rule has no pull request or author threshold.',
    DIRTY_TREE: 'Recorded flaky with a change applied, by at least 2 pull requests and at least 2 '
                'authors within 3 days.',
    BETWEEN_BUILDS: 'Recorded failing across at least 3 pull requests and at least 2 authors '
                    'within 3 days.',
}


@dataclass(frozen=True)
class Deployment:
    """A production change to EWS whose effect the trend chart has to be read against."""

    at: datetime.datetime
    label: str
    detail: str


DEPLOYMENTS = (
    Deployment(
        at=datetime.datetime(2026, 8, 14, tzinfo=datetime.timezone.utc),
        label='write',
        detail='EWS begins recording flaky and failed tests to results.webkit.org.',
    ),
    Deployment(
        at=datetime.datetime(2026, 8, 25, 11, 18, 23, tzinfo=datetime.timezone.utc),
        label='read',
        detail='First build carrying a results-db flakiness property, so the read path is live.',
    ),
    Deployment(
        at=datetime.datetime(2026, 8, 31, 10, 52, 37, tzinfo=datetime.timezone.utc),
        label='act',
        detail='First build to ignore a flaky failure rather than log that it would have, so '
               'suppression is live. The master picked the change up 58 hours after it landed.',
    ),
)


def database_path() -> str:
    """Where the sqlite database lives, overridable for tests and for a hosted refresh."""
    from_environment = os.environ.get(DATABASE_PATH_VARIABLE)
    if from_environment:
        return from_environment
    return os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                        DEFAULT_DATABASE_NAME)


def checkout_path() -> Optional[str]:
    """A WebKit checkout to read landings from, or None when the refresh has not been given one.

    There is no default. A path guessed wrong reads as thousands of pull requests that never landed,
    which is indistinguishable on a page from thousands that really did not.
    """
    return os.environ.get(CHECKOUT_PATH_VARIABLE) or None
