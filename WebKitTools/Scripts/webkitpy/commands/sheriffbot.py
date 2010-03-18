# Copyright (c) 2009, Google Inc. All rights reserved.
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

from webkitpy.commands.queues import AbstractQueue
from webkitpy.irc.ircproxy import IRCProxy
from webkitpy.webkit_logging import log


class SheriffBot(AbstractQueue):
    name = "sheriff-bot"

    # AbstractQueue methods

    def begin_work_queue(self):
        AbstractQueue.begin_work_queue(self)
        self.tool.ensure_irc_connected()

    def work_item_log_path(self, svn_revision):
        return os.path.join("%s-logs" % self.name, "%s.log" % svn_revision)

    def next_work_item(self):
        # FIXME: Call methods that analyze the build bots.
        return None # FIXME: Should be an SVN revision number.

    def should_proceed_with_work_item(self, svn_revision):
        # Currently, we don't have any reasons not to proceed with work items.
        return True

    def process_work_item(self, svn_revision):
        message = "r%s appears to have broken the build." % svn_revision
        self.tool.irc().post(message)
        # FIXME: What if run_webkit_patch throws an exception?
        self.run_webkit_patch([
            "create-rollout",
            "--force-clean",
            "--non-interactive",
            "--parent-command=%s" % self.name,
            # FIXME: We also need to CC the reviewer, committer, and contributor.
            "--cc=%s" % ",".join(self.watchers),
            svn_revision
        ])

    def handle_unexpected_error(self, svn_revision, message):
        log(message)
