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

from argparse import ArgumentParser, Namespace
from typing import Optional

from .command import Command
from .stack_metadata import StackMetadata

from webkitscmpy import local, log


class Stack(Command):
    name = 'stack'
    help = 'Manage a stack of dependent development branches'

    HEADER = 'Stacked pull requests, bottom of the stack first:'

    @classmethod
    def parser(cls, parser: ArgumentParser, loggers: Optional[list] = None) -> None:
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

    @classmethod
    def _recorded_parent(cls, git: local.Git, branch: Optional[str]) -> Optional[str]:
        """The branch a change claims to be stacked on, whether or not this checkout has it."""
        metadata = StackMetadata.for_branch(git, branch)
        return metadata.parent if metadata else None

    @classmethod
    def parent(cls, git: local.Git, branch: Optional[str]) -> Optional[str]:
        """The branch a change is stacked on, or None when this checkout does not have it."""
        if not (candidate := cls._recorded_parent(git, branch)):
            return None
        if candidate not in git.branches_for(remote=False):
            log.warning(f"'{branch}' is stacked on '{candidate}', which no longer exists in this checkout")
            return None
        return candidate

    @classmethod
    def _children(cls, git: local.Git, branch: str) -> list[str]:
        return sorted(
            candidate for candidate, parent in list(StackMetadata.stacked(git))
            if parent == branch and cls.parent(git, candidate) == branch
        )

    @classmethod
    def _ancestors(cls, git: local.Git, branch: str) -> Optional[list[str]]:
        result = []
        candidate = branch
        while candidate := cls.parent(git, candidate):
            if candidate == branch or candidate in result:
                sys.stderr.write(f"'{candidate}' is part of a cycle of stacked branches\n")
                return None
            result.append(candidate)
        return result[::-1]  # Bottom of stack to top

    @classmethod
    def _descendants(cls, git: local.Git, branch: str) -> Optional[list[str]]:  # DFS so children come before siblings
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
    def members(cls, git: local.Git, branch: str) -> Optional[list[str]]:
        if (below := cls._ancestors(git, branch)) is None:
            return None
        root = below[0] if below else branch
        if (above := cls._descendants(git, root)) is None:
            return None
        return [root] + above

    @classmethod
    def describe(cls, git: local.Git, branch: str) -> Optional[list[str]]:
        if (members := cls.members(git, branch)) is None:
            return None
        if len(members) < 2:
            return []

        depth = {}
        lines = [cls.HEADER]
        for member in members:
            depth[member] = depth.get(cls.parent(git, member), -1) + 1
            described = ' (this pull request)' if member == branch else ''
            lines.append(f"{'    ' * depth[member]}- {member}{described}")
        return lines

    @classmethod
    def resolve(cls, git: local.Git, argument: str, branch: Optional[str] = None) -> Optional[str]:
        """The branch in this checkout a name refers to, expanding development-branch shorthand."""
        from .branch import Branch

        branches = git.branches_for(remote=False)
        candidate = argument if argument in branches else Branch.normalize_branch_name(argument, repository=git)

        if candidate not in branches:
            sys.stderr.write(f"Could not find '{argument}' as a branch in this checkout\n")
            return None
        if not git.dev_branches.match(candidate):
            sys.stderr.write(f"'{candidate}' is not a development branch, a branch cannot be stacked on it\n")
            return None
        if candidate == branch:
            sys.stderr.write(f"'{branch}' cannot be stacked on itself\n")
            return None
        return candidate

    @classmethod
    def main(cls, args: Namespace, repository, **kwargs) -> int:
        if not isinstance(repository, local.Git):
            sys.stderr.write(f"Can only '{cls.name}' on a native Git repository\n")
            return 1

        git = repository
        branch = git.branch
        if args.parent and args.unstack:
            sys.stderr.write(f"Cannot both set and unset the branch '{branch}' is stacked on\n")
            return 1

        if args.parent:
            if not git.is_suitable_branch_for_pull_request(branch, git.default_remote):
                sys.stderr.write(f"'{branch}' is not a development branch, it cannot be stacked on another branch\n")
                return 1
            if not (parent := cls.resolve(git, args.parent, branch=branch)):
                return 1
            if (descendants := cls._descendants(git, branch)) is None:
                return 1
            if parent in descendants:
                sys.stderr.write(
                    f"'{parent}' is stacked on '{branch},' stacking '{branch}' on it would create a cycle\n"
                )
                return 1
            if StackMetadata(git, branch).write({'parent': parent}):
                return 1
            print(f"'{branch}' is stacked on '{parent}'")
            return 0

        if args.unstack:
            if StackMetadata(git, branch).clear():
                return 1
            print(f"'{branch}' is no longer stacked on another branch")
            return 0

        if (lines := cls.describe(git, branch)) is None:
            return 1
        if not lines:
            print(f"'{branch}' is not part of a stack")
            return 0
        print('\n'.join(lines))
        return 0
