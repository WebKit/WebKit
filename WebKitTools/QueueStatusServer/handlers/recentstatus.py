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

from google.appengine.ext import webapp
from google.appengine.ext.webapp import template

from model import QueueStatus

class RecentStatus(webapp.RequestHandler):
    def _title_case(self, string):
        words = string.split(" ")
        words = map(lambda word: word.capitalize(), words)
        return " ".join(words)

    def _pretty_queue_name(self, queue_name):
        return self._title_case(queue_name.replace("-", " "))

    # We could change "/" to just redirect to /queue-status/commit-queue in the future
    # at which point we would not need a default value for queue_name here.
    def get(self, queue_name="commit-queue"):
        statuses_query = QueueStatus.all().filter("queue_name =", queue_name).order("-date")
        statuses = statuses_query.fetch(6)
        if not statuses:
            return self.response.out.write("No status to report.")
        template_values = {
            "last_status" : statuses[0],
            "recent_statuses" : statuses[1:],
            "pretty_queue_name" : self._pretty_queue_name(queue_name),
            "show_commit_queue_footer" : queue_name == "commit-queue"
        }
        self.response.out.write(template.render("index.html", template_values))
