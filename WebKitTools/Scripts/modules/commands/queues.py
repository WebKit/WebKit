#!/usr/bin/env python
# Copyright (c) 2009, Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
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

import re

from datetime import datetime
from optparse import make_option

from modules.executive import ScriptError
from modules.grammar import pluralize
from modules.landingsequence import LandingSequence, LandingSequenceErrorHandler
from modules.logging import error, log
from modules.multicommandtool import Command
from modules.patchcollection import PersistentPatchCollection, PersistentPatchCollectionDelegate
from modules.statusbot import StatusBot
from modules.workqueue import WorkQueue, WorkQueueDelegate

class AbstractQueue(Command, WorkQueueDelegate):
    show_in_main_help = False
    watchers = "webkit-bot-watchers@googlegroups.com"
    def __init__(self, options=None): # Default values should never be collections (like []) as default values are shared between invocations
        options_list = (options or []) + [
            make_option("--no-confirm", action="store_false", dest="confirm", default=True, help="Do not ask the user for confirmation before running the queue.  Dangerous!"),
            make_option("--status-host", action="store", type="string", dest="status_host", default=StatusBot.default_host, help="Hostname (e.g. localhost or commit.webkit.org) where status updates should be posted."),
        ]
        Command.__init__(self, "Run the %s" % self.name, options=options_list)

    def _cc_watchers(self, bug_id):
        try:
            self.tool.bugs.add_cc_to_bug(bug_id, self.watchers)
        except Exception, e:
            log("Failed to CC watchers: %s." % e)

    def queue_log_path(self):
        return "%s.log" % self.name

    def work_logs_directory(self):
        return "%s-logs" % self.name

    def status_host(self):
        return self.options.status_host

    def begin_work_queue(self):
        log("CAUTION: %s will discard all local changes in %s" % (self.name, self.tool.scm().checkout_root))
        if self.options.confirm:
            response = raw_input("Are you sure?  Type \"yes\" to continue: ")
            if (response != "yes"):
                error("User declined.")
        log("Running WebKit %s. %s" % (self.name, datetime.now().strftime(WorkQueue.log_date_format)))

    def should_continue_work_queue(self):
        return True

    def next_work_item(self):
        raise NotImplementedError, "subclasses must implement"

    def should_proceed_with_work_item(self, work_item):
        raise NotImplementedError, "subclasses must implement"

    def process_work_item(self, work_item):
        raise NotImplementedError, "subclasses must implement"

    def handle_unexpected_error(self, work_item, message):
        raise NotImplementedError, "subclasses must implement"

    def run_bugzilla_tool(self, args):
        bugzilla_tool_args = [self.tool.path()] + map(str, args)
        self.tool.executive.run_and_throw_if_fail(bugzilla_tool_args)

    def log_progress(self, patch_ids):
        log("%s in %s [%s]" % (pluralize("patch", len(patch_ids)), self.name, ", ".join(map(str, patch_ids))))

    def execute(self, options, args, tool):
        self.options = options
        self.tool = tool
        work_queue = WorkQueue(self.name, self)
        return work_queue.run()


class CommitQueue(AbstractQueue, LandingSequenceErrorHandler):
    name = "commit-queue"
    def __init__(self):
        AbstractQueue.__init__(self)

    # AbstractQueue methods

    def begin_work_queue(self):
        AbstractQueue.begin_work_queue(self)

    def next_work_item(self):
        patches = self.tool.bugs.fetch_patches_from_commit_queue(reject_invalid_patches=True)
        if not patches:
            return None
        # Only bother logging if we have patches in the queue.
        self.log_progress([patch['id'] for patch in patches])
        return patches[0]

    def should_proceed_with_work_item(self, patch):
        red_builders_names = self.tool.buildbot.red_core_builders_names()
        if red_builders_names:
            red_builders_names = map(lambda name: "\"%s\"" % name, red_builders_names) # Add quotes around the names.
            return (False, "Builders [%s] are red. See http://build.webkit.org." % ", ".join(red_builders_names), None)
        return (True, "Landing patch %s from bug %s." % (patch["id"], patch["bug_id"]), patch)

    def process_work_item(self, patch):
        self._cc_watchers(patch["bug_id"])
        self.run_bugzilla_tool(["land-attachment", "--force-clean", "--non-interactive", "--parent-command=commit-queue", "--quiet", patch["id"]])

    def handle_unexpected_error(self, patch, message):
        self.tool.bugs.reject_patch_from_commit_queue(patch["id"], message)

    # LandingSequenceErrorHandler methods

    @classmethod
    def handle_script_error(cls, tool, patch, script_error):
        tool.bugs.reject_patch_from_commit_queue(patch["id"], script_error.message_with_output())


class AbstractReviewQueue(AbstractQueue, PersistentPatchCollectionDelegate, LandingSequenceErrorHandler):
    def __init__(self, options=None):
        AbstractQueue.__init__(self, options)

    # PersistentPatchCollectionDelegate methods

    def collection_name(self):
        return self.name

    def fetch_potential_patch_ids(self):
        return self.tool.bugs.fetch_attachment_ids_from_review_queue()

    def status_server(self):
        return self.tool.status()

    # AbstractQueue methods

    def begin_work_queue(self):
        AbstractQueue.begin_work_queue(self)
        self.tool.status().set_host(self.options.status_host)
        self._patches = PersistentPatchCollection(self)

    def next_work_item(self):
        patch_id = self._patches.next()
        if patch_id:
            return self.tool.bugs.fetch_attachment(patch_id)

    def should_proceed_with_work_item(self, patch):
        raise NotImplementedError, "subclasses must implement"

    def process_work_item(self, patch):
        raise NotImplementedError, "subclasses must implement"

    def handle_unexpected_error(self, patch, message):
        log(message)

    # LandingSequenceErrorHandler methods

    @classmethod
    def handle_script_error(cls, tool, patch, script_error):
        log(script_error.message_with_output())


class StyleQueue(AbstractReviewQueue):
    name = "style-queue"
    def __init__(self):
        AbstractReviewQueue.__init__(self)

    def should_proceed_with_work_item(self, patch):
        return (True, "Checking style for patch %s on bug %s." % (patch["id"], patch["bug_id"]), patch)

    def process_work_item(self, patch):
        try:
            self.run_bugzilla_tool(["check-style", "--force-clean", "--non-interactive", "--parent-command=style-queue", patch["id"]])
            message = "%s ran check-webkit-style on attachment %s without any errors." % (self.name, patch["id"])
            self.tool.bugs.post_comment_to_bug(patch["bug_id"], message, cc=self.watchers)
            self._patches.did_pass(patch)
        except ScriptError, e:
            self._patches.did_fail(patch)
            raise e

    @classmethod
    def handle_script_error(cls, tool, patch, script_error):
        command = script_error.script_args
        if type(command) is list:
            command = command[0]
        # FIXME: We shouldn't need to use a regexp here.  ScriptError should
        #        have a better API.
        if re.search("check-webkit-style", command):
            message = "Attachment %s did not pass %s:\n\n%s" % (patch["id"], cls.name, script_error.message_with_output(output_limit=5*1024))
            tool.bugs.post_comment_to_bug(patch["bug_id"], message, cc=cls.watchers)
