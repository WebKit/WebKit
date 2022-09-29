# Copyright (C) 2022 Apple Inc. All rights reserved.
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
from webkitscmpy import Commit, local, log, remote
from webkitbugspy import Tracker


COMMIT_REF_BASE = r'r?R?[a-f0-9A-F]+.?\d*@?[0-9a-zA-z\-/]*'
COMPOUND_COMMIT_REF = r'(?P<primary>{})(?P<secondary> \({}\))?'.format(COMMIT_REF_BASE, COMMIT_REF_BASE)
CHERRY_PICK_RE = re.compile(r'[Cc]herry[- ][Pp]ick {}'.format(COMPOUND_COMMIT_REF))
REVERT_RE = re.compile(r'Reverts? {}'.format(COMPOUND_COMMIT_REF))
FOLLOW_UP_FIXES_RE = [
    re.compile(r'Fix following {}'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'Follow-? ?up fix {}'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'Follow-? ?up {}'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'\[?[Gg]ardening\]?:? REGRESSION \(?{}\)?'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'REGRESSION ?\(?{}\)?'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'\[?[Gg]ardening\]?:? [Tt]est-? ?[Aa]ddition \(?{}\)?'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'[Tt]est-? ?[Aa]ddition \(?{}\)?'.format(COMPOUND_COMMIT_REF)),
]
UNPACK_SECONDARY_RE = re.compile(r' \(({})\)'.format(COMMIT_REF_BASE))


class Relationship(object):
    TYPES = (
        'references', 'referenced by',
        'cherry-picked', 'original',
        'reverts', 'reverted by',
        'follow-up', 'follow-up by',
    )
    REFERENCES, REFERENCED_BY, \
        CHERRY_PICK, ORIGINAL, \
        REVERTS, REVERTED_BY, \
        FOLLOW_UP, FOLLOW_UP_BY = TYPES

    PAIRED = [FOLLOW_UP, FOLLOW_UP_BY]
    IDENTITY = [CHERRY_PICK, ORIGINAL]
    UNDO = [REVERTS, REVERTED_BY]

    @classmethod
    def reversed(cls, type):
        return {
            cls.REFERENCES: cls.REFERENCED_BY,
            cls.REFERENCED_BY: cls.REFERENCES,
            cls.CHERRY_PICK: cls.ORIGINAL,
            cls.ORIGINAL: cls.CHERRY_PICK,
            cls.REVERTS: cls.REVERTED_BY,
            cls.REVERTED_BY: cls.REVERTS,
            cls.FOLLOW_UP: cls.FOLLOW_UP_BY,
            cls.FOLLOW_UP_BY: cls.FOLLOW_UP,
        }.get(type, type)

    @classmethod
    def parse(cls, commit):
        lines = commit.message.splitlines()

        for type, regexes in {
            cls.ORIGINAL: [CHERRY_PICK_RE],
            cls.REVERTS: [REVERT_RE],
            cls.FOLLOW_UP: FOLLOW_UP_FIXES_RE,
        }.items():
            for regex in regexes:
                match = regex.match(lines[0])
                if not match:
                    continue
                primary = match.group('primary')
                secondary = match.group('secondary')
                if secondary:
                    secondary = UNPACK_SECONDARY_RE.match(secondary).groups()[0]
                return type, [ref.rstrip('.').rstrip() for ref in [primary, secondary] if ref]
        return None, []

    def __init__(self, commit, type=None):
        self.commit = commit
        self.type = type or self.REFERENCES

    def __repr__(self):
        return '{} {}'.format(self.commit, self.type)


class CommitsStory(object):
    @classmethod
    def issues_for(cls, commit):
        filter = set()
        result = []
        line_count = 0
        for line in commit.message.splitlines():
            if not line and line_count >= 2:
                break
            if not line:
                continue
            line_count += 1
            for word in line.split(' '):
                bug = Tracker.from_string(word)
                if bug and bug.link not in filter:
                    filter.add(bug.link)
                    result.append(bug)
        return result

    def __init__(self, commits=None):
        self.commits = []
        self.by_ref = {}
        self.by_issue = {}
        self.relations = {}
        for commit in commits or []:
            self.add(commit)

    def add(self, commit):
        if str(commit) in self.by_ref:
            return True
        self.commits.append(commit)
        self.by_ref[str(commit)] = commit
        if commit.hash:
            self.by_ref[commit.hash[:Commit.HASH_LABEL_SIZE]] = commit
        if commit.revision:
            self.by_ref['r{}'.format(commit.revision)] = commit

        for issue in self.issues_for(commit):
            if issue.link not in self.by_issue:
                self.by_issue[issue.link] = []
            self.by_issue[issue.link].append(commit)

        type, refs = Relationship.parse(commit)
        if type:
            for ref in refs:
                if ref not in self.relations:
                    self.relations[ref] = []
                self.relations[ref].append(Relationship(commit, Relationship.reversed(type)))
        return False


class Trace(Command):
    name = 'trace'
    aliases = ['follow']
    help = "Given an identifier, revision, or hash, find related commits"

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'argument', nargs=1,
            type=str, default=None,
            help='String representation of a commit to trace relationships of',
        )
        parser.add_argument(
            '--limit',
            type=int, default=500,
            help='Search commit messages around the specified commit for relationships',
        )

    @classmethod
    def relationships(cls, commit, repository, commits_story=None):
        tracked = set([str(commit)])
        result = []
        type, refs = Relationship.parse(commit)
        if type and refs:
            for ref in refs:
                found = None
                if commits_story:
                    found = commits_story.by_ref.get(ref, None)
                if not found:
                    found = repository.find(ref)
                if not found:
                    continue
                if commits_story:
                    commits_story.add(found)

                tracked.add(str(found))
                result.append(Relationship(found, type))
                for relation in cls.relationships(found, repository):
                    if str(relation.commit) in tracked:
                        continue
                    tracked.add(str(relation.commit))
                    result.append(relation)
                break

            else:
                sys.stderr.write("'{}' {} something we can't find, continuing\n".format(commit, type))

        if not commits_story:
            return result

        type = Relationship.REFERENCES
        for issue in CommitsStory.issues_for(commit):
            for candidate in commits_story.by_issue.get(issue.link, []):
                if str(candidate) in tracked:
                    continue
                tracked.add(str(candidate))
                result.append(Relationship(candidate, type))

        references = [str(commit)]
        if commit.hash:
            references.append(commit.hash[:Commit.HASH_LABEL_SIZE])
        if commit.revision:
            references.append('r{}'.format(commit.revision))
        for reference in references:
            for candidate in commits_story.relations.get(reference, []):
                if str(candidate.commit) in tracked:
                    continue
                tracked.add(str(candidate.commit))
                result.append(candidate)
        return result

    @classmethod
    def summary(cls, commit):
        return '{identifier} | {hash}{revision}{title}'.format(
            identifier=commit,
            hash=commit.hash[:Commit.HASH_LABEL_SIZE] if commit.hash else '',
            revision='{}r{}'.format(', ' if commit.hash else '', commit.revision) if commit.revision else '',
            title=' | {}'.format(commit.message.splitlines()[0]) if commit.message else ''
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1

        try:
            commit = repository.find(args.argument[0], include_log=True)
        except (local.Scm.Exception, TypeError, ValueError) as exception:
            # ValueErrors and Scm exceptions usually contain enough information to be displayed
            # to the user as an error
            sys.stderr.write(str(exception) + '\n')
            return 1

        print(cls.summary(commit))

        story = CommitsStory()
        if args.limit:
            head = repository.commit()
            if head.branch != commit.branch:
                for c in repository.commits(
                    begin=dict(argument='{}~{}'.format(head.hash, args.limit)),
                    end=dict(argument=head.hash),
                ):
                    story.add(c)
                head = repository.commit(branch=commit.branch)

            for c in repository.commits(
                begin=dict(argument='{}~{}'.format(commit.hash, args.limit)),
                end=dict(argument=head.hash),
            ):
                story.add(c)

        relationship = cls.relationships(commit, repository, commits_story=story)
        if not relationship:
            sys.stderr.write('No relationships found\n')
            return 1

        for relationship in relationship:
            print('    {} {}'.format(relationship.type, cls.summary(relationship.commit)))

        return 0
