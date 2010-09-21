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

    def work_item_log_path(self, new_failures):
        return os.path.join("%s-logs" % self.name, "%s.log" % new_failures.keys()[0])

    def _new_failures(self, revisions_causing_failures, old_failing_svn_revisions):
        # We ignore failures that might have been caused by svn_revisions that
        # we've already complained about.  This is conservative in the sense
        # that we might be ignoring some new failures, but our experience has
        # been that skipping this check causes a lot of spam for builders that
        # take a long time to cycle.
        old_failing_builder_names = []
        for svn_revision in old_failing_svn_revisions:
            old_failing_builder_names.extend(
                [builder.name() for builder in revisions_causing_failures[svn_revision]])

        new_failures = {}
        for svn_revision, builders in revisions_causing_failures.items():
            if svn_revision in old_failing_svn_revisions:
                # FIXME: We should re-process the work item after some time delay.
                # https://bugs.webkit.org/show_bug.cgi?id=36581
                continue
            new_builders = [builder for builder in builders
                            if builder.name() not in old_failing_builder_names]
            if new_builders:
                new_failures[svn_revision] = new_builders

        return new_failures

    def next_work_item(self):
        self._irc_bot.process_pending_messages()
        self._update()

        # We do one read from buildbot to ensure a consistent view.
        revisions_causing_failures = self._tool.buildbot.revisions_causing_failures()

        # Similarly, we read once from our the status_server.
        old_failing_svn_revisions = []
        for svn_revision in revisions_causing_failures.keys():
            if self._tool.status_server.svn_revision(svn_revision):
                old_failing_svn_revisions.append(svn_revision)

        new_failures = self._new_failures(revisions_causing_failures,
                                          old_failing_svn_revisions)

        self._sheriff.provoke_flaky_builders(revisions_causing_failures)
        return new_failures

    def should_proceed_with_work_item(self, new_failures):
        # Currently, we don't have any reasons not to proceed with work items.
        return True

    def process_work_item(self, new_failures):
        blame_list = new_failures.keys()
        for svn_revision, builders in new_failures.items():
            try:
                commit_info = self._tool.checkout().commit_info_for_revision(svn_revision)
                if not commit_info:
                    print "FAILED to fetch CommitInfo for r%s, likely missing ChangeLog" % revision
                    continue
                self._sheriff.post_irc_warning(commit_info, builders)
                self._sheriff.post_blame_comment_on_bug(commit_info,
                                                        builders,
                                                        blame_list)
                self._sheriff.post_automatic_rollout_patch(commit_info,
                                                           builders)
            finally:
                for builder in builders:
                    self._tool.status_server.update_svn_revision(svn_revision,
                                                                builder.name())
        return True

    def handle_unexpected_error(self, new_failures, message):
        log(message)

    # StepSequenceErrorHandler methods

    @classmethod
    def handle_script_error(cls, tool, state, script_error):
        # Ideally we would post some information to IRC about what went wrong
        # here, but we don't have the IRC password in the child process.
        pass
