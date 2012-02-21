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

import models
import unittest

from datetime import datetime
from google.appengine.api import memcache
from google.appengine.ext import testbed
from time import mktime


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
        self.assertEqual(len(only_instasnce.__class__.all().fetch(5)), 1)
        self.assertTrue(only_instasnce.__class__.get(only_instasnce.key()))

    def assertEqualUnorderedList(self, list1, list2):
        self.assertEqual(set(list1), set(list2))


class HelperTests(DataStoreTestsBase):
    def _assert_there_is_exactly_one_id_holder_and_matches(self, id):
        id_holders = models.NumericIdHolder.all().fetch(5)
        self.assertEqual(len(id_holders), 1)
        self.assertTrue(id_holders[0])
        self.assertEqual(id_holders[0].key().id(), id)

    def test_create_in_transaction_with_numeric_id_holder(self):

        def execute(id):
            return models.Branch(id=id, name='some branch', key_name='some-branch').put()

        self.assertThereIsNoInstanceOf(models.Branch)
        self.assertThereIsNoInstanceOf(models.NumericIdHolder)

        self.assertTrue(models.create_in_transaction_with_numeric_id_holder(execute))

        branches = models.Branch.all().fetch(5)
        self.assertEqual(len(branches), 1)
        self.assertEqual(branches[0].name, 'some branch')
        self.assertEqual(branches[0].key().name(), 'some-branch')

        self._assert_there_is_exactly_one_id_holder_and_matches(branches[0].id)

    def test_failing_in_create_in_transaction_with_numeric_id_holder(self):

        def execute(id):
            return None

        self.assertThereIsNoInstanceOf(models.Branch)
        self.assertThereIsNoInstanceOf(models.NumericIdHolder)

        self.assertFalse(models.create_in_transaction_with_numeric_id_holder(execute))

        self.assertThereIsNoInstanceOf(models.Branch)
        self.assertThereIsNoInstanceOf(models.NumericIdHolder)

    def test_raising_in_create_in_transaction_with_numeric_id_holder(self):

        def execute(id):
            raise TypeError
            return None

        self.assertThereIsNoInstanceOf(models.Branch)
        self.assertThereIsNoInstanceOf(models.NumericIdHolder)

        self.assertRaises(TypeError, models.create_in_transaction_with_numeric_id_holder, (execute))

        self.assertThereIsNoInstanceOf(models.Branch)
        self.assertThereIsNoInstanceOf(models.NumericIdHolder)

    def test_delete_model_with_numeric_id_holder(self):

        def execute(id):
            return models.Branch(id=id, name='some branch', key_name='some-branch').put()

        branch = models.Branch.get(models.create_in_transaction_with_numeric_id_holder(execute))
        self.assertOnlyInstance(branch)

        models.delete_model_with_numeric_id_holder(branch)

        self.assertThereIsNoInstanceOf(models.Branch)
        self.assertThereIsNoInstanceOf(models.NumericIdHolder)

    def test_model_from_numeric_id(self):

        def execute(id):
            return models.Branch(id=id, name='some branch', key_name='some-branch').put()

        branch = models.Branch.get(models.create_in_transaction_with_numeric_id_holder(execute))

        self.assertEqual(models.model_from_numeric_id(branch.id, models.Branch).key(), branch.key())
        self.assertEqual(models.model_from_numeric_id(branch.id + 1, models.Branch), None)
        models.delete_model_with_numeric_id_holder(branch)
        self.assertEqual(models.model_from_numeric_id(branch.id, models.Branch), None)


class BranchTests(DataStoreTestsBase):
    def test_create_if_possible(self):
        self.assertThereIsNoInstanceOf(models.Branch)

        branch = models.Branch.create_if_possible('some-branch', 'some branch')
        self.assertTrue(branch)
        self.assertTrue(branch.key().name(), 'some-branch')
        self.assertTrue(branch.name, 'some branch')
        self.assertOnlyInstance(branch)

        self.assertFalse(models.Branch.create_if_possible('some-branch', 'some other branch'))
        self.assertTrue(branch.name, 'some branch')
        self.assertOnlyInstance(branch)


class PlatformTests(DataStoreTestsBase):
    def test_create_if_possible(self):
        self.assertThereIsNoInstanceOf(models.Platform)

        platform = models.Platform.create_if_possible('some-platform', 'some platform')
        self.assertTrue(platform)
        self.assertTrue(platform.key().name(), 'some-platform')
        self.assertTrue(platform.name, 'some platform')
        self.assertOnlyInstance(platform)

        self.assertFalse(models.Platform.create_if_possible('some-platform', 'some other platform'))
        self.assertTrue(platform.name, 'some platform')
        self.assertOnlyInstance(platform)


class BuilderTests(DataStoreTestsBase):
    def test_create(self):
        builder_key = models.Builder.create('some builder', 'some password')
        self.assertTrue(builder_key)
        builder = models.Builder.get(builder_key)
        self.assertEqual(builder.key().name(), 'some builder')
        self.assertEqual(builder.name, 'some builder')
        self.assertEqual(builder.password, models.Builder._hashed_password('some password'))

    def test_update_password(self):
        builder = models.Builder.get(models.Builder.create('some builder', 'some password'))
        self.assertEqual(builder.password, models.Builder._hashed_password('some password'))
        builder.update_password('other password')
        self.assertEqual(builder.password, models.Builder._hashed_password('other password'))

        # Make sure it's saved
        builder = models.Builder.get(builder.key())
        self.assertEqual(builder.password, models.Builder._hashed_password('other password'))

    def test_hashed_password(self):
        self.assertNotEqual(models.Builder._hashed_password('some password'), 'some password')
        self.assertFalse('some password' in models.Builder._hashed_password('some password'))
        self.assertEqual(len(models.Builder._hashed_password('some password')), 64)

    def test_authenticate(self):
        builder = models.Builder.get(models.Builder.create('some builder', 'some password'))
        self.assertTrue(builder.authenticate('some password'))
        self.assertFalse(builder.authenticate('bad password'))


def _create_some_builder():
    branch = models.Branch.create_if_possible('some-branch', 'Some Branch')
    platform = models.Platform.create_if_possible('some-platform', 'Some Platform')
    builder_key = models.Builder.create('some-builder', 'Some Builder')
    return branch, platform, models.Builder.get(builder_key)


class BuildTests(DataStoreTestsBase):
    def test_get_or_insert_from_log(self):
        branch, platform, builder = _create_some_builder()

        timestamp = datetime.now().replace(microsecond=0)
        log = models.ReportLog(timestamp=timestamp, headers='some headers',
            payload='{"branch": "some-branch", "platform": "some-platform", "builder-name": "some-builder",' +
                '"build-number": 123, "webkit-revision": 456, "timestamp": %d}' % int(mktime(timestamp.timetuple())))

        self.assertThereIsNoInstanceOf(models.Build)

        build = models.Build.get_or_insert_from_log(log)
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
        branch = models.Branch.create_if_possible('some-branch', 'Some Branch')
        platform = models.Platform.create_if_possible('some-platform', 'Some Platform')

        self.assertThereIsNoInstanceOf(models.Test)

        test = models.Test.update_or_insert('some-test', branch, platform)
        self.assertTrue(test)
        self.assertEqual(test.branches, [branch.key()])
        self.assertEqual(test.platforms, [platform.key()])
        self.assertOnlyInstance(test)

    def test_update_or_insert_to_update(self):
        branch = models.Branch.create_if_possible('some-branch', 'Some Branch')
        platform = models.Platform.create_if_possible('some-platform', 'Some Platform')
        test = models.Test.update_or_insert('some-test', branch, platform)
        self.assertOnlyInstance(test)

        other_branch = models.Branch.create_if_possible('other-branch', 'Other Branch')
        other_platform = models.Platform.create_if_possible('other-platform', 'Other Platform')
        test = models.Test.update_or_insert('some-test', other_branch, other_platform)
        self.assertOnlyInstance(test)
        self.assertEqualUnorderedList(test.branches, [branch.key(), other_branch.key()])
        self.assertEqualUnorderedList(test.platforms, [platform.key(), other_platform.key()])


class TestResultTests(DataStoreTestsBase):
    def _create_build(self):
        branch, platform, builder = _create_some_builder()
        build_key = models.Build(key_name='some-build', branch=branch, platform=platform, builder=builder,
            buildNumber=1, revision=100, timestamp=datetime.now()).put()
        return models.Build.get(build_key)

    def test_get_or_insert_value(self):
        build = self._create_build()
        self.assertThereIsNoInstanceOf(models.TestResult)
        result = models.TestResult.get_or_insert_from_parsed_json('some-test', build, 50)
        self.assertOnlyInstance(result)
        self.assertEqual(result.name, 'some-test')
        self.assertEqual(result.build.key(), build.key())
        self.assertEqual(result.value, 50.0)
        self.assertEqual(result.valueMedian, None)
        self.assertEqual(result.valueStdev, None)
        self.assertEqual(result.valueMin, None)
        self.assertEqual(result.valueMax, None)

    def test_get_or_insert_stat_value(self):
        build = self._create_build()
        self.assertThereIsNoInstanceOf(models.TestResult)
        result = models.TestResult.get_or_insert_from_parsed_json('some-test', build,
            {"avg": 40, "median": "40.1", "stdev": 3.25, "min": 30.5, "max": 45})
        self.assertOnlyInstance(result)
        self.assertEqual(result.name, 'some-test')
        self.assertEqual(result.build.key(), build.key())
        self.assertEqual(result.value, 40.0)
        self.assertEqual(result.valueMedian, 40.1)
        self.assertEqual(result.valueStdev, 3.25)
        self.assertEqual(result.valueMin, 30.5)
        self.assertEqual(result.valueMax, 45)

    def _create_results(self, test_name, values):
        branch, platform, builder = _create_some_builder()
        results = []
        for i, value in enumerate(values):
            build = models.Build(branch=branch, platform=platform, builder=builder,
                buildNumber=i, revision=100 + i, timestamp=datetime.now())
            build.put()
            result = models.TestResult(name=test_name, build=build, value=value)
            result.put()
            results.append(result)
        return branch, platform, results

    def test_generate_runs(self):
        branch, platform, results = self._create_results('some-test', [50.0, 51.0, 52.0, 49.0, 48.0])
        last_i = 0
        for i, (build, result) in enumerate(models.TestResult.generate_runs(branch, platform, "some-test")):
            self.assertEqual(build.buildNumber, i)
            self.assertEqual(build.revision, 100 + i)
            self.assertEqual(result.name, 'some-test')
            self.assertEqual(result.value, results[i].value)
            last_i = i
        self.assertTrue(last_i + 1, len(results))


class ReportLogTests(DataStoreTestsBase):
    def _create_log_with_payload(self, payload):
        return models.ReportLog(timestamp=datetime.now(), headers='some headers', payload=payload)

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

    def test_builder(self):
        log = self._create_log_with_payload('{"key": "value"}')
        self.assertEqual(log.builder(), None)

        builder_name = "Chromium Mac Release (Perf)"
        log = self._create_log_with_payload('{"builder-name": "%s"}' % builder_name)
        self.assertEqual(log.builder(), None)

        builder_key = models.Builder.create(builder_name, 'some password')
        log = self._create_log_with_payload('{"builder-name": "%s"}' % builder_name)
        self.assertEqual(log.builder().key(), builder_key)

    def test_branch(self):
        log = self._create_log_with_payload('{"key": "value"}')
        self.assertEqual(log.branch(), None)

        log = self._create_log_with_payload('{"branch": "some-branch"}')
        self.assertEqual(log.branch(), None)

        branch = models.Branch.create_if_possible("some-branch", "Some Branch")
        log = self._create_log_with_payload('{"branch": "some-branch"}')
        self.assertEqual(log.branch().key(), branch.key())

    def test_platform(self):
        log = self._create_log_with_payload('{"key": "value"}')
        self.assertEqual(log.platform(), None)

        log = self._create_log_with_payload('{"platform": "some-platform"}')
        self.assertEqual(log.platform(), None)

        platform = models.Platform.create_if_possible("some-platform", "Some Platform")
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
        self.testbed = testbed.Testbed()
        self.testbed.activate()
        self.testbed.init_datastore_v3_stub()
        self.testbed.init_memcache_stub()

    def _assert_persistent_cache(self, name, value):
        self.assertEqual(models.PersistentCache.get_by_key_name(name).value, value)
        self.assertEqual(memcache.get(name), value)

    def test_set(self):
        self.assertThereIsNoInstanceOf(models.PersistentCache)

        models.PersistentCache.set_cache('some-cache', 'some data')
        self._assert_persistent_cache('some-cache', 'some data')

        models.PersistentCache.set_cache('some-cache', 'some other data')

        self._assert_persistent_cache('some-cache', 'some other data')

    def test_get(self):
        self.assertEqual(memcache.get('some-cache'), None)
        self.assertEqual(models.PersistentCache.get_cache('some-cache'), None)

        models.PersistentCache.set_cache('some-cache', 'some data')

        self.assertEqual(memcache.get('some-cache'), 'some data')
        self.assertEqual(models.PersistentCache.get_cache('some-cache'), 'some data')

        memcache.delete('some-cache')
        self.assertEqual(memcache.get('some-cache'), None)
        self.assertEqual(models.PersistentCache.get_cache('some-cache'), 'some data')


if __name__ == '__main__':
    unittest.main()
