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

from google.appengine.ext import db

from datetime import timedelta
import time


class ActiveWorkItems(db.Model):
    queue_name = db.StringProperty()
    item_ids = db.ListProperty(int)
    item_dates = db.ListProperty(float)
    date = db.DateTimeProperty(auto_now_add=True)

    def deactivate_expired(self, now):
        one_hour_ago = time.mktime((now - timedelta(minutes=60)).timetuple())
        nonexpired_item_ids = []
        nonexpired_item_dates = []
        for i in range(len(self.item_ids)):
            if self.item_dates[i] > one_hour_ago:
                nonexpired_item_ids.append(self.item_ids[i])
                nonexpired_item_dates.append(self.item_dates[i])
        self.item_ids = nonexpired_item_ids
        self.item_dates = nonexpired_item_dates

    def next_item(self, work_item_ids, now):
        for item_id in work_item_ids:
            if item_id not in self.item_ids:
                self.item_ids.append(item_id)
                self.item_dates.append(time.mktime(now.timetuple()))
                return item_id
        return None
