#!/usr/bin/env python
# Copyright (c) 2009, Google Inc. All rights reserved.
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

class PersistentPatchCollectionDelegate:
    def collection_name(self):
        raise NotImplementedError, "subclasses must implement"

    def fetch_potential_patch_ids(self):
        raise NotImplementedError, "subclasses must implement"

    def status_server(self):
        raise NotImplementedError, "subclasses must implement"

    def is_terminal_status(self, status):
        raise NotImplementedError, "subclasses must implement"


class PersistentPatchCollection:
    def __init__(self, delegate):
        self._delegate = delegate
        self._name = self._delegate.collection_name()
        self._status = self._delegate.status_server()
        self._status_cache = {}

    def _cached_status(self, patch_id):
        cached = self._status_cache.get(patch_id)
        if cached:
            return cached
        status = self._status.patch_status(self._name, patch_id)
        if status and self._delegate.is_terminal_status(status):
            self._status_cache[patch_id] = status
        return status

    def next(self):
        patch_ids = self._delegate.fetch_potential_patch_ids()
        for patch_id in patch_ids:
            status = self._cached_status(patch_id)
            if not status or not self._delegate.is_terminal_status(status):
                return patch_id
