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

import json
from datetime import datetime
from datetime import timedelta
from time import mktime

from models import Build
from models import Builder
from models import Branch
from models import Platform
from models import Test
from models import TestResult


class JSONGeneratorBase(object):
    def to_json(self):
        return json.dumps(self.value())


class DashboardJSONGenerator(JSONGeneratorBase):
    def __init__(self):
        # FIXME: Don't hard-code webkit-trunk here; ideally we want this be customizable.
        webkit_trunk = Branch.get_by_key_name('webkit-trunk')
        if not webkit_trunk:
            self._dashboard = None
            return

        # FIXME: Determine popular branches, platforms, and tests
        self._dashboard = {
            'defaultBranch': 'WebKit trunk',
            'branchToId': {webkit_trunk.name: webkit_trunk.id},
            'platformToId': {},
            'testToId': {},
        }

        for platform in Platform.all():
            if not platform.hidden:
                self._dashboard['platformToId'][platform.name] = platform.id

        for test in Test.all():
            if not test.hidden:
                self._dashboard['testToId'][test.name] = test.id

    def value(self):
        return self._dashboard


class ManifestJSONGenerator(JSONGeneratorBase):
    def __init__(self):
        # FIXME: This function is massive. Break it up
        self._test_map = {}
        platform_id_map = {}
        branch_id_map = {}
        for test in Test.all():
            if test.hidden:
                continue

            branch_ids = [Branch.get(branch_key).id for branch_key in test.branches]
            platform_ids = [Platform.get(platform_key).id for platform_key in test.platforms]
            self._test_map[test.id] = {
                'name': test.name,
                'branchIds': branch_ids,
                'platformIds': platform_ids,
            }

            for platform_id in platform_ids:
                platform_id_map.setdefault(platform_id, {'tests': [], 'branches': []})
                platform_id_map[platform_id]['tests'].append(test.id)
                platform_id_map[platform_id]['branches'] += branch_ids

            for branch_id in branch_ids:
                branch_id_map.setdefault(branch_id, {'tests': [], 'platforms': []})
                branch_id_map[branch_id]['tests'].append(test.id)
                branch_id_map[branch_id]['platforms'] += platform_ids

        self._platform_map = {}
        for platform in Platform.all():
            if platform.id not in platform_id_map:
                continue

            if platform.hidden:
                for test_id in platform_id_map[platform.id]['tests']:
                    self._test_map[test_id]['platformIds'].remove(platform.id)
                for branch_id in platform_id_map[platform.id]['branches']:
                    branch_id_map[branch_id]['platforms'].remove(platform.id)
                continue

            self._platform_map[platform.id] = {
                'name': platform.name,
                'testIds': list(set(platform_id_map[platform.id]['tests'])),
                'branchIds': list(set(platform_id_map[platform.id]['branches'])),
            }

        self._branch_map = {}
        for branch in Branch.all():
            if branch.id not in branch_id_map:
                continue
            self._branch_map[branch.id] = {
                'name': branch.name,
                'testIds': list(set(branch_id_map[branch.id]['tests'])),
                'platformIds': list(set(branch_id_map[branch.id]['platforms'])),
            }

    def value(self):
        return {'branchMap': self._branch_map, 'platformMap': self._platform_map, 'testMap': self._test_map}
