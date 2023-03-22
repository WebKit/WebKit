# Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
import subprocess
import sys
import time

from .canonicalize import Canonicalize
from .command import Command
from .branch import Branch
from .install_hooks import InstallHooks
from .pull_request import PullRequest
from .squash import Squash
from argparse import Namespace
from webkitbugspy import Tracker
from webkitcorepy import arguments, run, string_utils, Terminal
from webkitscmpy import Commit, local, log, remote


class Land(Command):
    name = 'land'
    help = 'If on a pull-request or commit-queue branch, rebase the ' \
           'current branch onto the target production branch and push.'

    OOPS_RE = re.compile(r'\(O+P+S!*\)')
    REVIEWED_BY_RE = re.compile('Reviewed by (?P<approver>.+)')
    GIT_SVN_COMMITTED_RE = re.compile(r'Committed r(?P<revision>\d+)')
    REMOTE = 'origin'
    MIRROR_TIMEOUT = 60

    @classmethod
    def revert_branch(cls, repository, remote, branch):
        if run(
            [repository.executable(), 'branch', '-f', branch, 'remotes/{}/{}'.format(remote, branch)],
            cwd=repository.root_path,
        ).returncode:
            return False
        return True

    @classmethod
    def parser(cls, parser, loggers=None):
        PullRequest.parser(parser, loggers=loggers)
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
            '--safe', '--unsafe',
            dest='safe', default=None,
            help='Land change via safe (or unsafe) merge queue, if available.',
            action=arguments.NoAction,
        )

    @classmethod
    def merge_queue(cls, args, repository, branch_point, merge_labels=None):
        log.info('Detected merging automation, using that instead of local git tooling')
        merge_type = {
            True: 'safe',
            False: 'unsafe',
        }.get(args.safe, sorted(merge_labels.keys())[0])
        merge_label = merge_labels.get(merge_type)
        if not merge_label:
            sys.stderr.write("No {} merge-queue available for this repository\n".format(merge_type))
            return 1

        def callback(pr):
            pr_issue = pr._metadata.get('issue')
            if not pr_issue:
                sys.stderr.write("Cannot set any labels on '{}' because the service doesn't support labels\n".format(pr))
                return 1

            labels = pr_issue.labels
            if PullRequest.BLOCKED_LABEL in labels and merge_type == 'unsafe':
                log.info("Removing '{}' from PR {}...".format(PullRequest.BLOCKED_LABEL, pr.number))
                labels.remove(PullRequest.BLOCKED_LABEL)
            log.info("Adding '{}' to '{}'".format(merge_label, pr))
            labels.append(merge_label)
            if pr_issue.set_labels(labels):
                print("Added '{}' to '{}', change is in the queue to be landed".format(merge_label, pr))
                return 0
            sys.stderr.write("Failed to add '{}' to '{}', change is not landing\n".format(merge_label, pr))
            if pr.url:
                sys.stderr.write("See if you can add the label manually on '{}'".format(pr.url))
            return 1

        return PullRequest.create_pull_request(
            repository, args, branch_point,
            callback=callback,
            unblock=True if merge_type == 'unsafe' else False,
            update_issue=False,  # If we're immediately landing, no reason to track the code change as a WIP
        )

    @classmethod
    def main(cls, args, repository, identifier_template=None, canonical_svn=False, hooks=None, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1

        if not repository.path:
            sys.stderr.write("Cannot 'land' change in remote repository\n")
            return 1

        if not isinstance(repository, local.Git):
            sys.stderr.write("'land' only supported by local git repositories\n")
            return 1

        if canonical_svn and not repository.is_svn:
            sys.stderr.write("Cannot 'land' on a canonical SVN repository that is not configured as git-svn\n")
            return 1

        if not PullRequest.check_pull_request_args(repository, args):
            return 1

        if hooks and InstallHooks.hook_needs_update(repository, os.path.join(hooks, 'pre-push')):
            sys.stderr.write("Cannot run a command which invokes `git push` with an out-of-date pre-push hook\n")
            sys.stderr.write("Please re-run `git-webkit setup` to update all local hooks\n")
            return 1

        modified_files = [] if args.will_add is False else repository.modified()
        if args.will_add:
            modified_files = list(set(modified_files).union(set(repository.modified(staged=False))))
        if not Branch.editable(repository.branch, repository=repository) and not modified_files:
            sys.stderr.write("Can only 'land' editable branches\n")
            return 1

        branch_point = PullRequest.pull_request_branch_point(repository, args, **kwargs)
        if not branch_point:
            return 1
        source_branch = repository.branch
        if not Branch.editable(source_branch, repository=repository):
            sys.stderr.write("Can only 'land' editable branches\n")
            return 1

        result = PullRequest.create_commit(args, repository, **kwargs)
        if result:
            return result

        commits = list(repository.commits(begin=dict(hash=branch_point.hash), end=dict(branch=source_branch)))
        if not commits:
            sys.stderr.write('Failed to find commits to land\n')
            return 1

        if args.squash or (args.squash is None and len(commits) > 1):
            result = Squash.squash_commit(args, repository, branch_point, **kwargs)
            if result:
                return result
            commits = list(repository.commits(begin=dict(hash=branch_point.hash), end=dict(branch=source_branch)))

        rmt = repository.remote()
        if rmt and isinstance(rmt, remote.GitHub):
            merge_labels = dict()
            for name in rmt.tracker.labels.keys():
                if name in PullRequest.MERGE_LABELS:
                    merge_labels['safe'] = name
                if name in PullRequest.UNSAFE_MERGE_LABELS:
                    merge_labels['unsafe'] = name
            if merge_labels:
                return cls.merge_queue(args, repository, branch_point, merge_labels=merge_labels)

        if args.safe is not None:
            sys.stderr.write("No merge-queue available for this repository\n")
            return 1

        pull_request = None
        if rmt and rmt.pull_requests:
            candidates = list(rmt.pull_requests.find(opened=True, head=source_branch))
            if len(candidates) == 1:
                pull_request = candidates[0]
            elif candidates:
                sys.stderr.write("Multiple pull-request match '{}'\n".format(source_branch))

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
                log.info("Setting {} as reviewer{}".format(
                    string_utils.join([p.name for p in pull_request.approvers]),
                    's' if len(pull_request.approvers) > 1 else '',
                ))
                if run([
                    repository.executable(), 'filter-branch', '-f',
                    '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}'".format(
                        date='{} -{}'.format(int(time.time()), repository.gmtoffset())
                    ), '--msg-filter', 'sed "s/NOBODY (OO*PP*S!*)/{}/g"'.format(string_utils.join([p.name for p in pull_request.approvers])),
                    '{}...{}'.format(source_branch, branch_point.hash),
                ], cwd=repository.root_path, env={'FILTER_BRANCH_SQUELCH_WARNING': '1'}, capture_output=True).returncode:
                    sys.stderr.write('Failed to set reviewers\n')
                    return 1
                commits = list(repository.commits(begin=dict(hash=branch_point.hash), end=dict(branch=source_branch)))
                if not commits:
                    sys.stderr.write('Failed to find commits after setting reviewers\n')
                    return 1

        elif not pull_request:
            sys.stderr.write("Failed to find pull-request associated with '{}'\n".format(source_branch))

        if not args.oops and any([cls.OOPS_RE.search(commit.message) for commit in commits if commit.message]):
            sys.stderr.write("Found '(OOPS!)' message in commit messages, please resolve before committing\n")
            return 1

        if not args.oops:
            for line in repository.diff_lines(branch_point.hash, source_branch):
                if cls.OOPS_RE.search(line):
                    sys.stderr.write("Found '(OOPS!)' in commit diff, please resolve before committing\n")
                    return 1

        issue = None
        for line in commits[0].message.split() if commits[0] and commits[0].message else []:
            issue = Tracker.from_string(line)
            if issue:
                break

        target = pull_request.base if pull_request else branch_point.branch
        log.info("Rebasing '{}' from '{}' to '{}'...".format(source_branch, branch_point.branch, target))
        if repository.fetch(branch=target, remote=cls.REMOTE):
            sys.stderr.write("Failed to fetch '{}' from '{}'\n".format(target, cls.REMOTE))
            return 1
        if repository.rebase(target=target, base=branch_point.branch, head=source_branch):
            sys.stderr.write("Failed to rebase '{}' onto '{}', please resolve conflicts\n".format(source_branch, target))
            return 1
        log.info("Rebased '{}' from '{}' to '{}'!".format(source_branch, branch_point.branch, target))

        if run([repository.executable(), 'branch', '-f', target, source_branch], cwd=repository.root_path).returncode:
            sys.stderr.write("Failed to move '{}' ref\n".format(target))
            return 1 if cls.revert_branch(repository, cls.REMOTE, target) else -1

        if identifier_template:
            repository.checkout(target)
            if Canonicalize.main(Namespace(
                identifier=True, remote=cls.REMOTE, number=len(commits),
            ), repository, identifier_template=identifier_template):
                sys.stderr.write("Failed to embed identifiers to '{}'\n".format(target))
                return 1 if cls.revert_branch(repository, cls.REMOTE, target) else -1
            if run([repository.executable(), 'branch', '-f', source_branch, target], cwd=repository.root_path).returncode:
                sys.stderr.write("Failed to move '{}' ref to the canonicalized head of '{}'\n".format(source, target))
                cls.revert_branch(repository, cls.REMOTE, target)
                return -1

        # Need to compute the remote source
        remote_target = 'fork' if isinstance(rmt, remote.GitHub) else 'origin'

        push_env = os.environ.copy()
        push_env['VERBOSITY'] = str(args.verbose)

        if canonical_svn:
            if run([repository.executable(), 'svn', 'fetch'], cwd=repository.root_path).returncode:
                sys.stderr.write("Failed to update subversion refs\n".format(target))
                return 1 if cls.revert_branch(repository, cls.REMOTE, target) else -1

            dcommit = run(
                [repository.executable(), 'svn', 'dcommit'],
                cwd=repository.root_path,
                stdout=subprocess.PIPE,
                encoding='utf-8',
            )
            if dcommit.returncode:
                sys.stderr.write(dcommit.stdout)
                sys.stderr.write(dcommit.stderr)
                sys.stderr.write("Failed to commit '{}' to Subversion remote\n".format(target))
                return 1 if cls.revert_branch(repository, cls.REMOTE, target) else -1
            revisions = []
            for line in dcommit.stdout.splitlines():
                match = cls.GIT_SVN_COMMITTED_RE.match(line)
                if not match:
                    continue
                revisions.append(int(match.group('revision')))
            if not revisions:
                sys.stderr.write(dcommit.stdout)
                sys.stderr.write(dcommit.stderr)
                sys.stderr.write("Failed to find revision in '{}' when committing to Subversion remote\n".format(target))
                return 1 if cls.revert_branch(repository, cls.REMOTE, target) else -1

            run([repository.executable(), 'reset', 'HEAD~{}'.format(len(commits)), '--hard'], cwd=repository.root_path)

            # Verify the mirror processed our change
            started = time.time()
            latest = repository.find('HEAD', include_log=True, include_identifier=False)
            while latest.revision < revisions[-1]:
                if time.time() - started > cls.MIRROR_TIMEOUT:
                    sys.stderr.write("Timed out waiting for the git-svn mirror, '{}' landed but not closed\n".format(pull_request or source_branch))
                    return 1
                log.info('    Verifying mirror processesed change')
                time.sleep(5)
                run([repository.executable(), 'pull'], cwd=repository.root_path)
                latest = repository.find('HEAD', include_log=True, include_identifier=False)
            if repository.cache and target in repository.cache._last_populated:
                del repository.cache._last_populated[target]

            commits = []
            for revision in revisions:
                commits.append(repository.commit(revision=revision, include_log=True))
            commit = commits[-1]

            if pull_request:
                run([repository.executable(), 'branch', '-f', source_branch, target], cwd=repository.root_path)
                run([repository.executable(), 'push', '-f', remote_target, source_branch], cwd=repository.root_path, env=push_env)
                rmt.pull_requests.update(
                    pull_request=pull_request,
                    title=PullRequest.title_for(commits),
                    commits=commits,
                    base=branch_point.branch,
                    head=source_branch,
                )

        else:
            if pull_request:
                log.info("Updating '{}' to match landing commits...".format(pull_request))
                commits = list(repository.commits(begin=dict(argument='{}~{}'.format(source_branch, len(commits))), end=dict(branch=source_branch)))
                run([repository.executable(), 'push', '-f', remote_target, source_branch], cwd=repository.root_path, env=push_env)
                rmt.pull_requests.update(
                    pull_request=pull_request,
                    title=PullRequest.title_for(commits),
                    commits=commits,
                    base=branch_point.branch,
                    head=source_branch,
                )

            if run([repository.executable(), 'push', cls.REMOTE, target], cwd=repository.root_path, env=push_env).returncode:
                sys.stderr.write("Failed to push '{}' to '{}'\n".format(target, cls.REMOTE))
                return 1 if cls.revert_branch(repository, cls.REMOTE, target) else -1
            repository.checkout(target)
            commit = repository.commit(branch=target, include_log=False)

        if identifier_template and commit.identifier:
            land_message = 'Landed {} ({})!'.format(identifier_template.format(commit).split(': ')[-1], commit.hash[:Commit.HASH_LABEL_SIZE])
        else:
            land_message = 'Landed {}!'.format(commit.hash[:Commit.HASH_LABEL_SIZE])
        print(land_message)

        if pull_request:
            pull_request.comment(land_message)
        if issue:
            if canonical_svn and commit.revision:
                land_message = land_message.replace(commit.hash[:Commit.HASH_LABEL_SIZE], 'r{}'.format(commit.revision))
            issue.close(why=land_message)

        if args.defaults or Terminal.choose("Delete branch '{}'?".format(source_branch), default='Yes') == 'Yes':
            regex = re.compile(r'^{}-(?P<count>\d+)$'.format(source_branch))
            for to_delete in repository.branches_for(remote=remote_target):
                if to_delete == source_branch or regex.match(to_delete) and remote_target == 'fork':
                    run([repository.executable(), 'branch', '-D', to_delete], cwd=repository.root_path)
                    run([repository.executable(), 'push', remote_target, '--delete', to_delete], cwd=repository.root_path, env=push_env)
        return 0
