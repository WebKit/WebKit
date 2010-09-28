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

from google.appengine.api import users
from google.appengine.ext import webapp, db
from google.appengine.ext.webapp import template

from handlers.updatebase import UpdateBase
from model.activeworkitems import ActiveWorkItems
from model.attachment import Attachment
from model.queuestatus import QueueStatus

class UpdateStatus(UpdateBase):
    def get(self):
        self.response.out.write(template.render("templates/updatestatus.html", None))

    def _queue_status_from_request(self):
        queue_status = QueueStatus()

        # FIXME: I think this can be removed, no one uses it.
        if users.get_current_user():
            queue_status.author = users.get_current_user()

        bug_id = self._int_from_request("bug_id")
        patch_id = self._int_from_request("patch_id")
        queue_name = self.request.get("queue_name")
        queue_status.queue_name = queue_name
        queue_status.active_bug_id = bug_id
        queue_status.active_patch_id = patch_id
        queue_status.message = self.request.get("status")
        results_file = self.request.get("results_file")
        queue_status.results_file = db.Blob(str(results_file))
        return queue_status

    @staticmethod
    def _expire_item(key, item_id):
        active_work_items = db.get(key)
        active_work_items.expire_item(item_id)
        active_work_items.put()

    # FIXME: An explicit lock_release request would be cleaner than this magical "Retry" status.
    def _update_active_work_items(self, queue_status):
        if queue_status.message != "Retry":  # From AbstractQueue._retry_status
            return
        active_items = ActiveWorkItems.all().filter("queue_name =", queue_status.queue_name).get()
        if not active_items:
            return
        return db.run_in_transaction(self._expire_item, active_items.key(), queue_status.active_patch_id)

    def post(self):
        queue_status = self._queue_status_from_request()
        queue_status.put()
        self._update_active_work_items(queue_status)
        Attachment.dirty(queue_status.active_patch_id)
        self.response.out.write(queue_status.key().id())
