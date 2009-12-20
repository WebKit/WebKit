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

from google.appengine.ext import webapp
from google.appengine.ext.webapp import template

from model.queuestatus import QueueStatus
from handlers.statusbubble import StatusSummary

queues = [
    "style-queue",
    "chromium-ews",
    "qt-ews",
    "gtk-ews",
]

def dash_to_underscore(dashed_name):
    regexp = re.compile("-")
    return regexp.sub("_", dashed_name)

def state_from_status(status):
    table = {
        "Pass" : "pass",
        "Fail" : "fail",
    }
    return table.get(status.message, "none")

def summarize(attachment_id):
    summary = { "attachment_id" : attachment_id }

    # FIXME: We shouldn't have to make another query to figure this out.
    #        We'll fix this with memcache.  Notice that we can't grab it
    #        below because the patch might not have been processed by one
    #        these queues yet.
    summary["bug_id"] = QueueStatus.all().filter('active_patch_id =', attachment_id).fetch(1)[0].active_bug_id

    for queue in queues:
        summary[queue] = None
        status = QueueStatus.all().filter('queue_name =', queue).filter('active_patch_id =', attachment_id).order('-date').get()
        if status:
            summary[dash_to_underscore(queue)] = {
                "state" : state_from_status(status),
                "status" : status,
            }
    return summary


class Dashboard(webapp.RequestHandler):
    def get(self):
        status_summary = StatusSummary()
        statuses = QueueStatus.all().order("-date")

        attachment_ids = set()
        for status in statuses:
            if not status.active_patch_id:
                continue
            attachment_ids.add(status.active_patch_id)
            if len(attachment_ids) >= 25:
                break

        template_values = {
            "summaries" : map(summarize, sorted(attachment_ids)),
        }
        self.response.out.write(template.render("templates/dashboard.html", template_values))
