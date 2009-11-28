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

# FIXME: Trim down this import list once we have unit tests.
import os
import re
import StringIO
import subprocess
import sys
import time

from datetime import datetime, timedelta
from optparse import make_option

from modules.bugzilla import Bugzilla, parse_bug_id
from modules.buildbot import BuildBot
from modules.changelogs import ChangeLog
from modules.comments import bug_comment_from_commit_text
from modules.grammar import pluralize
from modules.landingsequence import LandingSequence, ConditionalLandingSequence, LandingSequenceErrorHandler
from modules.logging import error, log, tee
from modules.multicommandtool import MultiCommandTool, Command
from modules.patchcollection import PatchCollection, PersistentPatchCollection, PersistentPatchCollectionDelegate
from modules.processutils import run_and_throw_if_fail
from modules.scm import CommitMessage, detect_scm_system, ScriptError, CheckoutNeedsUpdate
from modules.statusbot import StatusBot
from modules.webkitport import WebKitPort
from modules.workqueue import WorkQueue, WorkQueueDelegate

class AbstractQueue(Command, WorkQueueDelegate):
    def __init__(self, options=[]):
        options += [
            make_option("--no-confirm", action="store_false", dest="confirm", default=True, help="Do not ask the user for confirmation before running the queue.  Dangerous!"),
            make_option("--status-host", action="store", type="string", dest="status_host", default=StatusBot.default_host, help="Hostname (e.g. localhost or commit.webkit.org) where status updates should be posted."),
        ]
        Command.__init__(self, "Run the %s" % self.name, options=options)

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
        bugzilla_tool_args = [self.tool.path()] + args
        run_and_throw_if_fail(bugzilla_tool_args)

    def log_progress(self, patch_ids):
        log("%s in %s [%s]" % (pluralize("patch", len(patch_ids)), self.name, ", ".join(patch_ids)))

    def execute(self, options, args, tool):
        self.options = options
        self.tool = tool
        work_queue = WorkQueue(self.name, self)
        work_queue.run()


class CommitQueue(AbstractQueue, LandingSequenceErrorHandler):
    name = "commit-queue"
    show_in_main_help = False
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
        self.run_bugzilla_tool(["land-attachment", "--force-clean", "--non-interactive", "--parent-command=commit-queue", "--quiet", patch["id"]])

    def handle_unexpected_error(self, patch, message):
        self.tool.bugs.reject_patch_from_commit_queue(patch["id"], message)

    # LandingSequenceErrorHandler methods

    @classmethod
    def handle_script_error(cls, tool, patch, script_error):
        tool.bugs.reject_patch_from_commit_queue(patch["id"], script_error.message_with_output())


class AbstractTryQueue(AbstractQueue, PersistentPatchCollectionDelegate, LandingSequenceErrorHandler):
    def __init__(self, options=[]):
        AbstractQueue.__init__(self, options)

    # PersistentPatchCollectionDelegate methods

    def collection_name(self):
        return self.name

    def fetch_potential_patches(self):
        return self.tool.bugs.fetch_patches_from_review_queue(limit=3)

    def status_server(self):
        return self.tool.status()

    # AbstractQueue methods

    def begin_work_queue(self):
        AbstractQueue.begin_work_queue(self)
        self.tool.status().set_host(self.options.status_host)
        self._patches = PersistentPatchCollection(self)

    def next_work_item(self):
        return self._patches.next()

    def should_proceed_with_work_item(self, patch):
        raise NotImplementedError, "subclasses must implement"

    def process_work_item(self, patch):
        raise NotImplementedError, "subclasses must implement"

    def handle_unexpected_error(self, patch, message):
        log(message)
        self._patches.done(patch)

    # LandingSequenceErrorHandler methods

    @classmethod
    def handle_script_error(cls, tool, patch, script_error):
        log(script_error.message_with_output())


class StyleQueue(AbstractTryQueue):
    name = "style-queue"
    show_in_main_help = False
    def __init__(self):
        AbstractTryQueue.__init__(self)

    def should_proceed_with_work_item(self, patch):
        return (True, "Checking style for patch %s on bug %s." % (patch["id"], patch["bug_id"]), patch)

    def process_work_item(self, patch):
        self.run_bugzilla_tool(["check-style", "--force-clean", "--non-interactive", "--parent-command=style-queue", patch["id"]])
        self._patches.done(patch)


class BuildQueue(AbstractTryQueue):
    name = "build-queue"
    show_in_main_help = False
    def __init__(self):
        options = WebKitPort.port_options()
        AbstractTryQueue.__init__(self, options)

    def begin_work_queue(self):
        AbstractTryQueue.begin_work_queue(self)
        self.port = WebKitPort.port(self.options)

    def should_proceed_with_work_item(self, patch):
        try:
            self.run_bugzilla_tool(["build", self.port.flag(), "--force-clean", "--quiet"])
        except ScriptError, e:
            return (False, "Unable to perform a build.", None)
        return (True, "Building patch %s on bug %s." % (patch["id"], patch["bug_id"]), patch)

    def process_work_item(self, patch):
        self.run_bugzilla_tool(["build-attachment", self.port.flag(), "--force-clean", "--quiet", "--non-interactive", "--parent-command=build-queue", "--no-update", patch["id"]])
        self._patches.done(patch)
