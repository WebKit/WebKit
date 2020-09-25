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

import boto3
import botocore
import io
import math
import os
import hashlib

from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad
from resultsdbpy.model.archiver import Archiver


class S3Archiver(Archiver):
    class Credentials(object):
        def __init__(
            self,
            region_name=None,
            aws_access_key_id=None, aws_secret_access_key=None, aws_session_token=None,
            key=None,
        ):
            self.region_name = region_name or 'us-west-2'
            self.aws_access_key_id = aws_access_key_id
            self.aws_secret_access_key = aws_secret_access_key
            self.aws_session_token = aws_session_token
            self.key = key

        def __repr__(self):
            return self.aws_access_key_id

    def __init__(self, credentials, bucket='results-archive', ttl_seconds=None):
        self.credentials = credentials
        self.resources = None
        self.bucket = bucket
        self._count = 0

        # Only modify the lifecycle of the bucket if we're editing the schema
        if os.environ['CQLENG_ALLOW_SCHEMA_MANAGEMENT'] == '1':
            client = boto3.client(
                service_name='s3',
                region_name=self.credentials.region_name,
                aws_access_key_id=self.credentials.aws_access_key_id,
                aws_secret_access_key=self.credentials.aws_secret_access_key,
                aws_session_token=self.credentials.aws_session_token,
            )

            ttl_seconds = ttl_seconds or 60 * 60 * 24 * 365
            ttl_days = int(math.ceil(ttl_seconds / (60 * 60 * 24)))

            client.put_bucket_lifecycle_configuration(
                Bucket=self.bucket,
                LifecycleConfiguration=dict(
                    Rules=[dict(
                        Expiration=dict(Days=ttl_days),
                        ID='ttl',
                        Filter=dict(Prefix='archives/'),
                        Status='Enabled',
                    )],
                ),
            )

    @property
    def _cipher(self):
        if not self.credentials.key:
            return None
        return AES.new(hashlib.sha256(self.credentials.key.encode()).digest(), AES.MODE_ECB)

    def __enter__(self):
        self._count += 1
        if not self.resources:
            self.resources = boto3.resource(
                service_name='s3',
                region_name=self.credentials.region_name,
                aws_access_key_id=self.credentials.aws_access_key_id,
                aws_secret_access_key=self.credentials.aws_secret_access_key,
                aws_session_token=self.credentials.aws_session_token,
            )

    def __exit__(self, *args, **kwargs):
        self._count -= 1
        if self._count <= 0:
            self.resources = None
            self._count = 0

    def save(self, archive, retain_for=None):
        with self:
            # S3 doesn't let us set custom expiration dates. This is a bit annoying because we really want
            # resources to stay alive based on their commit time, rather than their report time, but the
            # consequence will be resources kept around for a bit longer than we need access to them.
            digest = self.archive_digest(archive)
            cipher = self._cipher
            if cipher:
                archive = io.BytesIO(cipher.encrypt(pad(archive.read(), AES.block_size)))

            self.resources.Bucket(self.bucket).upload_fileobj(archive, 'archives/{}'.format(digest))
            return digest

    def retrieve(self, digest, size=None):
        with self:
            try:
                archive = io.BytesIO()
                self.resources.Bucket(self.bucket).download_fileobj('archives/{}'.format(digest), archive)
                archive.seek(0)

                cipher = self._cipher
                if cipher:
                    archive = io.BytesIO(unpad(cipher.decrypt(archive.read()), AES.block_size))

                if (size and self.archive_size(archive) != size) or digest != self.archive_digest(archive):
                    raise RuntimeError('Retrieved archive does not match provided digest')
                return archive
            except botocore.exceptions.ClientError as e:
                if e.response['Error']['Code'] == "404":
                    return None
                raise
