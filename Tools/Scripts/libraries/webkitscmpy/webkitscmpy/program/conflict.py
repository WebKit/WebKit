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

import re
import sys

from webkitbugspy import Tracker, radar
from .command import Command
from .. import local, log, remote
from ..commit import Commit


class Conflict(Command):
    name = 'conflict'
    help = "Given the representative Radar ID of a conflicting merge, checkout the branch with conflict markers in place."
    INTEGRATION_BRANCH_PREFIX = 'integration'
    COMMENT_PR_URL_RES = [
        re.compile(r'(?P<url>https?://\S+/projects/\S+/repos/\S+)/pull-requests/(?P<number>\d+)'),
        re.compile(r'(?P<url>https?://github\.\S+/\S+)/pull/(?P<number>\d+)'),
    ]

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'radar',
            type=str, default=None,
            help='Radar ID that caused a merge conflict. Ex. rdar://problem/123, rdar://123, 123',
        )
        parser.add_argument(
            '--branch',
            type=str, default=None,
            help='Target branch of the conflict, used to pick between multiple matching conflict pull requests',
        )

    @classmethod
    def find_conflict_pr_via_comments(cls, radar_obj, branch=None):
        """
        Return the open pull request linked by the most recent radar comment that references this radar.
        """
        radar_ref = re.compile(r'rdar://(?:problem/)?{}\b'.format(re.escape(str(radar_obj.id))))
        comments = sorted(radar_obj.comments or [], key=lambda comment: comment.timestamp or 0, reverse=True)

        for comment in comments:
            for candidate in cls.COMMENT_PR_URL_RES:
                match = candidate.search(comment.content or '')
                if not match:
                    continue
                try:
                    rmt = remote.Scm.from_url(match.group('url'))
                except OSError as e:
                    log.debug("{}: couldn't resolve '{}' to a known SCM server: {}".format(radar_obj, match.group('url'), e))
                    continue
                pr = rmt.pull_requests.get(match.group('number'))
                if not pr or not pr.opened or (branch and pr.base != branch):
                    log.debug('{}: PR #{} on {} is missing, closed or not targeting {}, skipping'.format(radar_obj, match.group('number'), match.group('url'), branch or 'any branch'))
                    continue
                if not radar_ref.search('{} {}'.format(pr.title or '', pr.body or '')):
                    log.debug("{}: {} doesn't reference this radar, skipping".format(radar_obj, pr))
                    continue
                return pr
        return None

    @classmethod
    def find_conflict_prs_via_branch_guess(cls, remote_obj, radar_obj):
        """
        Return open pull requests on the remote whose branch matches the radar's source-change shas.

        Automation applies source changes in order and stops at the first one that conflicts,
        so with changes [1, 2, 3] where 2 conflicts, the real branch spans {1}_{2}, not {1}_{3}.
        """
        repo_name = remote_obj.name if '/' not in remote_obj.name else remote_obj.name.split('/', 1)[-1]
        shas = []
        for entry in radar_obj.source_changes or []:
            repo, action, sha = entry.split(', ')
            if repo.lower() == repo_name.lower():
                shas.append(sha)
        if not shas:
            return []

        integration_branches = [
            '{}/{}/{}_{}'.format(cls.INTEGRATION_BRANCH_PREFIX, prefix, shas[0][:Commit.HASH_LABEL_SIZE], sha[:Commit.HASH_LABEL_SIZE])
            for prefix in ('ci', 'conflict')
            for sha in shas
        ]
        return [
            pr for pr in remote_obj.pull_requests.find(head=cls.INTEGRATION_BRANCH_PREFIX, opened=True)
            if any(pr.head.startswith(branch) for branch in integration_branches)
        ]

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
        if not radar_obj:
            sys.stderr.write('Unable to access rdar://{}, check out the conflict pull request linked in the merge conflict alert instead\n'.format(radar_id))
            return 1

        conflict_pr = cls.find_conflict_pr_via_comments(radar_obj, branch=args.branch)
        if not conflict_pr:
            log.debug('rdar://{}: no pull-request link in comments, falling back to branch-guessing'.format(radar_id))
            candidates = [
                pr for source_remote in repository.source_remotes()
                for pr in cls.find_conflict_prs_via_branch_guess(repository.remote(name=source_remote), radar_obj)
                if not args.branch or pr.base == args.branch
            ]
            if len(candidates) > 1:
                sys.stderr.write('Multiple conflict pull requests found for rdar://{}:\n'.format(radar_id))
                for pr in candidates:
                    sys.stderr.write('    {}:{} (targeting {})\n'.format(pr._metadata['full_name'], pr.head, pr.base))
                sys.stderr.write('Re-run with --branch <target branch> to pick one\n')
                return 1
            conflict_pr = candidates[0] if candidates else None

        if not conflict_pr:
            sys.stderr.write('No conflict pull request found for rdar://{}, check out the conflict pull request linked in the merge conflict alert instead\n'.format(radar_id))
            return 1

        full_branch = '{}:{}'.format(conflict_pr._metadata['full_name'], conflict_pr.head)
        print('Found conflict branch {}'.format(full_branch))
        checkout_response = repository.checkout(full_branch)

        source_remote = repository.remote_for(conflict_pr.base) or repository.default_remote
        msg = "\n\n-------------------------------------------------------"
        msg += "\nYou are now checked out into the conflict branch.\n"
        msg += "Conflict markers are present in files.\n"
        msg += "Please resolve them, amend the commit and force push.\n"
        msg += "\nAlternatively, if you want to get into the traditional conflict state (ex. `git status` shows conflicting files)\n"
        msg += "you can run the following commands. Warning: This will require a force push. Any changes made to the pull request branch will therefore be lost."
        msg += f'\n\ngit reset --hard {source_remote}/{conflict_pr.base}\n'
        for source in radar_obj.source_changes:
            source_sha = source.split(', ')[2]
            msg += 'git cherry-pick {}\n'.format(source_sha)
        print(msg)
        return checkout_response
