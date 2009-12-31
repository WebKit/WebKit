# Copyright (C) 2009 Google Inc. All rights reserved.
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

from google.appengine.api import memcache

from model.queues import queues
from model.queuestatus import QueueStatus


class Attachment(object):
    @classmethod
    def dirty(cls, attachment_id):
        memcache.delete(str(attachment_id), namespace="attachment-summary")

    @classmethod
    def recent(cls, limit=1):
        statuses = QueueStatus.all().order("-date")
        # Notice that we use both a set and a list here to keep the -date ordering.
        ids = []
        visited_ids = set()
        for status in statuses:
            attachment_id = status.active_patch_id
            if not attachment_id:
                continue
            if attachment_id in visited_ids:
                continue
            visited_ids.add(attachment_id)
            ids.append(attachment_id)
            if len(visited_ids) >= limit:
                break
        return map(cls, ids)

    def __init__(self, attachment_id):
        self.id = attachment_id
        self._summary = None

    def summary(self):
        if self._summary:
            return self._summary
        self._summary = memcache.get(str(self.id), namespace="attachment-summary")
        if self._summary:
            return self._summary
        self._summary = self._fetch_summary()
        memcache.set(str(self.id), self._summary, namespace="attachment-summary")
        return self._summary

    def _dash_to_underscore(self, dashed_name):
        regexp = re.compile("-")
        return regexp.sub("_", dashed_name)

    def _state_from_status(self, status):
        table = {
            "Pass" : "pass",
            "Fail" : "fail",
        }
        state = table.get(status.message)
        if state:
            return state
        if status.message.startswith("Error:"):
            return "error"
        if status:
            return "pending"
        return None

    def _fetch_summary(self):
        summary = { "attachment_id" : self.id }

        first_status = QueueStatus.all().filter('active_patch_id =', self.id).get()
        if not first_status:
            # We don't have any record of this attachment.
            return summary
        summary["bug_id"] = first_status.active_bug_id

        for queue in queues:
            summary[queue] = None
            status = QueueStatus.all().filter('queue_name =', queue).filter('active_patch_id =', self.id).order('-date').get()
            if status:
                summary[self._dash_to_underscore(queue)] = {
                    "state" : self._state_from_status(status),
                    "status" : status,
                }
        return summary
