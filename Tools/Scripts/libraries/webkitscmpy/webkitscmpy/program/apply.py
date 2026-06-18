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

from webkitbugspy import radar, Tracker
from webkitcorepy import run
from webkitscmpy import local, log


class Apply(Command):
    name = 'apply'
    help = "Apply a proposed revert or build fix that build-failure triage attached to a radar"

    # Stable attachment names produced by `combine triage --llm-enhance`.
    WHICH = ('revert', 'fix')

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'which', type=str.lower, choices=cls.WHICH,
            help="Which attached patch to apply: 'revert' or 'fix'",
        )
        parser.add_argument(
            'issue', type=str,
            help='Radar the patch is attached to (rdar://<id>, a bare id, or a radar URL)',
        )
        parser.add_argument(
            '--no-commit', default=True, action='store_false', dest='commit',
            help='Apply the patch to the working tree only, instead of committing it with `git am`',
        )

    @classmethod
    def attachment_name(cls, which):
        return 'triage-{}.patch'.format(which)

    @classmethod
    def radar_tracker(cls):
        '''The registered radar.Tracker, if any -- triage only ever attaches patches to radars.'''
        for tracker in Tracker._trackers:
            if isinstance(tracker, radar.Tracker):
                return tracker
        instance = Tracker.instance()
        return instance if isinstance(instance, radar.Tracker) else None

    @classmethod
    def resolve_issue(cls, string):
        '''Resolve a radar link, URL, or bare id to a webkitbugspy issue.'''
        issue = Tracker.from_string(string)
        if issue:
            return issue
        # from_string rejects a bare number; resolve one against the radar tracker, digits-only so a
        # malformed link is never coerced into the wrong id.
        if string.strip().isdigit():
            tracker = cls.radar_tracker()
            if tracker:
                return tracker.issue(int(string.strip()))
        return None

    @classmethod
    def contents(cls, issue, name):
        '''Bytes of the attachment named `name` on `issue`, or None. Emits a specific reason to
        stderr for every failure (unsupported tracker, fetch/download error, locked, or genuinely
        absent), so the caller does not need to -- and must not -- print its own.'''
        client = getattr(getattr(issue, 'tracker', None), 'client', None)
        if not client:
            sys.stderr.write('{} does not support attachments\n'.format(getattr(issue, 'link', 'This tracker')))
            return None
        # radar_for_id raises (rather than returning None) on a missing/unauthorized radar, and the
        # download is a network call -- report either cleanly instead of crashing.
        try:
            radar_object = client.radar_for_id(issue.id)
            attachments = list(radar_object.attachments.items()) if radar_object else []
        except Exception as error:
            sys.stderr.write('Could not read attachments from {}: {}\n'.format(issue.link, error))
            return None
        for attachment in attachments:
            if attachment.fileName != name:
                continue
            if getattr(attachment, 'locked', False):
                sys.stderr.write("'{}' on {} is locked and cannot be downloaded\n".format(name, issue.link))
                return None
            try:
                return attachment.content()
            except Exception as error:
                sys.stderr.write("Could not download '{}' from {}: {}\n".format(name, issue.link, error))
                return None
        sys.stderr.write("No '{}' attachment found on {}\n".format(name, issue.link))
        return None

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
            sys.stderr.write("Could not resolve '{}' to a radar\n".format(args.issue))
            return 1

        # The patch is radar-attachment content -- whoever can attach to the radar controls these
        # bytes, applied via git am/apply: the same trust as applying any colleague's patch.
        content = cls.contents(issue, cls.attachment_name(args.which))
        if content is None:
            return 1
        if not isinstance(content, bytes):
            content = content.encode('utf-8')
        if not content.strip():
            sys.stderr.write("'{}' on {} is empty\n".format(cls.attachment_name(args.which), issue.link))
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
                sys.stderr.write('Failed to apply the {} from {}\n'.format(args.which, issue.link))
                if args.commit:
                    run([repository.executable(), 'am', '--abort'], cwd=repository.root_path, capture_output=True)
                    sys.stderr.write('Re-run with --no-commit to apply it to the working tree and resolve conflicts by hand.\n')
                else:
                    sys.stderr.write('Conflicting hunks were left as conflict markers / .rej files; `git checkout -- .` to discard.\n')
                return 1
        finally:
            os.remove(patch_path)

        log.info('Applied the {} from {}'.format(args.which, issue.link))
        return 0
