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


class HelperTests(unittest.TestCase):
    def setUp(self):
        self.testbed = testbed.Testbed()
        self.testbed.activate()
        self.testbed.init_datastore_v3_stub()

    def tearDown(self):
        self.testbed.deactivate()

    def _assert_there_is_exactly_one_id_holder_and_matches(self, id):
        id_holders = models.NumericIdHolder.all().fetch(5)
        self.assertEqual(len(id_holders), 1)
        self.assertTrue(id_holders[0])
        self.assertEqual(id_holders[0].key().id(), id)

    def test_create_in_transaction_with_numeric_id_holder(self):

        def execute(id):
            return models.Branch(id=id, name='some branch', key_name='some-branch').put()

        self.assertEqual(len(models.Branch.all().fetch(5)), 0)
        self.assertEqual(len(models.NumericIdHolder.all().fetch(5)), 0)

        self.assertTrue(models.create_in_transaction_with_numeric_id_holder(execute))

        branches = models.Branch.all().fetch(5)
        self.assertEqual(len(branches), 1)
        self.assertEqual(branches[0].name, 'some branch')
        self.assertEqual(branches[0].key().name(), 'some-branch')

        self._assert_there_is_exactly_one_id_holder_and_matches(branches[0].id)

    def test_failing_in_create_in_transaction_with_numeric_id_holder(self):

        def execute(id):
            return None

        self.assertEqual(len(models.Branch.all().fetch(5)), 0)
        self.assertEqual(len(models.NumericIdHolder.all().fetch(5)), 0)

        self.assertFalse(models.create_in_transaction_with_numeric_id_holder(execute))

        self.assertEqual(len(models.Branch.all().fetch(5)), 0)
        self.assertEqual(len(models.NumericIdHolder.all().fetch(5)), 0)

    def test_raising_in_create_in_transaction_with_numeric_id_holder(self):

        def execute(id):
            raise TypeError
            return None

        self.assertEqual(len(models.Branch.all().fetch(5)), 0)
        self.assertEqual(len(models.NumericIdHolder.all().fetch(5)), 0)

        self.assertRaises(TypeError, models.create_in_transaction_with_numeric_id_holder, (execute))

        self.assertEqual(len(models.Branch.all().fetch(5)), 0)
        self.assertEqual(len(models.NumericIdHolder.all().fetch(5)), 0)

    def test_delete_model_with_numeric_id_holder(self):

        def execute(id):
            return models.Branch(id=id, name='some branch', key_name='some-branch').put()

        branch = models.Branch.get(models.create_in_transaction_with_numeric_id_holder(execute))
        self.assertEqual(len(models.NumericIdHolder.all().fetch(5)), 1)

        models.delete_model_with_numeric_id_holder(branch)

        self.assertEqual(len(models.Branch.all().fetch(5)), 0)
        self.assertEqual(len(models.NumericIdHolder.all().fetch(5)), 0)

    def test_model_from_numeric_id(self):

        def execute(id):
            return models.Branch(id=id, name='some branch', key_name='some-branch').put()

        branch = models.Branch.get(models.create_in_transaction_with_numeric_id_holder(execute))

        self.assertEqual(models.model_from_numeric_id(branch.id, models.Branch).key(), branch.key())
        self.assertEqual(models.model_from_numeric_id(branch.id + 1, models.Branch), None)
        models.delete_model_with_numeric_id_holder(branch)
        self.assertEqual(models.model_from_numeric_id(branch.id, models.Branch), None)


class BuilderTests(unittest.TestCase):
    def setUp(self):
        self.testbed = testbed.Testbed()
        self.testbed.activate()
        self.testbed.init_datastore_v3_stub()

    def tearDown(self):
        self.testbed.deactivate()

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


class ReportLog(unittest.TestCase):
    def setUp(self):
        self.testbed = testbed.Testbed()
        self.testbed.activate()
        self.testbed.init_datastore_v3_stub()

    def tearDown(self):
        self.testbed.deactivate()

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

    # FIXME test_branch and test_platform

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


class PersistentCacheTests(unittest.TestCase):
    def setUp(self):
        self.testbed = testbed.Testbed()
        self.testbed.activate()
        self.testbed.init_datastore_v3_stub()
        self.testbed.init_memcache_stub()

    def tearDown(self):
        self.testbed.deactivate()

    def _assert_persistent_cache(self, name, value):
        self.assertEqual(models.PersistentCache.get_by_key_name(name).value, value)
        self.assertEqual(memcache.get(name), value)

    def test_set(self):
        self.assertEqual(len(models.PersistentCache.all().fetch(5)), 0)

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
