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

"""The window a refresh walks, which is what the pages have verdicts to show over."""

from __future__ import annotations

import datetime
import unittest
from unittest import mock

from ews_dashboard import ingest
from ews_dashboard.analysis import trend
from ews_dashboard.web import app
from scripts import refresh
from tests import fixtures


class TestDefaultWindow(unittest.TestCase):
    """A verdict exists only for what the refresh assessed, while the convictions main could not be
    asked about are recounted per request over whatever window a reader picked, so a narrower refresh
    than the widest choice makes that choice look worse purely because nothing ever looked."""

    def test_a_default_run_covers_the_widest_window_a_page_offers(self) -> None:
        self.assertEqual(refresh.DEFAULT_DAYS, max(app.WINDOW_CHOICES))

    def test_the_default_window_reaches_back_that_many_days(self) -> None:
        with mock.patch.object(trend, 'today', return_value=datetime.date(2024, 1, 15)):
            since, until = refresh._window(refresh.DEFAULT_DAYS)
        self.assertEqual(until, 1705363200)
        self.assertEqual(since, 1697587200)


class TestFailRun(fixtures.DatabaseTest):
    """A run that dies partway still has to say how much it got done and how much of that failed,
    since a page reading `builds_failed` back should not see zero purely because the run never
    reached `_finish_run`."""

    def test_a_failed_run_records_both_ingested_and_failed_counts(self) -> None:
        started_at = fixtures.DEFAULT_BUILD_TIME
        refresh._begin_run(self.connection, started_at)
        report = ingest.IngestReport()
        report.record('ingested')
        report.errors.append(RuntimeError('boom'))
        refresh._fail_run(self.connection, started_at, RuntimeError('boom'), report)
        row = self.connection.execute(
            'SELECT builds_ingested, builds_failed FROM refresh_runs WHERE started_at = ?',
            (started_at,),
        ).fetchone()
        self.assertGreater(row['builds_ingested'], 0)
        self.assertGreater(row['builds_failed'], 0)
