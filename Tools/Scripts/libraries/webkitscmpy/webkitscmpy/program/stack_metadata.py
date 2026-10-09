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

from typing import Callable, Iterator, NamedTuple, Optional

from webkitcorepy import run
from webkitscmpy import local


class MetadataKey(NamedTuple):
    config: str
    describe: Callable[[str], str]


class StackMetadata:
    """Where a branch sits in its stack, as this checkout records it."""

    KEYS = {
        'parent': MetadataKey('stack-parent', lambda branch: f"which branch '{branch}' is stacked on"),
    }

    @classmethod
    def for_branch(cls, git: local.Git, branch: Optional[str]) -> Optional['StackMetadata']:
        """None for any repository or branch which cannot carry a stack at all."""
        return cls(git, branch) if isinstance(git, local.Git) and branch else None

    @classmethod
    def key_for(cls, branch: str, name: str) -> str:
        return f'branch.{branch}.{cls.KEYS[name].config}'

    @classmethod
    def stacked(cls, git: local.Git) -> Iterator[tuple[str, str]]:
        """Every branch this checkout records a parent for, whether or not it has that parent."""
        prefix, suffix = 'branch.', f".{cls.KEYS['parent'].config}"
        for key, value in git.config().items():
            if key.startswith(prefix) and key.endswith(suffix):
                yield key[len(prefix):-len(suffix)], value

    def __init__(self, git: local.Git, branch: str) -> None:
        self.git = git
        self.branch = branch

    @property
    def recorded(self) -> dict[str, str]:
        """Everything this checkout records about where the branch sits."""
        config = self.git.config()
        return {
            name: value for name in self.KEYS
            if (value := config.get(self.key_for(self.branch, name)))
        }

    @property
    def parent(self) -> Optional[str]:
        """The branch this one claims to be stacked on, whether or not this checkout has it."""
        candidate = self.recorded.get('parent')
        return None if candidate == self.branch else candidate

    def write(self, metadata: dict[str, Optional[str]]) -> int:
        """Write the keys named, unsetting those given no value and leaving any not named alone."""
        for name, spec in self.KEYS.items():
            if name not in metadata:
                continue

            value = metadata[name]
            key = self.key_for(self.branch, name)
            command = [self.git.executable(), 'config', key, value] if value else [
                self.git.executable(), 'config', '--unset', key,
            ]
            if not run(command, cwd=self.git.root_path, capture_output=True).returncode:
                continue

            # Unsetting a key which was never set fails, and only an unforgotten parent still stacks a branch
            if value:
                sys.stderr.write(f'Failed to record {spec.describe(self.branch)}\n')
                return 1
            if name == 'parent':
                sys.stderr.write(f'Failed to forget {spec.describe(self.branch)}\n')
                return 1
        self.git.config(cached=False)
        return 0

    def clear(self) -> int:
        return self.write(dict.fromkeys(self.KEYS))
