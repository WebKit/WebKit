# Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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
import io
import json
import time
import zipfile

from cassandra.cqlengine import columns
from cassandra.cqlengine.models import Model
from collections import OrderedDict
from datetime import datetime
from resultsdbpy.controller.commit import Commit
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.model.archiver import Archiver
from resultsdbpy.model.cassandra_archiver import CassandraArchiver
from resultsdbpy.model.commit_context import CommitContext
from resultsdbpy.model.configuration_context import ClusteredByConfiguration
from resultsdbpy.model.upload_context import UploadContext


def _get_time(input_time):
    if isinstance(input_time, datetime):
        return calendar.timegm(input_time.timetumple())
    if input_time:
        return int(input_time)
    return None


class ArchiveContext(object):
    DEFAULT_LIMIT = 10
    MEMORY_LIMIT = 2 * 1024 * 1024 * 1024  # Don't allow more than 2 gigs of archives in memory at one time

    class ArchiveMetaDataByCommit(ClusteredByConfiguration):
        __table_name__ = 'archive_metadata_by_commit_02'
        suite = columns.Text(partition_key=True, required=True)
        branch = columns.Text(partition_key=True, required=True)
        uuid = columns.BigInt(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        start_time = columns.BigInt(primary_key=True, required=True)
        digest = columns.Text(required=True)
        size = columns.BigInt(required=True)

        def unpack(self):
            return dict(
                uuid=self.uuid,
                start_time=self.start_time,
                digest=self.digest,
                size=self.size,
            )

    @classmethod
    def assert_zipfile(cls, archive):
        if not isinstance(archive, io.BytesIO):
            raise TypeError(f'Archive expected {io.BytesIO}, got {type(archive)} instead')
        if not zipfile.is_zipfile(archive):
            raise TypeError(f'Archive is not a zipfile')

    @classmethod
    def open_zipfile(cls, archive):
        cls.assert_zipfile(archive)
        return zipfile.ZipFile(archive, mode='r')

    def __init__(self, configuration_context, commit_context, ttl_seconds=None):
        self.configuration_context = configuration_context
        self.commit_context = commit_context
        self.cassandra = self.configuration_context.cassandra
        self.archiver = CassandraArchiver(self.cassandra)
        self.ttl_seconds = ttl_seconds

        with self:
            self.cassandra.create_table(self.ArchiveMetaDataByCommit)
            self.cassandra.create_table(UploadContext.SuitesByConfiguration)

    def __enter__(self):
        self.configuration_context.__enter__()
        self.commit_context.__enter__()
        self.archiver.__enter__()

    def __exit__(self, *args, **kwargs):
        self.commit_context.__exit__(*args, **kwargs)
        self.configuration_context.__exit__(*args, **kwargs)
        self.archiver.__exit__(*args, **kwargs)

    def register(self, archive, configuration, commits, suite, timestamp=None):
        self.assert_zipfile(archive)
        timestamp = _get_time(timestamp) or time.time()

        with self:
            uuid = self.commit_context.uuid_for_commits(commits)
            ttl = int((uuid // Commit.TIMESTAMP_TO_UUID_MULTIPLIER) + self.ttl_seconds - time.time()) if self.ttl_seconds else None

            for branch in self.commit_context.branch_keys_for_commits(commits):
                self.configuration_context.register_configuration(configuration, branch=branch, timestamp=timestamp)

                self.configuration_context.insert_row_with_configuration(
                    UploadContext.SuitesByConfiguration.__table_name__, configuration, suite=suite, branch=branch, ttl=ttl,
                )
                digest = self.archiver.save(archive, retain_for=ttl)

                self.configuration_context.insert_row_with_configuration(
                    self.ArchiveMetaDataByCommit.__table_name__, configuration=configuration, suite=suite,
                    branch=branch, uuid=uuid, ttl=ttl,
                    sdk=configuration.sdk or '?', start_time=timestamp,
                    digest=digest,
                    size=Archiver.archive_size(archive),
                )

    def find_archive(
            self, configurations=None, suite=None, recent=True,
            branch=None, begin=None, end=None,
            begin_query_time=None, end_query_time=None,
            limit=DEFAULT_LIMIT,
    ):
        if not configurations:
            configurations = []
        if not isinstance(suite, str):
            raise TypeError(f'Expected type {str}, got {type(suite)}')

        with self:
            metadata_by_config = {}
            for configuration in configurations:
                metadata_by_config.update({config: [value.unpack() for value in values] for config, values in self.configuration_context.select_from_table_with_configurations(
                    self.ArchiveMetaDataByCommit.__table_name__, configurations=[configuration], recent=recent,
                    suite=suite, sdk=configuration.sdk, branch=branch or self.commit_context.DEFAULT_BRANCH_KEY,
                    uuid__gte=CommitContext.convert_to_uuid(begin),
                    uuid__lte=CommitContext.convert_to_uuid(end, CommitContext.timestamp_to_uuid()),
                    start_time__gte=_get_time(begin_query_time), start_time__lte=_get_time(end_query_time),
                    limit=limit,
                ).items()})

            memory_used = 0
            for values in metadata_by_config.values():
                for value in values:
                    if not value.get('digest'):
                        continue

                    memory_used += value.get('size', 0)
                    if memory_used > self.MEMORY_LIMIT:
                        raise RuntimeError('Hit soft-memory cap when fetching archives, aborting')

            archive_by_digest = {}
            result = {}
            for config, values in metadata_by_config.items():
                for value in values:
                    if not value.get('digest'):
                        continue

                    if not archive_by_digest.get(value.get('digest')):
                        archive = self.archiver.retrieve(value.get('digest'), value.get('size', None))
                        if not archive:
                            continue
                        archive_by_digest[value.get('digest')] = archive

                    archive_by_digest.get(value.get('digest')).seek(0)
                    result.setdefault(config, [])
                    result[config].append(dict(
                        archive=archive_by_digest.get(value.get('digest')),
                        digest=value.get('digest'),
                        uuid=value['uuid'],
                        start_time=value['start_time'],
                    ))

            return result

    @classmethod
    def _files_for_archive(cls, archive):
        master = None
        files = set()

        archive_list = []
        is_mastered = True
        for item in archive.namelist():
            if item.startswith('__'):
                continue

            if not master:
                master = item
            else:
                if is_mastered and not item.startswith(master):
                    is_mastered = False
                    archive_list.append(master)
                archive_list.append(item)

        if not is_mastered:
            master = None
        for item in archive_list:
            if master:
                item_to_add = item[len(master):]
            else:
                item_to_add = item

            files.add(item_to_add)
            built = ''
            for partial in item_to_add.split('/')[:-1]:
                built = f'{built}{partial}/'
                files.add(built)

        return master, sorted([*files])

    def ls(self, *args, **kwargs):
        candidates = self.find_archive(*args, **kwargs)
        result = {}
        for config, archives in candidates.items():
            result[config] = []
            for archive in archives:
                result[config].append(dict(
                    uuid=archive['uuid'],
                    start_time=archive['start_time'],
                    files=set(),
                ))

                with self.open_zipfile(archive['archive']) as unpacked:
                    _, result[config][-1]['files'] = self._files_for_archive(unpacked)

        return result

    def file(self, path=None, **kwargs):
        candidates = self.find_archive(**kwargs)
        result = {}
        for config, archives in candidates.items():
            result[config] = []
            for archive in archives:
                file = None

                with self.open_zipfile(archive['archive']) as unpacked:
                    master, files = self._files_for_archive(unpacked)
                    if not path:
                        file = files
                    elif path[-1] == '/':
                        file = [f[len(path):] for f in files if f.startswith(path)]
                    elif path in files:
                        file = unpacked.open('{}{}'.format(master or '', path)).read()
                        query = f'?{config.to_query()}'
                        if len(query) > 1:
                            query += '&'
                        if archive['uuid']:
                            query += f"uuid={archive['uuid']}&"
                        if archive['start_time']:
                            query += f"after_time={archive['start_time']}&before_time={archive['start_time']}&"

                        query = query[:-1]

                        # If we find references to other files in the archive, we need to replace those references
                        # with references that include the query
                        parent_path = '/'.join(path.split('/')[:-1])
                        for f in files:
                            if f[-1] == '/' or f == path or not query:
                                continue
                            if f.startswith(parent_path):
                                file = file.replace(f[len(parent_path):].encode('utf-8'), f'{f[len(parent_path):]}{query}'.encode('utf-8'))
                            else:
                                file = file.replace(f.encode('utf-8'), f'{f}{query}'.encode('utf-8'))


                if file:
                    result[config].append(dict(
                        uuid=archive['uuid'],
                        start_time=archive['start_time'],
                        file=file,
                    ))

        return result
