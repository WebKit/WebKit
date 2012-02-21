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
import models
import unittest

from google.appengine.ext import testbed
from datetime import datetime
from json_generators import JSONGeneratorBase
from json_generators import DashboardJSONGenerator
from json_generators import ManifestJSONGenerator
from json_generators import RunsJSONGenerator
from models_unittest import DataStoreTestsBase
from models import Branch
from models import Build
from models import Builder
from models import Platform
from models import Test
from models import TestResult


def _create_results(branch, platform, builder, test_name, values):
    results = []
    for i, value in enumerate(values):
        build = Build(branch=branch, platform=platform, builder=builder,
            buildNumber=i, revision=100 + i, timestamp=datetime.now())
        build.put()
        result = TestResult(name=test_name, build=build, value=value)
        result.put()
        Test.update_or_insert(test_name, branch, platform)
        results.append(result)
    return results


class JSONGeneratorBaseTest(unittest.TestCase):
    def test_to_json(self):

        class AJSONGenerator(JSONGeneratorBase):
            def value(self):
                return {'key': 'value'}

        self.assertEqual(AJSONGenerator().value(), {"key": "value"})
        self.assertEqual(AJSONGenerator().to_json(), '{"key": "value"}')


class DashboardJSONGeneratorTest(DataStoreTestsBase):
    def test_value_no_branch(self):
        self.assertThereIsNoInstanceOf(Branch)
        self.assertEqual(DashboardJSONGenerator().value(), None)

    def test_value_no_plaforms(self):
        webkit_trunk = Branch.create_if_possible('webkit-trunk', 'WebKit trunk')
        self.assertEqual(DashboardJSONGenerator().value(), {
            'defaultBranch': 'WebKit trunk',
            'branchToId': {'WebKit trunk': webkit_trunk.id},
            'platformToId': {},
            'testToId': {},
        })

    def test_value_single_platform(self):
        webkit_trunk = Branch.create_if_possible('webkit-trunk', 'WebKit trunk')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        self.assertEqual(DashboardJSONGenerator().value(), {
            'defaultBranch': 'WebKit trunk',
            'branchToId': {'WebKit trunk': webkit_trunk.id},
            'platformToId': {'Some Platform': some_platform.id},
            'testToId': {},
        })

        Test.update_or_insert('some-test', webkit_trunk, some_platform)
        self.assertEqual(DashboardJSONGenerator().value(), {
            'defaultBranch': 'WebKit trunk',
            'branchToId': {'WebKit trunk': webkit_trunk.id},
            'platformToId': {'Some Platform': some_platform.id},
            'testToId': {'some-test': Test.get_by_key_name('some-test').id},
        })

    def test_value_two_platforms(self):
        webkit_trunk = Branch.create_if_possible('webkit-trunk', 'WebKit trunk')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        other_platform = Platform.create_if_possible('other-platform', 'Other Platform')
        Test.update_or_insert('some-test', webkit_trunk, some_platform)
        Test.update_or_insert('some-test', webkit_trunk, other_platform)
        self.assertEqual(DashboardJSONGenerator().value(), {
            'defaultBranch': 'WebKit trunk',
            'branchToId': {'WebKit trunk': webkit_trunk.id},
            'platformToId': {'Some Platform': some_platform.id, 'Other Platform': other_platform.id},
            'testToId': {'some-test': Test.get_by_key_name('some-test').id},
        })


class ManifestJSONGeneratorTest(DataStoreTestsBase):
    def test_value_no_branch(self):
        self.assertThereIsNoInstanceOf(Branch)
        self.assertEqual(ManifestJSONGenerator().value(), {'branchMap': {}, 'platformMap': {}, 'testMap': {}})

    def test_value_no_plaforms(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        self.assertEqual(ManifestJSONGenerator().value(), {'branchMap': {}, 'platformMap': {}, 'testMap': {}})

    def _assert_single_test(self, value, branch, platform, test):
        self.assertEqualUnorderedList(value.keys(), ['branchMap', 'platformMap', 'testMap'])
        self.assertEqual(value['branchMap'],
            {branch.id: {'name': branch.name, 'testIds': [test.id], 'platformIds': [platform.id]}})
        self.assertEqual(value['platformMap'],
            {platform.id: {'name': platform.name, 'branchIds': [branch.id], 'testIds': [test.id]}})
        self.assertEqual(value['testMap'],
            {test.id: {'name': test.name, 'branchIds': [branch.id], 'platformIds': [platform.id]}})

    def test_value_single_platform(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        self.assertEqual(ManifestJSONGenerator().value(), {'branchMap': {}, 'platformMap': {}, 'testMap': {}})

        some_test = Test.update_or_insert('some-test', some_branch, some_platform)
        self._assert_single_test(ManifestJSONGenerator().value(), some_branch, some_platform, some_test)

    def test_value_two_platforms(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        other_platform = Platform.create_if_possible('other-platform', 'Other Platform')
        some_test = Test.update_or_insert('some-test', some_branch, some_platform)

        self._assert_single_test(ManifestJSONGenerator().value(), some_branch, some_platform, some_test)

        some_test = Test.update_or_insert('some-test', some_branch, other_platform)
        self.assertEqualUnorderedList(some_test.platforms, [some_platform.key(), other_platform.key()])

        value = ManifestJSONGenerator().value()
        expected_platform_ids = [some_platform.id, other_platform.id]
        self.assertEqualUnorderedList(value.keys(), ['branchMap', 'platformMap', 'testMap'])
        self.assertEqualUnorderedList(value['branchMap'],
            {some_branch.id: {'name': some_branch.name, 'testIds': [some_test.id], 'platformIds': expected_platform_ids}})
        self.assertEqual(value['platformMap'],
            {some_platform.id: {'name': some_platform.name, 'branchIds': [some_branch.id], 'testIds': [some_test.id]},
            other_platform.id: {'name': other_platform.name, 'branchIds': [some_branch.id], 'testIds': [some_test.id]}})
        self.assertEqual(value['testMap'],
            {some_test.id: {'name': some_test.name, 'branchIds': [some_branch.id], 'platformIds': expected_platform_ids}})

    def test_value_two_tests(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        some_test = Test.update_or_insert('some-test', some_branch, some_platform)

        self._assert_single_test(ManifestJSONGenerator().value(), some_branch, some_platform, some_test)

        other_test = Test.update_or_insert('other-test', some_branch, some_platform)

        value = ManifestJSONGenerator().value()
        expected_test_ids = [some_test.id, other_test.id]
        self.assertEqualUnorderedList(value.keys(), ['branchMap', 'platformMap', 'testMap'])
        self.assertEqualUnorderedList(value['branchMap'],
            {some_branch.id: {'name': some_branch.name, 'testIds': expected_test_ids, 'platformIds': [some_platform.id]}})
        self.assertEqual(value['platformMap'],
            {some_platform.id: {'name': some_platform.name, 'branchIds': [some_branch.id], 'testIds': expected_test_ids}})
        self.assertEqual(value['testMap'],
            {some_test.id: {'name': some_test.name, 'branchIds': [some_branch.id], 'platformIds': [some_platform.id]},
            other_test.id: {'name': other_test.name, 'branchIds': [some_branch.id], 'platformIds': [some_platform.id]}})


class RunsJSONGeneratorTest(DataStoreTestsBase):
    def _create_results(self, branch, platform, builder, test_name, values):
        results = []
        for i, value in enumerate(values):
            build = Build(branch=branch, platform=platform, builder=builder,
                buildNumber=i, revision=100 + i, timestamp=datetime.now())
            build.put()
            result = TestResult(name=test_name, build=build, value=value)
            result.put()
            results.append(result)
        return results

    def test_generate_runs(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        some_builder = Builder.get(Builder.create('some-builder', 'Some Builder'))

        results = self._create_results(some_branch, some_platform, some_builder, 'some-test', [50.0, 51.0, 52.0, 49.0, 48.0])
        last_i = 0
        for i, (build, result) in enumerate(RunsJSONGenerator._generate_runs(some_branch, some_platform, "some-test")):
            self.assertEqual(build.buildNumber, i)
            self.assertEqual(build.revision, 100 + i)
            self.assertEqual(result.name, 'some-test')
            self.assertEqual(result.value, results[i].value)
            last_i = i
        self.assertTrue(last_i + 1, len(results))

    def test_value_without_results(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        self.assertThereIsNoInstanceOf(Test)
        self.assertThereIsNoInstanceOf(TestResult)
        self.assertEqual(RunsJSONGenerator(some_branch, some_platform, 'some-test').value(), {
            'test_runs': [],
            'averages': {},
            'min': None,
            'max': None,
            'date_range': None,
            'stat': 'ok'})

    def test_value_with_results(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        some_builder = Builder.get(Builder.create('some-builder', 'Some Builder'))
        results = self._create_results(some_branch, some_platform, some_builder, 'some-test', [50.0, 51.0, 52.0, 49.0, 48.0])

        value = RunsJSONGenerator(some_branch, some_platform, 'some-test').value()
        self.assertEqualUnorderedList(value.keys(), ['test_runs', 'averages', 'min', 'max', 'date_range', 'stat'])
        self.assertEqual(value['stat'], 'ok')
        self.assertEqual(value['min'], 48.0)
        self.assertEqual(value['max'], 52.0)
        self.assertEqual(value['date_range'], None)  # date_range is never given

        self.assertEqual(len(value['test_runs']), len(results))
        for i, run in enumerate(value['test_runs']):
            result = results[i]
            self.assertEqual(run[0], result.key().id())
            self.assertEqual(run[1][1], i)  # Build number
            self.assertEqual(run[1][2], 100 + i)  # Revision
            self.assertEqual(run[1][3], None)  # Supplementary revision
            self.assertEqual(run[3], result.value)
            self.assertEqual(run[6], some_builder.key().id())
            self.assertEqual(run[7], None)  # Statistics

    def _assert_entry(self, entry, build, result, value, statistics=None, supplementary_revisions=None):
        entry = entry[:]
        entry[2] = None  # timestamp
        self.assertEqual(entry, [result.key().id(), [build.key().id(), build.buildNumber, build.revision, supplementary_revisions],
            None,  # timestamp
            value, 0,  # runNumber
            [],  # annotations
            build.builder.key().id(), statistics])

    def test_run_from_build_and_result(self):
        branch = Branch.create_if_possible('some-branch', 'Some Branch')
        platform = Platform.create_if_possible('some-platform', 'Some Platform')
        builder = Builder.get(Builder.create('some-builder', 'Some Builder'))
        test_name = ' some-test'

        def create_build(build_number, revision):
            timestamp = datetime.now().replace(microsecond=0)
            build = Build(branch=branch, platform=platform, builder=builder, buildNumber=build_number,
                revision=revision, timestamp=timestamp)
            build.put()
            return build

        build = create_build(1, 101)
        result = TestResult(name=test_name, value=123.0, build=build)
        result.put()
        self._assert_entry(RunsJSONGenerator._entry_from_build_and_result(build, result), build, result, 123.0)

        build = create_build(2, 102)
        result = TestResult(name=test_name, value=456.0, valueMedian=789.0, build=build)
        result.put()
        self._assert_entry(RunsJSONGenerator._entry_from_build_and_result(build, result), build, result, 456.0)

        result.valueStdev = 7.0
        result.put()
        self._assert_entry(RunsJSONGenerator._entry_from_build_and_result(build, result), build, result, 456.0)

        result.valueStdev = None
        result.valueMin = 123.0
        result.valueMax = 789.0
        result.put()
        self._assert_entry(RunsJSONGenerator._entry_from_build_and_result(build, result), build, result, 456.0)

        result.valueStdev = 8.0
        result.valueMin = 123.0
        result.valueMax = 789.0
        result.put()
        self._assert_entry(RunsJSONGenerator._entry_from_build_and_result(build, result), build, result, 456.0,
            statistics={'stdev': 8.0, 'min': 123.0, 'max': 789.0})

        result.valueMedian = 345.0  # Median is never used by the frontend.
        result.valueStdev = 8.0
        result.valueMin = 123.0
        result.valueMax = 789.0
        result.put()
        self._assert_entry(RunsJSONGenerator._entry_from_build_and_result(build, result), build, result, 456.0,
            statistics={'stdev': 8.0, 'min': 123.0, 'max': 789.0})


if __name__ == '__main__':
    unittest.main()
