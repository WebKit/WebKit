# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2018, 2019 Apple Inc. All rights reserved.
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
#
# This the client designed to talk to Tools/QueueStatusServer.

import sys

from webkitpy.common.config.urls import statusserver_default_host

if sys.version_info > (3, 0):
    from urllib.error import HTTPError
    from urllib.request import Request, urlopen
else:
    from urllib2 import HTTPError, Request, urlopen


class StatusServer:
    def __init__(self, host=statusserver_default_host, browser=None, bot_id=None):
        self.set_host(host)
        self.set_bot_id(bot_id)

    def set_host(self, host):
        self.host = host
        self.url = "http://%s" % self.host

    def set_bot_id(self, bot_id):
        self.bot_id = bot_id

    def results_url_for_status(self, status_id):
        return None

    def submit_to_ews(self, attachment_id):
        return None

    def next_work_item(self, queue_name):
        return None

    def release_work_item(self, queue_name, patch):
        return None

    def release_lock(self, queue_name, patch):
        return None

    def update_work_items(self, queue_name, high_priority_work_items, work_items):
        return None

    def update_status(self, queue_name, status, patch=None, results_file=None):
        return None

    def update_svn_revision(self, svn_revision_number, broken_bot):
        return None

    def patch_status(self, queue_name, patch_id):
        return None

    def svn_revision(self, svn_revision_number):
        return None
