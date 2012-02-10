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
from models import Test
from models import TestResult
from models import create_in_transaction_with_numeric_id_holder


class ReportProcessHandler(webapp2.RequestHandler):
    def post(self):
        self.response.headers['Content-Type'] = 'text/plain; charset=utf-8'

        log_id = int(self.request.get('id', 0))

        log = ReportLog.get_by_id(log_id)
        if not log or not log.commit:
            self.response.out.write("Not processed")
            return

        def _float_or_none(dictionary, key):
            value = dictionary.get(key)
            if value:
                return float(value)
            return None

        branch = log.branch()
        platform = log.platform()
        build = self._create_build_if_possible(log, branch, platform)

        for test_name, result in log.results().iteritems():
            test = self._add_test_if_needed(test_name, branch, platform)
            schedule_runs_update(test.id, branch.id, platform.id)
            if isinstance(result, dict):
                TestResult(name=test_name, build=build, value=float(result['avg']), valueMedian=_float_or_none(result, 'median'),
                    valueStdev=_float_or_none(result, 'stdev'), valueMin=_float_or_none(result, 'min'), valueMax=_float_or_none(result, 'max')).put()
            else:
                TestResult(name=test_name, build=build, value=float(result)).put()

        log = ReportLog.get(log.key())
        log.delete()

        # We need to update dashboard and manifest because they are affected by the existance of test results
        schedule_dashboard_update()
        schedule_manifest_update()

        self.response.out.write('OK')

    def _create_build_if_possible(self, log, branch, platform):
        builder = log.builder()
        key_name = builder.name + ':' + str(int(time.mktime(log.timestamp().timetuple())))

        def execute():
            build = Build.get_by_key_name(key_name)
            if build:
                return build

            return Build(branch=branch, platform=platform, builder=builder, buildNumber=log.build_number(),
                timestamp=log.timestamp(), revision=log.webkit_revision(), chromiumRevision=log.chromium_revision(),
                key_name=key_name).put()
        return db.run_in_transaction(execute)

    def _add_test_if_needed(self, test_name, branch, platform):

        def execute(id):
            test = Test.get_by_key_name(test_name)
            returnValue = None
            if not test:
                test = Test(id=id, name=test_name, key_name=test_name)
                returnValue = test
            if branch.key() not in test.branches:
                test.branches.append(branch.key())
            if platform.key() not in test.platforms:
                test.platforms.append(platform.key())
            test.put()
            return returnValue
        return create_in_transaction_with_numeric_id_holder(execute) or Test.get_by_key_name(test_name)
