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
from models import createInTransactionWithNumericIdHolder


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

        builder = self._modelByKeyNameInBodyOrError(Builder, 'builder-name')
        branch = self._modelByKeyNameInBodyOrError(Branch, 'branch')
        platform = self._modelByKeyNameInBodyOrError(Platform, 'platform')
        buildNumber = self._integerInBody('build-number')
        revision = self._integerInBody('revision')
        timestamp = self._timestampInBody()

        failed = False
        if builder and not (self.bypassAuthentication() or builder.authenticate(self._body.get('password', ''))):
            self._output('Authentication failed')
            failed = True

        if not self._resultsAreValid():
            self._output("The payload doesn't contain results or results are malformed")
            failed = True

        if not (builder and branch and platform and buildNumber and revision and timestamp) or failed:
            return

        build = self._createBuildIfPossible(builder, buildNumber, branch, platform, revision, timestamp)
        if not build:
            return

        for test, result in self._body['results'].iteritems():
            self._addTestIfNeeded(test, branch, platform)
            if isinstance(result, dict):
                TestResult(name=test, build=build, value=float(result.get('avg', 0)), valueMedian=float(result.get('median', 0)),
                    valueStdev=float(result.get('stdev', 0)), valueMin=float(result.get('min', 0)), valueMax=float(result.get('max', 0))).put()
            else:
                TestResult(name=test, build=build, value=float(result)).put()

        log = ReportLog.get(log.key())
        log.delete()

        return self._output('OK')

    def _modelByKeyNameInBodyOrError(self, model, keyName):
        key = self._body.get(keyName, '')
        instance = key and model.get_by_key_name(key)
        if not instance:
            self._output('There are no %s named "%s"' % (model.__name__.lower(), key))
        return instance

    def _integerInBody(self, key):
        value = self._body.get(key, '')
        try:
            return int(value)
        except:
            return self._output('Invalid %s: "%s"' % (key.replace('-', ' '), value))

    def _timestampInBody(self):
        value = self._body.get('timestamp', '')
        try:
            return datetime.fromtimestamp(int(value))
        except:
            return self._output('Failed to parse the timestamp: %s' % value)

    def _output(self, message):
        self.response.out.write(message + '\n')

    def bypassAuthentication(self):
        return False

    def _resultsAreValid(self):

        def _isFloatConvertible(value):
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
                    if not _isFloatConvertible(value):
                        return False
                if 'avg' not in testResult:
                    return False
                continue
            if not _isFloatConvertible(testResult):
                return False

        return True

    def _createBuildIfPossible(self, builder, buildNumber, branch, platform, revision, timestamp):
        key_name = builder.name + ':' + str(int(time.mktime(timestamp.timetuple())))

        def execute():
            build = Build.get_by_key_name(key_name)
            if build:
                return self._output('The build at %s already exists for %s' % (str(timestamp), builder.name))

            return Build(branch=branch, platform=platform, builder=builder, buildNumber=buildNumber,
                timestamp=timestamp, revision=revision, key_name=key_name).put()
        return db.run_in_transaction(execute)

    def _addTestIfNeeded(self, testName, branch, platform):

        def execute(id):
            test = Test.get_by_key_name(testName)
            returnValue = None
            if not test:
                test = Test(id=id, name=testName, key_name=testName)
                returnValue = test
            if branch.key() not in test.branches:
                test.branches.append(branch.key())
            if platform.key() not in test.platforms:
                test.platforms.append(platform.key())
            test.put()
            return returnValue
        createInTransactionWithNumericIdHolder(execute)


class AdminReportHandler(ReportHandler):
    def bypassAuthentication(self):
        return True
