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

import json
from time import mktime
from datetime import datetime

from models import Build
from models import Builder
from models import Branch
from models import NumericIdHolder
from models import Platform
from models import Test
from models import TestResult
from models import modelFromNumericId


class RunsHandler(webapp2.RequestHandler):
    def get(self):
        self.response.headers['Content-Type'] = 'application/json; charset=utf-8'

        try:
            test_id = int(self.request.get('id', 0))
            branch_id = int(self.request.get('branchid', 0))
            platform_id = int(self.request.get('platformid', 0))
        except TypeError:
            # FIXME: Output an error here
            test_id = 0
            branch_id = 0
            platform_id = 0

        # FIXME: Just fetch builds specified by "days"
        # days = self.request.get('days', 365)

        cache_key = Test.cache_key(test_id, branch_id, platform_id)
        cache = memcache.get(cache_key)
        if cache:
            self.response.out.write(cache)
            return

        builds = Build.all()
        builds.filter('branch =', modelFromNumericId(branch_id, Branch))
        builds.filter('platform =', modelFromNumericId(platform_id, Platform))

        test = modelFromNumericId(test_id, Test)
        test_name = test.name if test else None
        test_runs = []
        averages = {}
        values = []
        timestamps = []

        for build in builds:
            results = TestResult.all()
            results.filter('name =', test_name)
            results.filter('build =', build)
            for result in results:
                builderId = build.builder.key().id()
                posixTimestamp = mktime(build.timestamp.timetuple())
                statistics = None
                if result.valueStdev != None and result.valueMin != None and result.valueMax != None:
                    statistics = {'stdev': result.valueStdev, 'min': result.valueMin, 'max': result.valueMax}
                test_runs.append([result.key().id(),
                    [build.key().id(), build.buildNumber, build.revision],
                    posixTimestamp, result.value, 0,  # runNumber
                    [],  # annotations
                    builderId, statistics])
                # FIXME: Calculate the average; in practice, we wouldn't have more than one value for a given revision
                averages[build.revision] = result.value
                values.append(result.value)
                timestamps.append(posixTimestamp)

        result = json.dumps({
            'test_runs': test_runs,
            'averages': averages,
            'min': min(values) if values else None,
            'max': max(values) if values else None,
            'date_range': [min(timestamps), max(timestamps)] if timestamps else None,
            'stat': 'ok'})
        self.response.out.write(result)
        memcache.add(cache_key, result)
