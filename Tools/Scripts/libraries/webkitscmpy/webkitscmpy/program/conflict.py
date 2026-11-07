# Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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

import sys
from typing import Any, Iterator, TYPE_CHECKING

from webkitbugspy import Tracker, radar
from .command import Command
from .. import local, remote
from ..commit import Commit

if TYPE_CHECKING:
    from argparse import ArgumentParser, Namespace
    from logging import Logger, RootLogger
    from webkitscmpy.pull_request import PullRequest
    from webkitscmpy.remote import BitBucket, GitHub


class Conflict(Command):
    name = 'conflict'
    help = "Given the representative Radar ID of a conflicting merge, checkout the branch with conflict markers in place."
    INTEGRATION_BRANCH_PREFIX = 'integration'

    @classmethod
    def parser(cls, parser: ArgumentParser, loggers: list[RootLogger | Logger] | None = None) -> None:
        parser.add_argument(
            'radar',
            type=str, default=None,
            help='Radar ID that caused a merge conflict. Ex. rdar://problem/123, rdar://123, 123',
        )

    @classmethod
    def find_conflict_pr(cls, remote: GitHub | BitBucket, radar_id: int) -> PullRequest | None:
        """
        Because we don't know what the target branch was just given the radar id,
        we need to search for PRs with branches that start with the integration prefix
        and have the first and last sha.
        """
        radar_obj = Tracker.from_string(f'rdar://{radar_id}')
        assert radar_obj, 'Could not fetch radar object for id {}'.format(radar_id)
        shas = []

        repo_name = remote.name if '/' not in remote.name else remote.name.split('/', 1)[-1]
        source_changes = radar_obj.source_changes
        assert source_changes is not None
        for entry in source_changes:
            repo, action, sha = entry.split(', ')
            if repo.lower() == repo_name.lower():
                shas.append(sha)

        if not shas:
            print(f'No source changes for {repo_name} found in {radar_obj}', file=sys.stderr)
            return None

        integration_branches = []
        for prefix in ('ci', 'conflict'):
            integration_branches.append("{}/{}/{}_{}".format(cls.INTEGRATION_BRANCH_PREFIX, prefix, shas[0][:Commit.HASH_LABEL_SIZE], shas[-1][:Commit.HASH_LABEL_SIZE]))

        for pr in cls.get_open_integration_prs(remote):
            assert pr.head is not None
            for branch in integration_branches:
                if pr.head.startswith(branch):
                    return pr
        return None

    @classmethod
    def get_open_integration_prs(cls, remote: GitHub | BitBucket) -> Iterator[PullRequest]:
        assert remote.pull_requests is not None
        return remote.pull_requests.find(head=cls.INTEGRATION_BRANCH_PREFIX, opened=True)

    @classmethod
    def main(cls, args: Namespace, repository: local.Git | None, **kwargs: Any) -> int:
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not repository.path or not isinstance(repository, local.Git):
            sys.stderr.write('Cannot checkout conflict, must be in a local git repository\n')
            return 1

        # This is to remove any extra inputs like rdar://problem/
        radar_id = ''.join(i for i in args.radar if i.isdigit())
        radar_obj = Tracker.from_string(f'rdar://{radar_id}')
        assert radar_obj is not None
        expected_branch = 'integration/conflict/{}'.format(radar_obj.id)
        conflict_pr = None
        source_remote = None
        for source_remote in repository.source_remotes():
            rmt = repository.remote(name=source_remote)
            assert isinstance(rmt, (remote.GitHub, remote.BitBucket))
            conflict_pr = cls.find_conflict_pr(rmt, radar_obj.id)
            if conflict_pr:
                break

        if not conflict_pr:
            sys.stderr.write('No conflict pull request found with branch {}\n'.format(expected_branch))
            return 1

        metadata = conflict_pr._metadata
        assert metadata is not None
        full_branch = '{}:{}'.format(metadata['full_name'], conflict_pr.head)
        print('Found conflict branch {}'.format(full_branch))
        checkout_response = repository.checkout(full_branch)

        msg = "\n\n-------------------------------------------------------"
        msg += "\nYou are now checked out into the conflict branch.\n"
        msg += "Conflict markers are present in files.\n"
        msg += "Please resolve them, amend the commit and force push.\n"
        msg += "\nAlternatively, if you want to get into the traditional conflict state (ex. `git status` shows conflicting files)\n"
        msg += "you can run the following commands. Warning: This will require a force push. Any changes made to the pull request branch will therefore be lost."
        msg += f'\n\ngit reset --hard {source_remote}/{conflict_pr.base}\n'
        source_changes = radar_obj.source_changes
        assert source_changes is not None
        for source in source_changes:
            source_sha = source.split(', ')[2]
            msg += 'git cherry-pick {}\n'.format(source_sha)
        print(msg)
        return 0 if checkout_response else 1

