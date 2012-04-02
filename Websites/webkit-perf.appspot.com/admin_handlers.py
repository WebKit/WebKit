#!/usr/bin/env python
# Copyright (C) 2012 Google Inc. All rights reserved.
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

import webapp2
import json

from google.appengine.api import taskqueue
from google.appengine.api import users
from google.appengine.ext.db import GqlQuery
from google.appengine.ext.webapp import template

from controller import schedule_runs_update
from controller import schedule_dashboard_update
from controller import schedule_manifest_update
from models import Branch
from models import Builder
from models import Platform
from models import ReportLog
from models import Test


class IsAdminHandler(webapp2.RequestHandler):
    def get(self):
        self.response.headers['Content-Type'] = 'application/json'
        self.response.out.write('true' if users.is_current_user_admin() else 'false')


class AdminDashboardHandler(webapp2.RequestHandler):
    def get(self, task):
        task_method_name = 'get_' + task
        if task_method_name in dir(self) and task_method_name not in dir(super):
            method = getattr(self, task_method_name)
            return method()

        report_log_total = GqlQuery("SELECT __key__ FROM ReportLog").count()
        report_log_committed = GqlQuery("SELECT __key__ FROM ReportLog WHERE commit = True").count()

        self.response.out.write(template.render('admin.html',
            {'report_log_total': report_log_total, 'report_log_committed': report_log_committed}))

    def get_branches(self):
        self.response.headers['Content-Type'] = 'application/json'
        result = {}
        for branch in Branch.all():
            result[branch.key().name()] = {'name': branch.name}
        self.response.out.write(json.dumps(result))

    def get_platforms(self):
        self.response.headers['Content-Type'] = 'application/json'
        result = {}
        for platform in Platform.all():
            result[platform.key().name()] = {'name': platform.name, 'hidden': platform.hidden}
        self.response.out.write(json.dumps(result))

    def get_builders(self):
        self.response.headers['Content-Type'] = 'application/json'
        self.response.out.write(json.dumps([builder.name for builder in Builder.all()]))

    def get_tests(self):
        self.response.headers['Content-Type'] = 'application/json'
        result = {}
        for test in Test.all():
            result[test.key().name()] = {'name': test.name, 'hidden': test.hidden}
        self.response.out.write(json.dumps(result))


class ChangeVisibilityHandler(webapp2.RequestHandler):
    def post(self):
        self.response.headers['Content-Type'] = 'text/plain; charset=utf-8'

        try:
            payload = json.loads(self.request.body)
            hide = payload['hide']
        except:
            self.response.out.write("Failed to parse the payload: %s" % self.request.body)
            return

        if 'platform' in payload:
            model = Platform.get_by_key_name(payload['platform'])
        elif 'test' in payload:
            model = Test.get_by_key_name(payload['test'])
        else:
            self.response.out.write('Not supported')
            return

        if not model:
            self.response.out.write('Could not find the model')
            return

        model.hidden = hide
        model.put()
        schedule_dashboard_update()
        schedule_manifest_update()

        self.response.out.write('OK')


class MergeTestsHandler(webapp2.RequestHandler):
    def post(self, task):
        self.response.headers['Content-Type'] = 'text/plain; charset=utf-8'

        if task != 'run':
            try:
                payload = json.loads(self.request.body)
                merge = payload.get('merge', '')
                into = payload.get('into', '')
            except:
                self.response.out.write("Failed to parse the payload: %s" % self.request.body)
                return

            if merge == into or not Test.get_by_key_name(merge) or not Test.get_by_key_name(into):
                self.response.out.write('Invalid test names')
                return

            taskqueue.add(url='/admin/merge-tests/run', params={'merge': merge, 'into': into}, target='model-manipulator')
            self.response.out.write('OK')
            return

        merge = Test.get_by_key_name(self.request.get('merge'))
        into = Test.get_by_key_name(self.request.get('into'))

        branches_and_platforms_to_update = into.merge(merge)
        if branches_and_platforms_to_update == None:
            # FIXME: This message is invisible. Need to store this somewhere and let the admin page pull it.
            self.response.out.write('Cannot merge %s into %s. There are conflicting results.' % (merge.name, into.name))
            return

        for branch_id, platform_id in branches_and_platforms_to_update:
            schedule_runs_update(into.id, branch_id, platform_id)

        schedule_dashboard_update()
        schedule_manifest_update()

        self.response.out.write('OK')
