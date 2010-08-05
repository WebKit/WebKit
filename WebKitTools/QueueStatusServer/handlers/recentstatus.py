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

import datetime

from google.appengine.ext import webapp
from google.appengine.ext.webapp import template

from model.queues import queues, display_name_for_queue
from model.queuestatus import QueueStatus
from model.workitems import WorkItems


class QueueBubble(object):
    """View support class for recentstatus.html"""
    def __init__(self, queue_name):
        self._queue_name = queue_name
        self._work_items = WorkItems.all().filter("queue_name =", queue_name).get()
        self._last_status = QueueStatus.all().filter("queue_name =", queue_name).order("-date").get()

    def name(self):
        return self._queue_name

    def display_name(self):
        return display_name_for_queue(self._queue_name)

    def _last_status_date(self):
        if not self._last_status:
            return None
        return self._last_status.date

    def last_heard_from(self):
        if not self._work_items:
            return self._last_status_date()
        return max(self._last_status_date(), self._work_items.date)

    def is_alive(self):
        if not self.last_heard_from():
            return False
        return self.last_heard_from() > (datetime.datetime.now() - datetime.timedelta(minutes=30))

    def status_class(self):
        if not self.is_alive():
            return "dead"
        if self.pending_items_count() > 1:
            return "behind"
        return "alive"

    def status_text(self):
        if not self._work_items:
            return "Offline"
        if not self._work_items.item_ids:
            return "Idle"
        return self._last_status.message

    def pending_items_count(self):
        if not self._work_items:
            return 0
        return len(self._work_items.item_ids)


class QueuesOverview(webapp.RequestHandler):

    def get(self):
        template_values = {
            "queues": [QueueBubble(queue_name) for queue_name in queues],
        }
        self.response.out.write(template.render("templates/recentstatus.html", template_values))
