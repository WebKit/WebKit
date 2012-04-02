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
from google.appengine.ext import db

import time

from controller import schedule_runs_update
from controller import schedule_dashboard_update
from controller import schedule_manifest_update
from models import Build
from models import ReportLog
from models import Runs
from models import Test
from models import TestResult


class ReportProcessHandler(webapp2.RequestHandler):
    def post(self):
        self.response.headers['Content-Type'] = 'text/plain; charset=utf-8'

        log_id = int(self.request.get('id', 0))

        log = ReportLog.get_by_id(log_id)
        if not log or not log.commit:
            self.response.out.write("Not processed")
            return

        branch = log.branch()
        platform = log.platform()
        build = Build.get_or_insert_from_log(log)

        for test_name, result_value in log.results().iteritems():
            unit = result_value.get('unit') if isinstance(result_value, dict) else None
            test = Test.update_or_insert(test_name, branch, platform, unit)
            result = TestResult.get_or_insert_from_parsed_json(test_name, build, result_value)
            runs = Runs.get_by_objects(branch, platform, test)
            regenerate_runs = True
            if runs:
                runs.update_incrementally(build, result)
                regenerate_runs = False
            schedule_runs_update(test.id, branch.id, platform.id, regenerate_runs)

        log = ReportLog.get(log.key())
        log.delete()

        # We need to update dashboard and manifest because they are affected by the existance of test results
        schedule_dashboard_update()
        schedule_manifest_update()

        self.response.out.write('OK')
