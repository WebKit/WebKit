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


from optparse import make_option

from webkitpy.buildbot import BuildBot
from webkitpy.committers import CommitterList
from webkitpy.webkit_logging import log
from webkitpy.multicommandtool import AbstractDeclarativeCommand


class BugsToCommit(AbstractDeclarativeCommand):
    name = "bugs-to-commit"
    help_text = "List bugs in the commit-queue"

    def execute(self, options, args, tool):
        # FIXME: This command is poorly named.  It's fetching the commit-queue list here.  The name implies it's fetching pending-commit (all r+'d patches).
        bug_ids = tool.bugs.queries.fetch_bug_ids_from_commit_queue()
        for bug_id in bug_ids:
            print "%s" % bug_id


class PatchesInCommitQueue(AbstractDeclarativeCommand):
    name = "patches-in-commit-queue"
    help_text = "List patches in the commit-queue"

    def execute(self, options, args, tool):
        patches = tool.bugs.queries.fetch_patches_from_commit_queue()
        log("Patches in commit queue:")
        for patch in patches:
            print patch.url()


class PatchesToCommitQueue(AbstractDeclarativeCommand):
    name = "patches-to-commit-queue"
    help_text = "Patches which should be added to the commit queue"
    def __init__(self):
        options = [
            make_option("--bugs", action="store_true", dest="bugs", help="Output bug links instead of patch links"),
        ]
        AbstractDeclarativeCommand.__init__(self, options=options)

    @staticmethod
    def _needs_commit_queue(patch):
        if patch.commit_queue() == "+": # If it's already cq+, ignore the patch.
            log("%s already has cq=%s" % (patch.id(), patch.commit_queue()))
            return False

        # We only need to worry about patches from contributers who are not yet committers.
        committer_record = CommitterList().committer_by_email(patch.attacher_email())
        if committer_record:
            log("%s committer = %s" % (patch.id(), committer_record))
        return not committer_record

    def execute(self, options, args, tool):
        patches = tool.bugs.queries.fetch_patches_from_pending_commit_list()
        patches_needing_cq = filter(self._needs_commit_queue, patches)
        if options.bugs:
            bugs_needing_cq = map(lambda patch: patch.bug_id(), patches_needing_cq)
            bugs_needing_cq = sorted(set(bugs_needing_cq))
            for bug_id in bugs_needing_cq:
                print "%s" % tool.bugs.bug_url_for_bug_id(bug_id)
        else:
            for patch in patches_needing_cq:
                print "%s" % tool.bugs.attachment_url_for_id(patch.id(), action="edit")


class PatchesToReview(AbstractDeclarativeCommand):
    name = "patches-to-review"
    help_text = "List patches that are pending review"

    def execute(self, options, args, tool):
        patch_ids = tool.bugs.queries.fetch_attachment_ids_from_review_queue()
        log("Patches pending review:")
        for patch_id in patch_ids:
            print patch_id


class TreeStatus(AbstractDeclarativeCommand):
    name = "tree-status"
    help_text = "Print the status of the %s buildbots" % BuildBot.default_host
    long_help = """Fetches build status from http://build.webkit.org/one_box_per_builder
and displayes the status of each builder."""

    def execute(self, options, args, tool):
        for builder in tool.buildbot.builder_statuses():
            status_string = "ok" if builder["is_green"] else "FAIL"
            print "%s : %s" % (status_string.ljust(4), builder["name"])
