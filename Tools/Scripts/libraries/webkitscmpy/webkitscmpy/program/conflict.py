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
    help = "Given the representative Radar ID of a conflicting merge, checkout the branch with conflict markers in place."
    INTEGRATION_BRANCH_PREFIX = 'integration'
    INTEGRATION_BRANCH_TYPES = ('conflict', 'ci')

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'radar',
            type=str, default=None,
            help='Radar ID that caused a merge conflict. Ex. rdar://problem/123, rdar://123, 123',
        )

    @staticmethod
    def parse_source_change(entry):
        """
        Parse a single source change entry of the form 'repo, action, sha'.

        The sourceChanges field on a radar is free-form text, so not every line
        is guaranteed to match this format. Returns a (repo, action, sha) tuple
        for well-formed entries, or None for anything that doesn't parse.
        """
        parts = entry.split(', ')
        return tuple(parts) if len(parts) == 3 else None

    @classmethod
    def find_conflict_pr(cls, remote, radar_id):
        """
        Given only the radar id, reconstruct the integration branch name from the
        radar's source changes and search open integration PRs for a matching head.

        A branch is named integration/<conflict|ci>/<first_sha>_<last_sha>/<target>
        (shas truncated to Commit.HASH_LABEL_SIZE), matched via startswith since we
        don't know the target branch. The source changes may form a range or, more
        commonly, one branch per change (<sha>_<sha>), and we can't assume they are
        ordered the same way the branch was named, so we try every ordered pair of
        shas, most-likely candidate first.
        """
        radar_obj = Tracker.from_string(f'rdar://{radar_id}')
        assert radar_obj, 'Could not fetch radar object for id {}'.format(radar_id)
        shas = []

        repo_name = remote.name if '/' not in remote.name else remote.name.split('/', 1)[-1]
        for entry in radar_obj.source_changes:
            parsed = cls.parse_source_change(entry)
            if not parsed:
                continue
            repo, action, sha = parsed
            if repo.lower() == repo_name.lower():
                shas.append(sha)

        if not shas:
            print(f'No source changes for {repo_name} found in {radar_obj}', file=sys.stderr)
            return None

        # Order candidates by likelihood of a hit: a conflict PR is created per
        # source change, so <sha>_<sha> under the 'conflict' prefix is by far the
        # common case; ranges (either ordering, since source_changes order isn't
        # guaranteed) and the 'ci' prefix are fallbacks.
        sha_pairs = [(sha, sha) for sha in shas]
        sha_pairs += [(first, last) for first in shas for last in shas if first != last]
        sha_pairs = list(dict.fromkeys(sha_pairs))
        integration_branches = [
            "{}/{}/{}_{}".format(cls.INTEGRATION_BRANCH_PREFIX, prefix, first[:Commit.HASH_LABEL_SIZE], last[:Commit.HASH_LABEL_SIZE])
            for prefix in cls.INTEGRATION_BRANCH_TYPES
            for first, last in sha_pairs
        ]

        prs = list(cls.get_open_integration_prs(remote))
        for branch in integration_branches:
            for pr in prs:
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
        conflict_pr = None
        source_remote = None
        for source_remote in repository.source_remotes():
            rmt = repository.remote(name=source_remote)
            conflict_pr = cls.find_conflict_pr(rmt, radar_obj.id)
            if conflict_pr:
                break

        if not conflict_pr:
            sys.stderr.write('No conflict pull request found with branch {}\n'.format(expected_branch))
            return 1

        full_branch = '{}:{}'.format(conflict_pr._metadata['full_name'], conflict_pr.head)
        print('Found conflict branch {}'.format(full_branch))
        checkout_response = repository.checkout(full_branch)

        msg = "\n\n-------------------------------------------------------"
        msg += "\nYou are now checked out into the conflict branch.\n"
        msg += "Conflict markers are present in files.\n"
        msg += "Please resolve them, amend the commit and force push.\n"
        msg += "\nAlternatively, if you want to get into the traditional conflict state (ex. `git status` shows conflicting files)\n"
        msg += "you can run the following commands. Warning: This will require a force push. Any changes made to the pull request branch will therefore be lost."
        msg += f'\n\ngit reset --hard {source_remote}/{conflict_pr.base}\n'
        for source in radar_obj.source_changes:
            parsed = cls.parse_source_change(source)
            if not parsed:
                continue
            source_sha = parsed[2]
            msg += 'git cherry-pick {}\n'.format(source_sha)
        print(msg)
        return checkout_response

