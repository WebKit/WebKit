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

import sys

from webkitbugspy import Tracker, radar
from .command import Command
from .. import local
from ..commit import Commit


class Conflict(Command):
    name = 'conflict'
    help = "Given the representative Radar ID of a conflicting merge, checkout the branch."
    INTEGRATION_BRANCH_PREFIX = 'integration'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'radar',
            type=str, default=None,
            help='Radar ID that caused a merge conflict. Ex. rdar://problem/123, rdar://123, 123',
        )

    @classmethod
    def find_conflict_pr(cls, remote, radar_id):
        """
        Because we don't know what the target branch was just given the radar id,
        we need to search for PRs with branches that start with the integration prefix
        and have the first and last sha.
        """
        radar_obj = Tracker.from_string(f'rdar://{radar_id}')
        assert radar_obj, 'Could not fetch radar object for id {}'.format(radar_id)
        shas = []

        repo_name = remote.name if '/' not in remote.name else remote.name.split('/', 1)[-1]
        for entry in radar_obj.source_changes:
            repo, action, sha = entry.split(', ')
            if repo == repo_name:
                shas.append(sha)

        integration_branches = []
        for prefix in ('ci', 'conflict'):
            integration_branches.append("{}/{}/{}_{}".format(cls.INTEGRATION_BRANCH_PREFIX, prefix, shas[0][:Commit.HASH_LABEL_SIZE], shas[-1][:Commit.HASH_LABEL_SIZE]))

        for pr in cls.get_open_integration_prs(remote):
            for branch in integration_branches:
                if pr.head.startswith(branch):
                    return pr

    @classmethod
    def get_open_integration_prs(cls, remote):
        return remote.pull_requests.find(head=cls.INTEGRATION_BRANCH_PREFIX, opened=True)

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not repository.path or not isinstance(repository, local.Git):
            sys.stderr.write('Cannot checkout conflict, must be in a local git repository\n')
            return 1

        # This is to remove any extra inputs like rdar://problem/
        radar_id = ''.join(i for i in args.radar if i.isdigit())
        radar_obj = Tracker.from_string(f'rdar://{radar_id}')
        expected_branch = 'integration/conflict/{}'.format(radar_obj.id)
        for source_remote in repository.source_remotes():
            rmt = repository.remote(name=source_remote)
            conflict_pr = cls.find_conflict_pr(rmt, radar_obj.id)
            if conflict_pr:
                return repository.checkout('{}:{}'.format(conflict_pr._metadata['full_name'], conflict_pr.head))

        sys.stderr.write('No conflict pull request found with branch {}\n'.format(expected_branch))
        return 1
