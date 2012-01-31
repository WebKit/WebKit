#!/usr/bin/env python
# Copyright (C) 2011 Google Inc. All rights reserved.
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

from models import Builder
from models import Branch
from models import Platform
from models import Test


class ManifestHandler(webapp2.RequestHandler):
    def get(self):
        self.response.headers['Content-Type'] = 'text/plain; charset=utf-8';
        cache = memcache.get('manifest')
        if cache:
            self.response.out.write(cache)
            return

        testMap = {}
        platformIdMap = {}
        branchIdMap = {}
        for test in Test.all():
            branchIds = [Branch.get(branchKey).id for branchKey in test.branches]
            platformIds = [Platform.get(platformKey).id for platformKey in test.platforms]
            testMap[test.id] = {
                'name': test.name,
                'branchIds': branchIds,
                'platformIds': platformIds,
            }

            for platformId in platformIds:
                platformIdMap.setdefault(platformId, {'tests': [], 'branches': []})
                platformIdMap[platformId]['tests'].append(test.id)
                platformIdMap[platformId]['branches'] += branchIds

            for branchId in branchIds:
                branchIdMap.setdefault(branchId, {'tests': [], 'platforms': []})
                branchIdMap[branchId]['tests'].append(test.id)
                branchIdMap[branchId]['platforms'] += platformIds

        platformMap = {}
        for platform in Platform.all():
            if platform.id not in platformIdMap:
                continue
            platformMap[platform.id] = {
                'name': platform.name,
                'testIds': list(set(platformIdMap[platform.id]['tests'])),
                'branchIds': list(set(platformIdMap[platform.id]['branches'])),
            }

        branchMap = {}
        for branch in Branch.all():
            if branch.id not in branchIdMap:
                continue
            branchMap[branch.id] = {
                'name': branch.name,
                'testIds': list(set(branchIdMap[branch.id]['tests'])),
                'platformIds': list(set(branchIdMap[branch.id]['platforms'])),
            }

        result = json.dumps({'testMap': testMap, 'platformMap': platformMap, 'branchMap': branchMap})
        self.response.out.write(result)
        memcache.add('manifest', result)
