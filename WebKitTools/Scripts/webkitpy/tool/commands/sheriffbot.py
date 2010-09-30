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
from webkitpy.common.config.ports import WebKitPort
from webkitpy.tool.bot.sheriff import Sheriff
from webkitpy.tool.bot.sheriffircbot import SheriffIRCBot
from webkitpy.tool.commands.queues import AbstractQueue
from webkitpy.tool.commands.stepsequence import StepSequenceErrorHandler


class SheriffBot(AbstractQueue, StepSequenceErrorHandler):
    name = "sheriff-bot"
    watchers = AbstractQueue.watchers + [
        "abarth@webkit.org",
        "eric@webkit.org",
    ]

    def _update(self):
        self.run_webkit_patch(["update", "--force-clean", "--quiet"])

    # AbstractQueue methods

    def begin_work_queue(self):
        AbstractQueue.begin_work_queue(self)
        self._sheriff = Sheriff(self._tool, self)
        self._irc_bot = SheriffIRCBot(self._tool, self._sheriff)
        self._tool.ensure_irc_connected(self._irc_bot.irc_delegate())

    def work_item_log_path(self, failure_map):
        return None

    def _is_old_failure(self, revision):
        return self._tool.status_server.svn_revision(revision)

    def next_work_item(self):
        self._irc_bot.process_pending_messages()
        self._update()

        # FIXME: We need to figure out how to provoke_flaky_builders.

        failure_map = self._tool.buildbot.failure_map()
        failure_map.filter_out_old_failures(self._is_old_failure)
        if failure_map.is_empty():
            return None
        return failure_map

    def should_proceed_with_work_item(self, failure_map):
        # Currently, we don't have any reasons not to proceed with work items.
        return True

    def process_work_item(self, failure_map):
        failing_revisions = failure_map.failing_revisions()
        for revision in failing_revisions:
            builders = failure_map.builders_failing_for(revision)
            tests = failure_map.tests_failing_for(revision)
            try:
                commit_info = self._tool.checkout().commit_info_for_revision(revision)
                if not commit_info:
                    print "FAILED to fetch CommitInfo for r%s, likely missing ChangeLog" % revision
                    continue
                self._sheriff.post_irc_warning(commit_info, builders)
                self._sheriff.post_blame_comment_on_bug(commit_info, builders, tests)

            finally:
                for builder in builders:
                    self._tool.status_server.update_svn_revision(revision, builder.name())
        return True

    def handle_unexpected_error(self, failure_map, message):
        log(message)

    # StepSequenceErrorHandler methods

    @classmethod
    def handle_script_error(cls, tool, state, script_error):
        # Ideally we would post some information to IRC about what went wrong
        # here, but we don't have the IRC password in the child process.
        pass
