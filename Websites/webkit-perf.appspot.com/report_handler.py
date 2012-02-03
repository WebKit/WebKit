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
from google.appengine.api import memcache
from google.appengine.ext import db

import json
import re
import time
from datetime import datetime

from models import Builder
from models import Branch
from models import Build
from models import NumericIdHolder
from models import Platform
from models import ReportLog
from models import Test
from models import TestResult
from models import create_in_transaction_with_numeric_id_holder


class ReportHandler(webapp2.RequestHandler):
    def post(self):
        self.response.headers['Content-Type'] = 'text/plain; charset=utf-8'

        headers = "\n".join([key + ': ' + value for key, value in self.request.headers.items()])

        # Do as best as we can to remove the password
        request_body_without_password = re.sub(r'"password"\s*:\s*".+?",', '', self.request.body)
        log = ReportLog(timestamp=datetime.now(), headers=headers, payload=request_body_without_password)
        log.put()

        try:
            self._body = json.loads(self.request.body)
        except ValueError:
            return self._output('Failed to parse the payload as a json. Report key: %d' % log.key().id())

        builder = self._model_by_key_name_in_body_or_error(Builder, 'builder-name')
        branch = self._model_by_key_name_in_body_or_error(Branch, 'branch')
        platform = self._model_by_key_name_in_body_or_error(Platform, 'platform')
        build_number = self._integer_in_body('build-number')
        timestamp = self._timestamp_in_body()
        revision = self._integer_in_body('webkit-revision')
        chromium_revision = self._integer_in_body('webkit-revision') if 'chromium-revision' in self._body else None

        failed = False
        if builder and not (self.bypass_authentication() or builder.authenticate(self._body.get('password', ''))):
            self._output('Authentication failed')
            failed = True

        if not self._results_are_valid():
            self._output("The payload doesn't contain results or results are malformed")
            failed = True

        if not (builder and branch and platform and build_number and revision and timestamp) or failed:
            return

        build = self._create_build_if_possible(builder, build_number, branch, platform, timestamp, revision, chromium_revision)
        if not build:
            return

        def _float_or_none(dictionary, key):
            value = dictionary.get(key)
            if value:
                return float(value)
            return None

        for test_name, result in self._body['results'].iteritems():
            test = self._add_test_if_needed(test_name, branch, platform)
            memcache.delete(Test.cache_key(test.id, branch.id, platform.id))
            if isinstance(result, dict):
                TestResult(name=test_name, build=build, value=float(result['avg']), valueMedian=_float_or_none(result, 'median'),
                    valueStdev=_float_or_none(result, 'stdev'), valueMin=_float_or_none(result, 'min'), valueMax=_float_or_none(result, 'max')).put()
            else:
                TestResult(name=test_name, build=build, value=float(result)).put()

        log = ReportLog.get(log.key())
        log.delete()

        # We need to update dashboard and manifest because they are affected by the existance of test results
        memcache.delete('dashboard')
        memcache.delete('manifest')

        return self._output('OK')

    def _model_by_key_name_in_body_or_error(self, model, keyName):
        key = self._body.get(keyName, '')
        instance = key and model.get_by_key_name(key)
        if not instance:
            self._output('There are no %s named "%s"' % (model.__name__.lower(), key))
        return instance

    def _integer_in_body(self, key):
        value = self._body.get(key, '')
        try:
            return int(value)
        except:
            return self._output('Invalid %s: "%s"' % (key.replace('-', ' '), value))

    def _timestamp_in_body(self):
        value = self._body.get('timestamp', '')
        try:
            return datetime.fromtimestamp(int(value))
        except:
            return self._output('Failed to parse the timestamp: %s' % value)

    def _output(self, message):
        self.response.out.write(message + '\n')

    def bypass_authentication(self):
        return False

    def _results_are_valid(self):

        def _is_float_convertible(value):
            try:
                float(value)
                return True
            except TypeError:
                return False

        if 'results' not in self._body or not isinstance(self._body['results'], dict):
            return False

        for testResult in self._body['results'].values():
            if isinstance(testResult, dict):
                for value in testResult.values():
                    if not _is_float_convertible(value):
                        return False
                if 'avg' not in testResult:
                    return False
                continue
            if not _is_float_convertible(testResult):
                return False

        return True

    def _create_build_if_possible(self, builder, build_number, branch, platform, timestamp, revision, chromium_revision):
        key_name = builder.name + ':' + str(int(time.mktime(timestamp.timetuple())))

        def execute():
            build = Build.get_by_key_name(key_name)
            if build:
                return self._output('The build at %s already exists for %s' % (str(timestamp), builder.name))

            return Build(branch=branch, platform=platform, builder=builder, buildNumber=build_number,
                timestamp=timestamp, revision=revision, chromiumRevision=chromium_revision, key_name=key_name).put()
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


class AdminReportHandler(ReportHandler):
    def bypass_authentication(self):
        return True
