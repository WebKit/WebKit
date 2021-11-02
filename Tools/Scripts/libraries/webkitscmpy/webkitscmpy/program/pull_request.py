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

import os
import sys

from .command import Command
from .branch import Branch

from webkitcorepy import arguments, run
from webkitscmpy import local, log, remote


class PullRequest(Command):
    name = 'pull-request'
    aliases = ['pr', 'pfr', 'upload']
    help = 'Push the current checkout state as a pull-request'

    @classmethod
    def parser(cls, parser, loggers=None):
        Branch.parser(parser, loggers=loggers)
        parser.add_argument(
            '--add', '--no-add',
            dest='will_add', default=None,
            help='When drafting a change, add (or never add) modified files to set of staged changes to be committed',
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--rebase', '--no-rebase',
            dest='rebase', default=None,
            help='Rebase (or do not rebase) the pull-request on the source branch before pushing',
            action=arguments.NoAction,
        )

    @classmethod
    def create_commit(cls, args, repository, **kwargs):
        # First, find the set of files to be modified
        modified = [] if args.will_add is False else repository.modified()
        if args.will_add:
            modified = list(set(modified).union(set(repository.modified(staged=False))))

        # Next, add all modified file
        for file in set(modified) - set(repository.modified(staged=True)):
            log.warning('    Adding {}...'.format(file))
            if run([repository.executable(), 'add', file], cwd=repository.root_path).returncode:
                sys.stderr.write("Failed to add '{}'\n".format(file))
                return 1

        # Then, see if we already have a commit associated with this branch we need to modify
        has_commit = repository.commit(include_log=False, include_identifier=False).branch == repository.branch and repository.branch != repository.default_branch
        if not modified and has_commit:
            log.warning('Using committed changes...')
            return 0

        # Otherwise, we need to create a commit
        if not modified:
            sys.stderr.write('No modified files\n')
            return 1
        log.warning('Amending commit...' if has_commit else 'Creating commit...')
        if run([repository.executable(), 'commit', '--date=now'] + (['--amend'] if has_commit else []), cwd=repository.root_path).returncode:
            sys.stderr.write('Failed to generate commit\n')
            return 1

        return 0

    @classmethod
    def title_for(cls, commits):
        title = os.path.commonprefix([commit.message.splitlines()[0] for commit in commits])
        if not title:
            title = commits[0].message.splitlines()[0]
        title = title.rstrip().lstrip()
        return title[:-5].rstrip() if title.endswith('(Part') else title

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only '{}' on a native Git repository\n".format(cls.name))
            return 1

        if not repository.DEV_BRANCHES.match(repository.branch):
            if Branch.main(args, repository, **kwargs):
                sys.stderr.write("Abandoning pushing pull-request because '{}' could not be created\n".format(args.issue))
                return 1
        elif args.issue and repository.branch != args.issue:
            sys.stderr.write("Creating a pull-request for '{}' but we're on '{}'\n".format(args.issue, repository.branch))
            return 1

        result = cls.create_commit(args, repository, **kwargs)
        if result:
            return result

        branch_point = Branch.branch_point(repository)
        if args.rebase or (args.rebase is None and repository.config().get('pull.rebase')):
            log.warning("Rebasing '{}' on '{}'...".format(repository.branch, branch_point.branch))
            if repository.pull(rebase=True, branch=branch_point.branch):
                sys.stderr.write("Failed to rebase '{}' on '{},' please resolve conflicts\n".format(repository.branch, branch_point.branch))
                return 1
            log.warning("Rebased '{}' on '{}!'".format(repository.branch, branch_point.branch))
            branch_point = Branch.branch_point(repository)

        rmt = repository.remote()
        if not rmt:
            sys.stderr.write("'{}' doesn't have a recognized remote\n".format(repository.root_path))
            return 1
        target = 'fork' if isinstance(rmt, remote.GitHub) else 'origin'
        log.warning("Pushing '{}' to '{}'...".format(repository.branch, target))
        if run([repository.executable(), 'push', '-f', target, repository.branch], cwd=repository.root_path).returncode:
            sys.stderr.write("Failed to push '{}' to '{}'\n".format(repository.branch, target))
            return 1

        if not rmt.pull_requests:
            sys.stderr.write("'{}' cannot generate pull-requests\n".format(rmt.url))
            return 1
        candidates = list(rmt.pull_requests.find(opened=None, head=repository.branch))
        commits = list(repository.commits(begin=dict(hash=branch_point.hash), end=dict(branch=repository.branch)))

        if candidates:
            log.warning("Updating pull-request for '{}'...".format(repository.branch))
            pr = rmt.pull_requests.update(
                pull_request=candidates[0],
                title=cls.title_for(commits),
                commits=commits,
                base=branch_point.branch,
                head=repository.branch,
            )
            if not pr:
                sys.stderr.write("Failed to update pull-request '{}'\n".format(candidates[0]))
                return 1
            log.warning("Updated '{}'!".format(pr))
        else:
            log.warning("Creating pull-request for '{}'...".format(repository.branch))
            pr = rmt.pull_requests.create(
                title=cls.title_for(commits),
                commits=commits,
                base=branch_point.branch,
                head=repository.branch,
            )
            if not pr:
                sys.stderr.write("Failed to create pull-request for '{}'\n".format(repository.branch))
                return 1
            log.warning("Created '{}'!".format(pr))

        return 0
