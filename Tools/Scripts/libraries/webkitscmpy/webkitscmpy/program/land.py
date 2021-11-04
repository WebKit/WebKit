# Copyright (C) 2021 Apple Inc. All rights reserved.
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
import time

from .canonicalize import Canonicalize
from .command import Command
from .branch import Branch
from argparse import Namespace
from webkitcorepy import arguments, run, string_utils, Terminal
from webkitscmpy import local, log


class Land(Command):
    name = 'land'
    help = 'If on a pull-request or commit-queue branch, rebase the ' \
           'current branch onto the target production branch and push.'

    OOPS_RE = re.compile(r'\(O+P+S!*\)')
    REVIEWED_BY_RE = re.compile('Reviewed by (?P<approver>.+)')
    REMOTE = 'origin'
    MIRROR_TIMEOUT = 60

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            '--no-force-review', '--force-review', '--no-review',
            dest='review', default=True,
            help='Check if the change has been approved or blocked by reviewers',
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--no-oops', '--no-allow-oops', '--allow-oops',
            dest='oops', default=False,
            help="Allow (OOPS!) in commit messages",
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--defaults', '--no-defaults', action=arguments.NoAction, default=None,
            help='Do not prompt the user for defaults, always use (or do not use) them',
        )

    @classmethod
    def main(cls, args, repository, identifier_template=None, canonical_svn=False, **kwargs):
        if not repository.path:
            sys.stderr.write("Cannot 'land' change in remote repository\n")
            return 1

        if not isinstance(repository, local.Git):
            sys.stderr.write("'land' only supported by local git repositories\n")
            return 1

        if canonical_svn and not repository.is_svn:
            sys.stderr.write("Cannot 'land' on a canonical SVN repository that is not configured as git-svn\n")
            return 1

        if not Branch.editable(repository.branch, repository=repository):
            sys.stderr.write("Can only 'land' editable branches\n")
            return 1
        branch_point = Branch.branch_point(repository)
        commits = list(repository.commits(begin=dict(hash=branch_point.hash), end=dict(branch=repository.branch)))
        if not commits:
            sys.stderr.write('Failed to find commits to land\n')
            return 1

        pull_request = None
        rmt = repository.remote()
        if rmt and rmt.pull_requests:
            candidates = list(rmt.pull_requests.find(opened=True, head=repository.branch))
            if len(candidates) == 1:
                pull_request = candidates[0]
            elif candidates:
                sys.stderr.write("Multiple pull-request match '{}'\n".format(repository.branch))

        if pull_request and args.review:
            if pull_request.blockers:
                sys.stderr.write("{} {} blocking landing '{}'\n".format(
                    string_utils.join([p.name for p in pull_request.blockers]),
                    'are' if len(pull_request.blockers) > 1 else 'is',
                    pull_request,
                ))
                return 1
            need_review = False
            if pull_request.approvers:
                review_lines = [cls.REVIEWED_BY_RE.search(commit.message) for commit in commits]
                need_review = any([cls.OOPS_RE.search(match.group('approver')) for match in review_lines if match])
            if need_review and (args.defaults or Terminal.choose("Set '{}' as your reviewer{}?".format(
                string_utils.join([p.name for p in pull_request.approvers]),
                's' if len(pull_request.approvers) > 1 else '',
            ), default='Yes') == 'Yes'):
                log.warning("Setting {} as reviewer{}".format(
                    string_utils.join([p.name for p in pull_request.approvers]),
                    's' if len(pull_request.approvers) > 1 else '',
                ))
                if run([
                    repository.executable(), 'filter-branch', '-f',
                    '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}'".format(
                        date='{} -{}'.format(int(time.time()), repository.gmtoffset())
                    ), '--msg-filter', 'sed "s/NOBODY (OO*PP*S!*)/{}/g"'.format(string_utils.join([p.name for p in pull_request.approvers])),
                    '{}...{}'.format(repository.branch, branch_point.hash),
                ], cwd=repository.root_path, env={'FILTER_BRANCH_SQUELCH_WARNING': '1'}, capture_output=True).returncode:
                    sys.stderr.write('Failed to set reviewers\n')
                    return 1
                commits = list(repository.commits(begin=dict(hash=branch_point.hash), end=dict(branch=repository.branch)))
                if not commits:
                    sys.stderr.write('Failed to find commits after setting reviewers\n')
                    return 1

        elif not pull_request:
            sys.stderr.write("Failed to find pull-request associated with '{}'\n".format(repository.branch))

        if not args.oops and any([cls.OOPS_RE.search(commit.message) for commit in commits]):
            sys.stderr.write("Found '(OOPS!)' message in commit messages, please resolve before committing\n")
            return 1

        if not args.oops:
            for line in repository.diff_lines(branch_point.hash, repository.branch):
                if cls.OOPS_RE.search(line):
                    sys.stderr.write("Found '(OOPS!)' in commit diff, please resolve before committing\n")
                    return 1

        target = pull_request.base if pull_request else branch_point.branch
        log.warning("Rebasing '{}' from '{}' to '{}'...".format(repository.branch, branch_point.branch, target))
        if repository.fetch(branch=target, remote=cls.REMOTE):
            sys.stderr.write("Failed to fetch '{}' from '{}'\n".format(target, cls.REMOTE))
            return 1
        if repository.rebase(target=target, base=branch_point.branch, head=repository.branch):
            sys.stderr.write("Failed to rebase '{}' onto '{}', please resolve conflicts\n".format(repository.branch, target))
            return 1
        log.warning("Rebased '{}' from '{}' to '{}'!".format(repository.branch, branch_point.branch, target))

        if run([repository.executable(), 'branch', '-f', target, repository.branch], cwd=repository.root_path).returncode:
            sys.stderr.write("Failed to move '{}' ref\n".format(target))
            return 1

        if identifier_template:
            source = repository.branch
            repository.checkout(target)
            if Canonicalize.main(Namespace(
                identifier=True, remote=cls.REMOTE, number=len(commits),
            ), repository, identifier_template=identifier_template):
                sys.stderr.write("Failed to embed identifiers to '{}'\n".format(target))
                return 1
            if run([repository.executable(), 'branch', '-f', source, target], cwd=repository.root_path).returncode:
                sys.stderr.write("Failed to move '{}' ref to the canonicalized head of '{}'\n".format(source, target))
                return -1

        if canonical_svn:
            if run([repository.executable(), 'svn', 'fetch'], cwd=repository.root_path).returncode:
                sys.stderr.write("Failed to update subversion refs\n".format(target))
                return 1
            if run([repository.executable(), 'svn', 'dcommit'], cwd=repository.root_path).returncode:
                sys.stderr.write("Failed to commit '{}' to Subversion remote\n".format(target))
                return 1
            run([repository.executable(), 'reset', 'HEAD~{}'.format(len(commits)), '--hard'], cwd=repository.root_path)

            # Verify the mirror processed our change
            started = time.time()
            original = repository.find('HEAD', include_log=False, include_identifier=False)
            latest = original
            while original.hash == latest.hash:
                if time.time() - started > cls.MIRROR_TIMEOUT:
                    sys.stderr.write("Timed out waiting for the git-svn mirror, '{}' landed but not closed\n".format(pull_request or repository.branch))
                    return 1
                log.warning('    Verifying mirror processesed change')
                time.sleep(5)
                run([repository.executable(), 'pull'], cwd=repository.root_path)
                original = repository.find('HEAD', include_log=False, include_identifier=False)

        else:
            if run([repository.executable(), 'push', cls.REMOTE, target], cwd=repository.root_path).returncode:
                sys.stderr.write("Failed to push '{}' to '{}'\n".format(target, cls.REMOTE))
                return 1

        commit = repository.commit(branch=target, include_log=False)
        if identifier_template and commit.identifier:
            land_message = 'Landed {} ({})!'.format(identifier_template.format(commit).split(': ')[-1], commit.hash)
        else:
            land_message = 'Landed {}!'.format(commit.hash)
        print(land_message)

        if pull_request:
            pull_request.comment(land_message)
            pull_request.close()

        return 0
