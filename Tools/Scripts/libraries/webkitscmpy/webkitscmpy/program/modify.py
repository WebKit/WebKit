# Copyright (C) 2025 Apple Inc. All rights reserved.
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
import sys

from .command import Command
from .trace import Trace, Relationship
from .commit import Commit as CommitProgram

from webkitbugspy import Tracker, bugzilla, radar
from webkitcorepy import arguments, run, string_utils, Terminal
from webkitscmpy import local, log
from ..commit import Commit


def add_keyword_to_issue(issue, keyword):
    if not isinstance(issue.tracker, radar.Tracker):
        for candidate in issue.references:
            if isinstance(candidate.tracker, radar.Tracker):
                issue = candidate
                break
        else:
            sys.stderr.write(f"No radar found for issue {issue}\n")
            return False

    try:
        issue.set_keywords([keyword] + issue.keywords)
        log.info(f"Successfully added '{keyword}' to the issue {issue}")
    except Exception as e:
        client = radar.radarclient()
        if client and hasattr(client, 'exceptions') and isinstance(e, client.exceptions.UnsuccessfulResponseException):
            sys.stderr.write(f"Failed to add keyword to radar issue {issue}\n")
        else:
            sys.stderr.write(f"Failed to add keyword to radar issue {issue}: {e}\n")
        return False
    return True


def get_keyword(args):
    if not args.keyword:
        sys.stderr.write("No keyword provided. Exiting.\n")
        return None
    return args.keyword


class Modify(Command):
    name = 'modify'
    help = 'Perform a given modification to an existing bug,' \
           'passing provided info to the associated radar.'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'arguments', nargs='+',
            type=str,
            help='String representation of the commit hash(es) to modify',
        )
        parser.add_argument(
            '--keyword', '-k',
            dest='keyword',
            required=True,
            help='Keyword to add to radar from commit hash(es)',
            type=str,
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        keyword = get_keyword(args)
        if not keyword:
            return 1

        rdar = None

        for tracker in Tracker._trackers:
            if isinstance(tracker, radar.Tracker):
                rdar = tracker
                break
        if not rdar or not rdar.radarclient():
            if not rdar:
                sys.stderr.write('Radar not available. Please validate credentials. \n')
            return 255

        for commit_hash in args.arguments:
            commit = repository.find(commit_hash, include_identifier=False)
            if not commit:
                sys.stderr.write(f"Failed to find '{commit_hash}' in the current repository\n")
                continue

            for relation in (Trace.relationships(commit, repository) or []):
                if relation.type in Relationship.IDENTITY:
                    commit = relation.commit
                    break

            if not commit.issues:
                sys.stderr.write(f"Found {commit}, but it doesn't reference any issues\n")
                continue

            issue = next((n for n in commit.issues if isinstance(n.tracker, radar.Tracker)), None)
            if not issue:
                issue = commit.issues[0]

            log.info(f"{commit} references {issue}")

            if not add_keyword_to_issue(issue, keyword):
                return 1

        return 0
