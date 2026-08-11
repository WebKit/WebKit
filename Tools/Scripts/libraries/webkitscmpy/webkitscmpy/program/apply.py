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

import argparse
import os
import sys
import tempfile

from .command import Command
from .diff.html_diff import HTMLDiff
from .pull_request import PullRequest

from webkitbugspy import radar, Tracker
from webkitcorepy import arguments, run, Terminal
from webkitscmpy import local, log


class Apply(Command):
    name = 'apply'
    help = 'Apply a patch attached to a bug tracker issue (for example, a proposed revert or build fix)'

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
    def review(cls, patch, repository, alternatives=False):
        '''Open the patch as a blocking HTML diff and return whether the user accepted it. Closing the
        diff or pressing Enter accepts it; answering 'n' rejects it.'''
        content = patch.contents()
        if content is None:
            sys.stderr.write("Could not download '{}'\n".format(patch.name))
            return False
        text = content if isinstance(content, str) else content.decode('utf-8', errors='replace')
        dismiss = 'pick a different patch' if alternatives else 'cancel'
        sys.stdout.write("Reviewing '{}' -- close the diff or press Enter to apply it, or enter 'n' to {}.\n".format(patch.name, dismiss))
        with HTMLDiff(block=True, repository=repository) as diff_viewer:
            diff_viewer.add_lines(text.splitlines())
        return diff_viewer.accepted

    @classmethod
    def select_patch(cls, patches, name=None, repository=None, interactive=False):
        '''Return the patch to apply, or None. With `name`, the exact match. Otherwise non-interactive
        returns the first patch; interactive shows a menu in the order the tracker lists the patches
        (skipped when there is only one), defaulting to the first, and reviews the chosen patch's diff
        -- closing or accepting the diff selects it, while answering 'n' returns to the menu.'''
        by_name = {patch.name: patch for patch in patches}
        if name:
            patch = by_name.get(name)
            if patch is None or not interactive:
                return patch
            return patch if cls.review(patch, repository) else None
        options = [patch.name for patch in patches]
        if not interactive:
            return patches[0]
        while True:
            choice = options[0] if len(options) == 1 else Terminal.choose(
                'Which patch would you like to apply?',
                options=options, default=options[0], numbered=True,
            )
            if cls.review(by_name[choice], repository, alternatives=len(options) > 1):
                return by_name[choice]
            if len(options) == 1:
                return None

    @classmethod
    def edit_commit_message(cls, repository):
        '''Amend the just-applied commit without a message flag, so git opens its message in the editor.
        No --date, so `git am`'s preserved author date survives.'''
        if run([repository.executable(), 'commit', '--amend'], cwd=repository.root_path).returncode:
            sys.stderr.write("Applied the patch, but couldn't open the commit message for editing; it keeps the patch's message.\n")

    @classmethod
    def offer_pull_request(cls, repository, **kwargs):
        '''Offer to open a pull request for the just-applied commit, running pull-request in-process
        with its default arguments. LoggingGroup supplies the shared verbosity args its main reads,
        mirroring how the top-level parser builds each sub-command.'''
        if Terminal.choose('Would you like to create a pull request?', default='Yes') != 'Yes':
            return
        parser = argparse.ArgumentParser()
        arguments.LoggingGroup(parser)
        PullRequest.parser(parser)
        PullRequest.main(parser.parse_args([]), repository, **kwargs)

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

        interactive = Terminal.isatty(sys.stdout) and Terminal.isatty(sys.stdin)
        patch = cls.select_patch(patches, name=args.name, repository=repository, interactive=interactive)
        if not patch:
            if args.name and not any(candidate.name == args.name for candidate in patches):
                sys.stderr.write("No '{}' patch is attached to {}\n".format(args.name, issue.link))
            else:
                sys.stderr.write('No patch applied\n')
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

        if args.commit and interactive:
            cls.edit_commit_message(repository)
            cls.offer_pull_request(repository, **kwargs)

        log.info("Applied '{}' from {}".format(patch.name, issue.link))
        return 0
