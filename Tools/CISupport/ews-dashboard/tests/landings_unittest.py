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

"""Which pull requests get looked for, and what is remembered about the answer."""

from __future__ import annotations

from typing import Optional

from ews_dashboard import config, landings, webkit_checkout
from tests import fixtures

FIRST_PR = 72736
SECOND_PR = 72737
FIRST_TITLE = '[Site Isolation] http/tests/misc/a.html is a timeout'
SECOND_TITLE = '[JSC] Introduce a WarmUpThread for MarkedBlocks'


def landed(identifier: str = '319902@main', at: int = fixtures.DEFAULT_BUILD_TIME + 3600,
           subject: Optional[str] = None) -> webkit_checkout.Resolution:
    return webkit_checkout.Resolution(
        status=webkit_checkout.LANDED,
        commit=webkit_checkout.Commit(
            commit_hash='b' * 40, landed_at=at, subject=subject or FIRST_TITLE,
            identifier=identifier,
        ),
        matches=1,
    )


class StubCheckout:
    """Answers by title, and records what it was asked and how far back."""

    def __init__(self, by_title: Optional[dict] = None, head: str = 'a' * 40) -> None:
        self.by_title = by_title or {}
        self.branch_head = head
        self.asked: list = []

    def head(self) -> str:
        return self.branch_head

    def landing_of(self, title: str,
                   not_before: Optional[int] = None) -> webkit_checkout.Resolution:
        self.asked.append((title, not_before))
        return self.by_title.get(title, webkit_checkout.Resolution(webkit_checkout.NOT_LANDED))


class TestLandings(fixtures.DatabaseTest):
    WINDOW = (fixtures.DEFAULT_BUILD_TIME - 86400, fixtures.DEFAULT_BUILD_TIME + 86400)

    def resolve(self, checkout: StubCheckout) -> dict:
        return landings.resolve(self.connection, checkout, *self.WINDOW)

    def store_convicted_build(self, number: int, pr_id: int = FIRST_PR,
                              title: str = FIRST_TITLE,
                              started_at: int = fixtures.DEFAULT_BUILD_TIME) -> int:
        return self.store_build(number, flaky={'fast/a.html': config.DIRTY_TREE},
                                pr_id=pr_id, pr_title=title, started_at=started_at)

    def test_the_title_ingest_stored_is_the_one_the_landing_is_looked_for_by(self) -> None:
        build_id = self.store_convicted_build(1)
        self.assertEqual(self.stored_build(build_id)['pr_title'], FIRST_TITLE)

    def test_a_convicted_pull_request_is_looked_for_and_its_landing_stored(self) -> None:
        self.store_convicted_build(1)
        outcomes = self.resolve(StubCheckout({FIRST_TITLE: landed()}))
        self.assertEqual(outcomes, {webkit_checkout.LANDED: 1})
        stored = landings.landing_of(self.connection, FIRST_PR)
        self.assertEqual(
            (stored['status'], stored['identifier'], stored['commit_hash'], stored['matches']),
            (webkit_checkout.LANDED, '319902@main', 'b' * 40, 1),
        )

    def test_a_build_that_convicted_nothing_is_never_looked_for(self) -> None:
        self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[],
                         pr_id=FIRST_PR, pr_title=FIRST_TITLE)
        checkout = StubCheckout()
        self.assertEqual(self.resolve(checkout), {})
        self.assertEqual(checkout.asked, [])

    def test_a_build_outside_the_window_is_not_looked_for(self) -> None:
        self.store_convicted_build(1, started_at=fixtures.DEFAULT_BUILD_TIME - 5 * 86400)
        checkout = StubCheckout()
        self.assertEqual(self.resolve(checkout), {})
        self.assertEqual(checkout.asked, [])

    def test_the_search_starts_at_the_earliest_build_of_the_pull_request(self) -> None:
        """A change cannot land before the build that tested it, and the bound is what keeps the
        search off the whole of WebKit's history."""
        earliest = fixtures.DEFAULT_BUILD_TIME - 3600
        self.store_convicted_build(1, started_at=fixtures.DEFAULT_BUILD_TIME)
        self.store_convicted_build(2, started_at=earliest)
        checkout = StubCheckout()
        self.resolve(checkout)
        self.assertEqual(checkout.asked, [(FIRST_TITLE, earliest)])

    def test_each_pull_request_is_looked_for_once_however_many_builds_convicted_on_it(self) -> None:
        self.store_convicted_build(1)
        self.store_convicted_build(2, started_at=fixtures.DEFAULT_BUILD_TIME + 60)
        self.store_convicted_build(3, pr_id=SECOND_PR, title=SECOND_TITLE)
        checkout = StubCheckout({FIRST_TITLE: landed()})
        self.resolve(checkout)
        self.assertEqual(sorted(title for title, _ in checkout.asked),
                         sorted([FIRST_TITLE, SECOND_TITLE]))

    def test_a_landing_already_stored_is_not_looked_for_again(self) -> None:
        self.store_convicted_build(1)
        self.resolve(StubCheckout({FIRST_TITLE: landed()}))
        again = StubCheckout({FIRST_TITLE: landed()})
        self.assertEqual(self.resolve(again), {'skipped': 1})
        self.assertEqual(again.asked, [])

    def test_a_pull_request_that_had_not_landed_is_asked_about_again_once_main_moves(self) -> None:
        """Not landed yet is the ordinary answer for a recent conviction, so it must not be cached
        as a verdict."""
        self.store_convicted_build(1)
        self.assertEqual(self.resolve(StubCheckout(head='a' * 40)),
                         {webkit_checkout.NOT_LANDED: 1})

        unmoved = StubCheckout({FIRST_TITLE: landed()}, head='a' * 40)
        self.assertEqual(self.resolve(unmoved), {'skipped': 1})
        self.assertEqual(unmoved.asked, [])

        moved = StubCheckout({FIRST_TITLE: landed()}, head='c' * 40)
        self.assertEqual(self.resolve(moved), {webkit_checkout.LANDED: 1})
        self.assertEqual(landings.landing_of(self.connection, FIRST_PR)['status'],
                         webkit_checkout.LANDED)

    def test_an_ambiguous_title_is_stored_as_ambiguous_with_its_count_and_no_commit(self) -> None:
        self.store_convicted_build(1)
        ambiguous = webkit_checkout.Resolution(webkit_checkout.AMBIGUOUS, matches=4)
        self.assertEqual(self.resolve(StubCheckout({FIRST_TITLE: ambiguous})),
                         {webkit_checkout.AMBIGUOUS: 1})
        stored = landings.landing_of(self.connection, FIRST_PR)
        self.assertEqual((stored['status'], stored['matches'], stored['commit_hash']),
                         (webkit_checkout.AMBIGUOUS, 4, None))

    def test_an_unreadable_checkout_stores_nothing_rather_than_a_window_of_false_negatives(self) -> None:
        self.store_convicted_build(1)

        class Unreadable(StubCheckout):
            def head(self) -> str:
                raise webkit_checkout.CheckoutUnavailable('no such checkout')

        with self.assertRaises(webkit_checkout.CheckoutUnavailable):
            self.resolve(Unreadable())
        self.assertIsNone(landings.landing_of(self.connection, FIRST_PR))
