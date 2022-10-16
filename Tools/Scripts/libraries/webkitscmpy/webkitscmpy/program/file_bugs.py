# Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
from .branch import Branch

from webkitbugspy import Tracker, radar
from webkitcorepy import arguments, run, Terminal
from webkitscmpy import local, log, remote

from webkitbugspy.radar import Tracker as RadarTracker
from webkitbugspy.bugzilla import Tracker as BugzillaTracker


class FileBugs(Command):
    name = 'file-bugs'
    aliases = ['bugs']
    help = 'File bugzilla issues for commits without one specified'

    BUG_PLACEHOLDER = 'Need the bug URL (OOPS!).'
    RADAR_PLACEHOLDER = 'Include a Radar link (OOPS!).'

    @classmethod
    def parse_description(cls, lines):
        # If there's less than 6 lines it can't really match the commit template
        if len(lines) < 6:
            return

        # Check if there's something on the 3rd line (for a rdar:)
        # If so, then start our search for the description one line
        # later.
        desc_start = 5
        if not lines[2] == '':
            desc_start = 6
            if len(lines) < 7:
                return

        # Search for a description from the 7th line until we see a '*' for file changes.
        # (should be title,bugzilla,rdar?,blank,reviewer,blank on first 5/6 lines).
        desc = []
        for line in lines[desc_start:]:
            if line.lstrip().startswith('* '):
                break
            desc.append(line)
        return "\n".join(desc)

    @classmethod
    def create_bug(cls, repository, commit):
        lines = commit.message.splitlines()
        if len(lines) < 2:
            return

        issue = None
        radar = None
        needs_radar = False
        if cls.BUG_PLACEHOLDER not in lines[1]:
            issue = Tracker.instance().from_string(lines[1])
            if not issue:
                print("Invalid issue found: " + lines[1])
                return
        if len(lines) > 2:
            needs_radar = cls.RADAR_PLACEHOLDER in lines[2]
            if not needs_radar:
                radar = Tracker.from_string(lines[2])
                if not radar or not isinstance(radar.tracker, RadarTracker):
                    print("Invalid radar found: " + lines[2])
                    return

        if issue and (radar or not needs_radar):
            return

        title = lines[0]
        print('Found a commit without an issue specified. Title: ' + title)
        if not Terminal.choose("Create issue now?", default='No'):
            return

        if not issue:
            if not Terminal.choose("Use existing title?", default='Yes'):
                title = Terminal.input('Issue title: ')
            desc = cls.parse_description(lines)
            print('Parsed description: ' + desc)
            if not Terminal.choose("Use existing description?", default='Yes'):
                desc = Terminal.input('Issue description: ')

            if getattr(Tracker.instance(), 'credentials', None):
                Tracker.instance().credentials(required=True, validate=True)
            issue = Tracker.instance().create(
                title=title,
                description=desc,
            )
            if not issue:
                sys.stderr.write('Failed to create new issue\n')
                return
            print("Created '{}'".format(issue))
            if radar:
                print('Found existing rdar to CC: ' + str(radar))
                issue.cc_radar(block=True, radar=radar)
        if needs_radar:
            radar = issue.cc_radar(block=True, radar=None)
            print("Created '{}'".format(radar))

        # Try to mutate the commit message and insert our newly filed issue.
        # Currently, we can only do this for the HEAD command, and if there's no staged
        # changes that our amend would accidentally bring in. Is this a sufficient check?
        # Detached HEAD state? Error checking on the amend git command?
        # Should we pre-warn at the start of the command if we won't be able to write
        # the results?
        if repository.find('HEAD') != commit:
            print('Commit is not head, cannot edit to insert issue')
            return

        if repository.modified(staged=True) != []:
            print('Staged files found, cannot edit to insert issue')
            return

        modified = commit.message.replace(cls.BUG_PLACEHOLDER, issue.link)
        if radar:
            modified = modified.replace(cls.RADAR_PLACEHOLDER, radar.link)

        print('Writing new commit message:' + modified)
        run([repository.executable(), 'commit', '--amend', '-m', modified])

        # Can we run a `git rebase --exec 'git webkit rewrite-message' <range>` style
        # command?
        # We'd need to store all our new messages somewhere that the other script can
        # access them. Maybe the env? Can't index by commit hash, since the rebase operation
        # will be changing them.
        # rewrite-message subcommand can lookup the new message (somehow) and then git
        # commit --amend -m <new-message>

    @classmethod
    def create_bugs(cls, repository, args, branch_point):
        commits = list(repository.commits(begin=dict(hash=branch_point.hash), end=dict(branch=repository.branch)))

        for commit in commits:
            cls.create_bug(repository, commit)

        # Should we try print a summary of filed issues that we couldn't write into commit
        # messages, so that it's easy for the user to then use them in an interactive rebase.
        return 0

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only '{}' on a native Git repository\n".format(cls.name))
            return 1

        branch_point = Branch.branch_point(repository)
        if not branch_point:
            return 1

        return cls.create_bugs(repository, args, branch_point)
