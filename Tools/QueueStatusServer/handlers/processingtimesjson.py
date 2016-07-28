# Copyright (C) 2014 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import datetime

from google.appengine.ext import webapp
from google.appengine.ext.webapp import template

from model.patchlog import PatchLog

# Fall back to simplejson, because we are still on Python 2.5.
try:
    import json
except ImportError:
    import simplejson as json


class ProcessingTimesJSON(webapp.RequestHandler):
    def _resultFromFinalStatus(self, status_message, queue_name):
        if status_message == "Pass":
            return "pass"
        elif status_message == "Fail":
            return "fail"
        elif "did not process patch" in status_message:
            return "not processed"
        elif status_message == "Error: " + queue_name + " unable to apply patch.":
            return "could not apply"
        else:
            return "internal error"

    def _fetch_patch_log(self, start_date, end_date):
        all_entries = PatchLog.all().filter('date >', start_date).filter('date <', end_date).fetch(limit=None)
        result = {}
        for entry in all_entries:
            result.setdefault(entry.attachment_id, {})
            result[entry.attachment_id][entry.queue_name] = {
                "date": entry.date,
                "wait_duration": entry.wait_duration,
                "process_duration": entry.process_duration,
                "retry_count": entry.retry_count,
                "resolution": self._resultFromFinalStatus(entry.latest_message, entry.queue_name) if entry.finished else "in progress"
            }
        return result

    def get(self, start_year, start_month, start_day, start_hour, start_minute, start_second, end_year, end_month, end_day, end_hour, end_minute, end_second):
        self.response.headers["Access-Control-Allow-Origin"] = "*"
        self.response.headers['Content-Type'] = 'application/json'

        patch_log = self._fetch_patch_log(datetime.datetime(int(start_year), int(start_month), int(start_day), int(start_hour), int(start_minute), int(start_second)),
            datetime.datetime(int(end_year), int(end_month), int(end_day), int(end_hour), int(end_minute), int(end_second)))
        dthandler = lambda obj: (obj.isoformat() + "Z") if isinstance(obj, datetime.datetime) or isinstance(obj, datetime.date) else None
        self.response.out.write(json.dumps(patch_log, default=dthandler))
