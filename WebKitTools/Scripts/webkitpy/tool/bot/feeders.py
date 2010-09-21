# Copyright (c) 2010 Google Inc. All rights reserved.
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

from webkitpy.common.system.deprecated_logging import log
from webkitpy.common.net.bugzilla import CommitterValidator


class AbstractFeeder(object):
    def __init__(self, tool):
        self._tool = tool

    def feed(tool):
        raise NotImplementedError, "subclasses must implement"

    def update_work_items(self, item_ids):
        self._tool.status_server.update_work_items(self.queue_name, item_ids)
        log("Feeding %s items %s" % (self.queue_name, item_ids))


class CommitQueueFeeder(AbstractFeeder):
    queue_name = "commit-queue"

    def __init__(self, tool):
        AbstractFeeder.__init__(self, tool)
        self.committer_validator = CommitterValidator(self._tool.bugs)

    def feed(self):
        patches = self._validate_patches()
        patches = sorted(patches, self._patch_cmp)
        patch_ids = [patch.id() for patch in patches]
        self.update_work_items(patch_ids)

    def _patches_for_bug(self, bug_id):
        return self._tool.bugs.fetch_bug(bug_id).commit_queued_patches(include_invalid=True)

    def _validate_patches(self):
        # Not using BugzillaQueries.fetch_patches_from_commit_queue() so we can reject patches with invalid committers/reviewers.
        bug_ids = self._tool.bugs.queries.fetch_bug_ids_from_commit_queue()
        all_patches = sum([self._patches_for_bug(bug_id) for bug_id in bug_ids], [])
        return self.committer_validator.patches_after_rejecting_invalid_commiters_and_reviewers(all_patches)

    def _patch_cmp(self, a, b):
        # Sort first by is_rollout, then by attach_date.
        # Reversing the order so that is_rollout is first.
        rollout_cmp = cmp(b.is_rollout(), a.is_rollout())
        if rollout_cmp != 0:
            return rollout_cmp
        return cmp(a.attach_date(), b.attach_date())
