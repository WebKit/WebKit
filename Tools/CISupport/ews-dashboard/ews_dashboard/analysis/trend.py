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

"""The false-positive rate over time, in daily buckets with a rolling line.

Days are UTC, so a bucket means the same span regardless of who is reading. The rolling value for a
day sums that day and the days before it rather than averaging their daily percentages, which would
weight a day with two builds the same as a day with two hundred.
"""

from __future__ import annotations

import datetime
import sqlite3
from dataclasses import dataclass
from typing import Optional

from ews_dashboard import config
from ews_dashboard.analysis import false_positive

ROLLING_DAYS = 7


@dataclass(frozen=True)
class Point:
    day: datetime.date
    counts: false_positive.Counts
    rolling_pct: Optional[float]

    @property
    def daily_pct(self) -> Optional[float]:
        return self.counts.author_fp_rate_pct


def day_bounds(day: datetime.date) -> tuple:
    start = datetime.datetime(day.year, day.month, day.day, tzinfo=datetime.timezone.utc)
    return int(start.timestamp()), int((start + datetime.timedelta(days=1)).timestamp())


def today() -> datetime.date:
    return datetime.datetime.now(datetime.timezone.utc).date()


def days_ending(last_day: datetime.date, count: int) -> list:
    return [last_day - datetime.timedelta(days=offset) for offset in reversed(range(count))]


def daily(
    connection: sqlite3.Connection,
    classifier: false_positive.Classifier,
    last_day: datetime.date,
    days: int = 30,
    rolling: int = ROLLING_DAYS,
    suite: Optional[str] = None,
    builders: tuple = (),
) -> list:
    """One Point per day, oldest first.

    Each day needs the `rolling - 1` days before it for its rolling value, so the walk starts that
    much earlier and the extra days are dropped before returning.
    """
    walked = days_ending(last_day, days + rolling - 1)
    per_day = []
    for day in walked:
        since, until = day_bounds(day)
        per_day.append(false_positive.rate(connection, classifier, since, until,
                                           suite=suite, builders=builders))

    points = []
    for index in range(rolling - 1, len(walked)):
        merged = false_positive.Counts()
        for counts in per_day[index - rolling + 1:index + 1]:
            merged.merge(counts)
        points.append(Point(walked[index], per_day[index], merged.author_fp_rate_pct))
    return points


def deployments_within(points: list) -> list:
    """The deployments that fall inside a series, so the chart can mark them.

    A chart of this metric without them invites the reader to attribute a step change to chance.
    """
    if not points:
        return []
    first, last = day_bounds(points[0].day)[0], day_bounds(points[-1].day)[1]
    return [
        deployment for deployment in config.DEPLOYMENTS
        if first <= int(deployment.at.timestamp()) < last
    ]
