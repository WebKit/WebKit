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

"""The landing oracle, against a real git repository.

A stub git would test the parsing and none of the invocation, and the invocation is where the traps
are: fixed-string matching, the subject-prefix filter and the date bound are all arguments to
`git log`.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest

from ews_dashboard import webkit_checkout

TITLE = '[JSC] Introduce a WarmUpThread for MarkedBlocks'
BUG_URL = 'https://bugs.webkit.org/show_bug.cgi?id=322493'
DAY = 86400
FIRST_BUILD_AT = 1_770_000_000


class TestCheckout(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = tempfile.mkdtemp(prefix='ews-dashboard-checkout-')
        self._git('init', '-q', '-b', 'main')
        self.checkout = webkit_checkout.Checkout(self.directory, branch='main')

    def tearDown(self) -> None:
        shutil.rmtree(self.directory)

    def _git(self, *arguments: str, at: int = FIRST_BUILD_AT) -> str:
        environment = dict(
            os.environ,
            GIT_AUTHOR_DATE=f'@{at} +0000',
            GIT_COMMITTER_DATE=f'@{at} +0000',
            GIT_AUTHOR_NAME='Test', GIT_AUTHOR_EMAIL='test@example.com',
            GIT_COMMITTER_NAME='Test', GIT_COMMITTER_EMAIL='test@example.com',
        )
        # A developer's own global config reaches this repository: signing every commit and running
        # their hooks would fail here, and the failure would read as a broken oracle.
        finished = subprocess.run(
            ('git', '-C', self.directory, '-c', 'commit.gpgsign=false', *arguments),
            capture_output=True, text=True, env=environment, check=True,
        )
        return finished.stdout

    def commit(self, message: str, at: int = FIRST_BUILD_AT + DAY) -> None:
        self._git('commit', '-q', '--no-verify', '--allow-empty', '-m', message, at=at)

    def landed_message(self, title: str = TITLE, identifier: str = '319902@main') -> str:
        """What a WebKit landing looks like: the title with the bug URL appended, then the trailers
        the commit queue writes."""
        return (
            f'{title} {BUG_URL}\n\n'
            'Reviewed by Somebody.\n\n'
            '* Source/JavaScriptCore/heap/Heap.cpp:\n\n'
            f'Canonical link: https://commits.webkit.org/{identifier}\n'
        )

    def test_a_title_landed_with_the_bug_url_appended_is_found_with_its_identifier(self) -> None:
        self.commit(self.landed_message())
        resolution = self.checkout.landing_of(TITLE)
        self.assertEqual(resolution.status, webkit_checkout.LANDED)
        self.assertEqual(resolution.commit.identifier, '319902@main')
        self.assertEqual(resolution.commit.subject, f'{TITLE} {BUG_URL}')
        self.assertEqual(resolution.commit.landed_at, FIRST_BUILD_AT + DAY)

    def test_a_landing_that_carries_no_canonical_link_is_still_a_landing(self) -> None:
        self.commit(f'{TITLE} {BUG_URL}\n\nReviewed by Somebody.\n')
        resolution = self.checkout.landing_of(TITLE)
        self.assertEqual(resolution.status, webkit_checkout.LANDED)
        self.assertIsNone(resolution.commit.identifier)

    def test_a_title_no_commit_carries_has_not_landed(self) -> None:
        self.commit(self.landed_message(title='Something else entirely'))
        self.assertEqual(self.checkout.landing_of(TITLE).status, webkit_checkout.NOT_LANDED)

    def test_a_title_several_commits_start_with_is_ambiguous_rather_than_the_first_of_them(self) -> None:
        """'Versioning.' is a real WebKit commit title and it lands every few days."""
        self.commit('Versioning.\n\nWebKit-7626.1.6\n')
        self.commit('Versioning.\n\nWebKit-7626.1.7\n', at=FIRST_BUILD_AT + 2 * DAY)
        resolution = self.checkout.landing_of('Versioning.')
        self.assertEqual(resolution.status, webkit_checkout.AMBIGUOUS)
        self.assertEqual(resolution.matches, 2)
        self.assertIsNone(resolution.commit)

    def test_a_title_quoted_in_a_later_message_is_not_a_landing_of_it(self) -> None:
        self.commit(f'Unreviewed, reverting a change\n\nThis reverts {TITLE} {BUG_URL}\n')
        self.assertEqual(self.checkout.landing_of(TITLE).status, webkit_checkout.NOT_LANDED)

    def test_a_commit_older_than_the_build_that_tested_the_change_is_not_its_landing(self) -> None:
        self.commit(self.landed_message(), at=FIRST_BUILD_AT - DAY)
        self.assertEqual(
            self.checkout.landing_of(TITLE, not_before=FIRST_BUILD_AT).status,
            webkit_checkout.NOT_LANDED,
        )
        self.assertEqual(self.checkout.landing_of(TITLE).status, webkit_checkout.LANDED)

    def test_a_title_holding_regex_characters_matches_itself_literally(self) -> None:
        bracketed = '[Site Isolation] http/tests/misc/a.b.html is a timeout'
        self.commit(self.landed_message(title=bracketed))
        self.assertEqual(self.checkout.landing_of(bracketed).status, webkit_checkout.LANDED)

    def test_a_branch_the_checkout_does_not_have_is_reported_rather_than_read_as_no_landing(self) -> None:
        self.commit(self.landed_message())
        elsewhere = webkit_checkout.Checkout(self.directory, branch='origin/main')
        with self.assertRaises(webkit_checkout.CheckoutUnavailable):
            elsewhere.landing_of(TITLE)

    def test_a_path_that_is_not_a_checkout_is_reported(self) -> None:
        with self.assertRaises(webkit_checkout.CheckoutUnavailable):
            webkit_checkout.Checkout(os.path.join(self.directory, 'nowhere')).head()

    def test_the_head_of_the_branch_is_the_commit_it_points_at(self) -> None:
        self.commit(self.landed_message())
        self.assertEqual(self.checkout.head(), self._git('rev-parse', 'main').strip())
