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

from datetime import datetime
from datetime import timedelta
from google.appengine.api import memcache
from google.appengine.ext import testbed
from time import mktime

from models import NumericIdHolder
from models import Branch
from models import Platform
from models import Builder
from models import Build
from models import Test
from models import TestResult
from models import ReportLog
from models import PersistentCache
from models import Runs
from models import DashboardImage
from models import create_in_transaction_with_numeric_id_holder
from models import delete_model_with_numeric_id_holder
from models import model_from_numeric_id


class DataStoreTestsBase(unittest.TestCase):
    def setUp(self):
        self.testbed = testbed.Testbed()
        self.testbed.activate()
        self.testbed.init_datastore_v3_stub()

    def tearDown(self):
        self.testbed.deactivate()

    def assertThereIsNoInstanceOf(self, model):
        self.assertEqual(len(model.all().fetch(5)), 0)

    def assertOnlyInstance(self, only_instasnce):
        self.assertOnlyInstances([only_instasnce])

    def assertOnlyInstances(self, expected_instances):
        actual_instances = expected_instances[0].__class__.all().fetch(len(expected_instances) + 1)
        self.assertEqualUnorderedModelList(actual_instances, expected_instances)

    def assertEqualUnorderedModelList(self, list1, list2):
        self.assertEqualUnorderedList([item.key() for item in list1], [item.key() for item in list1])

    def assertEqualUnorderedList(self, list1, list2):
        self.assertEqual(set(list1), set(list2))
        self.assertEqual(len(list1), len(list2))


class HelperTests(DataStoreTestsBase):
    def _assert_there_is_exactly_one_id_holder_and_matches(self, id):
        id_holders = NumericIdHolder.all().fetch(5)
        self.assertEqual(len(id_holders), 1)
        self.assertTrue(id_holders[0])
        self.assertEqual(id_holders[0].key().id(), id)

    def test_create_in_transaction_with_numeric_id_holder(self):

        def execute(id):
            return Branch(id=id, name='some branch', key_name='some-branch').put()

        self.assertThereIsNoInstanceOf(Branch)
        self.assertThereIsNoInstanceOf(NumericIdHolder)

        self.assertTrue(create_in_transaction_with_numeric_id_holder(execute))

        branches = Branch.all().fetch(5)
        self.assertEqual(len(branches), 1)
        self.assertEqual(branches[0].name, 'some branch')
        self.assertEqual(branches[0].key().name(), 'some-branch')

        self._assert_there_is_exactly_one_id_holder_and_matches(branches[0].id)

    def test_failing_in_create_in_transaction_with_numeric_id_holder(self):

        def execute(id):
            return None

        self.assertThereIsNoInstanceOf(Branch)
        self.assertThereIsNoInstanceOf(NumericIdHolder)

        self.assertFalse(create_in_transaction_with_numeric_id_holder(execute))

        self.assertThereIsNoInstanceOf(Branch)
        self.assertThereIsNoInstanceOf(NumericIdHolder)

    def test_raising_in_create_in_transaction_with_numeric_id_holder(self):

        def execute(id):
            raise TypeError
            return None

        self.assertThereIsNoInstanceOf(Branch)
        self.assertThereIsNoInstanceOf(NumericIdHolder)

        self.assertRaises(TypeError, create_in_transaction_with_numeric_id_holder, (execute))

        self.assertThereIsNoInstanceOf(Branch)
        self.assertThereIsNoInstanceOf(NumericIdHolder)

    def test_delete_model_with_numeric_id_holder(self):

        def execute(id):
            return Branch(id=id, name='some branch', key_name='some-branch').put()

        branch = Branch.get(create_in_transaction_with_numeric_id_holder(execute))
        self.assertOnlyInstance(branch)

        delete_model_with_numeric_id_holder(branch)

        self.assertThereIsNoInstanceOf(Branch)
        self.assertThereIsNoInstanceOf(NumericIdHolder)

    def test_model_from_numeric_id(self):

        def execute(id):
            return Branch(id=id, name='some branch', key_name='some-branch').put()

        branch = Branch.get(create_in_transaction_with_numeric_id_holder(execute))

        self.assertEqual(model_from_numeric_id(branch.id, Branch).key(), branch.key())
        self.assertEqual(model_from_numeric_id(branch.id + 1, Branch), None)
        delete_model_with_numeric_id_holder(branch)
        self.assertEqual(model_from_numeric_id(branch.id, Branch), None)


class BranchTests(DataStoreTestsBase):
    def test_create_if_possible(self):
        self.assertThereIsNoInstanceOf(Branch)

        branch = Branch.create_if_possible('some-branch', 'some branch')
        self.assertTrue(branch)
        self.assertTrue(branch.key().name(), 'some-branch')
        self.assertTrue(branch.name, 'some branch')
        self.assertOnlyInstance(branch)

        self.assertFalse(Branch.create_if_possible('some-branch', 'some other branch'))
        self.assertTrue(branch.name, 'some branch')
        self.assertOnlyInstance(branch)


class PlatformTests(DataStoreTestsBase):
    def test_create_if_possible(self):
        self.assertThereIsNoInstanceOf(Platform)

        platform = Platform.create_if_possible('some-platform', 'some platform')
        self.assertTrue(platform)
        self.assertTrue(platform.key().name(), 'some-platform')
        self.assertTrue(platform.name, 'some platform')
        self.assertOnlyInstance(platform)

        self.assertFalse(Platform.create_if_possible('some-platform', 'some other platform'))
        self.assertTrue(platform.name, 'some platform')
        self.assertOnlyInstance(platform)


class BuilderTests(DataStoreTestsBase):
    def test_create(self):
        builder_key = Builder.create('some builder', 'some password')
        self.assertTrue(builder_key)
        builder = Builder.get(builder_key)
        self.assertEqual(builder.key().name(), 'some builder')
        self.assertEqual(builder.name, 'some builder')
        self.assertEqual(builder.password, Builder._hashed_password('some password'))

    def test_update_password(self):
        builder = Builder.get(Builder.create('some builder', 'some password'))
        self.assertEqual(builder.password, Builder._hashed_password('some password'))
        builder.update_password('other password')
        self.assertEqual(builder.password, Builder._hashed_password('other password'))

        # Make sure it's saved
        builder = Builder.get(builder.key())
        self.assertEqual(builder.password, Builder._hashed_password('other password'))

    def test_hashed_password(self):
        self.assertNotEqual(Builder._hashed_password('some password'), 'some password')
        self.assertFalse('some password' in Builder._hashed_password('some password'))
        self.assertEqual(len(Builder._hashed_password('some password')), 64)

    def test_authenticate(self):
        builder = Builder.get(Builder.create('some builder', 'some password'))
        self.assertTrue(builder.authenticate('some password'))
        self.assertFalse(builder.authenticate('bad password'))


def _create_some_builder():
    branch = Branch.create_if_possible('some-branch', 'Some Branch')
    platform = Platform.create_if_possible('some-platform', 'Some Platform')
    builder_key = Builder.create('some-builder', 'Some Builder')
    return branch, platform, Builder.get(builder_key)


def _create_build(branch, platform, builder, key_name='some-build'):
    build_key = Build(key_name=key_name, branch=branch, platform=platform, builder=builder,
        buildNumber=1, revision=100, timestamp=datetime.now()).put()
    return Build.get(build_key)


class BuildTests(DataStoreTestsBase):
    def test_get_or_insert_from_log(self):
        branch, platform, builder = _create_some_builder()

        timestamp = datetime.now().replace(microsecond=0)
        log = ReportLog(timestamp=timestamp, headers='some headers',
            payload='{"branch": "some-branch", "platform": "some-platform", "builder-name": "some-builder",' +
                '"build-number": 123, "webkit-revision": 456, "timestamp": %d}' % int(mktime(timestamp.timetuple())))

        self.assertThereIsNoInstanceOf(Build)

        build = Build.get_or_insert_from_log(log)
        self.assertTrue(build)
        self.assertEqual(build.branch.key(), branch.key())
        self.assertEqual(build.platform.key(), platform.key())
        self.assertEqual(build.builder.key(), builder.key())
        self.assertEqual(build.buildNumber, 123)
        self.assertEqual(build.revision, 456)
        self.assertEqual(build.chromiumRevision, None)
        self.assertEqual(build.timestamp, timestamp)

        self.assertOnlyInstance(build)


class TestModelTests(DataStoreTestsBase):
    def test_update_or_insert(self):
        branch = Branch.create_if_possible('some-branch', 'Some Branch')
        platform = Platform.create_if_possible('some-platform', 'Some Platform')

        self.assertThereIsNoInstanceOf(Test)

        test = Test.update_or_insert('some-test', branch, platform)
        self.assertTrue(test)
        self.assertEqual(test.branches, [branch.key()])
        self.assertEqual(test.platforms, [platform.key()])
        self.assertEqual(test.unit, None)
        self.assertOnlyInstance(test)

    def test_update_or_insert_with_unit(self):
        branch = Branch.create_if_possible('some-branch', 'Some Branch')
        platform = Platform.create_if_possible('some-platform', 'Some Platform')
        test = Test.update_or_insert('some-test', branch, platform, 'runs/s')
        self.assertOnlyInstance(test)
        self.assertEqualUnorderedList(test.unit, 'runs/s')

    def test_update_or_insert_to_update(self):
        branch = Branch.create_if_possible('some-branch', 'Some Branch')
        platform = Platform.create_if_possible('some-platform', 'Some Platform')
        test = Test.update_or_insert('some-test', branch, platform)
        self.assertOnlyInstance(test)

        other_branch = Branch.create_if_possible('other-branch', 'Other Branch')
        other_platform = Platform.create_if_possible('other-platform', 'Other Platform')
        test = Test.update_or_insert('some-test', other_branch, other_platform, 'ms')
        self.assertOnlyInstance(test)
        self.assertEqualUnorderedList(test.branches, [branch.key(), other_branch.key()])
        self.assertEqualUnorderedList(test.platforms, [platform.key(), other_platform.key()])
        self.assertEqualUnorderedList(test.unit, 'ms')

    def test_merge(self):
        branch, platform, builder = _create_some_builder()
        some_build = _create_build(branch, platform, builder)
        some_result = TestResult.get_or_insert_from_parsed_json('some-test', some_build, 50)
        some_test = Test.update_or_insert('some-test', branch, platform)

        other_build = _create_build(branch, platform, builder, 'other-build')
        other_result = TestResult.get_or_insert_from_parsed_json('other-test', other_build, 30)
        other_test = Test.update_or_insert('other-test', branch, platform)

        self.assertOnlyInstances([some_result, other_result])
        self.assertNotEqual(some_result.key(), other_result.key())
        self.assertOnlyInstances([some_test, other_test])

        self.assertRaises(AssertionError, some_test.merge, (some_test))
        self.assertOnlyInstances([some_test, other_test])

        some_test.merge(other_test)
        results_for_some_test = TestResult.all()
        results_for_some_test.filter('name =', 'some-test')
        results_for_some_test = results_for_some_test.fetch(5)
        self.assertEqual(len(results_for_some_test), 2)

        self.assertEqual(results_for_some_test[0].name, 'some-test')
        self.assertEqual(results_for_some_test[1].name, 'some-test')

        if results_for_some_test[0].value == 50:
            self.assertEqual(results_for_some_test[1].value, 30)
        else:
            self.assertEqual(results_for_some_test[1].value, 50)


class TestResultTests(DataStoreTestsBase):
    def test_get_or_insert_value(self):
        branch, platform, builder = _create_some_builder()
        build = _create_build(branch, platform, builder)
        self.assertThereIsNoInstanceOf(TestResult)
        result = TestResult.get_or_insert_from_parsed_json('some-test', build, 50)
        self.assertOnlyInstance(result)
        self.assertEqual(result.name, 'some-test')
        self.assertEqual(result.build.key(), build.key())
        self.assertEqual(result.value, 50.0)
        self.assertEqual(result.valueMedian, None)
        self.assertEqual(result.valueStdev, None)
        self.assertEqual(result.valueMin, None)
        self.assertEqual(result.valueMax, None)

    def test_get_or_insert_stat_value(self):
        branch, platform, builder = _create_some_builder()
        build = _create_build(branch, platform, builder)
        self.assertThereIsNoInstanceOf(TestResult)
        result = TestResult.get_or_insert_from_parsed_json('some-test', build,
            {"avg": 40, "median": "40.1", "stdev": 3.25, "min": 30.5, "max": 45})
        self.assertOnlyInstance(result)
        self.assertEqual(result.name, 'some-test')
        self.assertEqual(result.build.key(), build.key())
        self.assertEqual(result.value, 40.0)
        self.assertEqual(result.valueMedian, 40.1)
        self.assertEqual(result.valueStdev, 3.25)
        self.assertEqual(result.valueMin, 30.5)
        self.assertEqual(result.valueMax, 45)

    def test_replace_to_change_test_name(self):
        branch, platform, builder = _create_some_builder()
        build = _create_build(branch, platform, builder)
        self.assertThereIsNoInstanceOf(TestResult)
        result = TestResult.get_or_insert_from_parsed_json('some-test', build, 50)
        self.assertOnlyInstance(result)
        self.assertEqual(result.name, 'some-test')

        new_result = result.replace_to_change_test_name('other-test')
        self.assertNotEqual(result, new_result)
        self.assertOnlyInstance(new_result)

        self.assertEqual(new_result.name, 'other-test')
        self.assertEqual(new_result.build.key(), result.build.key())
        self.assertEqual(new_result.value, result.value)
        self.assertEqual(new_result.valueMedian, None)
        self.assertEqual(new_result.valueStdev, None)
        self.assertEqual(new_result.valueMin, None)
        self.assertEqual(new_result.valueMax, None)

    def test_replace_to_change_test_name_with_stat_value(self):
        branch, platform, builder = _create_some_builder()
        build = _create_build(branch, platform, builder)
        self.assertThereIsNoInstanceOf(TestResult)
        result = TestResult.get_or_insert_from_parsed_json('some-test', build,
            {"avg": 40, "median": "40.1", "stdev": 3.25, "min": 30.5, "max": 45})
        self.assertOnlyInstance(result)
        self.assertEqual(result.name, 'some-test')

        new_result = result.replace_to_change_test_name('other-test')
        self.assertNotEqual(result, new_result)
        self.assertOnlyInstance(new_result)

        self.assertEqual(new_result.name, 'other-test')
        self.assertEqual(new_result.build.key(), result.build.key())
        self.assertEqual(new_result.value, result.value)
        self.assertEqual(result.value, 40.0)
        self.assertEqual(result.valueMedian, 40.1)
        self.assertEqual(result.valueStdev, 3.25)
        self.assertEqual(result.valueMin, 30.5)
        self.assertEqual(result.valueMax, 45)

    def test_replace_to_change_test_name_overrides_conflicting_result(self):
        branch, platform, builder = _create_some_builder()
        build = _create_build(branch, platform, builder)
        self.assertThereIsNoInstanceOf(TestResult)
        result = TestResult.get_or_insert_from_parsed_json('some-test', build, 20)
        self.assertOnlyInstance(result)

        conflicting_result = TestResult.get_or_insert_from_parsed_json('other-test', build, 10)

        new_result = result.replace_to_change_test_name('other-test')
        self.assertNotEqual(result, new_result)
        self.assertOnlyInstance(new_result)

        self.assertEqual(new_result.name, 'other-test')
        self.assertEqual(TestResult.get(conflicting_result.key()).value, 20)


class ReportLogTests(DataStoreTestsBase):
    def _create_log_with_payload(self, payload):
        return ReportLog(timestamp=datetime.now(), headers='some headers', payload=payload)

    def test_parsed_payload(self):
        log = self._create_log_with_payload('')
        self.assertFalse('_parsed' in log.__dict__)
        self.assertEqual(log._parsed_payload(), False)
        self.assertEqual(log._parsed, False)

        log = self._create_log_with_payload('{"key": "value", "another key": 1}')
        self.assertEqual(log._parsed_payload(), {"key": "value", "another key": 1})
        self.assertEqual(log._parsed, {"key": "value", "another key": 1})

    def test_get_value(self):
        log = self._create_log_with_payload('{"string": "value", "integer": 1, "float": 1.1}')
        self.assertEqual(log.get_value('string'), 'value')
        self.assertEqual(log.get_value('integer'), 1)
        self.assertEqual(log.get_value('float'), 1.1)
        self.assertEqual(log.get_value('bad'), None)

    def test_results(self):
        log = self._create_log_with_payload('{"results": 123}')
        self.assertEqual(log.results(), 123)

        log = self._create_log_with_payload('{"key": "value"}')
        self.assertEqual(log.results(), None)

    def test_results_are_well_formed(self):

        def assert_results_are_well_formed(json, expected):
            self.assertEqual(self._create_log_with_payload(json).results_are_well_formed(), expected)

        assert_results_are_well_formed('{"results": 123}', False)
        assert_results_are_well_formed('{"results": {"test": 123}}', True)
        assert_results_are_well_formed('{"results": {"test": 123, "other-test": 456}}', True)
        assert_results_are_well_formed('{"results": {"test": 123, "other-test": 456, "bad-test": "hi"}}', False)
        assert_results_are_well_formed('{"results": {"test": {"avg": 456}}}', True)
        assert_results_are_well_formed('{"results": {"test": {"avg": 456, "median": "hello"}}}', False)
        assert_results_are_well_formed('{"results": {"test": {"avg": 456, "median": 789}}}', True)
        assert_results_are_well_formed('{"results": {"test": {"avg": 456, "unit": "bytes"}}}', True)

    def test_builder(self):
        log = self._create_log_with_payload('{"key": "value"}')
        self.assertEqual(log.builder(), None)

        builder_name = "Chromium Mac Release (Perf)"
        log = self._create_log_with_payload('{"builder-name": "%s"}' % builder_name)
        self.assertEqual(log.builder(), None)

        builder_key = Builder.create(builder_name, 'some password')
        log = self._create_log_with_payload('{"builder-name": "%s"}' % builder_name)
        self.assertEqual(log.builder().key(), builder_key)

    def test_branch(self):
        log = self._create_log_with_payload('{"key": "value"}')
        self.assertEqual(log.branch(), None)

        log = self._create_log_with_payload('{"branch": "some-branch"}')
        self.assertEqual(log.branch(), None)

        branch = Branch.create_if_possible("some-branch", "Some Branch")
        log = self._create_log_with_payload('{"branch": "some-branch"}')
        self.assertEqual(log.branch().key(), branch.key())

    def test_platform(self):
        log = self._create_log_with_payload('{"key": "value"}')
        self.assertEqual(log.platform(), None)

        log = self._create_log_with_payload('{"platform": "some-platform"}')
        self.assertEqual(log.platform(), None)

        platform = Platform.create_if_possible("some-platform", "Some Platform")
        log = self._create_log_with_payload('{"platform": "some-platform"}')
        self.assertEqual(log.platform().key(), platform.key())

    def test_build_number(self):
        log = self._create_log_with_payload('{"build-number": 123}')
        self.assertEqual(log.build_number(), 123)

        log = self._create_log_with_payload('{"key": "value"}')
        self.assertEqual(log.build_number(), None)

    def test_webkit_revision(self):
        log = self._create_log_with_payload('{"key": "value"}')
        self.assertEqual(log.webkit_revision(), None)

        log = self._create_log_with_payload('{"webkit-revision": 123}')
        self.assertEqual(log.webkit_revision(), 123)

    def chromium_revision(self):
        log = self._create_log_with_payload('{"chromium-revision": 123}')
        self.assertEqual(log.webkit_revision(), 123)

        log = self._create_log_with_payload('{"key": "value"}')
        self.assertEqual(log.webkit_revision(), None)


class PersistentCacheTests(DataStoreTestsBase):
    def setUp(self):
        super(PersistentCacheTests, self).setUp()
        self.testbed.init_memcache_stub()

    def _assert_persistent_cache(self, name, value):
        self.assertEqual(PersistentCache.get_by_key_name(name).value, value)
        self.assertEqual(memcache.get(name), value)

    def test_set_cache(self):
        self.assertThereIsNoInstanceOf(PersistentCache)

        PersistentCache.set_cache('some-cache', 'some data')
        self._assert_persistent_cache('some-cache', 'some data')

        PersistentCache.set_cache('some-cache', 'some other data')

        self._assert_persistent_cache('some-cache', 'some other data')

    def test_get_cache(self):
        self.assertEqual(memcache.get('some-cache'), None)
        self.assertEqual(PersistentCache.get_cache('some-cache'), None)

        PersistentCache.set_cache('some-cache', 'some data')

        self.assertEqual(memcache.get('some-cache'), 'some data')
        self.assertEqual(PersistentCache.get_cache('some-cache'), 'some data')

        memcache.delete('some-cache')
        self.assertEqual(memcache.get('some-cache'), None)
        self.assertEqual(PersistentCache.get_cache('some-cache'), 'some data')


class RunsTest(DataStoreTestsBase):
    def setUp(self):
        super(RunsTest, self).setUp()
        self.testbed.init_memcache_stub()

    def _create_results(self, branch, platform, builder, test_name, values, timestamps=None, starting_revision=100):
        builds = []
        results = []
        for i, value in enumerate(values):
            build = Build(branch=branch, platform=platform, builder=builder,
                buildNumber=i, revision=starting_revision + i, timestamp=timestamps[i] if timestamps else datetime.now())
            build.put()
            result = TestResult(name=test_name, build=build, value=value)
            result.put()
            builds.append(build)
            results.append(result)
        return builds, results

    def test_generate_runs(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        some_builder = Builder.get(Builder.create('some-builder', 'Some Builder'))
        some_test = Test.update_or_insert('some-test', some_branch, some_platform)

        builds, results = self._create_results(some_branch, some_platform, some_builder, 'some-test', [50.0, 51.0, 52.0, 49.0, 48.0])
        last_i = 0
        for i, (build, result) in enumerate(Runs._generate_runs(some_branch, some_platform, some_test)):
            self.assertEqual(build.buildNumber, i)
            self.assertEqual(build.revision, 100 + i)
            self.assertEqual(result.name, 'some-test')
            self.assertEqual(result.value, results[i].value)
            last_i = i
        self.assertTrue(last_i + 1, len(results))

    def test_update_or_insert(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        some_builder = Builder.get(Builder.create('some-builder', 'Some Builder'))
        some_test = Test.update_or_insert('some-test', some_branch, some_platform)
        self.assertThereIsNoInstanceOf(Runs)

        runs = Runs.update_or_insert(some_branch, some_platform, some_test)
        self.assertOnlyInstance(runs)
        self.assertEqual(runs.json_runs, '')
        self.assertEqual(runs.json_averages, '')
        self.assertEqual(runs.json_min, None)
        self.assertEqual(runs.json_max, None)
        old_memcache_value = memcache.get(Runs._key_name(some_branch.id, some_platform.id, some_test.id))
        self.assertTrue(old_memcache_value)

        runs.delete()
        self.assertThereIsNoInstanceOf(Runs)

        builds, results = self._create_results(some_branch, some_platform, some_builder, 'some-test', [50.0])
        runs = Runs.update_or_insert(some_branch, some_platform, some_test)
        self.assertOnlyInstance(runs)
        self.assertTrue(runs.json_runs.startswith('[5, [4, 0, 100, null],'))
        self.assertEqual(json.loads('{' + runs.json_averages + '}'), {"100": 50.0})
        self.assertEqual(runs.json_min, 50.0)
        self.assertEqual(runs.json_max, 50.0)
        self.assertNotEqual(memcache.get(Runs._key_name(some_branch.id, some_platform.id, some_test.id)), old_memcache_value)

    def test_update_incrementally(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        some_builder = Builder.get(Builder.create('some-builder', 'Some Builder'))
        some_test = Test.update_or_insert('some-test', some_branch, some_platform)
        self.assertThereIsNoInstanceOf(Runs)

        timestamps = [datetime.now(), datetime.now()]
        builds, results = self._create_results(some_branch, some_platform, some_builder, 'some-test', [50.0, 52.0], timestamps)
        runs = Runs.update_or_insert(some_branch, some_platform, some_test)
        self.assertOnlyInstance(runs)
        self.assertEqual(json.loads('[' + runs.json_runs + ']'),
            [[5, [4, 0, 100, None], mktime(timestamps[0].timetuple()), 50.0, 0, [], None, None],
            [7, [6, 1, 101, None], mktime(timestamps[1].timetuple()), 52.0, 0, [], None, None]])
        self.assertEqual(json.loads('{' + runs.json_averages + '}'), {"100": 50.0, "101": 52.0})
        self.assertEqual(runs.json_min, 50.0)
        self.assertEqual(runs.json_max, 52.0)

        timestamps.append(datetime.now())
        builds, results = self._create_results(some_branch, some_platform, some_builder, 'some-test', [48.0],
            timestamps[2:], starting_revision=102)
        runs.update_incrementally(builds[0], results[0])

        self.assertOnlyInstance(runs)
        self.assertEqual(json.loads('[' + runs.json_runs + ']'),
            [[5, [4, 0, 100, None], mktime(timestamps[0].timetuple()), 50.0, 0, [], None, None],
            [7, [6, 1, 101, None], mktime(timestamps[1].timetuple()), 52.0, 0, [], None, None],
            [9, [8, 0, 102, None], mktime(timestamps[2].timetuple()), 48.0, 0, [], None, None]])
        self.assertEqual(json.loads('{' + runs.json_averages + '}'), {"100": 50.0, "101": 52.0, "102": 48.0})
        self.assertEqual(runs.json_min, 48.0)
        self.assertEqual(runs.json_max, 52.0)

    def test_json_by_ids(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        some_builder = Builder.get(Builder.create('some-builder', 'Some Builder'))
        some_test = Test.update_or_insert('some-test', some_branch, some_platform)

        self._create_results(some_branch, some_platform, some_builder, 'some-test', [50.0])
        runs = Runs.update_or_insert(some_branch, some_platform, some_test)
        runs_json = runs.to_json()

        key_name = Runs._key_name(some_branch.id, some_platform.id, some_test.id)
        self.assertEqual(Runs.json_by_ids(some_branch.id, some_platform.id, some_test.id), runs_json)
        self.assertEqual(memcache.get(key_name), runs_json)

        memcache.set(key_name, 'blah')
        self.assertEqual(Runs.json_by_ids(some_branch.id, some_platform.id, some_test.id), 'blah')

        memcache.delete(key_name)
        self.assertEqual(Runs.json_by_ids(some_branch.id, some_platform.id, some_test.id), runs_json)
        self.assertEqual(memcache.get(key_name), runs_json)

    def test_to_json_without_results(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        some_test = Test.update_or_insert('some-test', some_branch, some_platform)
        self.assertOnlyInstance(some_test)
        self.assertThereIsNoInstanceOf(TestResult)
        self.assertEqual(json.loads(Runs.update_or_insert(some_branch, some_platform, some_test).to_json()), {
            'test_runs': [],
            'averages': {},
            'min': None,
            'max': None,
            'unit': None,
            'date_range': None,
            'stat': 'ok'})

    def test_to_json_with_results(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        some_builder = Builder.get(Builder.create('some-builder', 'Some Builder'))
        some_test = Test.update_or_insert('some-test', some_branch, some_platform)
        builds, results = self._create_results(some_branch, some_platform, some_builder, 'some-test', [50.0, 51.0, 52.0, 49.0, 48.0])

        value = json.loads(Runs.update_or_insert(some_branch, some_platform, some_test).to_json())
        self.assertEqualUnorderedList(value.keys(), ['test_runs', 'averages', 'min', 'max', 'unit', 'date_range', 'stat'])
        self.assertEqual(value['stat'], 'ok')
        self.assertEqual(value['min'], 48.0)
        self.assertEqual(value['max'], 52.0)
        self.assertEqual(value['unit'], None)
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

    def test_to_json_with_unit(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        some_builder = Builder.get(Builder.create('some-builder', 'Some Builder'))
        some_test = Test.update_or_insert('some-test', some_branch, some_platform, 'runs/s')
        builds, results = self._create_results(some_branch, some_platform, some_builder, 'some-test', [50.0, 51.0, 52.0, 49.0, 48.0])

        value = json.loads(Runs.update_or_insert(some_branch, some_platform, some_test).to_json())
        self.assertEqual(value['unit'], 'runs/s')

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
        self._assert_entry(Runs._entry_from_build_and_result(build, result), build, result, 123.0)

        build = create_build(2, 102)
        result = TestResult(name=test_name, value=456.0, valueMedian=789.0, build=build)
        result.put()
        self._assert_entry(Runs._entry_from_build_and_result(build, result), build, result, 456.0)

        result.valueStdev = 7.0
        result.put()
        self._assert_entry(Runs._entry_from_build_and_result(build, result), build, result, 456.0)

        result.valueStdev = None
        result.valueMin = 123.0
        result.valueMax = 789.0
        result.put()
        self._assert_entry(Runs._entry_from_build_and_result(build, result), build, result, 456.0)

        result.valueStdev = 8.0
        result.valueMin = 123.0
        result.valueMax = 789.0
        result.put()
        self._assert_entry(Runs._entry_from_build_and_result(build, result), build, result, 456.0,
            statistics={'stdev': 8.0, 'min': 123.0, 'max': 789.0})

        result.valueMedian = 345.0  # Median is never used by the frontend.
        result.valueStdev = 8.0
        result.valueMin = 123.0
        result.valueMax = 789.0
        result.put()
        self._assert_entry(Runs._entry_from_build_and_result(build, result), build, result, 456.0,
            statistics={'stdev': 8.0, 'min': 123.0, 'max': 789.0})

    def test_chart_params_with_value(self):
        some_branch = Branch.create_if_possible('some-branch', 'Some Branch')
        some_platform = Platform.create_if_possible('some-platform', 'Some Platform')
        some_builder = Builder.get(Builder.create('some-builder', 'Some Builder'))
        some_test = Test.update_or_insert('some-test', some_branch, some_platform)

        start_time = datetime(2011, 2, 21, 12, 0, 0)
        end_time = datetime(2011, 2, 28, 12, 0, 0)
        results = self._create_results(some_branch, some_platform, some_builder, 'some-test',
            [50.0, 51.0, 52.0, 49.0, 48.0, 51.9, 50.7, 51.1],
            [start_time + timedelta(day) for day in range(0, 8)])

        # Use int despite of its impreciseness since tests may fail due to rounding errors otherwise.
        def split_as_int(string):
            return [int(float(value)) for value in string.split(',')]

        params = Runs.update_or_insert(some_branch, some_platform, some_test).chart_params(7)
        self.assertEqual(params['chxl'], '0:|Feb 21|Feb 22|Feb 23|Feb 24|Feb 25|Feb 26|Feb 27|Feb 28')
        self.assertEqual(split_as_int(params['chxr']), [1, 0, 57, int(52 * 1.1 / 5 + 0.5)])
        x_min, x_max, y_min, y_max = split_as_int(params['chds'])
        self.assertEqual(datetime.fromtimestamp(x_min), start_time)
        self.assertEqual(datetime.fromtimestamp(x_max), end_time)
        self.assertEqual(y_min, 0)
        self.assertEqual(y_max, int(52 * 1.1))
        self.assertEqual(split_as_int(params['chg']), [int(100 / 7), 20, 0, 0])

        params = Runs.update_or_insert(some_branch, some_platform, some_test).chart_params(14)
        self.assertEqual(params['chxl'], '0:|Feb 14|Feb 16|Feb 18|Feb 20|Feb 22|Feb 24|Feb 26|Feb 28')
        self.assertEqual(split_as_int(params['chxr']), [1, 0, 57, int(52 * 1.1 / 5 + 0.5)])
        x_min, x_max, y_min, y_max = split_as_int(params['chds'])
        self.assertEqual(datetime.fromtimestamp(x_min), datetime(2011, 2, 14, 12, 0, 0))
        self.assertEqual(datetime.fromtimestamp(x_max), end_time)
        self.assertEqual(y_min, 0)
        self.assertEqual(y_max, int(52 * 1.1))
        self.assertEqual(split_as_int(params['chg']), [int(100 / 7), 20, 0, 0])


class DashboardImageTests(DataStoreTestsBase):
    def setUp(self):
        super(DashboardImageTests, self).setUp()
        self.testbed.init_memcache_stub()

    def test_create(self):
        self.assertEqual(memcache.get('dashboard-image:1:2:3:7'), None)
        self.assertThereIsNoInstanceOf(DashboardImage)
        image = DashboardImage.create(1, 2, 3, 7, 'blah')
        self.assertOnlyInstance(image)
        self.assertEqual(memcache.get('dashboard-image:1:2:3:7'), 'blah')

    def test_get(self):
        image = DashboardImage.create(1, 2, 3, 7, 'blah')
        self.assertEqual(memcache.get('dashboard-image:1:2:3:7'), 'blah')
        memcache.set('dashboard-image:1:2:3:7', 'new value')

        # Check twice to make sure the first call doesn't clear memcache
        self.assertEqual(DashboardImage.get_image(1, 2, 3, 7), 'new value')
        self.assertEqual(DashboardImage.get_image(1, 2, 3, 7), 'new value')

        memcache.delete('dashboard-image:1:2:3:7')
        self.assertEqual(memcache.get('dashboard-image:1:2:3:7'), None)
        self.assertEqual(DashboardImage.get_image(1, 2, 3, 7), 'blah')
        self.assertEqual(memcache.get('dashboard-image:1:2:3:7'), 'blah')

    def test_needs_update(self):
        self.assertTrue(DashboardImage.needs_update(1, 2, 3, 7))
        self.assertTrue(DashboardImage.needs_update(1, 2, 3, 30))
        self.assertTrue(DashboardImage.needs_update(1, 2, 3, 60))
        self.assertTrue(DashboardImage.needs_update(1, 2, 3, 365))

        image = DashboardImage(key_name=DashboardImage.key_name(1, 2, 3, 7), image='blah')
        image.put()
        self.assertOnlyInstance(image)
        self.assertTrue(DashboardImage.needs_update(1, 2, 3, 7))

        DashboardImage(key_name=DashboardImage.key_name(1, 2, 3, 30), image='blah').put()
        self.assertFalse(DashboardImage.needs_update(1, 2, 3, 30, datetime.now() + timedelta(0, 10)))

        DashboardImage(key_name=DashboardImage.key_name(1, 2, 4, 30), image='blah').put()
        self.assertTrue(DashboardImage.needs_update(1, 2, 4, 30, datetime.now() + timedelta(1)))

        DashboardImage(key_name=DashboardImage.key_name(1, 2, 3, 90), image='blah').put()
        self.assertFalse(DashboardImage.needs_update(1, 2, 3, 90, datetime.now() + timedelta(0, 20)))

        DashboardImage(key_name=DashboardImage.key_name(1, 2, 4, 90), image='blah').put()
        self.assertTrue(DashboardImage.needs_update(1, 2, 4, 90, datetime.now() + timedelta(1)))

        DashboardImage(key_name=DashboardImage.key_name(1, 2, 3, 365), image='blah').put()
        self.assertFalse(DashboardImage.needs_update(1, 2, 3, 365, datetime.now() + timedelta(1)))

        DashboardImage(key_name=DashboardImage.key_name(1, 2, 4, 365), image='blah').put()
        self.assertTrue(DashboardImage.needs_update(1, 2, 4, 365, datetime.now() + timedelta(10)))


if __name__ == '__main__':
    unittest.main()
