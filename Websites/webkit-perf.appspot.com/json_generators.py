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
            self._dashboard['platformToId'][platform.name] = platform.id

        for test in Test.all():
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


# FIXME: This isn't a JSON generator anymore. We should move it elsewhere or rename the file.
class Runs(JSONGeneratorBase):
    def __init__(self, branch, platform, test_name):
        self._branch = branch
        self._platform = platform
        self._test_name = test_name

    @staticmethod
    def _generate_runs(branch, platform, test_name):
        builds = Build.all()
        builds.filter('branch =', branch)
        builds.filter('platform =', platform)

        for build in builds:
            results = TestResult.all()
            results.filter('name =', test_name)
            results.filter('build =', build)
            for result in results:
                yield build, result
        raise StopIteration

    @staticmethod
    def _entry_from_build_and_result(build, result):
        builder_id = build.builder.key().id()
        timestamp = mktime(build.timestamp.timetuple())
        statistics = None
        supplementary_revisions = None

        if result.valueStdev != None and result.valueMin != None and result.valueMax != None:
            statistics = {'stdev': result.valueStdev, 'min': result.valueMin, 'max': result.valueMax}

        if build.chromiumRevision != None:
            supplementary_revisions = {'Chromium': build.chromiumRevision}

        return [result.key().id(),
            [build.key().id(), build.buildNumber, build.revision, supplementary_revisions],
            timestamp, result.value, 0,  # runNumber
            [],  # annotations
            builder_id, statistics]

    def value(self):
        _test_runs = []
        _averages = {}
        values = []

        for build, result in Runs._generate_runs(self._branch, self._platform, self._test_name):
            _test_runs.append(Runs._entry_from_build_and_result(build, result))
            # FIXME: Calculate the average. In practice, we wouldn't have more than one value for a given revision.
            _averages[build.revision] = result.value
            values.append(result.value)

        _min = min(values) if values else None
        _max = max(values) if values else None

        return {
            'test_runs': _test_runs,
            'averages': _averages,
            'min': _min,
            'max': _max,
            'date_range': None,  # Never used by common.js.
            'stat': 'ok'}

    def chart_params(self, display_days, now=datetime.now().replace(hour=12, minute=0, second=0, microsecond=0)):
        chart_data_x = []
        chart_data_y = []
        end_time = now
        start_timestamp = mktime((end_time - timedelta(display_days)).timetuple())
        end_timestamp = mktime(end_time.timetuple())

        for build, result in self._generate_runs(self._branch, self._platform, self._test_name):
            timestamp = mktime(build.timestamp.timetuple())
            if timestamp < start_timestamp or timestamp > end_timestamp:
                continue
            chart_data_x.append(timestamp)
            chart_data_y.append(result.value)

        dates = [end_time - timedelta(display_days / 7.0 * (7 - i)) for i in range(0, 8)]

        y_max = max(chart_data_y) * 1.1
        y_axis_label_step = int(y_max / 5 + 0.5)  # This won't work for decimal numbers

        return {
            'cht': 'lxy',  # Specify with X and Y coordinates
            'chxt': 'x,y',  # Display both X and Y axies
            'chxl': '0:|' + '|'.join([date.strftime('%b %d') for date in dates]),  # X-axis labels
            'chxr': '1,0,%f,%f' % (int(y_max + 0.5), y_axis_label_step),  # Y-axis range: min=0, max, step
            'chds': '%f,%f,%f,%f' % (start_timestamp, end_timestamp, 0, y_max),  # X, Y data range
            'chxs': '1,676767,11.167,0,l,676767',  # Y-axis label: 1,color,font-size,centerd on tick,axis line/no ticks, tick color
            'chs': '360x240',  # Image size: 360px by 240px
            'chco': 'ff0000',  # Plot line color
            'chg': '%f,20,0,0' % (100 / (len(dates) - 1)),  # X, Y grid line step sizes - max is 100.
            'chls': '3',  # Line thickness
            'chf': 'bg,s,eff6fd',  # Transparent background
            'chd': 't:' + ','.join([str(x) for x in chart_data_x]) + '|' + ','.join([str(y) for y in chart_data_y]),  # X, Y data
        }
