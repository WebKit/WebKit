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
from google.appengine.api import taskqueue
from google.appengine.ext import db

from models import Test
from models import PersistentCache


def set_persistent_cache(name, value):
    memcache.set(name, value)

    def execute():
        cache = PersistentCache.get_by_key_name(name)
        if cache:
            cache.value = value
            cache.put()
        else:
            PersistentCache(key_name=name, value=value).put()

    db.run_in_transaction(execute)


def get_persistent_cache(name):
    value = memcache.get(name)
    if value:
        return value
    cache = PersistentCache.get_by_key_name(name)
    memcache.set(name, cache)
    return cache.value


def cache_manifest(cache):
    set_persistent_cache('manifest', cache)


def schedule_manifest_update():
    taskqueue.add(url='/api/test/update')


class CachedManifestHandler(webapp2.RequestHandler):
    def get(self):
        self.response.headers['Content-Type'] = 'application/json'
        manifest = get_persistent_cache('manifest')
        if manifest:
            self.response.out.write(manifest)
        else:
            schedule_manifest_update()


def cache_dashboard(cache):
    set_persistent_cache('dashboard', cache)


def schedule_dashboard_update():
    taskqueue.add(url='/api/test/dashboard/update')


class CachedDashboardHandler(webapp2.RequestHandler):
    def get(self):
        self.response.headers['Content-Type'] = 'application/json'
        dashboard = get_persistent_cache('dashboard')
        if dashboard:
            self.response.out.write(dashboard)
        else:
            schedule_dashboard_update()


def cache_runs(test_id, branch_id, platform_id, cache):
    set_persistent_cache(Test.cache_key(test_id, branch_id, platform_id), cache)


def schedule_runs_update(test_id, branch_id, platform_id):
    taskqueue.add(url='/api/test/runs/update', params={'id': test_id, 'branchid': branch_id, 'platformid': platform_id})


class CachedRunsHandler(webapp2.RequestHandler):
    def get(self):
        self.response.headers['Content-Type'] = 'application/json'

        try:
            test_id = int(self.request.get('id', 0))
            branch_id = int(self.request.get('branchid', 0))
            platform_id = int(self.request.get('platformid', 0))
        except TypeError:
            # FIXME: Output an error here
            test_id = 0
            branch_id = 0
            platform_id = 0

        runs = get_persistent_cache(Test.cache_key(test_id, branch_id, platform_id))
        if runs:
            self.response.out.write(runs)
        else:
            schedule_runs_update(test_id, branch_id, platform_id)
