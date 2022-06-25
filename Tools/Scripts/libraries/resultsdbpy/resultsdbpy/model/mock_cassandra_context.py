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

import mock
import re
import time

from cassandra.metadata import ColumnMetadata, KeyspaceMetadata, Metadata, TableMetadataV3
from cassandra.util import OrderedDict
from collections import defaultdict
from resultsdbpy.model.cassandra_context import CassandraContext


class MockCluster(object):

    metadata = Metadata()
    database = {}

    def __init__(self, contact_points=["127.0.0.1"], port=9042, auth_provider=None):
        assert isinstance(contact_points, list)
        self.contact_points = contact_points
        self.port = port
        self.sessions = []

    def connect(self, keyspace=None):
        result = MockSession(self, keyspace)
        self.sessions.append(result)
        return result

    def shutdown(self):
        self.sessions = []


class MockSession(object):

    def __init__(self, cluster, keyspace=None):
        self.cluster = cluster
        self.keyspace = keyspace

    def execute(self, *args):
        pass


class MockCQLEngineContext():

    connections = {}

    def __init__(self):
        self._depth = 0
        self.patches = [
            mock.patch('resultsdbpy.model.cassandra_context.Cluster', new=MockCluster),
            mock.patch('resultsdbpy.model.cassandra_context.register_connection', new=self.register_connection),
            mock.patch('resultsdbpy.model.cassandra_context.unregister_connection', new=self.unregister_connection),
            mock.patch('resultsdbpy.model.cassandra_context.get_cluster', new=self.get_cluster),
            mock.patch('resultsdbpy.model.cassandra_context.create_keyspace_simple', new=self.create_keyspace_simple),
            mock.patch('resultsdbpy.model.cassandra_context.create_keyspace_network_topology', new=self.create_keyspace_network_topology),
            mock.patch('resultsdbpy.model.cassandra_context.drop_keyspace', new=self.drop_keyspace),
            mock.patch('resultsdbpy.model.cassandra_context.sync_table', new=self.sync_table),
        ]

    def __enter__(self):
        if self._depth == 0:
            for patch in self.patches:
                patch.__enter__()
        self._depth += 1

    def __exit__(self, *args, **kwargs):
        self._depth -= 1
        if self._depth <= 0:
            for patch in self.patches:
                patch.__exit__(*args, **kwargs)

    @classmethod
    def register_connection(cls, name, session, **kwargs):
        if session is None:
            session = MockCluster().connect()
        cls.connections[name] = (session.cluster, session)

    @classmethod
    def unregister_connection(cls, name):
        if name in cls.connections:
            del cls.connections[name]

    @classmethod
    def get_cluster(cls, connection):
        return cls.connections.get(connection, [None])[0]

    @classmethod
    def create_keyspace_simple(cls, name, replication_factor=1, durable_writes=True, connections=[]):
        for connection in connections:
            assert connection in cls.connections
            if name not in cls.get_cluster(connection).metadata.keyspaces:
                cls.get_cluster(connection).metadata.keyspaces[name] = KeyspaceMetadata(name, durable_writes, None, None)
                MockCluster.database[name] = {}

    @classmethod
    def create_keyspace_network_topology(cls, name, dc_replication_map={}, durable_writes=True, connections=[]):
        return cls.create_keyspace_simple(name, durable_writes=durable_writes, connections=connections)

    @classmethod
    def drop_keyspace(cls, name, connections=[]):
        for connection in connections:
            assert connection in cls.connections
            if name in cls.get_cluster(connection).metadata.keyspaces:
                del cls.get_cluster(connection).metadata.keyspaces[name]
                del MockCluster.database[name]

    @classmethod
    def sync_table(cls, model, keyspaces=[], connections=[]):
        for connection in connections:
            assert connection in cls.connections

            for keyspace in keyspaces:
                keyspace_metadata = MockCQLEngineContext.get_cluster(connection).metadata.keyspaces.get(keyspace)
                assert keyspace_metadata is not None

                primary_keys = []
                column_keys = []
                for key, value in model._columns.items():
                    if value.partition_key or value.primary_key:
                        primary_keys.append(key)
                    else:
                        column_keys.append(key)

                columns = OrderedDict()
                partition_keys = []
                clustering_keys = []
                for key in primary_keys + column_keys:
                    value = model._columns[key]
                    meta_data = ColumnMetadata(None, key, value.db_type, is_reversed=(value.clustering_order == 'DESC'))
                    columns[key] = meta_data

                    if value.partition_key:
                        partition_keys.append(meta_data)
                    if value.primary_key:
                        clustering_keys.append(meta_data)

                MockCluster.database[keyspace][model._raw_column_family_name()] = defaultdict(lambda: {})
                keyspace_metadata.tables[model._raw_column_family_name()] = TableMetadataV3(
                    keyspace, model._raw_column_family_name(),
                    columns=columns, partition_key=partition_keys, clustering_key=clustering_keys,
                )


class MockCassandraContext(CassandraContext):

    @classmethod
    def drop_keyspace(cls, *args, **kwargs):
        with MockCQLEngineContext():
            CassandraContext.drop_keyspace(*args, **kwargs)

    def __init__(self, *args, **kwargs):
        self._mock_cql_engine = MockCQLEngineContext()
        with self._mock_cql_engine:
            super(MockCassandraContext, self).__init__(*args, **kwargs)

    def __enter__(self):
        self._mock_cql_engine.__enter__()
        super(MockCassandraContext, self).__enter__()

    def __exit__(self, *args, **kwargs):
        super(MockCassandraContext, self).__exit__(*args, **kwargs)
        self._mock_cql_engine.__exit__(*args, **kwargs)

    @CassandraContext.AssertConnectedDecorator()
    def insert_row(self, table_name, ttl=None, **kwargs):
        if table_name not in self._models:
            raise self.SchemaException(f'{table_name} does not exist in the database')

        item = self._models[table_name](**kwargs)
        schema = self.schema_for_table(table_name)

        partition = []
        for column in schema.partition_key:
            partition.append(getattr(item, column.name))
        cluster = []
        for column in schema.primary_key:
            if getattr(item, column.name) not in partition:
                cluster.append(getattr(item, column.name))

        self.cluster.database[self.keyspace][table_name][tuple(partition)][tuple(cluster)] = (None if ttl is None else time.time() + ttl, item)

    @CassandraContext.AssertConnectedDecorator()
    def select_from_table(self, table_name, limit=10000, **kwargs):
        result = []
        schema = self.schema_for_table(table_name)

        partition_names = []
        for column in schema.partition_key:
            partition_names.append(column.name)

        # Sort kwargs so they are in the same order as the columns.
        ordered_kwargs = OrderedDict()
        for name in self._models[table_name]._columns.keys():
            for key, value in kwargs.items():
                if key.split('__')[0] == name and value is not None:
                    ordered_kwargs[key] = value

        # Convert arguments to filters
        partitions = [[]]
        filters = []
        for key, value in ordered_kwargs.items():
            key_value = key.split('__')[0]
            operator = None if len(key.split('__')) == 1 else key.split('__')[1]

            if key_value in partition_names:
                if operator == 'in':
                    original_partitions = list(partitions)
                    partitions = []
                    for _ in range(len(value)):
                        for element in original_partitions:
                            partitions.append(list(element))
                    for multiplier in range(len(value)):
                        for index in range(len(original_partitions)):
                            partitions[multiplier * len(original_partitions) + index].append(value[multiplier])
                else:
                    for partition in partitions:
                        partition.append(value)

            filters.append(self.filter_for_argument(key, value))

        if len(partitions[0]) == 0:
            partitions = self.cluster.database[self.keyspace][table_name].keys()

        candidate_elements = []
        for partition in partitions:
            for key, row in self.cluster.database[self.keyspace][table_name][tuple(partition)].items():
                does_match = True
                for filter in filters:
                    if not filter(row[1]):
                        does_match = False
                        break
                if not does_match:
                    continue
                candidate_elements.append((partition, key))

        # To reverse, they all need to agree. This might not work with compound clustering keys, but we really
        # shouldn't use those anyways
        is_reversed = True
        for column in schema.primary_key:
            if column in schema.partition_key:
                continue
            if not column.is_reversed:
                is_reversed = False
        candidate_elements.sort(reverse=is_reversed)

        for candidate in candidate_elements:
            element = self.cluster.database[self.keyspace][table_name][tuple(candidate[0])][candidate[1]]
            deadline = element[0]
            if deadline is None or deadline > time.time():
                result.append(element[1])
                limit -= 1
                if limit <= 0:
                    return result
            else:
                del self.cluster.database[self.keyspace][table_name][tuple(candidate[0])][candidate[1]]
        return result
