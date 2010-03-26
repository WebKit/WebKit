# Copyright (C) 2010 Google Inc. All rights reserved.
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

from datetime import datetime
import logging

from google.appengine.ext import blobstore
from google.appengine.ext import db


class TestFile(db.Model):
    builder = db.StringProperty()
    name = db.StringProperty()
    test_type = db.StringProperty()
    blob_key = db.StringProperty()
    date = db.DateTimeProperty(auto_now_add=True)

    @classmethod
    def delete_file(cls, key, builder, test_type, name, limit):
        if key:
            file = db.get(key)
            if not file:
                logging.warning("File not found, key: %s.", key)
                return False

            file._delete_all()
        else:
            files = cls.get_files(builder, test_type, name, limit)
            if not files:
                logging.warning(
                    "File not found, builder: %s, test_type:%s, name: %s.",
                    builder, test_type, name)
                return False

            for file in files:
                file._delete_all()

        return True

    @classmethod
    def get_files(cls, builder, test_type, name, limit):
        query = TestFile.all()
        if builder:
            query = query.filter("builder =", builder)
        if test_type:
            query = query.filter("test_type =", test_type)
        if name:
            query = query.filter("name =", name)

        return query.order("-date").fetch(limit)

    @classmethod
    def add_file(cls, builder, test_type, blob_info):
        file = TestFile()
        file.builder = builder
        file.test_type = test_type
        file.name = blob_info.filename
        file.blob_key = str(blob_info.key())
        file.put()

        logging.info(
            "File saved, builder: %s, test_type: %s, name: %s, blob key: %s.",
            builder, test_type, file.name, file.blob_key)

        return file

    @classmethod
    def update_file(cls, builder, test_type, blob_info):
        files = cls.get_files(builder, test_type, blob_info.filename, 1)
        if not files:
            return cls.add_file(builder, test_type, blob_info)

        file = files[0]
        old_blob_info = blobstore.BlobInfo.get(file.blob_key)
        if old_blob_info:
            old_blob_info.delete()

        file.builder = builder
        file.test_type = test_type
        file.name = blob_info.filename
        file.blob_key = str(blob_info.key())
        file.date = datetime.now()
        file.put()

        logging.info(
            "File replaced, builder: %s, test_type: %s, name: %s, blob key: %s.",
            builder, test_type, file.name, file.blob_key)

        return file

    def _delete_all(self):
        if self.blob_key:
            blob_info = blobstore.BlobInfo.get(self.blob_key)
            if blob_info:
                blob_info.delete()

        self.delete()
