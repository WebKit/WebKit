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

from google.appengine.ext import webapp, db
from google.appengine.ext.webapp import template

from handlers.updatebase import UpdateBase
from model.queues import queues
from model.workitems import WorkItems

from datetime import datetime


class UpdateWorkItems(UpdateBase):
    def get(self):
        self.response.out.write(template.render("templates/updateworkitems.html", None))

    def _work_items_for_queue(self, queue_name):
        if queue_name not in queues:
            self.response.set_status(500)
            return
        work_items = WorkItems.all().filter("queue_name =", queue_name).get()
        if not work_items:
            work_items = WorkItems()
            work_items.queue_name = queue_name
        return work_items

    def _work_items_from_request(self):
        queue_name = self.request.get("queue_name")
        work_items = self._work_items_for_queue(queue_name)
        items_string = self.request.get("work_items")
        # Our parsing could be much more robust.
        work_items.item_ids = map(int, items_string.split(" "))
        work_items.date = datetime.now()
        return work_items

    def post(self):
        work_items = self._work_items_from_request()
        work_items.put()
