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

import itertools

from google.appengine.ext import webapp
from google.appengine.ext.webapp import template

from model.queues import Queue
from model import queuestatus


class QueueStatus(webapp.RequestHandler):
    def _rows_for_work_items(self, queue):
        queued_items = queue.work_items()
        active_items = queue.active_work_items()
        if not queued_items:
            return []
        rows = []
        for item_id in queued_items.item_ids:
            rows.append({
                "attachment_id": item_id,
                "bug_id": 1,
                "lock_time": active_items and active_items.time_for_item(item_id),
            })
        return rows

    def _grouping_key_for_status(self, status):
        return "%s-%s" % (status.active_patch_id, status.bot_id)

    def _build_status_groups(self, statuses):
        return [list(group) for key, group in itertools.groupby(statuses, self._grouping_key_for_status)]

    def get(self, queue_name):
        queue_name = queue_name.lower()
        queue = Queue.queue_with_name(queue_name)
        if not queue:
            self.error(404)
            return

        statuses = queuestatus.QueueStatus.all().filter("queue_name =", queue.name()).order("-date").fetch(15)
        template_values = {
            "display_queue_name": queue.display_name(),
            "work_item_rows": self._rows_for_work_items(queue),
            "status_groups": self._build_status_groups(statuses),
        }
        self.response.out.write(template.render("templates/queuestatus.html", template_values))
