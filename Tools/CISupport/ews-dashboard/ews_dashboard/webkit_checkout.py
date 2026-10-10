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

"""Where a pull request landed, read from a local WebKit checkout.

No landed commit names the pull request it came from: its message carries the bug URL, the radar and
a `Canonical link`, and nothing else. What it does carry is the title the pull request was opened
with, which EWS records as the `github.title` property, so the join is that title against the
subject of a commit on main. The match is a prefix rather than an equality because landing appends
the bug URL and the radar to the title.

Nothing here reads the flakiness database. `landings` remembers what this module works out.
"""

from __future__ import annotations

import re
import subprocess
from dataclasses import dataclass
from typing import Iterator, Optional

DEFAULT_BRANCH = 'origin/main'
GIT_TIMEOUT_SECONDS = 120

# A title matching more than this is ambiguous many times over; the exact count stops mattering.
MOST_MATCHES_CONSIDERED = 20

RECORD_SEPARATOR = '\x1e'
FIELD_SEPARATOR = '\x1f'
# The separators are git's own escapes rather than the characters themselves: a format string
# carrying a control byte cannot be passed as an argument.
LOG_FORMAT = '%x1e%H%x1f%ct%x1f%s%x1f%b'

CANONICAL_LINK = re.compile(r'commits\.webkit\.org/(\d+@[^\s>)]+)')

LANDED = 'landed'
NOT_LANDED = 'not_landed'
AMBIGUOUS = 'ambiguous'
STATUSES = (LANDED, NOT_LANDED, AMBIGUOUS)


class CheckoutUnavailable(Exception):
    """The path is not a git checkout, or the branch landings are read from is not in it."""


@dataclass(frozen=True)
class Commit:
    commit_hash: str
    landed_at: int
    subject: str
    # The WebKit identifier from the Canonical link, absent on a commit that landed without one.
    identifier: Optional[str]


@dataclass(frozen=True)
class Resolution:
    """What a search for one title found. `commit` is set only when exactly one commit matched."""

    status: str
    commit: Optional[Commit] = None
    matches: int = 0


def _identifier_in(body: str) -> Optional[str]:
    found = CANONICAL_LINK.search(body)
    return found.group(1) if found else None


def _commits_in(log_output: str) -> Iterator[Commit]:
    for record in log_output.split(RECORD_SEPARATOR):
        if not record.strip():
            continue
        commit_hash, landed_at, subject, body = record.split(FIELD_SEPARATOR, 3)
        yield Commit(
            commit_hash=commit_hash,
            landed_at=int(landed_at),
            subject=subject,
            identifier=_identifier_in(body),
        )


class Checkout:
    def __init__(self, path: str, branch: str = DEFAULT_BRANCH) -> None:
        self.path = path
        self.branch = branch

    def head(self) -> str:
        """The commit the branch is at, so an answer can be re-asked once main has moved."""
        return self._git('rev-parse', self.branch).strip()

    def landing_of(self, title: str, not_before: Optional[int] = None) -> Resolution:
        """The commit on the branch that this pull request landed as.

        `not_before` bounds the search to commits at or after it, which is both correct — a change
        cannot land before the build that tested it — and the difference between reading a few
        thousand messages and reading every message in WebKit's history.
        """
        arguments = [
            'log', self.branch, f'--format={LOG_FORMAT}',
            '--fixed-strings', f'--grep={title}',
            f'--max-count={MOST_MATCHES_CONSIDERED}',
        ]
        if not_before is not None:
            arguments.append(f'--since=@{not_before}')

        # A revert or a follow-up quoting the title in its body matches the grep as well, so the
        # subject decides.
        matched = [commit for commit in _commits_in(self._git(*arguments))
                   if commit.subject.startswith(title)]
        if not matched:
            return Resolution(NOT_LANDED)
        if len(matched) > 1:
            return Resolution(AMBIGUOUS, matches=len(matched))
        return Resolution(LANDED, commit=matched[0], matches=1)

    def _git(self, *arguments: str) -> str:
        try:
            finished = subprocess.run(
                ('git', '-C', self.path, *arguments),
                capture_output=True, text=True, timeout=GIT_TIMEOUT_SECONDS,
            )
        except (OSError, subprocess.SubprocessError) as error:
            raise CheckoutUnavailable(f'{self.path}: {error}') from error
        if finished.returncode:
            raise CheckoutUnavailable(f'{self.path}: {finished.stderr.strip()}')
        return finished.stdout
