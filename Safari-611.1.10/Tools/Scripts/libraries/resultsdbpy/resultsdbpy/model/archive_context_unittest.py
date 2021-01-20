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

import base64
import io
import mock
import os
import zipfile

from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.mock_repository import MockSVNRepository
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class ArchiveContextTest(WaitForDockerTestCase):
    KEYSPACE = 'archive_test_keyspace'

    def init_database(self, redis=StrictRedis, cassandra=CassandraContext, configuration=Configuration(), archive=None):
        cassandra.drop_keyspace(keyspace=self.KEYSPACE)
        self.model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True))
        MockModelFactory.add_mock_archives(self.model, configuration=configuration, archive=archive)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_find_archive(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        archives = self.model.archive_context.find_archive(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            begin=MockSVNRepository.webkit().commit_for_id(236542), end=MockSVNRepository.webkit().commit_for_id(236542),
            suite='layout-tests',
        )
        self.assertEqual(len(next(iter(archives.values()))), 1)
        self.assertEqual(next(iter(archives.values()))[0]['uuid'], 153804910800)
        self.assertEqual(next(iter(archives.values()))[0]['archive'].getvalue(), base64.b64decode(MockModelFactory.ARCHIVE_ZIP))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_archive_list(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        files = self.model.archive_context.ls(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            begin=MockSVNRepository.webkit().commit_for_id(236542), end=MockSVNRepository.webkit().commit_for_id(236542),
            suite='layout-tests',
        )
        self.assertEqual(len(next(iter(files.values()))), 1)
        self.assertEqual(next(iter(files.values()))[0]['uuid'], 153804910800)
        self.assertEqual(next(iter(files.values()))[0]['files'], ['file.txt', 'index.html'])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_file_access(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        files = self.model.archive_context.file(
            path='file.txt',
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            begin=MockSVNRepository.webkit().commit_for_id(236542), end=MockSVNRepository.webkit().commit_for_id(236542),
            suite='layout-tests',
        )
        self.assertEqual(len(next(iter(files.values()))), 1)
        self.assertEqual(next(iter(files.values()))[0]['uuid'], 153804910800)
        self.assertEqual(next(iter(files.values()))[0]['file'], 'data'.encode('ascii'))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_file_list(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        files = self.model.archive_context.file(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            begin=MockSVNRepository.webkit().commit_for_id(236542), end=MockSVNRepository.webkit().commit_for_id(236542),
            suite='layout-tests',
        )
        self.assertEqual(len(next(iter(files.values()))), 1)
        self.assertEqual(next(iter(files.values()))[0]['uuid'], 153804910800)
        self.assertEqual(next(iter(files.values()))[0]['file'], ['file.txt', 'index.html'])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_large_archive(self, redis=StrictRedis, cassandra=CassandraContext):
        FILE_SIZE = 25 * 1024 * 1024  # 25 MB files
        buff = io.BytesIO()
        with zipfile.ZipFile(buff, 'a', zipfile.ZIP_DEFLATED, False) as archive:
            for file, data in [
                ('file-1.txt', io.BytesIO(os.urandom(FILE_SIZE))),
                ('file-2.txt', io.BytesIO(os.urandom(FILE_SIZE))),
                ('file-3.txt', io.BytesIO(os.urandom(FILE_SIZE))),
            ]:
                archive.writestr(file, data.getvalue())

        self.init_database(
            redis=redis, cassandra=cassandra,
            configuration=Configuration(platform='Mac', style='Release', flavor='wk1'),
            archive=buff,
        )

        files = self.model.archive_context.find_archive(
            configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
            begin=MockSVNRepository.webkit().commit_for_id(236542), end=MockSVNRepository.webkit().commit_for_id(236542),
            suite='layout-tests',
        )
        self.assertEqual(len(next(iter(files.values()))), 1)
        self.assertEqual(next(iter(files.values()))[0]['uuid'], 153804910800)
        self.assertEqual(next(iter(files.values()))[0]['archive'].getvalue(), buff.getvalue())

        with mock.patch('resultsdbpy.model.archive_context.ArchiveContext.MEMORY_LIMIT', new=FILE_SIZE), self.assertRaises(RuntimeError):
            self.model.archive_context.find_archive(
                configurations=[Configuration(platform='Mac', style='Release', flavor='wk1')],
                begin=MockSVNRepository.webkit().commit_for_id(236542), end=MockSVNRepository.webkit().commit_for_id(236542),
                suite='layout-tests',
            )
