# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import io

from cassandra.cqlengine import columns
from cassandra.cqlengine.models import Model
from resultsdbpy.model.archiver import Archiver


class CassandraArchiver(Archiver):
    MAX_ARCHIVE = 500 * 1024 * 1024  # Archives should be smaller than 500 MB
    CHUNK_SIZE = 10 * 1024 * 1024    # Cassandra doesn't do well with data blobs of more than 10 MB

    # According to https://cwiki.apache.org/confluence/display/CASSANDRA2/CassandraLimitations, we should shard
    # large data blobs.
    class ArchiveChunks(Model):
        __table_name__ = 'archive_chunks_02'
        digest = columns.Text(partition_key=True, required=True)
        index = columns.Integer(primary_key=True, required=True)
        chunk = columns.Blob(required=True)

    def __init__(self, cassandra):
        self.cassandra = cassandra
        with self:
            self.cassandra.create_table(self.ArchiveChunks)

    def __enter__(self):
        self.cassandra.__enter__()

    def __exit__(self, *args, **kwargs):
        self.cassandra.__exit__(*args, **kwargs)

    def save(self, archive, retain_for=None):
        index = 0
        size = self.archive_size(archive)
        if size > self.MAX_ARCHIVE:
            raise ValueError('Archive larger than 500 MB')
        digest = self.archive_digest(archive)
        while size > index * self.CHUNK_SIZE:
            self.cassandra.insert_row(
                self.ArchiveChunks.__table_name__,
                digest=digest, index=index,
                chunk=archive.read(self.CHUNK_SIZE),
                ttl=retain_for,
            )
            index += 1
        return digest

    def retrieve(self, digest, size=None):
        rows = self.cassandra.select_from_table(
            self.ArchiveChunks.__table_name__,
            digest=digest,
            limit=1 + int(size or self.MAX_ARCHIVE / self.CHUNK_SIZE),
        )
        if len(rows) == 0:
            return None

        archive = io.BytesIO()
        for row in rows:
            archive.write(row.chunk)

        if (size and self.archive_size(archive) != size) or digest != self.archive_digest(archive):
            raise RuntimeError('Failed to reconstruct archive from chunks')

        return archive
