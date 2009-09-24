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
    active_bug_id = db.IntegerProperty()
    active_patch_id = db.IntegerProperty()
    message = db.StringProperty(multiline=True)
    date = db.DateTimeProperty(auto_now_add=True)

class MainPage(webapp.RequestHandler):
    def get(self):
        statuses_query = QueueStatus.all().order('-date')
        statuses = statuses_query.fetch(6)
        template_values = {
            'last_status' : statuses[0],
            'recent_statuses' : statuses[1:],
        }
        self.response.out.write(template.render('index.html', template_values))

class UpdateStatus(webapp.RequestHandler):
    def get(self):
        self.response.out.write(template.render('update_status.html', None))

    def _int_from_request(self, name):
        string_value = self.request.get(name)
        try:
            int_value = int(string_value)
        except ValueError, TypeError:
            pass
        return None

    def post(self):
        queue_status = QueueStatus()

        if users.get_current_user():
            queue_status.author = users.get_current_user()

        queue_status.active_bug_id = self._int_from_request('bug_id')
        queue_status.active_patch_id = self._int_from_request('patch_id')
        queue_status.message = self.request.get('status')
        queue_status.put()
        self.redirect('/')

routes = [
    ('/', MainPage),
    ('/update_status', UpdateStatus)
]

application = webapp.WSGIApplication(routes, debug=True)

def main():
    run_wsgi_app(application)

if __name__ == "__main__":
    main()
