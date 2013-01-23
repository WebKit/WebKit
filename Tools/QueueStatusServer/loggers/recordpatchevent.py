# Copyright (C) 2013 Google Inc. All rights reserved.
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

from config.logging import queue_log_duration
from model.patchlog import PatchLog
from model.queuelog import QueueLog
from model.workitems import WorkItems
from model.activeworkitems import ActiveWorkItems


class RecordPatchEvent(object):
    @classmethod
    def added(cls, attachment_id, queue_name):
        PatchLog.lookup(attachment_id, queue_name)
        queue_log = QueueLog.get_current(queue_name, queue_log_duration)
        patches_waiting = cls._get_patches_waiting(queue_name)
        if patches_waiting and queue_log.max_patches_waiting < patches_waiting:
            queue_log.max_patches_waiting = patches_waiting
            queue_log.put()

    @classmethod
    def retrying(cls, attachment_id, queue_name, bot_id=None):
        patch_log = PatchLog.lookup(attachment_id, queue_name)
        if bot_id:
            patch_log.bot_id = bot_id
        patch_log.retry_count += 1
        patch_log.put()

        queue_log = QueueLog.get_current(queue_name, queue_log_duration)
        queue_log.patch_retry_count += 1
        queue_log.put()

    @classmethod
    def started(cls, attachment_id, queue_name, bot_id=None):
        patch_log = PatchLog.lookup(attachment_id, queue_name)
        if bot_id:
            patch_log.bot_id = bot_id
        patch_log.calculate_wait_duration()
        patch_log.put()

        queue_log = QueueLog.get_current(queue_name, queue_log_duration)
        queue_log.patch_wait_durations.append(patch_log.wait_duration)
        queue_log.put()

    @classmethod
    def stopped(cls, attachment_id, queue_name, bot_id=None):
        patch_log = PatchLog.lookup(attachment_id, queue_name)
        if bot_id:
            patch_log.bot_id = bot_id
        patch_log.finished = True
        patch_log.calculate_process_duration()
        patch_log.put()

        if patch_log.process_duration:
            queue_log = QueueLog.get_current(queue_name, queue_log_duration)
            queue_log.patch_process_durations.append(patch_log.process_duration)
            queue_log.put()

    @classmethod
    def updated(cls, attachment_id, queue_name, bot_id=None):
        patch_log = PatchLog.lookup(attachment_id, queue_name)
        if bot_id:
            patch_log.bot_id = bot_id
        patch_log.status_update_count += 1
        patch_log.put()

        queue_log = QueueLog.get_current(queue_name, queue_log_duration)
        queue_log.status_update_count += 1
        queue_log.put()

    @classmethod
    def _get_patches_waiting(cls, queue_name):
        work_items = WorkItems.lookup_by_queue(queue_name)
        active_work_items = ActiveWorkItems.lookup_by_queue(queue_name)
        if work_items and active_work_items:
            return len(set(work_items.item_ids) - set(active_work_items.item_ids))
