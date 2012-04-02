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

from admin_handlers import AdminDashboardHandler
from admin_handlers import ChangeVisibilityHandler
from admin_handlers import IsAdminHandler
from admin_handlers import MergeTestsHandler
from controller import CachedDashboardHandler
from controller import CachedManifestHandler
from controller import CachedRunsHandler
from controller import DashboardImageHandler
from controller import DashboardUpdateHandler
from controller import ManifestUpdateHandler
from controller import RunsChartHandler
from controller import RunsUpdateHandler
from create_handler import CreateHandler
from report_handler import ReportHandler
from report_handler import AdminReportHandler
from report_process_handler import ReportProcessHandler
from report_logs_handler import ReportLogsHandler

routes = [
    ('/admin/report/?', AdminReportHandler),
    (r'/admin/merge-tests(?:/(.*))?', MergeTestsHandler),
    ('/admin/report-logs/?', ReportLogsHandler),
    ('/admin/create/(.*)', CreateHandler),
    ('/admin/change-visibility/?', ChangeVisibilityHandler),
    (r'/admin/([A-Za-z\-]*)', AdminDashboardHandler),

    ('/api/user/is-admin', IsAdminHandler),
    ('/api/test/?', CachedManifestHandler),
    ('/api/test/update', ManifestUpdateHandler),
    ('/api/test/report/?', ReportHandler),
    ('/api/test/report/process', ReportProcessHandler),
    ('/api/test/runs/?', CachedRunsHandler),
    ('/api/test/runs/update', RunsUpdateHandler),
    ('/api/test/runs/chart', RunsChartHandler),
    ('/api/test/dashboard/?', CachedDashboardHandler),
    ('/api/test/dashboard/update', DashboardUpdateHandler),

    ('/images/dashboard/flot-(\d+)-(\d+)-(\d+)_(\d+).png', DashboardImageHandler)]


def main():
    application = webapp2.WSGIApplication(routes, debug=True)
    util.run_wsgi_app(application)


if __name__ == '__main__':
    main()
