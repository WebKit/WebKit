# Copyright (c) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from webkitpy.tool.commands.abstractsequencedcommand import AbstractSequencedCommand

from webkitpy.tool import steps


default_changelog_message = "Unreviewed.  Rolled DEPS.\n\n"

class RollChromiumDEPS(AbstractSequencedCommand):
    name = "roll-chromium-deps"
    help_text = "Updates Chromium DEPS (defaults to the last-known good revision of Chromium)"
    argument_names = "[CHROMIUM_REVISION]"
    steps = [
        steps.UpdateChromiumDEPS,
        steps.PrepareChangeLogForDEPSRoll,
        steps.ConfirmDiff,
        steps.Commit,
    ]

    def _prepare_state(self, options, args, tool):
        return {
            "chromium_revision": (args and args[0]),
            "changelog_message": default_changelog_message,
        }


class PostChromiumDEPSRoll(AbstractSequencedCommand):
    name = "post-chromium-deps-roll"
    help_text = "Posts a patch to update Chromium DEPS (revision defaults to the last-known good revision of Chromium)"
    argument_names = "CHROMIUM_REVISION CHROMIUM_REVISION_NAME [CHANGELOG_MESSAGE]"
    steps = [
        steps.CleanWorkingDirectory,
        steps.Update,
        steps.UpdateChromiumDEPS,
        steps.PrepareChangeLogForDEPSRoll,
        steps.CreateBug,
        steps.PostDiff,
    ]

    def _prepare_state(self, options, args, tool):
        options.review = False
        options.request_commit = True

        chromium_revision = args[0]
        chromium_revision_name = args[1]
        changelog_message = args[2] if len(args) >= 3 else default_changelog_message
        return {
            "chromium_revision": chromium_revision,
            "changelog_message": changelog_message,
            "bug_title": "Roll Chromium DEPS to %s" % chromium_revision_name,
            "bug_description": "A DEPS roll a day keeps the build break away.",
        }
