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

import os
import sys
import tempfile

from .command import Command
from .diff.html_diff import HTMLDiff

from webkitbugspy import radar, Tracker
from webkitcorepy import run, Terminal
from webkitscmpy import local, log


class Apply(Command):
    name = 'apply'
    help = 'Apply a patch attached to a bug tracker issue (for example, a proposed revert or build fix)'

    # Patch names build-failure triage tends to attach, most-preferred first. When an issue has more
    # than one patch and the user does not name one, the first of these that is present is the
    # default selection in the chooser.
    PREFERRED_PATCHES = ('triage-revert.patch', 'triage-fix.patch')

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'issue', type=str,
            help='Issue the patch is attached to (an issue URL, rdar://<id>, or a bare radar id)',
        )
        parser.add_argument(
            'name', type=str, default=None, nargs='?',
            help='Name of the attached patch to apply; if omitted, choose from the issue\'s patches',
        )
        parser.add_argument(
            '--no-commit', default=True, action='store_false', dest='commit',
            help='Apply the patch to the working tree only, instead of committing it with `git am`',
        )

    @classmethod
    def resolve_issue(cls, string):
        '''Resolve a tracker link, URL, or bare radar id to a webkitbugspy issue.'''
        issue = Tracker.from_string(string)
        if issue:
            return issue
        # from_string rejects a bare number; a bare id is only meaningful for radar, so resolve one
        # against the radar tracker -- digits-only, so a malformed link is never coerced into an id.
        stripped = string.strip()
        if stripped.isdigit():
            for tracker in Tracker._trackers:
                if isinstance(tracker, radar.Tracker):
                    return tracker.issue(int(stripped))
        return None

    @classmethod
    def select_patch(cls, patches, name=None):
        '''Pick one attachment from `patches`. With an explicit `name`, return the exact match (or
        None if there is none). Otherwise return the only patch, or prompt the user to choose,
        defaulting to the most-preferred known patch that is present.'''
        by_name = {patch.name: patch for patch in patches}
        if name:
            return by_name.get(name)
        if len(patches) == 1:
            return patches[0]
        options = sorted(by_name.keys())
        default = next((name for name in cls.PREFERRED_PATCHES if name in by_name), options[0])
        return by_name.get(Terminal.choose(
            'Which patch would you like to apply?',
            options=options, default=default, numbered=True,
        ))

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only '{}' on a native Git repository\n".format(cls.name))
            return 1
        if repository.modified():
            sys.stderr.write('Please commit or stash your changes before applying a patch\n')
            return 1

        issue = cls.resolve_issue(args.issue)
        if not issue:
            sys.stderr.write("Could not resolve '{}' to an issue\n".format(args.issue))
            return 1

        patches = issue.patches
        if patches is None:
            sys.stderr.write('{} does not support patch attachments\n'.format(issue.tracker.NAME))
            return 1
        if not patches:
            sys.stderr.write('No patches are attached to {}\n'.format(issue.link))
            return 1

        patch = cls.select_patch(patches, name=args.name)
        if not patch:
            sys.stderr.write("No '{}' patch is attached to {}\n".format(args.name, issue.link))
            return 1

        # The patch is issue-attachment content -- whoever can attach to the issue controls these
        # bytes, applied via git am/apply: the same trust as applying any colleague's patch.
        content = patch.contents()
        if content is None:
            sys.stderr.write("Could not download '{}' from {}\n".format(patch.name, issue.link))
            return 1
        if not isinstance(content, bytes):
            content = content.encode('utf-8')
        if not content.strip():
            sys.stderr.write("'{}' on {} is empty\n".format(patch.name, issue.link))
            return 1

        # On an interactive terminal, open the patch as an HTML diff and block until the viewer is
        # dismissed -- these bytes came from an attachment, so let the user see what they're applying
        # first. Dismissing the diff continues; a user who disagrees interrupts the program instead.
        if Terminal.isatty(sys.stdout) and Terminal.isatty(sys.stdin):
            with HTMLDiff(block=True, repository=repository) as diff_viewer:
                diff_viewer.add_lines(content.decode('utf-8', errors='replace').splitlines())

        handle, patch_path = tempfile.mkstemp(suffix='.patch')
        try:
            with os.fdopen(handle, 'wb') as patch_file:
                patch_file.write(content)

            if args.commit:
                result = run([repository.executable(), 'am', patch_path], cwd=repository.root_path)
            else:
                result = run([repository.executable(), 'apply', '--3way', patch_path], cwd=repository.root_path)

            if result.returncode:
                sys.stderr.write("Failed to apply '{}' from {}\n".format(patch.name, issue.link))
                if args.commit:
                    run([repository.executable(), 'am', '--abort'], cwd=repository.root_path, capture_output=True)
                    sys.stderr.write('Re-run with --no-commit to apply it to the working tree and resolve conflicts by hand.\n')
                else:
                    sys.stderr.write('Conflicting hunks were left as conflict markers / .rej files; `git checkout -- .` to discard.\n')
                return 1
        finally:
            os.remove(patch_path)

        log.info("Applied '{}' from {}".format(patch.name, issue.link))
        return 0
