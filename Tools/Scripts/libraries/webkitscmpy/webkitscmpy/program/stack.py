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

import sys

from .command import Command

from webkitcorepy import run
from webkitscmpy import local, log


class Stack(Command):
    name = 'stack'
    help = 'Manage a stack of dependent development branches'

    PARENT_KEY = 'stack-parent'
    HEADER = 'Stacked pull requests, bottom of the stack first:'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            '--on', '--stacked-on',
            dest='parent', type=str, default=None,
            help='Record that the current development branch is stacked on the provided branch',
        )
        parser.add_argument(
            '--unstack',
            dest='unstack', action='store_true', default=False,
            help='Forget which branch the current development branch is stacked on',
        )
        parser.add_argument(
            '--remote', dest='remote', type=str, default=None,
            help="Look for the stack's pull requests on the provided remote",
        )

    @classmethod
    def _key_for(cls, branch):
        return f'branch.{branch}.{cls.PARENT_KEY}'

    @classmethod
    def _parent(cls, git, branch):
        if not isinstance(git, local.Git) or not branch:
            return None

        candidate = git.config().get(cls._key_for(branch))
        if not candidate or candidate == branch:
            return None

        if candidate not in git.branches_for(remote=False):
            log.warning(f"'{branch}' is stacked on '{candidate}', which no longer exists in this checkout")
            return None
        return candidate

    @classmethod
    def _set_parent(cls, git, branch, parent):
        if run(
            [git.executable(), 'config', cls._key_for(branch), parent],
            cwd=git.root_path, capture_output=True,
        ).returncode:
            sys.stderr.write(f"Failed to record that '{branch}' is stacked on '{parent}'\n")
            return 1
        git.config(cached=False)
        return 0

    @classmethod
    def _unset_parent(cls, git, branch):
        if run(
            [git.executable(), 'config', '--unset', cls._key_for(branch)],
            cwd=git.root_path, capture_output=True,
        ).returncode:
            sys.stderr.write(f"Failed to forget which branch '{branch}' is stacked on\n")
            return 1
        git.config(cached=False)
        return 0

    @classmethod
    def _resolve_parent(cls, git, candidate, branch=None):
        from .branch import Branch

        branches = git.branches_for(remote=False)
        if candidate not in branches:
            candidate = Branch.normalize_branch_name(candidate, repository=git)
            if candidate not in branches:
                sys.stderr.write(f"'{candidate}' does not exist in this checkout\n")
                return None

        if not git.dev_branches.match(candidate):
            sys.stderr.write(f"'{candidate}' is not a development branch, a branch cannot be stacked on it\n")
            return None

        if candidate == branch:
            sys.stderr.write(f"'{branch}' cannot be stacked on itself\n")
            return None
        return candidate

    @classmethod
    def _children(cls, git, branch):
        prefix, suffix = 'branch.', f'.{cls.PARENT_KEY}'
        result = []
        for key, value in git.config().items():
            if value != branch or not key.startswith(prefix) or not key.endswith(suffix):
                continue
            candidate = key[len(prefix):-len(suffix)]
            if cls._parent(git, candidate) == branch:
                result.append(candidate)
        return sorted(result)

    @classmethod
    def _ancestors(cls, git, branch):
        result = []
        candidate = branch
        while candidate := cls._parent(git, candidate):
            if candidate == branch or candidate in result:
                sys.stderr.write(f"'{candidate}' is part of a cycle of stacked branches\n")
                return None
            result.append(candidate)
        return result[::-1]  # Bottom of stack to top

    @classmethod
    def _descendants(cls, git, branch):  # DFS so children come before siblings
        result = []
        stack = list(reversed(cls._children(git, branch)))
        while stack:
            candidate = stack.pop()
            if candidate == branch or candidate in result:
                sys.stderr.write(f"'{candidate}' is part of a cycle of stacked branches\n")
                return None
            result.append(candidate)
            stack.extend(reversed(cls._children(git, candidate)))
        return result

    @classmethod
    def _members(cls, git, branch):
        if (below := cls._ancestors(git, branch)) is None:
            return None
        root = below[0] if below else branch
        if (above := cls._descendants(git, root)) is None:
            return None
        return [root] + above

    @classmethod
    def _pull_request_for(cls, git, branch, remote_repo):
        if not remote_repo or not remote_repo.pull_requests:
            return None
        if not git.config().get(f'branch.{branch}.target'):
            return None

        # Avoids a circular import
        from .pull_request import PullRequest
        return PullRequest.find_existing_pull_request(git, remote_repo, branch=branch)

    @classmethod
    def _describe(cls, git, branch, remote_repo=None):
        if (members := cls._members(git, branch)) is None:
            return None
        if len(members) < 2:
            return []

        depth = {}
        lines = [cls.HEADER]
        for member in members:
            depth[member] = depth.get(cls._parent(git, member), -1) + 1
            pull_request = cls._pull_request_for(git, member, remote_repo)

            annotations = []
            if member == branch:
                annotations.append('this pull request')
            elif remote_repo and not pull_request:
                annotations.append('not uploaded')
            elif pull_request and pull_request.merged:
                annotations.append('merged')
            elif pull_request and pull_request.opened is False:
                annotations.append('closed')

            number = f'#{pull_request.number} ' if pull_request else ''
            described = f" ({', '.join(annotations)})" if annotations else ''
            lines.append(f"{'    ' * depth[member]}- {number}{member}{described}")
        return lines

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write(f"Can only '{cls.name}' on a native Git repository\n")
            return 1

        git = repository
        branch = git.branch
        if args.parent and args.unstack:
            sys.stderr.write(f"Cannot both set and unset the branch '{branch}' is stacked on\n")
            return 1

        if args.parent:
            if not git.is_suitable_branch_for_pull_request(branch, args.remote or git.default_remote):
                sys.stderr.write(f"'{branch}' is not a development branch, it cannot be stacked on another branch\n")
                return 1
            if not (parent := cls._resolve_parent(git, args.parent, branch=branch)):
                return 1
            if (descendants := cls._descendants(git, branch)) is None:
                return 1
            if parent in descendants:
                sys.stderr.write(
                    f"'{parent}' is stacked on '{branch},' stacking '{branch}' on it would create a cycle\n"
                )
                return 1
            if cls._set_parent(git, branch, parent):
                return 1
            print(f"'{branch}' is stacked on '{parent}'")
            return 0

        if args.unstack:
            if cls._unset_parent(git, branch):
                return 1
            print(f"'{branch}' is no longer stacked on another branch")
            return 0

        remote_repo = git.remote(name=args.remote or git.default_remote)
        if (lines := cls._describe(git, branch, remote_repo=remote_repo)) is None:
            return 1
        if not lines:
            print(f"'{branch}' is not part of a stack")
            return 0
        print('\n'.join(lines))
        return 0
