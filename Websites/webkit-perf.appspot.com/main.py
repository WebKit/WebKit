#!/usr/bin/env python
#
# Copyright 2007, 2011 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import webapp2
from google.appengine.ext.webapp import util

import json

from create_handler import CreateHandler
from dashboard_handler import DashboardHandler
from manifest_handler import ManifestHandler
from report_handler import ReportHandler
from report_handler import AdminReportHandler
from runs_handler import RunsHandler
from merge_tests_handler import MergeTestsHandler

routes = [
    ('/admin/report/?', AdminReportHandler),
    ('/admin/merge-tests/?', MergeTestsHandler),
    ('/admin/create/(.*)', CreateHandler),
    ('/api/test/?', ManifestHandler),
    ('/api/test/report/?', ReportHandler),
    ('/api/test/runs/?', RunsHandler),
    ('/api/test/dashboard/?', DashboardHandler),
]


def main():
    application = webapp2.WSGIApplication(routes, debug=True)
    util.run_wsgi_app(application)


if __name__ == '__main__':
    main()
