# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import argparse
import os
import re
import sys

from .command import Command
from webkitbugspy import Tracker, radar
from webkitcorepy import arguments, string_utils, Terminal
from webkitscmpy import Commit, ScmBase, local


class MarkFixed(Command):
    name = 'mark-fixed'
    help = 'Record landed commits in the issues they reference, moving radars to Integrate or Verify'

    REMOTE = 'origin'
    RADAR_STATES = ['Analyze', 'Integrate', 'Build', 'Verify', 'Closed']
    WORD_RE = re.compile(r'[^\s"\'()\[\]<>]+')

    @classmethod
    def radar_state_parser(cls, parser):
        parser.add_argument(
            '--radar-state',
            dest='radar_state', default=None, choices=local.Git.PROJECT_CONFIG_OPTIONS['webkitscmpy.land-radar-state'],
            help=("State to move landed radars to, defaults to 'webkitscmpy.land-radar-state' or Verify"
                  if radar.Tracker.radarclient() is not None
                  else argparse.SUPPRESS),
        )

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'arguments', nargs='*',
            type=str, default=['HEAD'],
            help='Landed commits or commit ranges (A..B) to record, defaults to HEAD',
        )
        parser.add_argument(
            '--issue',
            dest='issues', action='append', default=None,
            help='Issue to update instead of the ones the commits reference, may be repeated',
        )
        cls.radar_state_parser(parser)
        parser.add_argument(
            '--update-radar', '--no-update-radar',
            dest='update_radar', default=True,
            help=('Move radars to the landed state'
                  if radar.Tracker.radarclient() is not None
                  else argparse.SUPPRESS),
            action=arguments.NoAction,
        )
        parser.add_argument(
            '--defaults', '--no-defaults', action=arguments.NoAction, default=None,
            help='Do not prompt the user for defaults, always use (or do not use) them',
        )

    @classmethod
    def issue_for(cls, commit):
        seen_empty = False
        for index, line in enumerate((commit.message or '').splitlines()):
            if not line and seen_empty:
                break
            seen_empty = seen_empty or not line
            if index == 0:
                # A revert's quoted title names the issue of the change it reverts.
                line = line.split('Revert "')[0]
            for word in cls.WORD_RE.findall(line):
                issue = Tracker.from_string(word.strip('.,;:/'))
                if issue:
                    return issue
        return None

    @classmethod
    def commits_by_issue(cls, commits):
        result = {}
        for commit in commits:
            issue = cls.issue_for(commit)
            if issue:
                result.setdefault(issue, []).append(commit)
        return result

    @classmethod
    def issues_for(cls, commits):
        return list(cls.commits_by_issue(commits))

    @classmethod
    def radar_state(cls, args, repository):
        return args.radar_state or repository.config().get('webkitscmpy.land-radar-state', 'Verify')

    @classmethod
    def should_move(cls, args, issue, state):
        return args.update_radar and isinstance(issue.tracker, radar.Tracker) and issue.state in cls.RADAR_STATES[:cls.RADAR_STATES.index(state)]

    @classmethod
    def is_gardening(cls, repository, commits):
        if not repository.classifier or not repository.classifier.classes:
            return False
        return all(getattr(repository.classifier.classify(commit, repository), 'name', None) == 'Gardening' for commit in commits)

    @classmethod
    def held_issues(cls, args, repository, commits, issues, state):
        commits_for_issue = cls.commits_by_issue(commits)
        held = []
        for issue in issues:
            if not cls.should_move(args, issue, state) or not cls.is_gardening(repository, commits_for_issue.get(issue, commits)):
                continue
            # A radar titled like a gardening commit is gardening work, so a gardening change can finish it.
            finished = bool(repository.classifier.classify(Commit(message=issue.title or '')))
            if not args.defaults:
                finished = Terminal.choose(
                    'Move {} to {}? It only has gardening changes'.format(issue.link, state),
                    default='Yes' if finished else 'No',
                ) == 'Yes'
            if not finished:
                held.append(issue)
        return held

    @classmethod
    def label(cls, commit, identifier_template=None, canonical_svn=False):
        hash = 'r{}'.format(commit.revision) if canonical_svn and commit.revision else commit.hash[:Commit.HASH_LABEL_SIZE]
        if identifier_template and commit.identifier:
            return '{} ({})'.format(identifier_template.value_template.format(commit), hash)
        return hash

    @classmethod
    def comment(cls, repository, commits, branch, identifier_template=None, canonical_svn=False):
        rmt = repository.remote()
        result = 'Committed {} to {} ({}/{}).'.format(
            string_utils.join([cls.label(commit, identifier_template, canonical_svn) for commit in commits]),
            rmt.url if rmt else repository.url(), cls.REMOTE, branch,
        )
        urls = [rmt.commit_url(commit.hash) for commit in commits] if rmt else []
        if urls and all(urls):
            result += '\n\n' + '\n'.join(urls)
        return result

    @classmethod
    def confirm(cls, args, issues):
        if len(issues) > 1 and not args.defaults and Terminal.choose(
            'Update {} once landed?'.format(string_utils.join([issue.link for issue in issues])), default='Yes',
        ) == 'No':
            sys.stderr.write("Pass '--no-issue' to land without updating issues\n")
            return False
        return True

    @classmethod
    def validate(cls, args, issues, held, state):
        for issue in issues:
            if not isinstance(issue.tracker, radar.Tracker):
                continue
            if not issue.state:
                sys.stderr.write("Failed to read {}, pass '--no-issue' to land without updating it\n".format(issue.link))
                return False
            if cls.should_move(args, issue, state) and issue not in held and not issue.milestone:
                sys.stderr.write("{} has no milestone, set one or pass '--no-update-radar' to land without moving it to {}\n".format(issue.link, state))
                return False
        return True

    @classmethod
    def update_issues(cls, args, repository, commits, branch, issues=None, held=None, identifier_template=None, canonical_svn=False):
        state = cls.radar_state(args, repository)
        name = os.path.splitext(os.path.basename(repository.url()))[0]
        name = name[:1].upper() + name[1:]

        commits_for_issue = cls.commits_by_issue(commits)
        issues = issues or list(commits_for_issue)
        if held is None:
            held = cls.held_issues(args, repository, commits, issues, state)

        did_succeed = True
        for issue in issues:
            # Squashing combines commits, so an issue that no longer leads a commit belongs to all of them.
            issue_commits = commits_for_issue.get(issue, commits)
            why = cls.comment(repository, issue_commits, branch, identifier_template=identifier_template, canonical_svn=canonical_svn)
            if not isinstance(issue.tracker, radar.Tracker):
                issue.close(why=why)
                continue
            try:
                for commit in issue_commits:
                    issue.add_source_change('{}, merge, {}'.format(name, commit.hash))
                if not any(comment.content == why for comment in issue.comments or []):
                    issue.add_comment(why)
                if not cls.should_move(args, issue, state):
                    continue
                if issue in held:
                    print('Not moving {} to {} for a gardening change'.format(issue.link, state))
                elif not issue.milestone:
                    sys.stderr.write('{} has no milestone, leaving it in {}\n'.format(issue.link, issue.state))
                    did_succeed = False
                elif issue.set_state(state):
                    print('Moved {} to {}'.format(issue.link, state))
            except issue.tracker.radarclient().exceptions.UnsuccessfulResponseException as error:
                sys.stderr.write('Failed to update {}:\n{}\n'.format(issue.link, error))
                did_succeed = False
        return did_succeed

    @classmethod
    def main(cls, args, repository, identifier_template=None, canonical_svn=False, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not isinstance(repository, local.Git):
            sys.stderr.write("'{}' only supported by local git repositories\n".format(cls.name))
            return 1

        commits = []
        for argument in args.arguments:
            try:
                if '..' in argument:
                    begin, end = argument.split('..', 1)
                    commits += list(repository.commits(begin=dict(argument=begin), end=dict(argument=end)))
                else:
                    commits.append(repository.find(argument, include_log=True))
            except (ScmBase.Exception, TypeError, ValueError) as exception:
                sys.stderr.write('{}\n'.format(exception))
                return 1
        if not commits:
            sys.stderr.write('No commits found in {}\n'.format(string_utils.join(args.arguments)))
            return 1
        for commit in commits:
            if commit.branch not in repository.branches_for(hash=commit.hash, remote=cls.REMOTE):
                sys.stderr.write("'{}' has not been pushed to '{}'\n".format(commit, cls.REMOTE))
                return 1

        issues = []
        for link in args.issues or []:
            issue = Tracker.from_string(link)
            if not issue:
                sys.stderr.write("'{}' is not an issue this repository tracks\n".format(link))
                return 1
            issues.append(issue)

        branches = {commit.branch for commit in commits}
        if len(branches) > 1:
            sys.stderr.write('Commits landed on different branches: {}\n'.format(string_utils.join(sorted(branches))))
            return 1
        if not issues and not cls.issues_for(commits):
            sys.stderr.write('No issues referenced by {}\n'.format(string_utils.join([str(commit) for commit in commits])))
            return 1

        if cls.update_issues(
            args, repository, commits, branches.pop(), issues=issues,
            identifier_template=identifier_template, canonical_svn=canonical_svn,
        ):
            return 0
        return 1
