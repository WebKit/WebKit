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

from webkitpy.tool.commands.queues import AbstractQueue
from webkitpy.common.system.deprecated_logging import log
from webkitpy.common.config.ports import WebKitPort


class SheriffBot(AbstractQueue):
    name = "sheriff-bot"

    def update(self):
        self.tool.executive.run_and_throw_if_fail(WebKitPort.update_webkit_command(), quiet=True)

    # AbstractQueue methods

    def begin_work_queue(self):
        AbstractQueue.begin_work_queue(self)
        self.tool.ensure_irc_connected()

    def work_item_log_path(self, failure_info):
        return os.path.join("%s-logs" % self.name, "%s.log" % failure_info["svn_revision"])

    def next_work_item(self):
        self.update()
        for svn_revision, builders in self.tool.buildbot.revisions_causing_failures().items():
            if self.tool.status_server.svn_revision(svn_revision):
                continue
            return {
                "svn_revision": svn_revision,
                "builders": builders
            }
        return None

    def should_proceed_with_work_item(self, failure_info):
        # Currently, we don't have any reasons not to proceed with work items.
        return True

    def process_work_item(self, failure_info):
        svn_revision = failure_info["svn_revision"]
        builders = failure_info["builders"]

        self.update()
        commit_info = self.tool.checkout().commit_info_for_revision(svn_revision)
        responsible_parties = [
            commit_info.committer(),
            commit_info.author(),
            commit_info.reviewer()
        ]
        irc_nicknames = sorted(set([party.irc_nickname for party in responsible_parties if party and party.irc_nickname]))
        irc_prefix = ": " if irc_nicknames else ""
        irc_message = "%s%sr%s appears to have broken %s" % (
            ", ".join(irc_nicknames),
            irc_prefix,
            svn_revision,
            ", ".join([builder.name() for builder in builders]))

        self.tool.irc().post(irc_message)

        for builder in builders:
            self.tool.status_server.update_svn_revision(svn_revision, builder.name())

    def handle_unexpected_error(self, failure_info, message):
        log(message)
