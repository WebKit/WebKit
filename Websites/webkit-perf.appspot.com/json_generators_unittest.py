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
from datetime import timedelta
from json_generators import JSONGeneratorBase
from json_generators import DashboardJSONGenerator
from json_generators import ManifestJSONGenerator
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

    def test_value_with_hidden_platform_and_tesst(self):
        webkit_trunk = Branch.create_if_possible('webkit-trunk', 'WebKit trunk')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        hidden_platform = Platform.create_if_possible('hidden-platform', 'Hidden Platform')
        hidden_platform.hidden = True
        hidden_platform.put()
        Test.update_or_insert('some-test', webkit_trunk, some_platform)
        Test.update_or_insert('some-test', webkit_trunk, hidden_platform)
        Test.update_or_insert('other-test', webkit_trunk, some_platform)
        Test.update_or_insert('other-test', webkit_trunk, hidden_platform)
        Test.update_or_insert('hidden-test', webkit_trunk, some_platform)
        Test.update_or_insert('hidden-test', webkit_trunk, hidden_platform)
        hidden_test = Test.get_by_key_name('hidden-test')
        hidden_test.hidden = True
        hidden_test.put()
        self.assertEqual(DashboardJSONGenerator().value()['platformToId'], {'Some Platform': some_platform.id})
        self.assertEqual(DashboardJSONGenerator().value()['testToId'],
            {'some-test': Test.get_by_key_name('some-test').id, 'other-test': Test.get_by_key_name('other-test').id})


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
        self.assertEqual(value['branchMap'],
            {some_branch.id: {'name': some_branch.name, 'testIds': expected_test_ids, 'platformIds': [some_platform.id]}})
        self.assertEqual(value['platformMap'],
            {some_platform.id: {'name': some_platform.name, 'branchIds': [some_branch.id], 'testIds': expected_test_ids}})
        self.assertEqual(value['testMap'],
            {some_test.id: {'name': some_test.name, 'branchIds': [some_branch.id], 'platformIds': [some_platform.id]},
            other_test.id: {'name': other_test.name, 'branchIds': [some_branch.id], 'platformIds': [some_platform.id]}})

    def test_value_with_hidden_platform_and_test(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        hidden_platform = Platform.create_if_possible('hidden-platform', 'Hidden Platform')
        hidden_platform.hidden = True
        hidden_platform.put()

        Test.update_or_insert('some-test', some_branch, some_platform)
        some_test = Test.update_or_insert('some-test', some_branch, hidden_platform)

        Test.update_or_insert('hidden-test', some_branch, some_platform)
        hidden_test = Test.update_or_insert('hidden-test', some_branch, hidden_platform)
        hidden_test.hidden = True
        hidden_test.put()

        value = ManifestJSONGenerator().value()
        expected_test_ids = []
        self.assertEqualUnorderedList(value.keys(), ['branchMap', 'platformMap', 'testMap'])
        self.assertEqual(value['branchMap'],
            {some_branch.id: {'name': some_branch.name, 'testIds': [some_test.id], 'platformIds': [some_platform.id]}})
        self.assertEqual(value['platformMap'],
            {some_platform.id: {'name': some_platform.name, 'branchIds': [some_branch.id], 'testIds': [some_test.id]}})
        self.assertEqual(value['testMap'],
            {some_test.id: {'name': some_test.name, 'branchIds': [some_branch.id], 'platformIds': [some_platform.id]}})


if __name__ == '__main__':
    unittest.main()
