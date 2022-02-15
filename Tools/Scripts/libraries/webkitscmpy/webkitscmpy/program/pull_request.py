# Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
import re
import sys

from .command import Command
from .branch import Branch

from webkitbugspy import Tracker
from webkitcorepy import arguments, run, Terminal
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
            '--rebase', '--no-rebase', '--update', '--no-update',
            dest='rebase', default=None,
            help='Rebase (or do not rebase) the pull-request on the source branch before pushing',
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--defaults', '--no-defaults', action=arguments.NoAction, default=None,
            help='Do not prompt the user for defaults, always use (or do not use) them',
        )
        parser.add_argument(
            '--overwrite', '--amend', action='store_const', const='overwrite',
            dest='technique', default=None,
            help='When creating a pull request, overwrite the existing commit by default',
        )
        parser.add_argument(
            '--append', action='store_const', const='append',
            dest='technique', default=None,
            help='When creating a pull request, append a new commit on the existing branch by default',
        )
        parser.add_argument(
            '--with-history', '--no-history',
            dest='history', default=None,
            help='Create numbered branches to track the history of a change',
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--draft', dest='draft', action='store_true', default=None,
            help='Mark a pull request as a draft when creating it',
        )

    @classmethod
    def create_commit(cls, args, repository, **kwargs):
        # First, find the set of files to be modified
        modified = [] if args.will_add is False else repository.modified()
        if args.will_add:
            modified = list(set(modified).union(set(repository.modified(staged=False))))

        # Next, add all modified file
        for file in set(modified) - set(repository.modified(staged=True)):
            log.info('    Adding {}...'.format(file))
            if run([repository.executable(), 'add', file], cwd=repository.root_path).returncode:
                sys.stderr.write("Failed to add '{}'\n".format(file))
                return 1

        # Then, see if we already have a commit associated with this branch we need to modify
        has_commit = repository.commit(include_log=False, include_identifier=False).branch == repository.branch and repository.branch != repository.default_branch
        if not modified and has_commit:
            log.info('Using committed changes...')
            return 0

        # Otherwise, we need to create a commit
        will_amend = has_commit and args.technique == 'overwrite'
        if not modified:
            sys.stderr.write('No modified files\n')
            return 1
        log.info('Amending commit...' if will_amend else 'Creating commit...')
        if run([repository.executable(), 'commit', '--date=now'] + (['--amend'] if will_amend else []), cwd=repository.root_path).returncode:
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

        if not args.technique:
            args.technique = repository.config()['webkitscmpy.pull-request']
        if args.history is None:
            args.history = dict(
                always=True,
                disabled=False,
                never=False,
            ).get(repository.config()['webkitscmpy.history'])
        if args.history and repository.config()['webkitscmpy.history'] == 'never':
            sys.stderr.write('History retention was requested, but repository configuration forbids it\n')
            return 1

        if not repository.DEV_BRANCHES.match(repository.branch):
            if Branch.main(args, repository, why="'{}' is not a pull request branch".format(repository.branch), **kwargs):
                sys.stderr.write("Abandoning pushing pull-request because '{}' could not be created\n".format(args.issue))
                return 1
        elif args.issue and repository.branch != args.issue:
            sys.stderr.write("Creating a pull-request for '{}' but we're on '{}'\n".format(args.issue, repository.branch))
            return 1

        # FIXME: Source remote will not always be origin
        source_remote = 'origin'
        branch_point = Branch.branch_point(repository)
        if run([
            repository.executable(), 'branch', '-f',
            branch_point.branch,
            'remotes/{}/{}'.format(source_remote, branch_point.branch),
        ], cwd=repository.root_path).returncode:
            sys.stderr.write("Failed to match '{}' to it's remote '{}'\n".format(branch_point.branch, source_remote))
            return 1

        result = cls.create_commit(args, repository, **kwargs)
        if result:
            return result

        if args.rebase or (args.rebase is None and repository.config().get('pull.rebase')):
            log.info("Rebasing '{}' on '{}'...".format(repository.branch, branch_point.branch))
            if repository.pull(rebase=True, branch=branch_point.branch):
                sys.stderr.write("Failed to rebase '{}' on '{},' please resolve conflicts\n".format(repository.branch, branch_point.branch))
                return 1
            log.info("Rebased '{}' on '{}!'".format(repository.branch, branch_point.branch))
            branch_point = Branch.branch_point(repository)
        else:
            branch_point = Branch.branch_point(repository)

        rmt = repository.remote(name=source_remote)
        if not rmt:
            sys.stderr.write("'{}' doesn't have a recognized remote\n".format(repository.root_path))
            return 1
        target = 'fork' if isinstance(rmt, remote.GitHub) else source_remote
        log.info("Pushing '{}' to '{}'...".format(repository.branch, target))
        if run([repository.executable(), 'push', '-f', target, repository.branch], cwd=repository.root_path).returncode:
            sys.stderr.write("Failed to push '{}' to '{}' (alias of '{}')\n".format(repository.branch, target, repository.url(name=target)))
            sys.stderr.write("Your checkout may be mis-configured, try re-running 'git-webkit setup' or\n")
            sys.stderr.write("your checkout may not have permission to push to '{}'\n".format(repository.url(name=target)))
            return 1

        if args.history or (target != source_remote and args.history is None and args.technique == 'overwrite'):
            regex = re.compile(r'^{}-(?P<count>\d+)$'.format(repository.branch))
            count = max([
                int(regex.match(branch).group('count')) if regex.match(branch) else 0 for branch in
                repository.branches_for(remote=target)
            ] + [0]) + 1

            history_branch = '{}-{}'.format(repository.branch, count)
            log.info("Creating '{}' as a reference branch".format(history_branch))
            if run([
                repository.executable(), 'branch', history_branch, repository.branch,
            ], cwd=repository.root_path).returncode or run([
                repository.executable(), 'push', '-f', target, history_branch,
            ], cwd=repository.root_path).returncode:
                sys.stderr.write("Failed to create and push '{}' to '{}'\n".format(history_branch, target))

        if not rmt.pull_requests:
            sys.stderr.write("'{}' cannot generate pull-requests\n".format(rmt.url))
            return 1
        if args.draft and not rmt.pull_requests.SUPPORTS_DRAFTS:
            sys.stderr.write("'{}' does not support draft pull requests, aborting\n".format(rmt.url))
            return 1

        existing_pr = None
        for pr in rmt.pull_requests.find(opened=None, head=repository.branch):
            existing_pr = pr
            if existing_pr.opened:
                continue
        if existing_pr and not existing_pr.opened and not args.defaults and (args.defaults is False or Terminal.choose(
            "'{}' is already associated with '{}', which is closed.\nWould you like to create a new pull-request?".format(
                repository.branch, existing_pr,
            ), default='No',
        ) == 'Yes'):
            existing_pr = None
        commits = list(repository.commits(begin=dict(hash=branch_point.hash), end=dict(branch=repository.branch)))

        issue = None
        for line in commits[0].message.split() if commits[0] and commits[0].message else []:
            issue = Tracker.from_string(line)
            if issue:
                break

        if existing_pr:
            log.info("Updating pull-request for '{}'...".format(repository.branch))
            pr = rmt.pull_requests.update(
                pull_request=existing_pr,
                title=cls.title_for(commits),
                commits=commits,
                base=branch_point.branch,
                head=repository.branch,
                opened=None if existing_pr.opened else True,
                draft=args.draft,
            )
            if not pr:
                sys.stderr.write("Failed to update pull-request '{}'\n".format(existing_pr))
                return 1
            print("Updated '{}'!".format(pr))
        else:
            log.info("Creating pull-request for '{}'...".format(repository.branch))
            pr = rmt.pull_requests.create(
                title=cls.title_for(commits),
                commits=commits,
                base=branch_point.branch,
                head=repository.branch,
                draft=args.draft,
            )
            if not pr:
                sys.stderr.write("Failed to create pull-request for '{}'\n".format(repository.branch))
                return 1
            print("Created '{}'!".format(pr))

        if issue:
            log.info('Checking issue assignee...')
            if issue.assignee != issue.tracker.me():
                issue.assign(issue.tracker.me())
                print('Assigning associated issue to {}'.format(issue.tracker.me()))
            log.info('Checking for pull request link in associated issue...')
            if pr.url and not any([pr.url in comment.content for comment in issue.comments]):
                issue.add_comment('Pull request: {}'.format(pr.url))
                print('Posted pull request link to {}'.format(issue.link))

        if pr.url:
            print(pr.url)

        return 0
