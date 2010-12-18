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

import logging
import mimetypes
import urllib2

from google.appengine.api import users
from google.appengine.ext import webapp
from google.appengine.ext.webapp import template

from model.dashboardfile import DashboardFile

PARAM_FILE = "file"


def get_content_type(filename):
    return mimetypes.guess_type(filename)[0] or "application/octet-stream"


class GetDashboardFile(webapp.RequestHandler):
    def get(self, resource):
        if not resource:
            logging.debug("Getting dashboard file list.")
            return self._get_file_list()

        filename = str(urllib2.unquote(resource))

        logging.debug("Getting dashboard file: %s", filename)

        files = DashboardFile.get_files(filename)
        if not files:
            logging.error("Failed to find dashboard file: %s, request: %s",
                filename, self.request)
            self.response.set_status(404)
            return

        content_type = "%s; charset=utf-8" % get_content_type(filename)
        logging.info("content type: %s", content_type)
        self.response.headers["Content-Type"] = content_type
        self.response.out.write(files[0].data)

    def _get_file_list(self):
        logging.info("getting dashboard file list.")

        files = DashboardFile.get_files("", 100)
        if not files:
            logging.info("Failed to find dashboard files.")
            self.response.set_status(404)
            return

        template_values = {
            "admin": users.is_current_user_admin(),
            "files": files,
        }
        self.response.out.write(
            template.render("templates/dashboardfilelist.html",
                template_values))


class UpdateDashboardFile(webapp.RequestHandler):
    def get(self):
        files = self.request.get_all(PARAM_FILE)
        if not files:
            files = ["flakiness_dashboard.html",
                     "dashboard_base.js",
                     "aggregate_results.html",
                     "dygraph-combined.js",
                     "timeline_explorer.html"]

        errors = []
        for file in files:
            if not DashboardFile.update_file(file):
                errors.append("Failed to update file: %s" % file)

        if errors:
            messages = "; ".join(errors)
            logging.warning(messages)
            self.response.set_status(500, messages)
            self.response.out.write("FAIL")
        else:
            self.response.set_status(200)
            self.response.out.write("OK")


class DeleteDashboardFile(webapp.RequestHandler):
    def get(self):
        files = self.request.get_all(PARAM_FILE)
        if not files:
            logging.warning("No dashboard file to delete.")
            self.response.set_status(400)
            return

        for file in files:
            DashboardFile.delete_file(file)

        # Display dashboard file list after deleting the file.
        self.redirect("/dashboards/")
