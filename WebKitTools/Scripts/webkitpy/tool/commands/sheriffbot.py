# Copyright (c) 2009 Google Inc. All rights reserved.
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

import os

from webkitpy.common.system.deprecated_logging import log
from webkitpy.common.checkout.changelog import view_source_url
from webkitpy.common.config.ports import WebKitPort
from webkitpy.tool.bot.sheriffircbot import SheriffIRCBot
from webkitpy.tool.commands.queues import AbstractQueue
from webkitpy.tool.grammar import join_with_separators

class SheriffBot(AbstractQueue):
    name = "sheriff-bot"

    def update(self):
        self.tool.executive.run_and_throw_if_fail(WebKitPort.update_webkit_command(), quiet=True)

    # AbstractQueue methods

    def begin_work_queue(self):
        AbstractQueue.begin_work_queue(self)
        self._irc_bot = SheriffIRCBot(self.tool)
        self.tool.ensure_irc_connected(self._irc_bot.irc_delegate())

    def work_item_log_path(self, failure_info):
        return os.path.join("%s-logs" % self.name, "%s.log" % failure_info["svn_revision"])

    def next_work_item(self):
        self._irc_bot.process_pending_messages()
        self.update()
        for svn_revision, builders in self.tool.buildbot.revisions_causing_failures().items():
            if self.tool.status_server.svn_revision(svn_revision):
                # FIXME: We should re-process the work item after some time delay.
                # https://bugs.webkit.org/show_bug.cgi?id=36581
                continue
            return {
                "svn_revision": svn_revision,
                "builders": builders,
                # FIXME: _rollout_reason needs Build objects which we could pass here.
            }
        return None

    def should_proceed_with_work_item(self, failure_info):
        # Currently, we don't have any reasons not to proceed with work items.
        return True

    # _post* methods should move onto some new class where they can share more logic and state.
    def _post_irc_warning(self, commit_info, builders):
        irc_nicknames = sorted([party.irc_nickname for party in commit_info.responsible_parties() if party.irc_nickname])
        irc_prefix = ": " if irc_nicknames else ""
        irc_message = "%s%s%s appears to have broken %s" % (
            ", ".join(irc_nicknames),
            irc_prefix,
            view_source_url(commit_info.revision()),
            join_with_separators([builder.name() for builder in builders]))

        self.tool.irc().post(irc_message)

    def _rollout_reason(self, builders):
        # FIXME: This should explain which layout tests failed
        # however, that would require Build objects here, either passed
        # in through failure_info, or through Builder.latest_build.
        builder_names = [builder.name() for builder in builders]
        return "Caused builders %s to fail." % join_with_separators(builder_names)

    def _post_rollout_patch(self, commit_info, rollout_reason):
        # For now we're only posting rollout patches for commit-queue'd patches.
        commit_bot_email = "eseidel@chromium.org"
        if commit_bot_email not in commit_info.committer().emails:
            return

        args = [
            "create-rollout",
            "--force-clean",
            "--non-interactive",
            "--parent-command=%s" % self.name,
            commit_info.revision(),
            self._rollout_reason(builders),
        ]
        try:
            self.run_webkit_patch(args)
        except:
            log("Failed to create-rollout.")

    def _post_blame_comment_on_bug(self, commit_info, builders):
        if not commit_info.bug_id():
            return
        comment = "%s appears to have broken %s" % (
            view_source_url(commit_info.revision()),
            join_with_separators([builder.name() for builder in builders]))
        self.tool.bugs.post_comment_to_bug(commit_info.bug_id(), comment)

    def process_work_item(self, failure_info):
        svn_revision = failure_info["svn_revision"]
        builders = failure_info["builders"]

        self.update()
        commit_info = self.tool.checkout().commit_info_for_revision(svn_revision)
        self._post_irc_warning(commit_info, builders)
        self._post_blame_comment_on_bug(commit_info, builders)
        self._post_rollout_patch(commit_info, builders)

        for builder in builders:
            self.tool.status_server.update_svn_revision(svn_revision, builder.name())
        return True

    def handle_unexpected_error(self, failure_info, message):
        log(message)
