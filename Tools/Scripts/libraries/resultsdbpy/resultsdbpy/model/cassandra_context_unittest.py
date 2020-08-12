# Copyright (C) 2019 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import time

from cassandra.cqlengine import columns
from cassandra.cqlengine.models import Model
from resultsdbpy.model.cassandra_context import CassandraContext, RegexCluster
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class CassandraTest(WaitForDockerTestCase):

    REAL_CASSANDRA_REQUIRED = 'Real Cassandra instance required'
    KEYSPACE = 'keyspace_for_testing'
    TABLE = 'example_table'

    def init_database(self, cassandra=CassandraContext):
        cassandra.drop_keyspace(keyspace=self.KEYSPACE)
        self.database = cassandra(keyspace=self.KEYSPACE, create_keyspace=True)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_keyspace_creation_failure(self, cassandra=CassandraContext):
        with self.assertRaises(Exception):
            cassandra(keyspace='does_not_exist')

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_session_assertion(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.assertRaises(AssertionError):
            self.database.assert_connected()

    def create_testing_table(self):
        with self.database:

            class ExampleTable(Model):
                __table_name__ = self.TABLE
                value1 = columns.Text(partition_key=True)
                value2 = columns.Integer(primary_key=True)
                value3 = columns.Integer()

            self.database.create_table(ExampleTable)
            return ExampleTable

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_create_table(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:
            example_table = self.create_testing_table()
            self.assertTrue(self.database.does_table_model_match_schema(example_table))

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_existing_table_more_columns(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:
            self.create_testing_table()
            with self.assertRaises(CassandraContext.SchemaException):

                class ExampleTable(Model):
                    __table_name__ = self.TABLE
                    value1 = columns.Text(partition_key=True)
                    value2 = columns.Integer(primary_key=True)
                    value3 = columns.Integer()
                    value4 = columns.Text()

                self.database.create_table(ExampleTable)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_existing_table_different_columns(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:
            self.create_testing_table()
            with self.assertRaises(CassandraContext.SchemaException):

                class ExampleTable(Model):
                    __table_name__ = self.TABLE
                    value1 = columns.Text(partition_key=True)
                    value2 = columns.Integer(primary_key=True)
                    value3 = columns.Text()

                self.database.create_table(ExampleTable)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_existing_table_different_primary_key(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:
            self.create_testing_table()
            with self.assertRaises(CassandraContext.SchemaException):

                class ExampleTable(Model):
                    __table_name__ = self.TABLE
                    value1 = columns.Text(partition_key=True)
                    value2 = columns.Integer()
                    value3 = columns.Integer(primary_key=True)

                self.database.create_table(ExampleTable)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_existing_table_different_clustering_order(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:
            self.create_testing_table()
            with self.assertRaises(CassandraContext.SchemaException):

                class ExampleTable(Model):
                    __table_name__ = self.TABLE
                    value1 = columns.Text(partition_key=True)
                    value2 = columns.Integer(primary_key=True, clustering_order='DESC')
                    value3 = columns.Integer()

                self.database.create_table(ExampleTable)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_existing_table_column_order(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:

            class ExampleTable(Model):
                __table_name__ = self.TABLE
                value1 = columns.Text(partition_key=True)
                value2 = columns.Integer(primary_key=True)
                value3 = columns.Text(primary_key=True)
                value4 = columns.Integer()

            self.database.create_table(ExampleTable)

            with self.assertRaises(CassandraContext.SchemaException):
                class ExampleTable(Model):
                    __table_name__ = self.TABLE
                    value1 = columns.Text(partition_key=True)
                    value3 = columns.Text(primary_key=True)
                    value2 = columns.Integer(primary_key=True)
                    value4 = columns.Integer()

                self.database.create_table(ExampleTable)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_insert_rows(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:

            class ExampleTable(Model):
                __table_name__ = self.TABLE
                value1 = columns.Text(partition_key=True)
                value2 = columns.Integer(primary_key=True)
                value3 = columns.Text()

            self.database.create_table(ExampleTable)

            for i in range(10):
                for j in range(10):
                    self.database.insert_row(self.TABLE, value1=f'key{i}', value2=j, value3=f'value{i}-{j}')

            for i in range(10):
                for j in range(10):
                    result = self.database.select_from_table(self.TABLE, value1=f'key{i}', value2=j)
                    self.assertEqual(1, len(result))
                    self.assertEqual(result[0], ExampleTable(value1=f'key{i}', value2=j, value3=f'value{i}-{j}'))

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_duplicate_row(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:

            class ExampleTable(Model):
                __table_name__ = self.TABLE
                value1 = columns.Text(partition_key=True)
                value2 = columns.Integer(primary_key=True)
                value3 = columns.Text()

            self.database.create_table(ExampleTable)

            self.database.insert_row(self.TABLE, value1='key', value2=1, value3='duplicate-1')
            self.database.insert_row(self.TABLE, value1='key', value2=1, value3='duplicate-2')

            result = self.database.select_from_table(self.TABLE, value1='key', value2=1)
            self.assertEqual(1, len(result))
            self.assertEqual(result[0], ExampleTable(value1='key', value2=1, value3='duplicate-2'))

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_search_by_other_value(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:

            class ExampleTable(Model):
                __table_name__ = self.TABLE
                value1 = columns.Text(partition_key=True)
                value2 = RegexCluster(primary_key=True)
                value3 = columns.Integer()

            self.database.create_table(ExampleTable)

            self.database.insert_row(self.TABLE, value1='key', value2='value 1', value3=1)
            self.database.insert_row(self.TABLE, value1='key', value2='other 1', value3=2)
            self.database.insert_row(self.TABLE, value1='key', value2='other 2', value3=3)

            result = self.database.select_from_table(self.TABLE, value2__like='value%', value1='key')
            self.assertEqual(1, len(result))
            self.assertEqual(result[0], ExampleTable(value1='key', value2='value 1', value3=1))

            result = self.database.select_from_table(self.TABLE, value2__like='other%', value1='key')
            self.assertEqual(2, len(result))

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_search_with_multiple_keys(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:

            class ExampleTable(Model):
                __table_name__ = self.TABLE
                value1 = columns.Text(partition_key=True)
                value2 = columns.Text(primary_key=True)
                value3 = columns.Integer()

            self.database.create_table(ExampleTable)

            self.database.insert_row(self.TABLE, value1='key1', value2='value1', value3=1)
            self.database.insert_row(self.TABLE, value1='key2', value2='value1', value3=2)
            self.database.insert_row(self.TABLE, value1='key3', value2='value1', value3=3)

            result = self.database.select_from_table(self.TABLE, value1__in=['key1', 'key2'], value2='value1')
            self.assertEqual(2, len(result))
            self.assertIn(ExampleTable(value1='key1', value2='value1', value3=1), result)
            self.assertIn(ExampleTable(value1='key2', value2='value1', value3=2), result)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    @WaitForDockerTestCase.run_if_slow()
    def test_time_to_live(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:

            class ExampleTable(Model):
                __table_name__ = self.TABLE
                value1 = columns.Text(partition_key=True)
                value2 = columns.Integer(primary_key=True)
                value3 = columns.Text()

            self.database.create_table(ExampleTable)

            self.database.insert_row(self.TABLE, value1='key', value2=1, value3='value-to-kill', ttl=1)
            self.assertEqual(1, len(self.database.select_from_table(self.TABLE, value1='key', value2=1)))
            time.sleep(1)
            self.assertEqual(0, len(self.database.select_from_table(self.TABLE, value1='key', value2=1)))

    def populate_comparison_table(self, clustering_order='ASC'):
        with self.database:

            class ExampleTable(Model):
                __table_name__ = self.TABLE
                value1 = columns.Text(partition_key=True)
                value2 = columns.Integer(primary_key=True, clustering_order=clustering_order)
                value3 = columns.Text()

            self.database.create_table(ExampleTable)
            keys = ['one', 'two', 'three', 'four', 'five']
            for index in range(len(keys)):
                for value in range(index + 1):
                    self.database.insert_row(
                        self.TABLE, value1=keys[index], value2=value + 1,
                        value3=f'{keys[index]}-{value + 1}',
                    )

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_limit(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:
            self.populate_comparison_table()
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='five')), 5)
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1__in=['four', 'five'])), 9)
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='five', limit=3)), 3)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_order(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:
            self.populate_comparison_table()
            previous = None
            results = self.database.select_from_table(self.TABLE, value1='five')
            for result in results:
                if previous:
                    self.assertLess(previous.value2, result.value2)
                previous = result

            previous = None
            results = self.database.select_from_table(self.TABLE, value1='five', limit=3)
            for result in results:
                if previous:
                    self.assertLess(previous.value2, result.value2)
                previous = result

            self.assertEqual(results[0].value2, 1)
            self.assertEqual(results[-1].value2, 3)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_reverse_order(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:
            self.populate_comparison_table(clustering_order='DESC')
            previous = None
            results = self.database.select_from_table(self.TABLE, value1='five')
            for result in results:
                if previous:
                    self.assertGreater(previous.value2, result.value2)
                previous = result

            previous = None
            results = self.database.select_from_table(self.TABLE, value1='five', limit=3)
            for result in results:
                if previous:
                    self.assertGreater(previous.value2, result.value2)
                previous = result

            self.assertEqual(results[0].value2, 5)
            self.assertEqual(results[-1].value2, 3)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_gt(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:
            self.populate_comparison_table()
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='one', value2__gt=1)), 0)
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='two', value2__gt=1)), 1)
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='three', value2__gt=1)), 2)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_lt(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:
            self.populate_comparison_table()
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='one', value2__lt=1)), 0)
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='two', value2__lt=2)), 1)
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='three', value2__lt=3)), 2)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_gte(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:
            self.populate_comparison_table()
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='one', value2__gte=2)), 0)
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='two', value2__gte=2)), 1)
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='three', value2__gte=2)), 2)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_lte(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:
            self.populate_comparison_table()
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='one', value2__lte=0)), 0)
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='two', value2__lte=1)), 1)
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='three', value2__lte=2)), 2)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_between(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:
            self.populate_comparison_table()
            self.assertEqual(len(self.database.select_from_table(self.TABLE, value1='five', value2__gt=1, value2__lt=5)), 3)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_multiple_primary(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:

            class ExampleTable(Model):
                __table_name__ = self.TABLE
                value1 = columns.Text(partition_key=True)
                value2 = columns.Integer(primary_key=True)
                value3 = columns.Integer(primary_key=True)
                value4 = columns.Text()

            self.database.create_table(ExampleTable)
            self.database.insert_row(self.TABLE, value1='key', value2=0, value3=0, value4='0-0')
            self.database.insert_row(self.TABLE, value1='key', value2=0, value3=1, value4='0-1')
            self.database.insert_row(self.TABLE, value1='key', value2=1, value3=0, value4='1-0')

            results = self.database.select_from_table(self.TABLE, value1='key', value2=0, value3=0)
            self.assertEqual(1, len(results))
            self.assertEqual('0-0', results[0].value4)

            results = self.database.select_from_table(self.TABLE, value1='key', value2=0)
            self.assertEqual(2, len(results))
            self.assertEqual('0-0', results[0].value4)
            self.assertEqual('0-1', results[1].value4)

            results = self.database.select_from_table(self.TABLE, value1='key', value3=0)
            self.assertEqual(2, len(results))
            self.assertEqual('0-0', results[0].value4)
            self.assertEqual('1-0', results[1].value4)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_query_with_none(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        self.create_testing_table()

        with self.database:
            self.database.insert_row(self.TABLE, value1='key', value2=0, value3=0)
            self.database.insert_row(self.TABLE, value1='key', value2=1, value3=1)

            results = self.database.select_from_table(self.TABLE, value1='key', value2=None)
            self.assertEqual(2, len(results))
            self.assertEqual(0, results[0].value3)
            self.assertEqual(1, results[1].value3)

    @WaitForDockerTestCase.mock_if_no_docker(mock_cassandra=MockCassandraContext)
    def test_batch_query(self, cassandra=CassandraContext):
        self.init_database(cassandra=cassandra)
        with self.database:
            self.create_testing_table()

            with self.database.batch_query_context():
                for value1 in ['key1', 'key2', 'key3']:
                    for value2 in range(40):
                        self.database.insert_row(self.TABLE, value1=value1, value2=value2, value3=value2)

            for value1 in ['key1', 'key2', 'key3']:
                results = self.database.select_from_table(self.TABLE, value1=value1)
                for index in range(len(results)):
                    self.assertEqual(index, results[index].value2)
                    self.assertEqual(index, results[index].value3)
