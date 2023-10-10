# Copyright (C) 2023 Apple Inc. All rights reserved.
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

import collections
import getpass
import os
import re
import sys

from .command import Command
from .install_hooks import InstallHooks
from .track import Track
from webkitcorepy import arguments, run, string_utils, Terminal
from webkitscmpy import log, local


class Publish(Command):
    name = 'publish'
    help = "Push a specific branch or tag and all its history to a specified remote."

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'arguments', nargs='+',
            type=str, default=None,
            help='String representation(s) of branches or tags to be published',
        )
        parser.add_argument(
            '--remote', dest='remote', type=str, default=None,
            help='Publish content to the specified remote',
        )
        parser.add_argument(
            '--user', dest='user', type=str, default=None,
            help="Run 'git push' as the specified user with https. Program will prompt for a password.",
        )
        parser.add_argument(
            '--exclude', action='append', type=str, default=[],
            help='Exclude inferred branch or tag from publication',
        )

    @classmethod
    def branches_on(cls, repository, ref, exclude):
        output = run(
            [repository.executable(), 'branch', '-a', '--format', '%(refname)', '--merged', ref],
            cwd=repository.root_path,
            capture_output=True,
            encoding='utf-8',
        )
        if output.returncode:
            sys.stderr.write(output.stderr)
            return {}
        result = collections.defaultdict(set)
        for line in output.stdout.splitlines():
            _, typ, name = line.split('/', 2)
            if typ == 'remotes':
                remote, name = name.split('/', 1)
            else:
                remote = None
            if name in exclude:
                continue
            result[remote].add(name)
        return result

    @classmethod
    def tags_on(cls, repository, ref, exclude):
        result = run(
            [repository.executable(), 'tag', '--merged', ref],
            cwd=repository.root_path,
            capture_output=True,
            encoding='utf-8',
        )
        if not result.returncode:
            return [tag for tag in result.stdout.splitlines() if tag not in exclude]
        sys.stderr.write(result.stderr)
        return []

    @classmethod
    def parental_intersection(cls, repository, commit):
        if commit.branch == repository.default_branch:
            return None
        if commit.identifier <= 1:
            return None
        try:
            parent_branch = repository.find('{}~{}'.format(commit.hash, commit.identifier - 1)).branch
        except (ValueError, repository.Exception):
            return None
        if parent_branch == commit.branch:
            return None
        remote = repository.remote_for(parent_branch)
        intersection = repository.merge_base(commit.hash, 'refs/remotes/{}/{}'.format(remote, parent_branch))
        if intersection.hash == commit.hash:
            return None
        return intersection

    @classmethod
    def _add_branch_ref_to(cls, mapping, repository, branch, commit):
        if branch not in mapping:
            mapping[branch] = commit
        elif mapping[branch].hash != commit.hash and repository.merge_base(
                mapping[branch].hash, commit.hash,
                include_log=False, include_identifier=False
        ).hash == mapping[branch].hash:
            mapping[branch] = commit

    @classmethod
    def main(cls, args, repository, hooks=None, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only 'publish' branches in a git checkout\n")
            return 1
        if hooks and InstallHooks.hook_needs_update(repository, os.path.join(hooks, 'pre-push')):
            sys.stderr.write("Cannot run a command which invokes `git push` with an out-of-date pre-push hook\n")
            sys.stderr.write("Please re-run `git-webkit setup` to update all local hooks\n")
            return 1
        args.exclude.append(repository.default_branch)

        commits = set()
        branches_to_publish = {}
        tags_to_publish = set()
        for ref in args.arguments:
            try:
                if ref in args.exclude:
                    sys.stderr.write("'{}' has been explicitly excluded from publication\n".format(ref))
                    continue
                commit = repository.find(ref)
                commits.add(commit)
                if ref in repository.tags():
                    log.info('    {} is a tag, referencing {}'.format(ref, commit))
                    tags_to_publish.add(ref)
                    continue
                if ref.startswith('ref/remotes/') or ref.startswith('remotes/'):
                    ref = ref.split('remotes/')[-1].split('/', 1)[-1]
                if ref in repository.branches:
                    log.info('    {} is a branch, referencing {}'.format(ref, commit))
                    cls._add_branch_ref_to(
                        mapping=branches_to_publish,
                        repository=repository,
                        branch=ref, commit=commit,
                    )
                else:
                    log.info('    {} references {}'.format(ref, commit))
            except (ValueError, repository.Exception):
                sys.stderr.write("'{}' cannot be found in this repository to be published\n".format(ref))

        if not branches_to_publish and not tags_to_publish and not commits:
            sys.stderr.write("No branches or tags to publish found\n")
            return 1

        requested_refs = tags_to_publish | set(branches_to_publish.keys())

        log.warning('User specified {} and {}, request comprises {}'.format(
            string_utils.pluralize(len(branches_to_publish), 'branch', 'branches'),
            string_utils.pluralize(len(tags_to_publish), 'tag'),
            string_utils.pluralize(len(commits), 'commit'),
        ))

        if not args.remote:
            for commit in commits:
                prevailing_remote = None
                remotes_for = repository.branches_for(hash=commit.hash, remote=None).keys()
                for candidate in repository.source_remotes():
                    if candidate in remotes_for:
                        prevailing_remote = candidate
                        break
                for candidate in reversed(repository.source_remotes()):
                    if candidate == args.remote or candidate == prevailing_remote:
                        args.remote = prevailing_remote
                        break
            if not args.remote:
                args.remote = repository.default_remote
            args.remote = repository.source_remotes()[max(repository.source_remotes().index(args.remote) - 1, 0)]
            log.warning("Inferred '{}' as the target remote".format(args.remote))
        if args.remote not in repository.source_remotes():
            sys.stderr.write("'{}' is not a recognized remote\n".format(args.remote))
            return 1

        existing_tags = set(repository.tags(remote=args.remote))
        tags_to_publish = tags_to_publish - existing_tags
        for commit in commits:
            tags_to_publish |= set(cls.tags_on(repository, commit.hash, exclude=args.exclude)) - existing_tags
            intersection = cls.parental_intersection(repository, commit)
            if intersection and intersection.branch not in args.exclude:
                cls._add_branch_ref_to(
                    mapping=branches_to_publish,
                    repository=repository,
                    branch=intersection.branch, commit=intersection,
                )
            for remote, branches in cls.branches_on(repository, commit.hash, exclude=args.exclude).items():
                if remote is not None and remote not in repository.source_remotes():
                    continue
                for branch in branches:
                    if branch in args.exclude:
                        continue
                    commit = repository.find('remotes/{}/{}'.format(remote, branch) if remote else branch)
                    if commit.branch == repository.default_branch:
                        continue
                    cls._add_branch_ref_to(
                        mapping=branches_to_publish,
                        repository=repository,
                        branch=branch, commit=commit,
                    )

        if tags_to_publish - requested_refs:
            log.warning("Inferred the following tags:")
            for tag in tags_to_publish - requested_refs:
                log.warning('    {}'.format(tag))
            log.warning('')

        if set(branches_to_publish.keys()) - requested_refs:
            log.warning("Inferred the following branches:")
            for branch in set(branches_to_publish.keys()) - requested_refs:
                log.warning('    {} @ {}'.format(branch, branches_to_publish[branch]))
            log.warning('')

        if tags_to_publish:
            print("Pushing the following tags to {}:".format(args.remote))
            for tag in sorted(tags_to_publish):
                print('    {}'.format(tag))
            print('')
        if branches_to_publish:
            print("Pushing the branches at the following commits to {}:".format(args.remote))
            for branch in sorted(branches_to_publish.keys()):
                print('    {} @ {}'.format(branch, branches_to_publish[branch]))
            print('')

        if Terminal.choose(
            "Are you sure you would like to publish to '{}' remote?".format(args.remote),
            default='No',
        ) == 'No':
            sys.stderr.write("Publication canceled\n")
            return 1

        push_env = os.environ.copy()
        push_env['VERBOSITY'] = str(args.verbose)
        push_env['PUSH_HOOK_MODE'] = 'publish'
        remote_arg = args.remote

        if args.user:
            remote = repository.remote(remote_arg)
            if not remote or not getattr(remote, 'domain', None):
                sys.stderr.write("Cannot convert '{}' to an ephemeral HTTP url\n".format(remote_arg))
                return 1
            remote_arg = remote.checkout_url(http=True)

            credentials = (args.user, getpass.getpass('API token for {}: '.format(args.user)).strip())
            tokenized_domain = remote.domain.replace('.', '_').upper()
            push_env['{}_USERNAME'.format(tokenized_domain)] = credentials[0]
            push_env['{}_TOKEN'.format(tokenized_domain)] = credentials[1]

        return_code = 0
        if branches_to_publish:
            print('Pushing branches to {}...'.format(args.remote))
            command = [repository.executable(), 'push', '--atomic', remote_arg] + [
                '{}:refs/heads/{}'.format(commit.hash[:commit.HASH_LABEL_SIZE], branch) for branch, commit in branches_to_publish.items()
            ]
            log.info("Invoking '{}'".format(' '.join(command)))
            if run(
                command,
                cwd=repository.root_path,
                encoding='utf-8',
                env=push_env,
            ).returncode:
                sys.stderr.write('Failed to push branches to {}\n'.format(args.remote))
                return_code += 1

        if not return_code and tags_to_publish:
            print('Pushing tags to {}...'.format(args.remote))
            command = [repository.executable(), 'push', '--atomic', remote_arg] + list(tags_to_publish)
            log.info("Invoking '{}'".format(' '.join(command)))
            if run(
                command,
                cwd=repository.root_path,
                encoding='utf-8',
                env=push_env,
            ).returncode:
                sys.stderr.write('Failed to push tags to {}\n'.format(args.remote))
                return_code += 1

        if return_code:
            print('Publication failed.')
        else:
            print('Publication succeeded!')
        return return_code
