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

import contextlib
import os
import re
import uuid

from cassandra.cluster import Cluster
from cassandra.cqlengine.columns import Text
from cassandra.cqlengine.connection import register_connection, unregister_connection
from cassandra.cqlengine.management import CQLENG_ALLOW_SCHEMA_MANAGEMENT, get_cluster, create_keyspace_network_topology, create_keyspace_simple, drop_keyspace, sync_table
from cassandra.cqlengine.models import Model
from cassandra.cqlengine.query import BatchQuery


class RegexCluster(Text):

    def __init__(self, min_length=1, max_length=None, primary_key=True, partition_key=False, **kwargs):
        assert primary_key
        assert not partition_key
        super(RegexCluster, self).__init__(min_length=min_length, max_length=max_length, primary_key=primary_key, partition_key=partition_key, **kwargs)


class CountedBatchQuery(BatchQuery):
    DEFAULT_LIMIT = 100

    def __init__(self, limit=DEFAULT_LIMIT, **kwargs):
        super(CountedBatchQuery, self).__init__(**kwargs)
        self.limit = limit

    def add_query(self, query):
        if len(self.queries) >= self.limit:
            self.execute()
            self._executed = False
        return super(CountedBatchQuery, self).add_query(query)


class CassandraContext(object):

    @classmethod
    def can_modify_schema(cls):
        return os.getenv(CQLENG_ALLOW_SCHEMA_MANAGEMENT, False)

    @classmethod
    def drop_keyspace(cls, nodes=None, keyspace='results_database', auth_provider=None):
        nodes = nodes if nodes else ['localhost']
        connection_id = uuid.uuid4()

        try:
            register_connection(name=str(connection_id), session=Cluster(nodes, auth_provider=auth_provider).connect())
            does_keyspace_exist = keyspace in get_cluster(str(connection_id)).metadata.keyspaces
            if does_keyspace_exist:
                drop_keyspace(keyspace, connections=[str(connection_id)])
        finally:
            unregister_connection(name=str(connection_id))

    def __init__(self, nodes=None, keyspace='results_database', auth_provider=None, create_keyspace=False, replication_map=None):
        self.keyspace = keyspace
        self._depth = 0
        self._connection_id = uuid.uuid4()
        self._models = {}
        self._nodes = nodes if nodes else ['localhost']
        self._auth_provider = auth_provider
        self._batch = []

        try:
            register_connection(name=str(self._connection_id), session=Cluster(self._nodes, auth_provider=self._auth_provider).connect())
            does_keyspace_exist = self.keyspace in get_cluster(str(self._connection_id)).metadata.keyspaces

            if create_keyspace and not does_keyspace_exist:
                if not self.can_modify_schema():
                    raise Exception('Cannot create keyspace, Schema modification is disabled')
                if replication_map is None:
                    create_keyspace_simple(self.keyspace, replication_factor=1, connections=[str(self._connection_id)])
                else:
                    create_keyspace_network_topology(self.keyspace, dc_replication_map=replication_map, connections=[str(self._connection_id)])
            elif not does_keyspace_exist:
                raise Exception(f'Keyspace {self.keyspace} does not exist and will not be created')
        finally:
            unregister_connection(name=str(self._connection_id))

    def __enter__(self):
        if self._depth == 0:
            register_connection(name=str(self._connection_id), session=Cluster(self._nodes, auth_provider=self._auth_provider).connect(keyspace=self.keyspace))
        self._depth += 1

    def __exit__(self, *args, **kwargs):
        self._depth -= 1
        if self._depth <= 0:
            unregister_connection(name=str(self._connection_id))

    def assert_connected(self):
        if self._depth <= 0:
            raise AssertionError('No Cassandra session available')

    class AssertConnectedDecorator():

        def __call__(self, function):
            def decorator(obj, *args, **kwargs):
                obj.assert_connected()
                return function(obj, *args, **kwargs)
            return decorator

    @property
    @AssertConnectedDecorator()
    def cluster(self):
        return get_cluster(str(self._connection_id))

    def schema_for_table(self, table_name):
        if not self.cluster.metadata:
            return None
        keyspace_metadata = self.cluster.metadata.keyspaces.get(self.keyspace, None)
        if not keyspace_metadata:
            return None
        return keyspace_metadata.tables.get(table_name)

    @AssertConnectedDecorator()
    def create_table(self, model):
        does_schema_match = self.does_table_model_match_schema(model)
        if does_schema_match is False:
            raise self.SchemaException('Existing schema does not match provided model')

        table_name = model._raw_column_family_name()
        self._models[table_name] = model
        self._models[table_name].__connection__ = str(self._connection_id)
        self._models[table_name].__keyspace__ = self.keyspace

        if does_schema_match:
            return

        assert self.can_modify_schema()

        # We have a special model named RegexCluster which allows LIKE operations to be preformed on a primary key quickly.
        # This was specifically intended for git commits, although has a few other potential uses.
        sasi_index_column = None
        for attr in dir(model):
            if isinstance(getattr(getattr(model, attr, None), 'column', None), RegexCluster):
                if sasi_index_column:
                    raise self.SchemaException('Only one RegexCluster allowed')
                sasi_index_column = attr

        sync_table(model, keyspaces=[self.keyspace], connections=[str(self._connection_id)])
        if sasi_index_column:
            for session in self.cluster.sessions:
                if session.keyspace != self.keyspace:
                    continue
                # https://docs.datastax.com/en/dse/5.1/cql/cql/cql_using/useSASIIndex.html
                session.execute(f"""CREATE CUSTOM INDEX index_{table_name}_{sasi_index_column} ON {table_name} ({sasi_index_column}) USING \
'org.apache.cassandra.index.sasi.SASIIndex' WITH OPTIONS = {{ \
'analyzer_class': 'org.apache.cassandra.index.sasi.analyzer.StandardAnalyzer', \
'case_sensitive': 'true'}}""")
                break

    @AssertConnectedDecorator()
    def does_table_model_match_schema(self, model):
        if not issubclass(model, Model):
            raise self.SchemaException('Models must be derived from base Model.')
        if model.__abstract__:
            raise self.SchemaException('Cannot create table from abstract model')

        schema = self.schema_for_table(model._raw_column_family_name())
        if schema is None:
            return None

        primary_columns = []
        data_columns = []
        for key in model._columns.keys():
            if getattr(model, key).column.partition_key or getattr(model, key).column.primary_key:
                primary_columns.append(key)
            else:
                data_columns.append(key)

        schema_columns = []
        for column in schema.columns:
            schema_columns.append(column)

        if len(primary_columns) + len(data_columns) != len(schema_columns):
            return False

        for i in range(len(primary_columns)):
            if primary_columns[i] != schema_columns[i]:
                return False
        for element in data_columns:
            if element not in schema_columns:
                return False

        partition_keys = [column.name for column in schema.partition_key]
        primary_keys = [column.name for column in schema.primary_key]

        for column in primary_columns + data_columns:
            model_column = getattr(model, column).column
            if schema.columns[column].cql_type != model_column.db_type:
                return False

            if model_column.partition_key and column not in partition_keys:
                return False
            if model_column.primary_key and column not in primary_keys:
                return False

            if model_column.clustering_order == 'DESC' and not schema.columns[column].is_reversed:
                return False
            if (model_column.clustering_order == 'ASC' or model_column.clustering_order is None) and schema.columns[column].is_reversed:
                return False

        return True

    @AssertConnectedDecorator()
    @contextlib.contextmanager
    def batch_query_context(self, limit=CountedBatchQuery.DEFAULT_LIMIT):
        self._batch.append(CountedBatchQuery(limit=limit, connection=str(self._connection_id)))
        try:
            with self._batch[-1]:
                yield
        finally:
            del self._batch[-1]

    @AssertConnectedDecorator()
    def insert_row(self, table_name, ttl=None, **kwargs):
        if table_name not in self._models:
            raise self.SchemaException(f'{table_name} does not exist in the database')

        # If the ttl has already expired, don't even bother sending the data.
        if ttl and ttl < 0:
            return

        if len(self._batch):
            self._models[table_name].batch(self._batch[-1]).ttl(ttl).create(**kwargs)
        else:
            self._models[table_name].ttl(ttl).create(**kwargs)

    @staticmethod
    def filter_for_argument(key, value):
        key_value = key.split('__')[0]
        operator = None if len(key.split('__')) == 1 else key.split('__')[1]

        if operator == 'in':
            return lambda v, key_value=key_value, value=value: getattr(v, key_value) in value
        elif operator == 'gt':
            return lambda v, key_value=key_value, value=value: getattr(v, key_value) > value
        elif operator == 'gte':
            return lambda v, key_value=key_value, value=value: getattr(v, key_value) >= value
        elif operator == 'lt':
            return lambda v, key_value=key_value, value=value: getattr(v, key_value) < value
        elif operator == 'lte':
            return lambda v, key_value=key_value, value=value: getattr(v, key_value) <= value
        elif operator == 'like':

            def regex_filter(v, key_value=key_value, value=value):
                regex = re.escape(value)
                regex = regex.replace(re.escape('%'), '.*')
                return bool(re.match(r'\A' + regex + r'\Z', getattr(v, key_value)))

            return regex_filter

        elif operator is None or operator is 'in':
            return lambda v, key_value=key_value, value=value: getattr(v, key_value) == value

        raise self.SelectException('Unrecognized operator {}'.format(operator))

    @AssertConnectedDecorator()
    def select_from_table(self, table_name, limit=10000, **kwargs):
        if table_name not in self._models:
            raise self.SchemaException(f'{table_name} does not exist in the database')

        create_args = {}
        for name, column in self._models[table_name]._columns.items():
            if not column.partition_key and not column.primary_key:
                continue
            did_find_column_name = False
            using_range_query = False
            for arg, value in kwargs.items():
                # cqlengine will use arguments like '<column_name>__like' to indicate that an argument will have some kind of
                # operation performed. We're looking for column name here.
                if name == arg.split('__')[0] and value is not None:
                    create_args[arg] = value
                    did_find_column_name = True
                    using_range_query = '__' in arg
            if not did_find_column_name or using_range_query:
                break

        # Not all versions of Cassandra support filtering. Since filtering is inefficient in Cassandra anyways, queries which rely
        # on filtering should be able to retrieve the data from the database and filter it server-side.
        filters = []
        for arg, value in kwargs.items():
            if arg not in create_args and value is not None:
                filters.append(self.filter_for_argument(arg, value))

        query_to_be_run = self._models[table_name].objects(**create_args).limit(limit)

        # Forces cqlengine to dispatch the query before exiting the function
        result = []
        for element in query_to_be_run:
            for f in filters:
                if not f(element):
                    break
            else:
                result.append(element)
        return result

    class SchemaException(RuntimeError):
        pass

    class SelectException(RuntimeError):
        pass
