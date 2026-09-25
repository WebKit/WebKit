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

"""`queues.resolve` is the one place a reader's group/version/builder choice becomes a concrete
builder tuple; the analysis layer never re-derives group membership itself."""

from __future__ import annotations

import unittest

from ews_dashboard import queues
from tests import fixtures

KNOWN = (fixtures.LAYOUT_BUILDER, fixtures.API_BUILDER, fixtures.GTK_BUILDER, fixtures.WPE_BUILDER)


def resolved(**selection: object) -> tuple:
    return queues.resolve(queues.Selection(**selection), KNOWN)


class TestResolve(unittest.TestCase):
    def test_a_single_group_narrows_to_that_groups_builders(self) -> None:
        self.assertEqual(resolved(groups=('GTK',)), (fixtures.GTK_BUILDER,))

    def test_two_groups_selected_yields_the_union(self) -> None:
        one_group = resolved(groups=('GTK',))
        two_groups = resolved(groups=('GTK', 'WPE'))
        self.assertEqual(set(two_groups), {fixtures.GTK_BUILDER, fixtures.WPE_BUILDER})
        self.assertGreater(len(two_groups), len(one_group))

    def test_no_group_selected_returns_everything_the_unfiltered_query_does(self) -> None:
        self.assertEqual(resolved(), ())
        self.assertEqual(resolved(groups=()), ())

    def test_an_unrecognised_group_is_dropped_rather_than_reaching_sql(self) -> None:
        self.assertEqual(resolved(groups=('not-a-queue',)), ())
        self.assertEqual(resolved(groups=('GTK', 'not-a-queue')), (fixtures.GTK_BUILDER,))
