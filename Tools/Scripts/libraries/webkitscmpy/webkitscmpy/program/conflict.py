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

from .command import Command
from .. import local


class Conflict(Command):
    name = 'conflict'
    help = "Given the representative Radar ID of a conflicting merge, checkout the branch."

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'radar_id',
            type=int, default=None,
            help='Radar ID that caused a merge conflict.',
        )

    @classmethod
    def find_conflict_pr(cls, remote, branch):
        for pr in remote.pull_requests.find(head=branch, opened=True):
            return pr

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not repository.path or not isinstance(repository, local.Git):
            sys.stderr.write('Cannot checkout conflict, must be in a local git repository\n')
            return 1

        expected_branch = 'integration/conflict/{}'.format(args.radar_id)
        for source_remote in repository.source_remotes():
            rmt = repository.remote(name=source_remote)

            conflict_pr = cls.find_conflict_pr(rmt, expected_branch)
            if conflict_pr:
                return repository.checkout('{}:{}'.format(conflict_pr._metadata['full_name'], conflict_pr.head))

        sys.stderr.write('No conflict pull request found with branch {}\n'.format(expected_branch))
        return 1
