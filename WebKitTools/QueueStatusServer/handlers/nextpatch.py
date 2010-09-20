# Copyright (C) 2010 Google Inc. All rights reserved.
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

from google.appengine.ext import webapp

from model.workitems import WorkItems
from model import queuestatus

from datetime import datetime, timedelta


class NextPatch(webapp.RequestHandler):
    def _get_next_patch_id(self, queue_name):
        work_items = WorkItems.all().filter("queue_name =", queue_name).get()
        if not work_items:
            return None
        one_hour_ago = datetime.now() - timedelta(minutes=60)
        statuses = queuestatus.QueueStatus.all().filter("queue_name =", queue_name).filter("date >", one_hour_ago).fetch(15)
        active_patch_ids = set([status.active_patch_id for status in statuses])
        for item_id in work_items.item_ids:
            if item_id not in active_patch_ids:
                return item_id
        # Either there were no work items, or they're all active.
        return None

    def _assign(self, queue_name, patch_id):
        queue_status = queuestatus.QueueStatus()
        queue_status.queue_name = queue_name
        queue_status.active_patch_id = patch_id
        queue_status.message = "Assigned for processing"
        queue_status.put()

    def get(self, queue_name):
        patch_id = self._get_next_patch_id(queue_name)
        if not patch_id:
            self.error(404)
            return
        self._assign(queue_name, patch_id)
        self.response.out.write(patch_id)
