# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Which builder belongs to which queue group, and what version each one is.

Membership is a per-group pattern rather than a name prefix, the way `ews_dashboard.suites`
classifies a queue by pattern rather than by prefix: a prefix left `API-Tests-*`, `Win-Tests-EWS` and
`visionOS-26-Simulator-WK2-Tests-EWS` unreachable by any chip, since none of them start with a group
name. That was six of the seventeen EWS builders, 39% of all builds. A builder no group's pattern
matches lands in `OTHER`, so nothing is ever unreachable again.

Each group's version pattern is its own, not a generic guesser: a per-group pattern fails visibly on
a new OS name a group was never taught about, where a generic one would misclassify silently. The iOS
membership pattern is anchored against `vision` so `visionOS-26-Simulator-WK2-Tests-EWS` cannot also
satisfy the iOS rule.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Iterable, Mapping, Optional

OTHER = 'other'

_SIMULATOR_VERSION = re.compile(r'(\d+)-Simulator')


@dataclass(frozen=True)
class QueueGroup:
    name: str
    builder_pattern: 're.Pattern[str]'
    version_pattern: 'Optional[re.Pattern[str]]' = None


# `Release`/`Debug` are the build style, not the OS name, so the macOS version pattern excludes them
# explicitly rather than accepting whatever capitalised word comes right after `macOS-`; without that
# exclusion `macOS-Release-WK2-Stress-Tests-EWS` would report `Release` as its version.
QUEUE_GROUPS = (
    QueueGroup('macOS', re.compile(r'macOS'),
               re.compile(r'^macOS-(?!Release-|Debug-)([A-Z][a-z]+)-')),
    QueueGroup('iOS', re.compile(r'(?<!vision)iOS'), _SIMULATOR_VERSION),
    QueueGroup('GTK', re.compile(r'GTK')),
    QueueGroup('WPE', re.compile(r'WPE')),
    QueueGroup('Windows', re.compile(r'^Win-')),
    QueueGroup('visionOS', re.compile(r'visionOS'), _SIMULATOR_VERSION),
)

QUEUE_GROUP_NAMES = tuple(group.name for group in QUEUE_GROUPS) + (OTHER,)

_GROUPS_BY_NAME = {group.name: group for group in QUEUE_GROUPS}


def group_of(builder: str) -> str:
    """The group a builder belongs to: the name of the first group whose pattern claims it, or
    `OTHER` where none does. Never unreachable, which is the point of classifying by pattern."""
    for group in QUEUE_GROUPS:
        if group.builder_pattern.search(builder):
            return group.name
    return OTHER


def version_of(builder: str) -> Optional[str]:
    """The version a builder's own group reads off its name, or None where that group has no version
    pattern (GTK, WPE, Windows, `OTHER`) or the pattern found nothing to capture."""
    group = _GROUPS_BY_NAME.get(group_of(builder))
    if group is None or group.version_pattern is None:
        return None
    match = group.version_pattern.search(builder)
    return match.group(1) if match else None


@dataclass(frozen=True)
class BuilderLeaf:
    builder: str
    convictions: int


@dataclass(frozen=True)
class VersionNode:
    """One version bucket under a group in the dropdown tree. `version` is None for the leftover
    bucket of a group's unversioned queues, rendered as "unversioned"."""

    version: Optional[str]
    builders: tuple


@dataclass(frozen=True)
class GroupNode:
    """One group in the dropdown tree.

    `versions` is empty where a version level would not divide the group -- every one of its builders
    fell in the same bucket, versioned or not -- and `builders` then holds the group's own builders
    directly. Where `versions` is non-empty, `builders` is empty and every builder is under one of
    them instead.
    """

    name: str
    versions: tuple
    builders: tuple


def tree(builders: Iterable[str], counts: Mapping[str, int]) -> tuple:
    """The render-ready group/version/builder tree for exactly the builders given, so a queue absent
    from the caller's window is never offered and a new queue needs no code change here."""
    by_group: dict = {}
    for builder in builders:
        by_group.setdefault(group_of(builder), []).append(builder)
    order = [group.name for group in QUEUE_GROUPS] + [OTHER]
    nodes = []
    for name in order:
        members = sorted(by_group.get(name, ()))
        if not members:
            continue
        by_version: dict = {}
        for builder in members:
            by_version.setdefault(version_of(builder), []).append(builder)
        if len(by_version) > 1:
            versions = tuple(
                VersionNode(
                    version=version,
                    builders=tuple(BuilderLeaf(builder, counts.get(builder, 0))
                                   for builder in sorted(bucket)),
                )
                for version, bucket in sorted(by_version.items(),
                                              key=lambda item: (item[0] is None, item[0]))
            )
            nodes.append(GroupNode(name=name, versions=versions, builders=()))
        else:
            nodes.append(GroupNode(
                name=name, versions=(),
                builders=tuple(BuilderLeaf(builder, counts.get(builder, 0)) for builder in members),
            ))
    return tuple(nodes)


@dataclass(frozen=True)
class Selection:
    """A reader's choice of queues: any combination of whole groups, group-qualified versions and
    individual builders, unioned. Every field here is already validated -- dropping an unknown value
    is the caller's job -- so `resolve` never has to guess at one."""

    groups: tuple = ()
    versions: tuple = ()  # (group, version) pairs
    builders: tuple = ()

    @property
    def empty(self) -> bool:
        return not (self.groups or self.versions or self.builders)


def resolve(selection: Selection, known_builders: Iterable[str]) -> tuple:
    """The concrete builders among `known_builders` the selection reaches, in `known_builders`' own
    order, or `()` where nothing was selected -- which callers read as no filter at all, not as a
    filter that matches nothing.

    Resolved here rather than re-derived as SQL is what keeps the classification to one
    implementation: the tree a reader clicked and the WHERE clause a query runs both come from the
    same `group_of`/`version_of` calls, so they cannot disagree.
    """
    if selection.empty:
        return ()
    known = list(known_builders)
    matched = set(selection.builders) & set(known)
    for builder in known:
        group = group_of(builder)
        if group in selection.groups:
            matched.add(builder)
            continue
        version = version_of(builder)
        if version is not None and (group, version) in selection.versions:
            matched.add(builder)
    return tuple(builder for builder in known if builder in matched)


def builder_filter(builders: tuple, column: str = 'build.builder') -> tuple:
    """A `column IN (...)` test for a resolved, concrete set of builder names, and the parameters it
    binds, returned together so a caller cannot bind one without the other. `builders` empty
    contributes no fragment and no parameters, which is what leaves every queue in when nothing was
    selected.

    Every parameter name is generated here, never taken from `builders`, since a builder name arrives
    from a reader and must never become part of a bound parameter's own name.
    """
    if not builders:
        return '', {}
    parameters = {f'builder_{index}': builder for index, builder in enumerate(builders)}
    fragment = f"{column} IN (" + ', '.join(f':{name}' for name in parameters) + ')'
    return fragment, parameters
