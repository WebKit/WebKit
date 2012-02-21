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
from google.appengine.api import taskqueue
from google.appengine.ext import db

from json_generators import DashboardJSONGenerator
from json_generators import ManifestJSONGenerator
from json_generators import RunsJSONGenerator
from models import Branch
from models import Platform
from models import Test
from models import PersistentCache
from models import model_from_numeric_id


def cache_manifest(cache):
    PersistentCache.set_cache('manifest', cache)


def schedule_manifest_update():
    taskqueue.add(url='/api/test/update')


class ManifestUpdateHandler(webapp2.RequestHandler):
    def post(self):
        self.response.headers['Content-Type'] = 'text/plain; charset=utf-8'
        cache_manifest(ManifestJSONGenerator().to_json())
        self.response.out.write('OK')


class CachedManifestHandler(webapp2.RequestHandler):
    def get(self):
        self.response.headers['Content-Type'] = 'application/json'
        manifest = PersistentCache.get_cache('manifest')
        if manifest:
            self.response.out.write(manifest)
        else:
            schedule_manifest_update()


def cache_dashboard(cache):
    PersistentCache.set_cache('dashboard', cache)


def schedule_dashboard_update():
    taskqueue.add(url='/api/test/dashboard/update')


class DashboardUpdateHandler(webapp2.RequestHandler):
    def post(self):
        self.response.headers['Content-Type'] = 'text/plain; charset=utf-8'
        cache_dashboard(DashboardJSONGenerator().to_json())
        self.response.out.write('OK')


class CachedDashboardHandler(webapp2.RequestHandler):
    def get(self):
        self.response.headers['Content-Type'] = 'application/json'
        dashboard = PersistentCache.get_cache('dashboard')
        if dashboard:
            self.response.out.write(dashboard)
        else:
            schedule_dashboard_update()


def cache_runs(test_id, branch_id, platform_id, cache):
    PersistentCache.set_cache(Test.cache_key(test_id, branch_id, platform_id), cache)


def schedule_runs_update(test_id, branch_id, platform_id):
    taskqueue.add(url='/api/test/runs/update', params={'id': test_id, 'branchid': branch_id, 'platformid': platform_id})


def _get_test_branch_platform_ids(handler):
    try:
        test_id = int(handler.request.get('id', 0))
        branch_id = int(handler.request.get('branchid', 0))
        platform_id = int(handler.request.get('platformid', 0))
        return test_id, branch_id, platform_id
    except TypeError:
        # FIXME: Output an error here
        return 0, 0, 0


class RunsUpdateHandler(webapp2.RequestHandler):
    def get(self):
        self.post()

    def get(self):
        self.response.headers['Content-Type'] = 'text/plain; charset=utf-8'
        test_id, branch_id, platform_id = _get_test_branch_platform_ids(self)

        branch = model_from_numeric_id(branch_id, Branch)
        platform = model_from_numeric_id(platform_id, Platform)
        test = model_from_numeric_id(test_id, Test)
        assert branch
        assert platform
        assert test

        cache_runs(test_id, branch_id, platform_id, RunsJSONGenerator(branch, platform, test.name).to_json())
        self.response.out.write('OK')


class CachedRunsHandler(webapp2.RequestHandler):
    def get(self):
        self.response.headers['Content-Type'] = 'application/json'

        test_id, branch_id, platform_id = _get_test_branch_platform_ids(self)
        runs = PersistentCache.get_cache(Test.cache_key(test_id, branch_id, platform_id))
        if runs:
            self.response.out.write(runs)
        else:
            schedule_runs_update(test_id, branch_id, platform_id)


def schedule_report_process(log):
    taskqueue.add(url='/api/test/report/process', params={'id': log.key().id()})
