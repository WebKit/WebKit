# Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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


COMMIT_REF_BASE = r'r?R?[a-f0-9A-F]+(\.\d+)?@?([0-9a-zA-z\-\/\.]+[0-9a-zA-z\-\/])?'
COMPOUND_COMMIT_REF = r'(?P<primary>{})(?P<secondary> \({}\))?'.format(COMMIT_REF_BASE, COMMIT_REF_BASE)
CHERRY_PICK_RE = [
    re.compile(r'\S* ?[Cc]herry[- ][Pp]ick of:? {}'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'\S* ?[Cc]herry[- ][Pp]ick:? {}'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'\S* ?[Cc]herry[- ][Pp]icked:? {}'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'^[Oo]riginally[- ]landed[- ]as: {}'.format(COMPOUND_COMMIT_REF)),
]
REVERT_BASES = [
    r'Reverts? {}',
    r'Reverts? \[{}\]',
    r'Reverts? \({}\)',
    r'Reverts? "{}"',
    r'Reverts? "?[Cc]herry[- ][Pp]ick {}',
]
REVERT_RE = [
    re.compile(base.format(COMPOUND_COMMIT_REF)) for base in REVERT_BASES
]
DOUBLE_REVERT = [
    re.compile(r'Reverts? \"{}\.'.format(base.format(COMPOUND_COMMIT_REF))) for base in REVERT_BASES
] + [
    re.compile(r'Reverts? \"{}'.format(base.format(COMPOUND_COMMIT_REF))) for base in REVERT_BASES
]

FOLLOW_UP_FIXES_RE = [
    re.compile(r'Fix following {}'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'Follow-? ?up fix to {}'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'Follow-? ?up fix {}'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'Follow-? ?up to {}'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'Follow-? ?up {}'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'\[?[Gg]ardening\]?:? REGRESSION \({}\)'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'\[?[Gg]ardening\]?:? REGRESSION {}'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'REGRESSION ?\({}\)'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'REGRESSION ?{}'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'\[?[Gg]ardening\]?:? [Tt]est-? ?[Aa]ddition \(?{}\)?'.format(COMPOUND_COMMIT_REF)),
    re.compile(r'[Tt]est-? ?[Aa]ddition \(?{}\)?'.format(COMPOUND_COMMIT_REF)),
]
UNPACK_SECONDARY_RE = re.compile(r' \(({})\)'.format(COMMIT_REF_BASE))

# Unlike COMMIT_REF_BASE, these only match things which look like commit references, so they
# can be searched for anywhere in a title (for example, "[GTK] REGRESSION(1234@main): ...").
STRICT_COMMIT_REF = r'(?<![\w@/.])(?:\d+(?:\.\d+)?@[\w\-/.]*[\w\-/]|r\d{3,}|[a-fA-F0-9]{7,40})(?![\w@])'
STRICT_COMPOUND_COMMIT_REF = r'(?P<primary>{0})(?: \((?P<secondary>{0})\))?'.format(STRICT_COMMIT_REF)
STRICT_COMMIT_REF_LIST = r'(?P<list>{0}(?: \({0}\))?(?:(?:,? and |, | & | \+ ){0}(?: \({0}\))?)*)'.format(STRICT_COMMIT_REF)
STRICT_COMPOUND_COMMIT_REF_RE = re.compile(STRICT_COMPOUND_COMMIT_REF)
DOUBLE_REVERT_SEARCH_RE = [
    re.compile(r'\b[Rr]everts? "(?:[Uu]nreviewed,? )?[Rr](?:everting|olling out) {}'.format(STRICT_COMMIT_REF_LIST)),
]
REVERT_SEARCH_RE = [
    re.compile(r'\b[Rr]everts? "?(?:[Cc]herry[- ][Pp]ick )?{}'.format(STRICT_COMMIT_REF_LIST)),
    re.compile(r'\b[Rr]everting {}'.format(STRICT_COMMIT_REF_LIST)),
    re.compile(r'\b[Rr]olling out {}'.format(STRICT_COMMIT_REF_LIST)),
]
# Also matches the indented message of a cherry-picked revert, and reverts listed in a body
REVERT_BODY_RE = re.compile(r'^\s*(?:This reverts commits?|[Rr]everts?|(?:[Uu]nreviewed,? )?[Rr]everting) {}'.format(STRICT_COMMIT_REF_LIST))
FOLLOW_UP_SEARCH_RE = [
    re.compile(r'REGRESSION ?(?:\([^()]*?)?{}'.format(STRICT_COMMIT_REF_LIST)),
    re.compile(r'\b[Ff]ollow-? ?up(?: fix)?(?: to| for)? ?\(?{}'.format(STRICT_COMMIT_REF_LIST)),
    re.compile(r'\bNew [Tt]est ?\(?{}'.format(STRICT_COMMIT_REF_LIST)),
    re.compile(r'\bFix following {}'.format(STRICT_COMMIT_REF_LIST)),
    re.compile(r'\b[Tt]est-? ?[Aa]ddition \(?{}'.format(STRICT_COMMIT_REF_LIST)),
]


class Relationship(object):
    TYPES = (
        'references', 'referenced by',
        'cherry-picked', 'original',
        'reverts', 'reverted by',
        'follow-up to', 'followed-up by',
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
        if not commit.message:
            return None, []
        lines = commit.message.splitlines()
        lines_to_check = commit.trailers + [lines[0]]

        for type, regexes in {
            cls.ORIGINAL: CHERRY_PICK_RE + DOUBLE_REVERT,
            cls.REVERTS: REVERT_RE,
            cls.FOLLOW_UP: FOLLOW_UP_FIXES_RE,
        }.items():
            for regex in regexes:
                match = None
                for line in lines_to_check:
                    if match:
                        break
                    match = regex.match(line)
                if not match:
                    continue
                primary = match.group('primary')
                secondary = match.group('secondary')
                if secondary:
                    secondary = UNPACK_SECONDARY_RE.match(secondary).groups()[0]
                if secondary and Commit.HASH_RE.match(secondary):
                    primary, secondary = secondary, primary
                return type, [ref.rstrip() for ref in [primary, secondary] if ref]
        return None, []

    @classmethod
    def _unpack_list(cls, string):
        result = []
        for match in STRICT_COMPOUND_COMMIT_REF_RE.finditer(string):
            primary, secondary = match.group('primary'), match.group('secondary')
            if secondary and Commit.HASH_RE.match(secondary):
                primary, secondary = secondary, primary
            result.append([ref[:Commit.HASH_LABEL_SIZE] if Commit.HASH_RE.match(ref) else ref for ref in [primary, secondary] if ref])
        return result

    @classmethod
    def parse_all(cls, commit):
        # Unlike parse(), find every commit referenced, not just the first one, and search the whole title
        # instead of only matching its start. Returns a list of (type, refs) tuples, one per referenced commit,
        # where refs are alternative representations of the same commit.
        if not commit.message:
            return []
        lines = commit.message.splitlines()
        lines_to_check = commit.trailers + [lines[0]]

        # Squashed cherry-picks put each picked commit on its own unindented line
        result = []
        for line in lines:
            for regex in CHERRY_PICK_RE:
                match = regex.match(line)
                if match:
                    result += [(cls.ORIGINAL, refs) for refs in cls._unpack_list(line[match.start('primary'):])[:1]]
                    break

        if not result:
            result = cls._parse_title(commit, lines_to_check)

        for line in lines[1:]:
            match = REVERT_BODY_RE.match(line)
            if match:
                result += [(cls.REVERTS, refs) for refs in cls._unpack_list(match.group('list'))]

        deduplicated = []
        seen = set()
        for type, refs in result:
            if refs[0] not in seen:
                seen.add(refs[0])
                deduplicated.append((type, refs))
        return deduplicated

    @classmethod
    def _parse_title(cls, commit, lines):
        for type, regexes in (
            (cls.ORIGINAL, DOUBLE_REVERT_SEARCH_RE),
            (cls.REVERTS, REVERT_SEARCH_RE),
            (cls.FOLLOW_UP, FOLLOW_UP_SEARCH_RE),
        ):
            for regex in regexes:
                for line in lines:
                    match = regex.search(line)
                    if match:
                        return [(type, refs) for refs in cls._unpack_list(match.group('list'))]

        type, refs = cls.parse(commit)
        return [(type, refs)] if type else []

    def __init__(self, commit, type=None):
        self.commit = commit
        self.type = type or self.REFERENCES

    def __repr__(self):
        return '{} {}'.format(self.commit, self.type)


class CommitsStory(object):
    def __init__(self, commits=None):
        self.commits = []
        self.by_ref = {}
        self.by_issue = {}
        self.relations = {}
        for commit in commits or []:
            self.add(commit)

    def __contains__(self, commit):
        if str(commit) in self.by_ref:
            return True
        if commit.hash and commit.hash[:Commit.HASH_LABEL_SIZE] in self.by_ref:
            return True
        if commit.revision and 'r{}'.format(commit.revision) in self.by_ref:
            return True
        return False

    def add(self, commit):
        if commit in self:
            return True
        self.commits.append(commit)
        self.by_ref[str(commit)] = commit
        if commit.hash:
            self.by_ref[commit.hash[:Commit.HASH_LABEL_SIZE]] = commit
        if commit.revision:
            self.by_ref['r{}'.format(commit.revision)] = commit

        for issue in commit.issues:
            if issue.link not in self.by_issue:
                self.by_issue[issue.link] = []
            self.by_issue[issue.link].append(commit)

        for type, refs in Relationship.parse_all(commit):
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
        for type, refs in Relationship.parse_all(commit):
            for ref in refs:
                found = None
                if commits_story:
                    found = commits_story.by_ref.get(ref, None)
                if not found:
                    try:
                        found = repository.find(ref)
                    except (ValueError, repository.Exception):
                        continue
                if not found:
                    continue
                if commits_story:
                    commits_story.add(found)
                if str(found) in tracked:
                    break

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

        type = Relationship.REFERENCES
        for issue in commit.issues:
            for candidate in commits_story.by_issue.get(issue.link, []):
                if str(candidate) in tracked:
                    continue
                tracked.add(str(candidate))
                result.append(Relationship(candidate, type))

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
