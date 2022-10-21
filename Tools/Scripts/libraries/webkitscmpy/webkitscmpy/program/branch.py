# Copyright (C) 2021, 2022 Apple Inc. All rights reserved.
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

from .command import Command
from .commit import Commit

from webkitbugspy import Tracker, radar
from webkitcorepy import arguments, run, string_utils, Terminal
from webkitscmpy import local, log, remote


class Branch(Command):
    name = 'branch'
    help = 'Create a local development branch from the current checkout state'

    PR_PREFIX = 'eng'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            '-i', '--issue', '-b', '--bug', '-r',
            dest='issue', type=str,
            help='Number (or name) of the issue or bug to create branch for',
        )
        parser.add_argument(
            '-d', '--delete-existing', '--no-delete-existing',
            dest='delete_existing', default=None,
            help='Delete (or do not delete) an existing local branch which collides with the proposed name. '
                 'If pushing to a remote, note that the remote branch of the same name may be overwritten even '
                 'if a local one does not exist.',
            action=arguments.NoAction,
        )

    @classmethod
    def normalize_branch_name(cls, name, repository=None):
        if not name or (repository or local.Scm).DEV_BRANCHES.match(name):
            return name
        return '{}/{}'.format(cls.PR_PREFIX, name)

    @classmethod
    def editable(cls, branch, repository=None):
        if (repository or local.Scm).DEV_BRANCHES.match(branch):
            return True
        if branch in (repository or local.Scm).DEFAULT_BRANCHES:
            return False
        if (repository or local.Scm).PROD_BRANCHES.match(branch):
            return False
        if not repository or not isinstance(repository, local.Git):
            return False

        remote_repo = repository.remote(name=repository.default_remote)
        if remote_repo and isinstance(remote_repo, remote.GitHub):
            for name in repository.source_remotes():
                if branch in repository.branches_for(remote=name, cached=True):
                    return False
        return True

    @classmethod
    def branch_point(cls, repository, limit=None):
        cnt = 0
        commit = repository.commit(include_log=False, include_identifier=False)
        while cls.editable(commit.branch, repository=repository):
            cnt += 1
            if limit and cnt > limit:
                return None
            commit = repository.find(argument='HEAD~{}'.format(cnt), include_log=False, include_identifier=False)
            if cnt > 1 or commit.branch != repository.branch or cls.editable(commit.branch, repository=repository):
                log.info('    Found {}...'.format(string_utils.pluralize(cnt, 'commit')))
            else:
                log.info('    No commits on editable branch')

        return commit

    @classmethod
    def to_branch_name(cls, value):
        result = ''
        for c in string_utils.decode(value):
            if c in [u'-', u' ', u'\n', u'\t', u'.']:
                result += u'-'
            elif c.isalnum() or c == u'_':
                result += c
        return string_utils.encode(result, target_type=str)

    @classmethod
    def main(cls, args, repository, why=None, redact=False, target_remote='fork', **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only 'branch' on a native Git repository\n")
            return 1

        if not args.issue:
            if Tracker.instance():
                prompt = '{}nter issue URL or title of new issue: '.format('{}, e'.format(why) if why else 'E')
            else:
                prompt = '{}nter name of new branch (or issue URL): '.format('{}, e'.format(why) if why else 'E')
            args.issue = Terminal.input(prompt, alert_after=2 * Terminal.RING_INTERVAL)

        if string_utils.decode(args.issue).isnumeric() and Tracker.instance() and not redact:
            issue = Tracker.instance().issue(int(args.issue))
            if issue and issue.title and not issue.redacted:
                args.issue = cls.to_branch_name(issue.title)
        else:
            issue = Tracker.from_string(args.issue)
            if issue and issue.title and not redact and not issue.redacted:
                args.issue = cls.to_branch_name(issue.title)
            elif issue:
                args.issue = str(issue.id)

        if not issue and Tracker.instance():
            if ' ' in args.issue:
                if getattr(Tracker.instance(), 'credentials', None):
                    Tracker.instance().credentials(required=True, validate=True)
                issue = Tracker.instance().create(
                    title=args.issue,
                    description=Terminal.input('Issue description: '),
                )
                if not issue:
                    sys.stderr.write('Failed to create new issue\n')
                    return 1
                print("Created '{}'".format(issue))
                if issue and issue.title and not redact and not issue.redacted:
                    args.issue = cls.to_branch_name(issue.title)
                elif issue:
                    args.issue = str(issue.id)
            else:
                log.warning("'{}' has no spaces, assuming user intends it to be a branch name".format(args.issue))

        needs_radar = issue and not isinstance(issue.tracker, radar.Tracker)
        needs_radar = needs_radar and any([
            isinstance(tracker, radar.Tracker) and tracker.radarclient()
            for tracker in Tracker._trackers
        ])
        needs_radar = needs_radar and not any([
            isinstance(reference.tracker, radar.Tracker)
            for reference in issue.references
        ])

        if needs_radar:
            rdar = None
            if not getattr(args, 'defaults', None):
                sys.stdout.write('Existing radar to CC (leave empty to create new radar)')
                sys.stdout.flush()
                input = Terminal.input(': ')
                if re.match(r'\d+', input):
                    input = '<rdar://problem/{}>'.format(input)
                rdar = Tracker.from_string(input)
            issue.cc_radar(block=True, radar=rdar)

        if issue and not isinstance(issue.tracker, radar.Tracker):
            args._title = issue.title
            args._bug_urls = Commit.bug_urls(issue)

        args.issue = cls.normalize_branch_name(args.issue)

        if run([repository.executable(), 'check-ref-format', args.issue], capture_output=True).returncode:
            sys.stderr.write("'{}' is an invalid branch name, cannot create it\n".format(args.issue))
            return 1

        if args.issue in repository.branches_for(remote=target_remote):
            if not args.delete_existing:
                sys.stderr.write("'{}' exists on the remote '{}' and will be overwritten by a push\n".format(args.issue, target_remote))
                if args.delete_existing is False:
                    return 1

        if args.issue in repository.branches_for(remote=False):
            if args.delete_existing:
                log.info("Locally deleting existing branch '{}'".format(args.issue))
                if run([repository.executable(), 'branch', '-D', args.issue], cwd=repository.root_path).returncode:
                    sys.stderr.write("Failed to locally delete '{}'\n".format(args.issue))
            elif args.delete_existing is None:
                log.warning("Rebasing existing branch '{}' instead of creating a new one".format(args.issue))
                if run([repository.executable(), 'rebase', 'HEAD', args.issue, '--autostash'], cwd=repository.root_path).returncode:
                    return 1
                print("Rebased the local development branch '{}'".format(args.issue))
                return 0
            else:
                sys.stderr.write("'{}' already exists in this checkout\n".format(args.issue))
                return 1

        log.info("Creating the local development branch '{}'...".format(args.issue))
        if run([repository.executable(), 'checkout', '-b', args.issue], cwd=repository.root_path).returncode:
            sys.stderr.write("Failed to create '{}'\n".format(args.issue))
            return 1
        repository._branch = args.issue  # Assign the cache because of repository.branch's caching
        print("Created the local development branch '{}'".format(args.issue))
        return 0
