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
import logging
import requests
import sys
import time
import urllib.parse

from cassandra.cqlengine import columns
from datetime import datetime
from fakeredis import FakeStrictRedis
from resultsdbpy.controller.commit import Commit
from resultsdbpy.model.commit_context import CommitContext
from resultsdbpy.model.configuration_context import ClusteredByConfiguration
from resultsdbpy.model.upload_context import UploadCallbackContext


class CIContext(UploadCallbackContext):
    DEFAULT_CASSANDRA_LIMIT = 100

    class URLsBase(ClusteredByConfiguration):
        suite = columns.Text(partition_key=True, required=True)
        branch = columns.Text(partition_key=True, required=True)
        urls = columns.Text(required=False)

        def unpack(self):
            result = dict(
                uuid=self.uuid,
                start_time=calendar.timegm(self.start_time.timetuple()),
            )
            for key, value in json.loads(self.urls).items():
                if value is not None:
                    result[key] = value
            return result

    class URLsByCommit(URLsBase):
        __table_name__ = 'ci_urls_by_commit'
        uuid = columns.BigInt(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        start_time = columns.DateTime(primary_key=True, required=True)

    class URLsByStartTime(URLsBase):
        __table_name__ = 'ci_urls_by_start_time'
        start_time = columns.DateTime(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        uuid = columns.BigInt(primary_key=True, required=True)

    class URLByQueue(ClusteredByConfiguration):
        __table_name__ = 'ci_url_by_queue'
        branch = columns.Text(partition_key=True, required=True)
        suite = columns.Text(primary_key=True, required=True)
        sdk = columns.Text(primary_key=True, required=True)
        url = columns.Text(required=False)

        def unpack(self):
            return self.url

    def __init__(self, configuration_context, commit_context, ttl_seconds=None, url_factories=None):
        super(CIContext, self).__init__(
            name='ci-urls',
            configuration_context=configuration_context,
            commit_context=commit_context,
            ttl_seconds=ttl_seconds,
        )
        self._url_factories = {}
        for factory in url_factories or []:
            self.add_url_factory(factory)

        with self:
            self.cassandra.create_table(self.URLsByCommit)
            self.cassandra.create_table(self.URLsByStartTime)
            self.cassandra.create_table(self.URLByQueue)

    def add_url_factory(self, factory):
        self._url_factories[factory.master] = factory

    def url(self, master, should_fetch=False, **kwargs):
        factory = self._url_factories.get(master)
        if not factory:
            return None
        return factory.url(fetch=should_fetch, **kwargs)

    def register(self, configuration, commits, suite, test_results, timestamp=None):
        timestamp = timestamp or time.time()
        if not isinstance(timestamp, datetime):
            timestamp = datetime.utcfromtimestamp(int(timestamp))

        try:
            details = test_results.get('details', {})
            urls = {}

            url_factory = self._url_factories.get(details.get('buildbot-master'))
            if url_factory:
                urls['queue'] = url_factory.url(builder_name=details.get('builder-name'))
                urls['build'] = url_factory.url(
                    builder_name=details.get('builder-name'),
                    build_number=details.get('build-number'),
                    should_fetch=True,
                )
                urls['worker'] = url_factory.url(
                    worker_name=details.get('buildbot-worker'),
                    should_fetch=True,
                )
            elif details.get('buildbot-master') and 'link' not in details:
                raise ValueError(f"No link defined or URL factory for {details['buildbot-master']}")

            # Custom build links override constructed buildbot links
            if 'link' in details:
                urls['build'] = details['link']

            for key in details.keys():
                if details[key] is None:
                    del details[key]

            uuid = self.commit_context.uuid_for_commits(commits)
            ttl = int((uuid / Commit.TIMESTAMP_TO_UUID_MULTIPLIER) + self.ttl_seconds - time.time()) if self.ttl_seconds else None

            with self, self.cassandra.batch_query_context():
                for branch in self.commit_context.branch_keys_for_commits(commits):
                    if urls.get('queue'):
                        self.configuration_context.insert_row_with_configuration(
                            self.URLByQueue.__table_name__, configuration=configuration, suite=suite,
                            branch=branch, sdk=configuration.sdk or '?', ttl=ttl,
                            url=urls['queue'],
                        )

                    for table in [self.URLsByCommit, self.URLsByStartTime]:
                        self.configuration_context.insert_row_with_configuration(
                            table.__table_name__, configuration=configuration, suite=suite,
                            branch=branch, uuid=uuid, ttl=ttl,
                            sdk=configuration.sdk or '?', start_time=timestamp,
                            urls=json.dumps(urls),
                        )
        except Exception as e:
            return self.partial_status(e)
        return self.partial_status()

    def find_urls_by_queue(self, configurations, suite, recent=True, branch=None, limit=DEFAULT_CASSANDRA_LIMIT):
        if not isinstance(suite, str):
            raise TypeError(f'Expected type {str}, got {type(suite)}')

        with self:
            result = {}
            for configuration in configurations:
                result.update({config: values[0].unpack() for config, values in self.configuration_context.select_from_table_with_configurations(
                    self.URLByQueue.__table_name__, configurations=[configuration], recent=recent,
                    suite=suite, sdk=configuration.sdk, branch=branch or self.commit_context.DEFAULT_BRANCH_KEY,
                    limit=limit,
                ).items()})
            return result

    def _find_urls(
            self, table, configurations, suite, recent=True,
            branch=None, begin=None, end=None,
            begin_query_time=None, end_query_time=None,
            limit=DEFAULT_CASSANDRA_LIMIT,
    ):
        if not isinstance(suite, str):
            raise TypeError(f'Expected type {str}, got {type(suite)}')

        def get_time(time):
            if isinstance(time, datetime):
                return time
            elif time:
                return datetime.utcfromtimestamp(int(time))
            return None

        with self:
            result = {}
            for configuration in configurations:
                data = self.configuration_context.select_from_table_with_configurations(
                    table.__table_name__, configurations=[configuration], recent=recent,
                    suite=suite, sdk=configuration.sdk, branch=branch or self.commit_context.DEFAULT_BRANCH_KEY,
                    uuid__gte=CommitContext.convert_to_uuid(begin),
                    uuid__lte=CommitContext.convert_to_uuid(end, CommitContext.timestamp_to_uuid()),
                    start_time__gte=get_time(begin_query_time), start_time__lte=get_time(end_query_time),
                    limit=limit,
                ).items()
                result.update({config: [value.unpack() for value in values] for config, values in data})
            return result

    def find_urls_by_commit(self, *args, **kwargs):
        return self._find_urls(self.URLsByCommit, *args, **kwargs)

    def find_urls_by_start_time(self, *args, **kwargs):
        return self._find_urls(self.URLsByStartTime, *args, **kwargs)


class BuildbotEightURLFactory(object):
    SCHEME = 'https://'

    def __init__(self, master):
        self.master = master

    def url(self, builder_name=None, build_number=None, worker_name=None, should_fetch=False):
        if builder_name:
            if build_number:
                return self.SCHEME + urllib.parse.quote(f'{self.master}/builders/{builder_name}/builds/{build_number}')
            return self.SCHEME + urllib.parse.quote(f'{self.master}/builders/{builder_name}')
        elif worker_name:
            return f'{self.SCHEME}{self.master}/buildslaves/{worker_name}'

        return None


class BuildbotURLFactory(object):
    SCHEME = 'https://'
    BUILDER_KEY = '_builder_'
    WORKER_KEY = '_worker_'
    FETCH_RATE = 60 * 60  # Allow a re-fetch every hour

    def __init__(self, master, redis=None):
        self.master = master
        self.redis = redis or FakeStrictRedis()

    def fetch(self):
        self.redis.set(f'{self.master}_refreshed', 0, ex=self.FETCH_RATE)

        response = requests.get(f'{self.SCHEME}{self.master}/api/v2/builders')
        if response.status_code == 200:
            for builder in response.json()['builders']:
                self.redis.set(
                    f"{self.master}_{self.BUILDER_KEY}_{builder['name']}",
                    str(builder['builderid']),
                )
        else:
            sys.stderr.write(f'Failed to retrieve builders information from {self.master}')

        response = requests.get(f'{self.SCHEME}{self.master}/api/v2/workers')
        if response.status_code == 200:
            for worker in response.json()['workers']:
                self.redis.set(
                    f"{self.master}_{self.WORKER_KEY}_{worker['name']}",
                    str(worker['workerid']),
                )
        else:
            sys.stderr.write(f'Failed to retrieve workers information from {self.master}')

    def _builder_url(self, builder_id):
        return f'{self.SCHEME}{self.master}/#/builders/{builder_id}'

    def _build_url(self, builder_id, build_number):
        return f'{self._builder_url(builder_id=builder_id)}/builds/{build_number}'

    def _worker_url(self, worker_id):
        return f'{self.SCHEME}{self.master}/#/workers/{worker_id}'

    def url(self, builder_name=None, build_number=None, worker_name=None, should_fetch=False):
        if should_fetch and self.redis.get(f'{self.master}_refreshed'):
            should_fetch = False

        while True:
            if builder_name:
                builder_id = self.redis.get(f'{self.master}_{self.BUILDER_KEY}_{builder_name}')
                if builder_id:
                    if build_number:
                        return self._build_url(builder_id=builder_id.decode('utf-8'), build_number=build_number)
                    return self._builder_url(builder_id=builder_id.decode('utf-8'))

            elif worker_name:
                worker_id = self.redis.get(f'{self.master}_{self.WORKER_KEY}_{worker_name}')
                if worker_id:
                    return self._worker_url(worker_id=worker_id.decode('utf-8'))

            if not should_fetch:
                break
            self.fetch()
            should_fetch = False

        logging.error(f'Could not generate URL for CI links from {self.master}')
        return None
