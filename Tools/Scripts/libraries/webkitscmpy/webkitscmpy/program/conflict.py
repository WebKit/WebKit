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
from .review import Review
from .. import local, log, remote
from ..commit import Commit


class Conflict(Command):
    name = 'conflict'
    help = "Given the representative Radar ID of a conflicting merge, checkout the branch with conflict markers in place."
    INTEGRATION_BRANCH_PREFIX = 'integration'
    PR_URL_KV_KEY = 'scm-integrate:pr_url'
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

    @classmethod
    def find_conflict_pr_via_kv(cls, radar_obj):
        """
        The merge automation writes the conflict PR's URL directly onto the radar's key-value
        store (the 'scm-integrate:pr_url' key) when it opens a conflict PR. Read that instead of
        guessing the PR's branch name from source-change shas, since branch names can't be
        prefix-searched through the GitHub or Bitbucket APIs.

        Returns the matching PullRequest, or None if no key-value/link was found.
        """
        library = getattr(radar_obj.tracker, 'library', None)
        client = getattr(radar_obj.tracker, 'client', None)
        if not library or not client:
            log.debug('{}: tracker has no Radar client available, skipping key-value lookup'.format(radar_obj))
            return None

        try:
            pair = client.key_values_for_radar_id(radar_obj.id).key_value_pair_for_key(cls.PR_URL_KV_KEY)
        except library.exceptions.KeyValuePairNotFoundException:
            log.debug('{}: no {} key-value set'.format(radar_obj, cls.PR_URL_KV_KEY))
            return None

        if not pair or not pair.value:
            log.debug('{}: {} key-value is empty'.format(radar_obj, cls.PR_URL_KV_KEY))
            return None

        number, rmt = Review.args_for_url(pair.value)
        if not number or not rmt:
            log.debug("{}: {} value '{}' isn't a recognized pull-request URL".format(radar_obj, cls.PR_URL_KV_KEY, pair.value))
            return None

        pr = rmt.pull_requests.get(number)
        if not pr:
            log.debug("{}: PR #{} not found on {}".format(radar_obj, number, rmt))
        return pr

    @classmethod
    def find_conflict_pr_via_comments(cls, radar_obj):
        """
        Fallback used when the key-value lookup doesn't resolve a PR (e.g. the key-value was
        never written, or points at a since-deleted/invalid PR). Scans the radar's own comments,
        most recent first, for a pull-request link. A link is only trusted if the PR it points to
        actually references this radar (by rdar:// in its title or body) -- a PR link mentioned in
        an unrelated comment shouldn't be picked up.

        Returns the matching PullRequest, or None if nothing was found.
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
                if not pr:
                    log.debug("{}: PR #{} not found on {}".format(radar_obj, match.group('number'), match.group('url')))
                    continue
                if not radar_ref.search('{} {}'.format(pr.title or '', pr.body or '')):
                    log.debug("{}: {} doesn't reference this radar, skipping".format(radar_obj, pr))
                    continue
                return pr

        log.debug('{}: no comment referenced a pull-request that references this radar'.format(radar_obj))
        return None

    @classmethod
    def find_conflict_pr_via_branch_guess(cls, remote_obj, radar_obj):
        """
        Fallback used when the key-value lookup doesn't resolve a PR (e.g. an older radar
        the automation never wrote a key-value on). Guesses the conflict/CI branch name from
        the radar's source-change shas and searches the given remote for a matching open PR.

        Automation applies source changes in order and stops at the first one that conflicts,
        so with changes [1, 2, 3] where 2 conflicts, the real branch spans {1}_{2}, not {1}_{3}.
        Try every {shas[0]}_{sha} prefix rather than just {shas[0]}_{shas[-1]}.

        Returns the matching PullRequest, or None if nothing was found.
        """
        shas = []
        repo_name = remote_obj.name if '/' not in remote_obj.name else remote_obj.name.split('/', 1)[-1]
        for entry in radar_obj.source_changes:
            repo, action, sha = entry.split(', ')
            if repo.lower() == repo_name.lower():
                shas.append(sha)

        if not shas:
            log.debug('{}: no source changes for {} found in {}'.format(radar_obj, repo_name, remote_obj))
            return None

        integration_branches = [
            '{}/{}/{}_{}'.format(cls.INTEGRATION_BRANCH_PREFIX, prefix, shas[0][:Commit.HASH_LABEL_SIZE], sha[:Commit.HASH_LABEL_SIZE])
            for prefix in ('ci', 'conflict')
            for sha in shas
        ]
        log.debug('{}: searching {} for branches matching {}'.format(radar_obj, remote_obj, integration_branches))

        found_any_open_pr = False
        for pr in remote_obj.pull_requests.find(head=cls.INTEGRATION_BRANCH_PREFIX, opened=True):
            found_any_open_pr = True
            for branch in integration_branches:
                if pr.head.startswith(branch):
                    return pr
        if not found_any_open_pr:
            log.debug('{}: no open integration PRs found on {}'.format(radar_obj, remote_obj))
        else:
            log.debug('{}: open integration PRs on {} did not match {}'.format(radar_obj, remote_obj, integration_branches))
        return None

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
        conflict_pr = cls.find_conflict_pr_via_kv(radar_obj)

        if not conflict_pr:
            log.debug('rdar://{}: key-value lookup found nothing, checking comments for a pull-request link'.format(radar_id))
            conflict_pr = cls.find_conflict_pr_via_comments(radar_obj)

        if not conflict_pr:
            log.debug('rdar://{}: no pull-request link in comments, falling back to branch-guessing per local remote'.format(radar_id))
            for source_remote in repository.source_remotes():
                conflict_pr = cls.find_conflict_pr_via_branch_guess(repository.remote(name=source_remote), radar_obj)
                if conflict_pr:
                    break

        if not conflict_pr:
            sys.stderr.write('No conflict pull request found via the {} key-value, radar comments, or branch-guessing on rdar://{}\n'.format(cls.PR_URL_KV_KEY, radar_id))
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

