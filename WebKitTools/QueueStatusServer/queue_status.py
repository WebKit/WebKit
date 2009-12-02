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

import cgi
import os

# Request a modern Django
from google.appengine.dist import use_library
use_library('django', '1.1')

from google.appengine.ext.webapp import template
from google.appengine.api import users
from google.appengine.ext import webapp
from google.appengine.ext.webapp.util import run_wsgi_app
from google.appengine.ext import db

webapp.template.register_template_library('filters.webkit_extras')


class QueueStatus(db.Model):
    author = db.UserProperty()
    queue_name = db.StringProperty()
    active_bug_id = db.IntegerProperty()
    active_patch_id = db.IntegerProperty()
    message = db.StringProperty(multiline=True)
    date = db.DateTimeProperty(auto_now_add=True)


class MainPage(webapp.RequestHandler):
    def get(self):
        statuses_query = QueueStatus.all().filter('queue_name =', 'commit-queue').order('-date')
        statuses = statuses_query.fetch(6)
        if not statuses:
            return self.response.out.write("No status to report.")
        template_values = {
            'last_status' : statuses[0],
            'recent_statuses' : statuses[1:],
        }
        self.response.out.write(template.render('index.html', template_values))


class PatchStatus(webapp.RequestHandler):
    def get(self, queue_name, attachment_id):
        statuses = QueueStatus.all().filter('queue_name =', queue_name).filter('active_patch_id =', int(attachment_id)).order('-date').fetch(1)
        if not statuses:
            self.error(404)
            return
        self.response.out.write(statuses[0].message)


class StatusSummary(object):
    def _status_to_code(self, status):
        code_names = {
            "Pass": "pass",
            "Pending": "pending",
            "Fail": "fail",
            "Error": "error",
        }
        return code_names.get(status, "none")

    def _queue_name_to_code(self, queue_name):
        code_names = {
            "style-queue": "style",
        }
        return code_names[queue_name]

    queues = [
        "style-queue",
    ]

    def __init__(self):
        self._summary = {}

    def summarize(self, attachment_id):
        if self._summary.get(attachment_id):
            return self._summary.get(attachment_id)

        attachment_summary = {}
        for queue in self.queues:
            statuses = QueueStatus.all().filter('queue_name =', queue).filter('active_patch_id =', attachment_id).order('-date').fetch(1)
            status_code = self._status_to_code(statuses[0].message if statuses else None)
            queue_code = self._queue_name_to_code(queue)
            attachment_summary[queue_code] = status_code

        self._summary[attachment_id] = attachment_summary
        return attachment_summary


class StatusBubble(webapp.RequestHandler):
    def get(self, attachment_id):
        status_summary = StatusSummary()
        template_values = {
            "queue_status" : status_summary.summarize(int(attachment_id)),
        }
        self.response.out.write(template.render('status_bubble.html', template_values))


class UpdateStatus(webapp.RequestHandler):
    def get(self):
        self.response.out.write(template.render('update_status.html', None))

    def _int_from_request(self, name):
        string_value = self.request.get(name)
        try:
            int_value = int(string_value)
            return int_value
        except ValueError, TypeError:
            pass
        return None

    def post(self):
        queue_status = QueueStatus()

        if users.get_current_user():
            queue_status.author = users.get_current_user()

        queue_name = self.request.get('queue_name')
        queue_status.queue_name = queue_name
        queue_status.active_bug_id = self._int_from_request('bug_id')
        queue_status.active_patch_id = self._int_from_request('patch_id')
        queue_status.message = self.request.get('status')
        queue_status.put()
        self.redirect('/')


routes = [
    ('/', MainPage),
    ('/update-status', UpdateStatus),
    (r'/patch-status/(.*)/(.*)', PatchStatus),
    (r'/status-bubble/(.*)', StatusBubble),
]

application = webapp.WSGIApplication(routes, debug=True)

def main():
    run_wsgi_app(application)

if __name__ == "__main__":
    main()
