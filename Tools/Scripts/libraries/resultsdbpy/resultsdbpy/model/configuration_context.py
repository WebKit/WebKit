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

import calendar
import json
import time

from cassandra.cqlengine import columns
from cassandra.cqlengine.models import Model
from collections import Iterable, OrderedDict
from datetime import datetime
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.model.commit_context import CommitContext


class ClusteredByConfiguration(Model):
    platform = columns.Text(partition_key=True, required=True)
    version = columns.Integer(partition_key=True, required=True)
    is_simulator = columns.Boolean(partition_key=True, required=True)
    architecture = columns.Text(partition_key=True, required=True)
    attributes = columns.Text(partition_key=True, required=True)

    def to_configuration(self):
        return Configuration(
            platform=self.platform,
            version=self.version,
            is_simulator=self.is_simulator,
            architecture=self.architecture,
            sdk=getattr(self, 'sdk', None) or None,
            **json.loads(self.attributes)
        )


class ConfigurationContext(object):

    class ConfigurationModel(object):

        def to_configuration(self):
            return Configuration(
                platform=self.platform,
                version=self.version, version_name=self.version_name or None,
                sdk=getattr(self, 'sdk', None) or None,
                is_simulator=self.is_simulator,
                architecture=self.architecture, model=self.model or None,
                style=self.style or None, flavor=self.flavor or None,
            )

    class ByPlatform(Model, ConfigurationModel):
        __table_name__ = 'configs_by_platform_with_branch'
        platform = columns.Text(partition_key=True, required=True)
        style = columns.Text(primary_key=True, required=False)
        flavor = columns.Text(primary_key=True, required=False)
        architecture = columns.Text(primary_key=True, required=True)
        model = columns.Text(primary_key=True, required=False)
        is_simulator = columns.Boolean(primary_key=True, required=True)
        version = columns.Integer(primary_key=True, required=True)
        branch = columns.Text(primary_key=True, required=True)
        version_name = columns.Text(primary_key=True, required=False)
        last_run = columns.DateTime(required=True)

    class ByPlatformAndVersion(Model, ConfigurationModel):
        __table_name__ = 'configs_by_platform_and_version_with_branch'
        platform = columns.Text(partition_key=True, required=True)
        version = columns.Integer(partition_key=True, required=True)
        style = columns.Text(primary_key=True, required=False)
        flavor = columns.Text(primary_key=True, required=False)
        architecture = columns.Text(primary_key=True, required=True)
        model = columns.Text(primary_key=True, required=False)
        is_simulator = columns.Boolean(primary_key=True, required=True)
        branch = columns.Text(primary_key=True, required=True)
        version_name = columns.Text(primary_key=True, required=False)
        last_run = columns.DateTime(required=True)

    class ByArchitecture(Model, ConfigurationModel):
        __table_name__ = 'configs_by_architecture_with_branch'
        architecture = columns.Text(partition_key=True, required=True)
        style = columns.Text(primary_key=True, required=False)
        platform = columns.Text(primary_key=True, required=True)
        version = columns.Integer(primary_key=True, required=True)
        model = columns.Text(primary_key=True, required=False)
        flavor = columns.Text(primary_key=True, required=False)
        is_simulator = columns.Boolean(primary_key=True, required=True)
        branch = columns.Text(primary_key=True, required=True)
        version_name = columns.Text(primary_key=True, required=False)
        last_run = columns.DateTime(required=True)

    class ByModel(Model, ConfigurationModel):
        __table_name__ = 'configs_by_model_with_branch'
        model = columns.Text(partition_key=True, required=True)
        version = columns.Integer(primary_key=True, required=True)
        style = columns.Text(primary_key=True, required=False)
        platform = columns.Text(primary_key=True, required=True)
        flavor = columns.Text(primary_key=True, required=False)
        architecture = columns.Text(primary_key=True, required=True)
        is_simulator = columns.Boolean(primary_key=True, required=True)
        branch = columns.Text(primary_key=True, required=True)
        version_name = columns.Text(primary_key=True, required=False)
        last_run = columns.DateTime(required=True)

    def __init__(self, redis, cassandra, cache_timeout=60 * 60 * 24 * 14):
        assert redis
        assert cassandra

        self.redis = redis
        self.cassandra = cassandra
        self.cache_timeout = cache_timeout

        with self:
            self.cassandra.create_table(self.ByPlatform)
            self.cassandra.create_table(self.ByPlatformAndVersion)
            self.cassandra.create_table(self.ByArchitecture)
            self.cassandra.create_table(self.ByModel)

            for configuration in self.cassandra.select_from_table(self.ByPlatform.__table_name__):
                if configuration.last_run >= datetime.utcfromtimestamp(int(time.time() - cache_timeout)):
                    self._register_in_redis(configuration.to_configuration(), configuration.branch, configuration.last_run)

    def __enter__(self):
        self.cassandra.__enter__()

    def __exit__(self, *args, **kwargs):
        self.cassandra.__exit__(*args, **kwargs)

    @classmethod
    def _convert_to_redis_key(cls, configuration, branch):
        return 'configs_with_branch:{}:{}:{}:{}:{}:{}:{}:{}:{}'.format(
            configuration.platform or '*', '*' if configuration.is_simulator is None else (1 if configuration.is_simulator else 0),
            '*' if configuration.version is None else Configuration.integer_to_version(configuration.version),
            configuration.version_name or '*',
            configuration.architecture or '*', configuration.model or '*',
            configuration.style or '*', configuration.flavor or '*',
            branch,
        )

    def _register_in_redis(self, configuration, branch, timestamp):
        if isinstance(timestamp, datetime):
            timestamp = calendar.timegm(timestamp.timetuple())
        expiration = int(self.cache_timeout - (time.time() - int(timestamp)))
        if expiration > 0:
            sdk = configuration.sdk
            try:
                configuration.sdk = None
                self.redis.set(self._convert_to_redis_key(configuration, branch), configuration.to_json(), ex=expiration)
            finally:
                configuration.sdk = sdk

    def register_configuration(self, configuration, branch=None, timestamp=time.time()):
        if not isinstance(configuration, Configuration):
            raise TypeError(f'Expected type {Configuration}, got {type(configuration)}')
        if not configuration.is_complete():
            raise TypeError('Cannot register a partial configuration')
        if not isinstance(timestamp, datetime):
            timestamp = datetime.utcfromtimestamp(int(timestamp))

        branch = branch or CommitContext.DEFAULT_BRANCH_KEY

        with self:
            tables_to_insert_to = [self.ByPlatform.__table_name__, self.ByPlatformAndVersion.__table_name__, self.ByArchitecture.__table_name__]
            if configuration.model is not None:
                tables_to_insert_to.append(self.ByModel.__table_name__)
            for table in tables_to_insert_to:
                self.cassandra.insert_row(
                    table, branch=branch,
                    platform=configuration.platform, is_simulator=configuration.is_simulator,
                    version=configuration.version, version_name=configuration.version_name or '',
                    architecture=configuration.architecture, model=configuration.model or '',
                    style=configuration.style or '', flavor=configuration.flavor or '',
                    last_run=timestamp,
                )

        self._register_in_redis(configuration, branch, timestamp)

    def search_for_configuration(self, configuration=Configuration(), branch=None):
        if not isinstance(configuration, Configuration):
            raise TypeError(f'Expected type {Configuration}, got {type(configuration)}')

        kwargs = dict(branch=branch or CommitContext.DEFAULT_BRANCH_KEY)
        for member in Configuration.REQUIRED_MEMBERS + Configuration.OPTIONAL_MEMBERS:
            if getattr(configuration, member):
                kwargs[member] = getattr(configuration, member)

        if 'platform' in kwargs and 'version' in kwargs:
            table = self.ByPlatformAndVersion
        elif 'platform' in kwargs:
            table = self.ByPlatform
        elif 'architecture' in kwargs:
            table = self.ByArchitecture
        elif 'model' in kwargs:
            table = self.ByModel
        else:
            # Platforms rarely expire, so we can do a decent job of wildcard matching expired configurations
            # if we try all platforms in the cache.
            platforms = set([config.platform for config in self.search_for_recent_configuration(branch=branch)])
            with self:
                result = []
                for platform in platforms:
                    configuration.platform = platform
                    result.extend(self.search_for_configuration(configuration, branch))
                configuration.platform = None
                return result

        with self:
            return [model.to_configuration() for model in self.cassandra.select_from_table(table.__table_name__, **kwargs)]

    def search_for_recent_configuration(self, configuration=Configuration(), branch=None):
        if not isinstance(configuration, Configuration):
            raise TypeError(f'Expected type {Configuration}, got {type(configuration)}')

        configurations = []
        for key in self.redis.scan_iter(self._convert_to_redis_key(configuration, branch or CommitContext.DEFAULT_BRANCH_KEY)):
            candidate = Configuration.from_json(self.redis.get(key.decode('utf-8')).decode('utf-8'))
            if candidate == configuration:
                configurations.append(candidate)
        return configurations

    def insert_row_with_configuration(self, table_name, configuration, **kwargs):
        if not isinstance(configuration, Configuration):
            raise TypeError(f'Expected type {Configuration}, got {type(configuration)}')
        if not configuration.is_complete():
            raise TypeError(f'Cannot insert to {table_name} with a partial configuration')

        with self:
            attributes_dict = OrderedDict()
            for member in Configuration.OPTIONAL_MEMBERS:
                if getattr(configuration, member) is not None:
                    attributes_dict[member] = getattr(configuration, member)

            return self.cassandra.insert_row(
                table_name,
                platform=configuration.platform, is_simulator=configuration.is_simulator,
                version=configuration.version,
                architecture=configuration.architecture,
                attributes=json.dumps(attributes_dict),
                **kwargs)

    def select_from_table_with_configurations(self, table_name, configurations=None, branch=None, recent=True, limit=100, **kwargs):
        if not isinstance(configurations, Iterable):
            raise TypeError('Expected configurations to be iterable')
        if not configurations:
            configurations.append(Configuration())

        with self:
            complete_configurations = set()
            for config in configurations:
                if not isinstance(config, Configuration):
                    raise TypeError(f'Expected type {Configuration}, got {type(config)}')
                if config.is_complete():
                    complete_configurations.add(config)
                elif recent:
                    [complete_configurations.add(element) for element in self.search_for_recent_configuration(config, branch=branch)]
                else:
                    [complete_configurations.add(element) for element in self.search_for_configuration(config, branch=branch)]

            results = {}
            for configuration in complete_configurations:
                attributes_dict = OrderedDict()
                for member in Configuration.OPTIONAL_MEMBERS:
                    if getattr(configuration, member) is not None:
                        attributes_dict[member] = getattr(configuration, member)

                rows = self.cassandra.select_from_table(
                    table_name,
                    platform=configuration.platform, is_simulator=configuration.is_simulator,
                    version=configuration.version,
                    architecture=configuration.architecture,
                    attributes=json.dumps(attributes_dict),
                    limit=limit,
                    branch=branch or CommitContext.DEFAULT_BRANCH_KEY,
                    **kwargs)
                if len(rows) == 0:
                    continue

                for row in rows:
                    full_config = row.to_configuration()
                    if not results.get(full_config):
                        results[full_config] = []
                    results[full_config].append(row)

            return results
