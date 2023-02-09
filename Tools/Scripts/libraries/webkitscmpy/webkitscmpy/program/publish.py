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
import re
import sys

from .command import Command
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

    @classmethod
    def branches_on(cls, repository, ref):
        output = run(
            [repository.executable(), 'branch', '-a', '--merged', ref],
            cwd=repository.root_path,
            capture_output=True,
            encoding='utf-8',
        )
        if output.returncode:
            sys.stderr.write(output.stderr)
            return {}
        result = collections.defaultdict(set)
        for line in output.stdout.splitlines():
            split = line.lstrip().split('/', 2)
            if len(split) < 3 or split[0] != 'remotes':
                result[None].add('/'.join(split))
            else:
                result[split[1]].add(split[2])
        return result

    @classmethod
    def tags_on(cls, repository, ref):
        result = run(
            [repository.executable(), 'tag', '--merged', ref],
            cwd=repository.root_path,
            capture_output=True,
            encoding='utf-8',
        )
        if not result.returncode:
            return result.stdout.splitlines()
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
    def main(cls, args, repository, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only 'publish' branches in a git checkout\n")
            return 1

        commits = set()
        branches_to_publish = {}
        tags_to_publish = set()
        for ref in args.arguments:
            try:
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
            tags_to_publish |= set(cls.tags_on(repository, commit.hash)) - existing_tags
            intersection = cls.parental_intersection(repository, commit)
            if intersection and intersection.branch != repository.default_branch:
                cls._add_branch_ref_to(
                    mapping=branches_to_publish,
                    repository=repository,
                    branch=intersection.branch, commit=intersection,
                )
            for remote, branches in cls.branches_on(repository, commit.hash).items():
                if remote is not None and remote not in repository.source_remotes():
                    continue
                for branch in branches:
                    commit = repository.find('remotes/{}/{}'.format(remote, branch) if remote else branch)
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

        return_code = 0
        print('Pushing branches to {}...'.format(args.remote))
        command = [repository.executable(), 'push', '--atomic', args.remote] + [
            '{}:refs/heads/{}'.format(commit.hash[:commit.HASH_LABEL_SIZE], branch) for branch, commit in branches_to_publish.items()
        ]
        log.info("Invoking '{}'".format(' '.join(command)))
        if run(
            command,
            cwd=repository.root_path,
            capture_output=True,
            encoding='utf-8',
        ).returncode:
            sys.stderr.write('Failed to push branches to {}\n'.format(args.remote))
            return_code += 1

        if tags_to_publish:
            print('Pushing tags to {}...'.format(args.remote))
            command = [repository.executable(), 'push', '--atomic', args.remote] + list(tags_to_publish)
            log.info("Invoking '{}'".format(' '.join(command)))
            if run(
                command,
                cwd=repository.root_path,
                capture_output=True,
                encoding='utf-8',
            ).returncode:
                sys.stderr.write('Failed to push tags to {}\n'.format(args.remote))
                return_code += 1

        if return_code:
            print('Publication failed.')
        else:
            print('Publication succeeded!')
        return return_code
