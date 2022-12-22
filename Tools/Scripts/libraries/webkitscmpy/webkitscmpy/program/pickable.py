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

import itertools
import json
import sys
import re

from .command import Command
from .find import Info
from .trace import Trace, Relationship, CommitsStory
from datetime import datetime
from webkitcorepy import arguments, run, string_utils
from webkitscmpy import Commit, local

fuzz = None

if sys.version_info > (3, 6):
    try:
        from rapidfuzz import fuzz
    except ModuleNotFoundError:
        pass


class Pickable(Command):
    class Filters(object):
        DEFAULT_FUZZ_RATIO = 90

        @classmethod
        def fuzzy(cls, string, ratio=None):
            if not fuzz:
                return re.compile(string)

            ratio = cls.DEFAULT_FUZZ_RATIO if not ratio else ratio
            return lambda commit, repository=None: fuzz.partial_ratio(string, commit.message.splitlines()[0]) >= ratio

        @classmethod
        def gardening(cls, string, ratio=None):
            def result(commit, repository=None, string=string, ratio=ratio):
                base = cls.fuzzy(string, ratio=ratio)
                if not base(commit, repository=repository) if fuzz else base.search(commit.message.splitlines()[0]):
                    return False
                if not repository:
                    return True

                test_paths = [
                    value for key, value in repository.config().items()
                    if key.startswith('webkitscmpy.tests')
                ]
                if not test_paths:
                    return False
                for file in repository.files_changed(commit.hash):
                    if any([file.startswith(path) for path in test_paths]):
                        continue
                    return False
                return True

            return result


    name = 'pickable'
    help = 'List commits in a range which can be cherry-picked'

    DEFAULT_PATTERNS = [
        'Versioning',
        'Gardening',
        'Build-Fix',
        'Apply-Patch',
    ]
    PATTERNS = {
        'Cherry-pick': [re.compile(r'^[Cc]herry[- ][Pp]ick')],
        'Versioning': [
            re.compile(r'^[Vv]ersioning\.?$'),
            re.compile(r'^Revert "?[Vv]ersioning\.?"?$'),
        ],
        'Gardening': [
            Filters.gardening(r'GARDENING', ratio=85),
            Filters.gardening(r'gardening', ratio=85),
            Filters.gardening(r'REBASELINE'),
            Filters.gardening(r'rebaseline'),
            Filters.gardening(r'is a constant failure'),
            Filters.gardening(r'is an almost constant failure'),
            Filters.gardening('are constant failures'),
            Filters.gardening('tests consistently failing'),
        ],
        'Build-Fix': [
            re.compile(r'^Apply build[ -]fix'),
            re.compile(r'^Unreviewed build[ -]fix'),
        ],
        'Apply-Patch': [re.compile(r'^Apply patch')],
        'None': [],
    }

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'argument', nargs=1,
            type=str, default=None,
            help='String representation of a commit, branch or range of commits to filter for cherry-pickable commits',
        )
        parser.add_argument(
            '--json', '-j',
            help='Convert the commit to a machine-readable JSON object',
            action='store_true',
            dest='json',
            default=False,
        )
        parser.add_argument(
            '--into', '-i',
            help='Branch to pick changes into',
            type=str,
            dest='into',
            default=None,
        )
        parser.add_argument(
            '--exclude', '-e',
            help='Exclude certain patterns. By default, assumes the string argument is a regex. The program '
                'will use certain prepared regexes for some string arguments (some of which are enabled by default), '
                'those arguments are: {}'.format(string_utils.join([
                    "'{}' ({})".format(pattern, 'enabled' if pattern in cls.DEFAULT_PATTERNS else 'disabled')
                    for pattern in cls.PATTERNS.keys()
                ])),
            action='append',
            dest='excluded',
            default=None,
        )
        parser.add_argument(
            '--scope', '-s',
            help='Scope pickable query to a specific path in the project',
            action='append',
            dest='scopes',
            default=None,
        )
        parser.add_argument(
            '--filter', '-f',
            help='Print only commits whose message matches a provided filter',
            action='append',
            dest='filters',
            default=[],
        )
        parser.add_argument(
            '--by', '-b',
            help='Print only commits made by a specific individual (or individuals)',
            action='append',
            dest='by',
            default=[],
        )

    @classmethod
    def pickable(cls, commits, repository, commits_story=None, excluded=None):
        filtered_in = set()
        all_commits = dict()

        commits_story = commits_story or CommitsStory()

        if excluded is None:
            excluded = cls.DEFAULT_PATTERNS
        excluded = list(itertools.chain(
            *[cls.PATTERNS[arg] if arg in cls.PATTERNS else [re.compile(arg)] for arg in excluded]
        ))

        for commit in commits:
            all_commits[str(commit)] = commit
            if any([ex(commit, repository=repository) if callable(ex) else ex.search(commit.message.splitlines()[0]) for ex in excluded]):
                continue
            if commit in commits_story:
                continue
            filtered_in.add(str(commit))

        for ref in list(filtered_in):
            commit = all_commits[ref]
            relationships = Trace.relationships(commit, repository)
            if not relationships:
                continue
            for rel in relationships:
                if rel.type in Relationship.IDENTITY:
                    if rel.commit in commits_story:
                        filtered_in.remove(ref)
                    break
                if rel.type in Relationship.PAIRED + Relationship.UNDO and str(rel.commit) not in filtered_in:
                    filtered_in.remove(ref)
                    break

        return [commit for commit in commits if str(commit) in filtered_in]

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only run '{}' on a native Git repository\n".format(cls.name))
            return 1

        reference = args.argument[0]
        if not args.into:
            args.into = repository.default_branch
        if reference == args.into:
            sys.stderr.write("Cannot merge '{}' into itself\n".format(args.into))
            if args.into == repository.default_branch:
                sys.stderr.write("Specify branch to merge into with the --into flag\n")
            return 1

        branch_point = None
        if reference in repository.branches:
            branch_point = repository.merge_base(args.into, reference)
            reference = '{}..{}'.format(branch_point.hash if branch_point else args.into, reference)

        try:
            if '..' in reference:
                if '...' in reference:
                    sys.stderr.write("'pickable' sub-command only supports '..' notation\n")
                    return 1
                references = reference.split('..')
                if len(references) > 2:
                    sys.stderr.write('Can only include two references in a range\n')
                    return 1
                if not branch_point:
                    branch_point = repository.merge_base(args.into, references[1])
                commits = [commit for commit in repository.commits(
                    begin=dict(argument=references[0]),
                    end=dict(argument=references[1]),
                    scopes=args.scopes,
                )]

            else:
                if args.scopes:
                    sys.stderr.write('Scope argument invalid when only one commit specified\n')
                    return 1
                branch_point = repository.merge_base(args.into, reference)
                commits = [repository.find(reference, include_log=True)]

        except (local.Scm.Exception, TypeError, ValueError) as exception:
            # ValueErrors and Scm exceptions usually contain enough information to be displayed
            # to the user as an error
            sys.stderr.write(str(exception) + '\n')
            return 1

        story = None
        if branch_point:
            story = CommitsStory()
            for commit in repository.commits(begin=dict(argument=branch_point.hash), end=dict(argument=args.into)):
                story.add(commit)
                relationships = Trace.relationships(commit, repository)
                for rel in relationships or []:
                    if rel.type in Relationship.IDENTITY:
                        story.add(rel.commit)

        commits = cls.pickable(commits, repository, commits_story=story, excluded=args.excluded)
        if not commits:
            sys.stderr.write("No commits in specified range are 'pickable'\n")
            return 1

        filters = []
        if args.by:
            for person in args.by:
                filters.append(lambda commit: commit.author == person or person in commit.author.emails or person == commit.author.github)
        if args.filters:
            for filter in args.filters:
                filters.append(lambda commit: re.search(filter, commit.message))
        if filters:
            commits = [commit for commit in commits if any([f(commit) for f in filters])]

        return Info.print_(args, commits, verbose_default=1)
