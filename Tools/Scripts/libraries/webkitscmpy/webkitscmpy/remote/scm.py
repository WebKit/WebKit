# Copyright (C) 2020-2023 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import annotations

import re
from typing import TYPE_CHECKING, Callable, Iterable, Iterator

from webkitscmpy.scm_base import ScmBase
from webkitcorepy import string_utils

if TYPE_CHECKING:
    from webkitscmpy import Commit, CommitClassifier, Contributor, PullRequest


class Scm(ScmBase):
    EMAIL_RE = re.compile(r'(?P<email>[^@]+@[^@]+)(@.*)?')

    class PRGenerator(object):
        SUPPORTS_DRAFTS = False

        def __init__(self, repository: Scm) -> None:
            self.repository = repository

        def get(self, number: int) -> PullRequest | None:
            raise NotImplementedError()

        def find(self, opened: bool | None = True, head: str | None = None, base: str | None = None) -> Iterator[PullRequest]:
            raise NotImplementedError()

        def create(
            self, head: str, title: str, body: str | None = None, commits: list[Commit] | None = None,
            base: str | None = None, draft: bool | None = None,
        ) -> PullRequest | None:
            raise NotImplementedError()

        def update(
            self, pull_request: PullRequest, head: str | None = None, title: str | None = None, body: str | None = None,
            commits: list[Commit] | None = None, base: str | None = None, opened: bool | None = None, draft: bool | None = None,
        ) -> PullRequest | None:
            raise NotImplementedError()

        def reviewers(self, pull_request: PullRequest) -> PullRequest:
            raise NotImplementedError()

        def comment(self, pull_request: PullRequest, content: str) -> PullRequest | None:
            raise NotImplementedError()

        def comments(self, pull_request: PullRequest) -> Iterator[PullRequest.Comment]:
            raise NotImplementedError()

        # Diff comments are keyed by file, and then by line (or None, for comments on the whole file).
        def review(
            self, pull_request: PullRequest, comment: str | None = None, approve: bool | None = None,
            diff_comments: dict[str, dict[int | None, list[str]]] | None = None,
        ) -> PullRequest | None:
            raise NotImplementedError()

        def statuses(self, pull_request: PullRequest) -> Iterator[PullRequest.Status]:
            raise NotImplementedError()

        def diff(
            self, pull_request: PullRequest, comments: bool = False,
            diff_comments: dict[str, dict[int | None, list[str]]] | None = None,
        ) -> Iterator[str]:
            raise NotImplementedError()

    @classmethod
    def is_webserver(cls, url: str) -> bool:
        raise NotImplementedError()

    @classmethod
    def from_url(cls, url: str, contributors: Contributor.Mapping | None = None, classifier: CommitClassifier | None = None) -> Scm:
        from webkitscmpy import remote

        if 'bitbucket' in url or 'stash' in url:
            match = re.match(r'(?P<protocol>https?)://(?P<host>[^/]+)/(projects/)?(?P<project>[^/]+)/(repos/)?(?P<repo>[^/]+)', url)
            if not match:
                raise OSError("'{}' is not a known SCM server".format(url))
            url = '{}://{}/projects/{}/repos/{}'.format(
                match.group('protocol'),
                match.group('host'),
                match.group('project').upper(),
                match.group('repo'),
            )

        candidates: list[type[Scm]] = [remote.Svn, remote.GitHub, remote.BitBucket]
        for candidate in candidates:
            if candidate.is_webserver(url):
                return candidate(url, contributors=contributors, classifier=classifier)

        raise OSError("'{}' is not a known SCM server".format(url))

    @classmethod
    def insert_diff_comments(cls, generator: Callable[[], Iterable[str]], comments: dict[str, dict[int | None, list[str]]] | None = None) -> Iterator[str]:
        file = None
        count = 0
        comments = comments or dict()
        for line in generator():
            if line.startswith('+++ b/'):
                file = line.split('/', 1)[-1]
                count = -1
            yield line
            comments_on = comments.get(file or '', {}).get(None if count < 0 else count, [])
            if comments_on:
                yield '>>>>'
                for comment in comments_on:
                    for comment_line in comment.splitlines():
                        yield comment_line
                yield '<<<<'
            count += 1

    def __init__(
        self, url: str, dev_branches: re.Pattern[str] | None = None, prod_branches: re.Pattern[str] | None = None,
        contributors: Contributor.Mapping | None = None, id: str | None = None, classifier: CommitClassifier | None = None,
    ) -> None:
        super(Scm, self).__init__(
            dev_branches=dev_branches,
            prod_branches=prod_branches,
            contributors=contributors,
            id=id,
            classifier=classifier,
        )

        if not isinstance(url, string_utils.basestring):
            raise ValueError("Expected 'url' to be a string type, not '{}'".format(type(url)))
        self.url = url
        self.pull_requests: Scm.PRGenerator | None = None

    def checkout_url(self, ssh: bool = False, http: bool = False) -> str:
        raise NotImplementedError()
