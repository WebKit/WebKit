#!/usr/bin/env python
# Copyright (C) 2012 Google Inc. All rights reserved.
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

import hashlib
import re

from google.appengine.ext import db


class NumericIdHolder(db.Model):
    owner = db.ReferenceProperty()
    # Dummy class whose sole purpose is to generate key().id()


def createInTransactionWithNumericIdHolder(callback):
    idHolder = NumericIdHolder()
    idHolder.put()
    idHolder = NumericIdHolder.get(idHolder.key())
    owner = db.run_in_transaction(callback, idHolder.key().id())
    if owner:
        idHolder.owner = owner
        idHolder.put()
    else:
        idHolder.delete()
    return owner


def modelFromNumericId(id, expectedKind):
    idHolder = NumericIdHolder.get_by_id(id)
    return idHolder.owner if idHolder and idHolder.owner and isinstance(idHolder.owner, expectedKind) else None


class Branch(db.Model):
    id = db.IntegerProperty(required=True)
    name = db.StringProperty(required=True)


class Platform(db.Model):
    id = db.IntegerProperty(required=True)
    name = db.StringProperty(required=True)


class Builder(db.Model):
    name = db.StringProperty(required=True)
    password = db.StringProperty(required=True)

    def authenticate(self, rawPassword):
        return self.password == hashlib.sha256(rawPassword).hexdigest()

    @staticmethod
    def hashedPassword(rawPassword):
        return hashlib.sha256(rawPassword).hexdigest()


class Build(db.Model):
    branch = db.ReferenceProperty(Branch, required=True, collection_name='build_branch')
    platform = db.ReferenceProperty(Platform, required=True, collection_name='build_platform')
    builder = db.ReferenceProperty(Builder, required=True, collection_name='builder_key')
    buildNumber = db.IntegerProperty(required=True)
    revision = db.IntegerProperty(required=True)
    timestamp = db.DateTimeProperty(required=True)


# Used to generate TestMap in the manifest efficiently
class Test(db.Model):
    id = db.IntegerProperty(required=True)
    name = db.StringProperty(required=True)
    branches = db.ListProperty(db.Key)
    platforms = db.ListProperty(db.Key)


class TestResult(db.Model):
    name = db.StringProperty(required=True)
    build = db.ReferenceProperty(Build, required=True)
    value = db.FloatProperty(required=True)
    valueMedian = db.FloatProperty()
    valueStdev = db.FloatProperty()
    valueMin = db.FloatProperty()
    valueMax = db.FloatProperty()


# Temporarily log reports sent by bots
class ReportLog(db.Model):
    timestamp = db.DateTimeProperty(required=True)
    headers = db.TextProperty()
    payload = db.TextProperty()
